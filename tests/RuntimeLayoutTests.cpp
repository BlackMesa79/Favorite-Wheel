#include "RuntimeSupport.h"
#include "MagicCategories.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

static_assert(Wheel::MagicCategory(false,RE::MagicSystem::SpellType::kSpell)==Wheel::Category::Spells);
static_assert(Wheel::MagicCategory(false,RE::MagicSystem::SpellType::kPower)==Wheel::Category::Powers);
static_assert(Wheel::MagicCategory(false,RE::MagicSystem::SpellType::kLesserPower)==Wheel::Category::Powers);
static_assert(Wheel::MagicCategory(false,RE::MagicSystem::SpellType::kVoicePower)==Wheel::Category::Powers);
static_assert(Wheel::MagicCategory(true,RE::MagicSystem::SpellType::kVoicePower)==Wheel::Category::Shouts);
static_assert(!Wheel::MagicCategory(false,RE::MagicSystem::SpellType::kAbility));
static_assert(!Wheel::MagicCategory(false,RE::MagicSystem::SpellType::kDisease));

namespace
{
    void Check(bool value, const char *what)
    {
        if (!value)
        {
            std::cerr << what << '\n';
            std::exit(1);
        }
    }
    template <class T, class V> void Write(T &bytes, std::size_t offset, V value)
    {
        Check(offset + sizeof(value) <= bytes.size(), "fixture overflow");
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
    const std::array criticalIDs = {
        REL::RelocationID(67315, 68617),   // input dispatch enclosing function
        REL::RelocationID(524907, 411393), // renderer singleton
        REL::RelocationID(37938, 38894),   // EquipObject
        REL::RelocationID(37945, 38901),   // UnequipObject
        REL::RelocationID(15757, 15995),   // instance value
        REL::RelocationID(15788, 16026),   // instance enchantment
        REL::RelocationID(39175, 40249),   // player armor display value
        REL::RelocationID(39179, 40253),   // player damage display value
        REL::RelocationID(11213, 11321),   // spell cost
        REL::RelocationID(517014, 403521), // player singleton
        REL::RelocationID(514705, 400863), // control map singleton
        REL::RelocationID(515124, 401263), // menu controls singleton
        REL::RelocationID(516858, 403337), // magic favorites singleton
        REL::RelocationID(37939, 38895),   // EquipSpell
        REL::RelocationID(37941, 38897),   // EquipShout
        REL::RelocationID(523657, 410196), // BSTimer singleton
        REL::RelocationID(511882, 388442), // current global multiplier
        REL::RelocationID(511883, 388443), // target global multiplier
        REL::RelocationID(66988, 68245),   // SetGlobalTimeMultiplier
        REL::RelocationID(14108, 14298)   // ScriptEventSourceHolder for live inventory dirty events
    };
    void AddressLibrary(const wchar_t *path)
    {
        Check(REL::IDDB::inject(path, REL::Module::get().version()), "address library auto-detection");
        // Confirm actual installed databases contain the critical hook/engine IDs.
        // This verifies ID resolution, not executable instructions at the offset.
        for (const auto id : criticalIDs)
        {
            const auto offset = id.offset();
            Check(offset != 0, "critical relocation resolves to zero");
            std::cout << "  ID " << id.id() << " -> 0x" << std::hex << offset << std::dec << '\n';
        }
        Check(RE::VTABLE_PlayerCharacter[0].offset() != 0, "player update vtable resolves");
        REL::IDDB::reset();
    }
    void DenseAddressLibrary(REL::Version version)
    {
        const auto path = std::filesystem::temp_directory_path() /
                          ("FavoriteWheel-v5-" + std::to_string(GetCurrentProcessId()) + ".bin");
        constexpr std::uint64_t vtableID = 208040; // Pinned upstream PlayerCharacter primary AE vtable ID.
        std::uint64_t maxID = vtableID;
        for (const auto id : criticalIDs)
            maxID = std::max(maxID, id.id());
        Check(maxID < 1000000, "synthetic dense fixture unexpectedly large");
        std::vector<std::uint32_t> offsets(maxID + 1);
        for (const auto id : criticalIDs)
            offsets[id.id()] = 0x100000 + static_cast<std::uint32_t>(id.id()) * 8;
        offsets[vtableID] = 0x800000;
        std::ofstream out(path, std::ios::binary);
        const auto write = [&](const auto &value) { out.write(reinterpret_cast<const char *>(&value), sizeof(value)); };
        const std::int32_t format = 5, pointerSize = 8, dataFormat = 0,
                           count = static_cast<std::int32_t>(offsets.size());
        const std::array<std::uint32_t, 4> parts{version[0], version[1], version[2], version[3]};
        std::array<char, 64> name{};
        std::memcpy(name.data(), "SkyrimSE.exe", 12);
        write(format);
        write(parts);
        write(name);
        write(pointerSize);
        write(dataFormat);
        write(count);
        out.write(reinterpret_cast<const char *>(offsets.data()), offsets.size() * sizeof(offsets[0]));
        out.close();
        Check(bool(out), "write dense address library fixture");
        Check(REL::IDDB::inject(path.wstring(), version), "format 5 auto-detection");
        for (const auto id : criticalIDs)
            Check(id.offset() == offsets[id.id()], "dense AE ID resolution");
        Check(RE::VTABLE_PlayerCharacter[0].offset() == offsets[vtableID], "dense player vtable ID resolution");
        REL::IDDB::reset();
        std::filesystem::remove(path);
    }
    void PluginMetadata()
    {
        const auto path = std::filesystem::absolute("build/windows/x64/release/FavoriteWheel.dll");
        const auto dll = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(dll != nullptr, "load plugin for metadata inspection");
        const auto info = reinterpret_cast<const SKSE::PluginVersionData *>(GetProcAddress(dll, "SKSEPlugin_Version"));
        Check(info && info->GetPluginName() == "FavoriteWheel", "exported plugin name");
        Check(info->GetPluginVersion() == REL::Version{0, 5, 2, 0}, "exported plugin version");
        Check(info->versionIndependenceEx & SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5,
              "exported Address Library v5 flag");
        Check(info->versionIndependence & SKSE::PluginVersionData::kVersionIndependent_AddressLibraryPostAE,
              "exported legacy AE Address Library flag");
        Check(GetProcAddress(dll, "SKSEPlugin_Load") != nullptr, "plugin load export");
        FreeLibrary(dll);
    }
} // namespace
int wmain(int argc, wchar_t **argv)
{
    using namespace Wheel::RuntimeSupport;
    Check(argc == 1 || argc == 3 || argc == 5 || argc == 6 || argc == 7,
          "optional args: 1.5.97, 1.6.1170, then 1.7.99, 1.7.104, then 1.6.640, then GOG 1.6.1179 address libraries");
    Check(Supported({1, 5, 97, 0}) && Supported({1, 6, 640, 0}) && Supported({1, 6, 1170, 0}) && Supported({1, 7, 99, 0}) &&
              Supported({1, 7, 104, 0}) && Supported({1, 6, 1179, 0}),
          "target runtimes accepted");
    for (const auto version :
         {REL::Version{1, 5, 80, 0}, REL::Version{1, 6, 659, 0}, REL::Version{1, 6, 1130, 0}, REL::Version{1, 4, 15, 0},
          REL::Version{1, 7, 98, 0}, REL::Version{1, 7, 105, 0}, REL::Version{1, 7, 104, 1},
          REL::Version{1, 6, 1178, 0}, REL::Version{1, 6, 1180, 0}, REL::Version{1, 6, 1179, 1}})
        Check(!Supported(version), "unaudited releases and VR stay rejected");
    struct Layout
    {
        REL::Version version;
        std::size_t controls, player, state;
        std::uint64_t input;
    };
    const std::array layouts = {
        Layout{{1, 5, 97, 0}, 0xE8, 0x3D8, 0xB8, 67315}, Layout{{1, 6, 1170, 0}, 0xF0, 0x3E0, 0xC0, 68617},
        Layout{{1, 7, 99, 0}, 0xF0, 0x3E8, 0xC0, 68617}, Layout{{1, 7, 104, 0}, 0xF0, 0x3E8, 0xC0, 68617},
        // Preserve the existing optional-library argument order; append 1.6.640.
        Layout{{1, 6, 640, 0}, 0xE8, 0x3E0, 0xC0, 68617},
        Layout{{1, 6, 1179, 0}, 0xF0, 0x3E0, 0xC0, 68617}};
    unsigned index = 0;
    for (const auto &layout : layouts)
    {
        Check(REL::Module::mock(layout.version), "mock runtime initialization");
        Check(REL::Module::IsAE() == (layout.version.minor() >= 6), "Steam and GOG 1.6/1.7 select AE family");
        Check(REL::RelocationID(inputDispatchSE, inputDispatchAE).id() == layout.input, "SE/AE input ID selection");
        // Native byte offsets are independent of the accessors under test.
        // Poison the alternate version so a fixed layout cannot pass both.
        alignas(RE::ControlMap) std::array<std::byte, 0x150> controls{};
        auto map = reinterpret_cast<RE::ControlMap *>(controls.data());
        controls[(layout.controls == 0xE8 ? 0xF0 : 0xE8) + 0x38] = std::byte{99};
        controls[layout.controls + 0x38] = std::byte{3};
        Check(reinterpret_cast<std::byte *>(&map->GetRuntimeData()) == controls.data() + layout.controls,
              "versioned ControlMap base");
        Check(map->GetRuntimeData().textEntryCount == 3, "versioned text entry count");
        alignas(RE::PlayerCharacter) std::array<std::byte, 0x2000> player{};
        auto character = reinterpret_cast<RE::PlayerCharacter *>(player.data());
        Check(reinterpret_cast<std::byte *>(&character->GetPlayerRuntimeData()) == player.data() + layout.player,
              "versioned player data");
        Check(reinterpret_cast<std::byte *>(character->AsActorState()) == player.data() + layout.state,
              "versioned actor state base");
        const auto actorOffset = layout.state == 0xB8 ? 0xE0 : 0xE8;
        Check(reinterpret_cast<std::byte *>(&character->GetActorRuntimeData()) == player.data() + actorOffset,
              "versioned actor runtime block");
        alignas(RE::BSTimer) std::array<std::byte,0x40> timerBytes{};
        Write(timerBytes,0x34,static_cast<std::uint32_t>(2));Write(timerBytes,0x3A,true);
        auto timer=reinterpret_cast<RE::BSTimer*>(timerBytes.data());
        Check(timer->pauseCount==2 && timer->useGlobalTimeMultiplierTarget,"Native timer nested-pause/target fields keep their offsets");
        alignas(RE::ButtonEvent) std::array<std::byte, 0x38> button{};
        Write(button, 0x28, 1.f);
        Write(button, 0x2C, 0.f);
        auto event = reinterpret_cast<RE::ButtonEvent *>(button.data());
        Check(event->IsDown() && !event->IsHeld(), "native button press read");
        event->GetRuntimeData().value = 0;
        event->GetRuntimeData().heldDownSecs = .5f;
        float value, held;
        std::memcpy(&value, button.data() + 0x28, sizeof(value));
        std::memcpy(&held, button.data() + 0x2C, sizeof(held));
        Check(value == 0 && held == .5f && event->IsUp(), "input gate writes native button release");
        event->GetRuntimeData().value=1.f;
        event->GetRuntimeData().heldDownSecs=0.f;
        Check(event->IsDown() && !event->IsHeld(),"Resumed movement writes a native fresh-down event at the correct offset");
        alignas(RE::ExtraHotkey) std::array<std::byte, 0x18> extraHotkey{};
        Write(extraHotkey,0x10,static_cast<std::uint8_t>(255));
        auto quick=reinterpret_cast<RE::ExtraHotkey*>(extraHotkey.data());
        Check(quick->hotkey.underlying()==255,"native unbound favorite marker");
        quick->hotkey=RE::ExtraHotkey::Hotkey::kSlot8;
        Check(extraHotkey[0x10]==std::byte{7},"quick-slot assignment writes native instance byte");
        quick->hotkey=RE::ExtraHotkey::Hotkey::kUnbound;
        Check(extraHotkey[0x10]==std::byte{255},"unbinding retains ExtraHotkey marker and writes unbound value");
        alignas(RE::MagicFavorites) std::array<std::byte,0x40> magicFavorites{};
        const auto favoriteData=reinterpret_cast<RE::MagicFavorites*>(magicFavorites.data());
        Check(reinterpret_cast<const std::byte*>(&favoriteData->hotkeys)==magicFavorites.data()+0x28,"native magic quick-slot array offset");
        alignas(RE::BSGraphics::Renderer) std::array<std::byte, 0x3000> graphics{};
        // Flat render data starts at 0x10; swap chain 0x58+0x18,
        // UI framebuffer RTV 0xA58+0x10. Never dereference sentinel pointers.
        const std::uintptr_t chain = 0x12340000, rtv = 0x56780000;
        Write(graphics, 0x70, chain);
        Write(graphics, 0xA68, rtv);
        auto renderer = reinterpret_cast<RE::BSGraphics::Renderer *>(graphics.data());
        Check(reinterpret_cast<std::uintptr_t>(renderer->GetRuntimeData().renderWindows[0].swapChain) == chain,
              "native swap-chain access");
        Check(reinterpret_cast<std::uintptr_t>(
                  renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].RTV) == rtv,
              "native UI framebuffer access");
        std::cout << layout.version.string()
                  << ": runtime gate, input ID, control/player/actor/button/render layouts passed\n";
        if (index < static_cast<unsigned>(argc - 1))
            AddressLibrary(argv[index + 1]);
        if (layout.version.minor() == 7)
            DenseAddressLibrary(layout.version);
        ++index;
    }
    Check(REL::IDDB::inject(L"extern/CommonLibVR/tests/REL/version-1-5-97-0.bin", REL::Version{1, 5, 97, 0}),
          "legacy format 1 fixture");
    Check(REL::IDDB::inject(L"extern/CommonLibVR/tests/REL/versionlib-1-6-1170-0.bin", REL::Version{1, 6, 1170, 0}),
          "legacy format 2 fixture");
    Check(REL::IDDB::get().id2offset(11483) == 0x14FCD0, "legacy AE lookup value");
    REL::IDDB::reset();
    REL::Module::reset();
    PluginMetadata();
    std::cout << "Runtime layouts, address-library formats 1/2/5 and DLL metadata passed\n";
}
