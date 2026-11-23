#pragma once
namespace Wheel::FrameworkSettings {
    void Register();
    // Follow the framework's input-blocking state, including Resume Game.
    bool BlockingWindowOpen();
}
