#pragma once
namespace Wheel::FrameworkSettings {
    void Register();
    // True while SKSE Menu Framework owns the screen: its main window or any
    // other blocking window is open. Favorite Wheel must never open on top of it
    // and must hand every input device back until the framework window closes.
    bool BlockingWindowOpen();
}
