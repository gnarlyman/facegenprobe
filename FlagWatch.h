#pragma once

namespace StormLog {
namespace FlagWatch {
    // Install the TESCharacter::Update hook and register VEH.
    // No-op if cfg.bEnableFlagWatch == 0.
    bool Install();

    // Called from Main.cpp OnExitGame, before Pipeline::Shutdown.
    void Shutdown();
}
}
