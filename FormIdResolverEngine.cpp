// FormIdResolverEngine.cpp — real xOBSE/engine lookups
//
// Design: avoid linking against xOBSE Game*.cpp files (they pull in hundreds of
// transitive dependencies).  Instead we define the three global symbols we need
// with static linkage directly here, using the same hardcoded addresses the
// xOBSE sources use.  Member functions that would require Game*.cpp are replaced
// by direct struct-field reads.
//
// Symbols used and their sources:
//   LookupFormByID  — GameAPI.cpp  : 0x0046B250
//   g_dataHandler   — GameAPI.cpp  : 0x00B33A98
//   g_thePlayer     — GameObjects.cpp : 0x00B333C4
//
// Field reads (header-only, no .cpp needed):
//   TESForm::refID               — GameForms.h offset 0x00C
//   TESActorBase::fullName.name  — GameForms.h; TESFullName at 0x0A0,
//                                   BSStringT::m_data at +0x00
//   ModEntry::data->name         — GameData.h; char name[0x104] at offset 0x01C
//   PlayerCharacter::parentCell  — GameObjects.h offset 0x040
//   TESObjectCELL inherits TESForm::refID at 0x00C

#include "FormIdResolver.h"

// Pull in the struct definitions we need (header-only).
// We do NOT #include GameAPI.h because it declares the externs we are
// replacing with static definitions below.
#include "obse/GameForms.h"    // TESForm, TESActorBase, TESFullName, TESObjectCELL
#include "obse/GameData.h"     // DataHandler, ModEntry
#include "obse/GameObjects.h"  // PlayerCharacter, g_thePlayer (extern decl)

#include <unordered_map>
#include <string>
#include <mutex>

// ---------------------------------------------------------------------------
// Local definitions of the three engine globals we need.
// Declared static to avoid ODR conflicts with any other TU that might later
// include the originals via GameAPI.h / GameObjects.h.
// ---------------------------------------------------------------------------
namespace {

// Function-pointer type for the engine's form-lookup routine.
typedef TESForm* (__cdecl *FnLookupFormByID)(UInt32 id);
static const FnLookupFormByID s_LookupFormByID =
    reinterpret_cast<FnLookupFormByID>(0x0046B250);

// DataHandler** at 0x00B33A98
static DataHandler** const s_dataHandler =
    reinterpret_cast<DataHandler**>(0x00B33A98);

// PlayerCharacter** at 0x00B333C4
static PlayerCharacter** const s_thePlayer =
    reinterpret_cast<PlayerCharacter**>(0x00B333C4);

// ---------------------------------------------------------------------------
// Look up a mod name by load-order index by walking the DataHandler modList.
// Equivalent to DataHandler::GetNthModName but inline — avoids linking GameData.cpp.
// ---------------------------------------------------------------------------
static const char* GetModNameByIndex(UInt8 modIndex)
{
    if (!s_dataHandler || !*s_dataHandler) return nullptr;

    // Walk the linked list counting loaded entries.
    UInt8 idx = 0;
    for (ModEntry* e = &(*s_dataHandler)->modList; e; e = e->next)
    {
        if (!e->IsLoaded()) continue;
        if (idx == modIndex)
            return (e->data) ? e->data->name : nullptr;
        ++idx;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Get the display name for any TESForm.  For actor-base forms (NPC_, creature)
// we read TESActorBase::fullName.name.m_data directly — no function call needed.
// For other form types we fall back to nullptr (caller stores empty string).
//
// Note: EDITOR_SPECIFIC(EditorData) expands to nothing at runtime so there is
// no editorID field on TESForm at runtime.  EDID is not in game memory for
// most form types; the display name is the best we can do.
// ---------------------------------------------------------------------------
static const char* GetFormDisplayName(TESForm* form)
{
    if (!form) return nullptr;

    // Most NPC/creature base records are TESActorBase subclasses.
    // TESActorBase::fullName is at offset 0x0A0; TESFullName::name is at +0x04;
    // BSStringT::m_data is at +0x00.
    // We check typeID to avoid mis-casting unrelated forms.
    const UInt8 t = form->typeID;
    // kFormType_NPC = 0x13, kFormType_Creature = 0x14 (from GameForms.h enum FormType)
    if (t == kFormType_NPC || t == kFormType_Creature)
    {
        TESActorBase* ab = static_cast<TESActorBase*>(form);
        return ab->fullName.name.m_data;  // may be nullptr or ""
    }

    return nullptr;  // unsupported type — caller will use empty string
}

// ---------------------------------------------------------------------------
// Per-FormID cache.
// ---------------------------------------------------------------------------
struct Cached {
    std::string name;  // display name (NPC full name)
    std::string mod;   // mod filename (e.g. "Cobl Glue.esp")
};

static std::unordered_map<UInt32, Cached> s_cache;
static std::mutex s_mutex;

} // anonymous namespace

// ---------------------------------------------------------------------------

namespace StormLog {

void FormIdResolver::Resolve(uint32_t formID,
                             const char** edidOut,
                             const char** modOut)
{
    static const char* empty = "";
    *edidOut = empty;
    *modOut  = empty;

    if (!formID) return;

    // Fast path: already cached.
    {
        std::lock_guard<std::mutex> lk(s_mutex);
        auto it = s_cache.find(formID);
        if (it != s_cache.end()) {
            *edidOut = it->second.name.c_str();
            *modOut  = it->second.mod.c_str();
            return;
        }
    }

    // Slow path: first time we have seen this FormID.
    Cached c;

    // --- display name ---
    if (s_LookupFormByID)
    {
        TESForm* form = s_LookupFormByID(static_cast<UInt32>(formID));
        const char* nm = GetFormDisplayName(form);
        if (nm && nm[0]) c.name = nm;
    }

    // --- mod source: top 8 bits of FormID are the load-order index ---
    const UInt8 modIndex = static_cast<UInt8>((formID >> 24) & 0xFF);
    const char* modName = GetModNameByIndex(modIndex);
    if (modName && modName[0]) c.mod = modName;

    // Store in cache and return stable c_str() pointers.
    std::lock_guard<std::mutex> lk(s_mutex);
    auto& entry = s_cache[formID];
    entry = std::move(c);
    *edidOut = entry.name.c_str();
    *modOut  = entry.mod.c_str();
}

uint32_t FormIdResolver::CurrentCellFormId()
{
    if (!s_thePlayer || !*s_thePlayer) return 0;

    TESObjectCELL* cell = (*s_thePlayer)->parentCell;
    if (!cell) return 0;

    // TESObjectCELL inherits TESForm::refID at offset 0x00C.
    return static_cast<uint32_t>(cell->refID);
}

bool FormIdResolver::PlayerWorldPos(float& x, float& y, float& z, uint32_t& cellFid)
{
    if (!s_thePlayer || !*s_thePlayer) return false;

    // PlayerCharacter -> ... -> TESObjectREFR: public posX/posY/posZ at 0x02C.
    PlayerCharacter* p = *s_thePlayer;
    x = p->posX;
    y = p->posY;
    z = p->posZ;

    TESObjectCELL* cell = p->parentCell;
    cellFid = cell ? static_cast<uint32_t>(cell->refID) : 0;
    return true;
}

} // namespace StormLog
