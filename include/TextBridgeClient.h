#pragma once
#include <Windows.h>
#include "TextInputPolicy.h"
#include <cstdint>

namespace Wheel::TextBridge {
    struct State {bool available=false,enabled=false,active=false,composing=false;unsigned hotkey=0;};
    inline State Query() {
        using QueryFn=std::uint32_t (*)(std::uint32_t,std::uint32_t*);
        const auto module=GetModuleHandleW(L"SkyrimTextBridge.dll");
        auto query=module?reinterpret_cast<QueryFn>(GetProcAddress(module,"SkyrimTextBridge_QueryInputV1")):nullptr;
        if(!query)return {};
        std::uint32_t hotkey=0;const auto flags=query(1,&hotkey);
        return {bool(flags&1),bool(flags&2),bool(flags&4),bool(flags&8),hotkey};
    }
    inline void Reset() {
        using ResetFn=void (*)();
        const auto module=GetModuleHandleW(L"SkyrimTextBridge.dll");
        auto reset=module?reinterpret_cast<ResetFn>(GetProcAddress(module,"SkyrimTextBridge_ResetInputV1")):nullptr;
        if(reset)reset();
    }
}

namespace Wheel::SimpleIME {
    inline bool Available(){return GetModuleHandleW(L"SimpleIME.dll")!=nullptr;}
    inline HWND window=nullptr;
    inline bool ownsProperty=false;
    inline void Begin() {
        if(window || !Available())return;
        window=GetForegroundWindow();
        DWORD process=0;GetWindowThreadProcessId(window,&process);
        if(process!=GetCurrentProcessId()){window=nullptr;return;}
        if(!GetPropW(window,L"SimpleIME.ImeAware"))
            ownsProperty=SetPropW(window,L"SimpleIME.ImeAware",reinterpret_cast<HANDLE>(&window))!=FALSE;
    }
    inline void End() {
        if(ownsProperty && GetPropW(window,L"SimpleIME.ImeAware")==reinterpret_cast<HANDLE>(&window))
            RemovePropW(window,L"SimpleIME.ImeAware");
        ownsProperty=false;window=nullptr;
    }
    inline bool Composing() {
        const auto tick=reinterpret_cast<std::uintptr_t>(GetPropW(window,L"SimpleIME.Composing"));
        return window && FreshComposition(tick,GetTickCount64());
    }
}
