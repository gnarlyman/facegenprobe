#include "FormIdResolver.h"

namespace StormLog {

void FormIdResolver::Resolve(uint32_t, const char** edidOut, const char** modOut) {
    static const char* empty = "";
    *edidOut = empty;
    *modOut  = empty;
}

uint32_t FormIdResolver::CurrentCellFormId() { return 0; }

} // namespace StormLog
