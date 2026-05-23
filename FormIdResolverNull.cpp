#include "FormIdResolver.h"

namespace StormLog {

void FormIdResolver::Resolve(uint32_t, const char** edidOut, const char** modOut) {
    static const char* empty = "";
    *edidOut = empty;
    *modOut  = empty;
}

uint32_t FormIdResolver::CurrentCellFormId() { return 0; }

bool FormIdResolver::PlayerWorldPos(float&, float&, float&, uint32_t&) { return false; }

} // namespace StormLog
