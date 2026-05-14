#include "FormIdResolver.h"
#include "obse/GameForms.h"
#include "obse/GameData.h"
#include "obse/GameObjects.h"

namespace StormLog {

void FormIdResolver::Resolve(uint32_t formID, const char** edidOut, const char** modOut) {
    static const char* empty = "";
    *edidOut = empty;
    *modOut  = empty;
    // Stub: real lookup wired in Task 20 (after L1 is firing in-game so the
    // implementer can validate xOBSE API names by experiment).
}

uint32_t FormIdResolver::CurrentCellFormId() {
    return 0; // Stub: real impl in Task 20.
}

} // namespace StormLog
