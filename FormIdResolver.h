#pragma once
#include <cstdint>

namespace StormLog {

// Pure interface; one of the impl .cpp files is linked per target.
namespace FormIdResolver {
    // Fills edidOut and modOut with cached strings (lifetime: process). Both
    // outputs may be empty strings if resolution fails. Never returns null.
    void Resolve(uint32_t formID,
                 const char** edidOut,
                 const char** modOut);

    // Returns the FormID of the player's current cell, or 0.
    uint32_t CurrentCellFormId();
}

} // namespace StormLog
