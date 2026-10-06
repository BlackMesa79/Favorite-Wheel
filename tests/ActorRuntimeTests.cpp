#include "ActorRuntime.h"
#include "RuntimeSupport.h"
#include <SKSE/SKSE.h>
#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
    bool dead = false, wrongSlot = false, drank = false, updated = false;
    RE::AlchemyItem *expectedPotion = nullptr;
    RE::ExtraDataList *expectedExtra = nullptr;
    void Check(bool condition, const char *message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }
    bool DeathSlot(const RE::Actor *, bool notEssential)
    {
        Check(notEssential, "IsDead argument lost");
        return dead;
    }
    bool WrongDeathSlot(const RE::Actor *, bool)
    {
        wrongSlot = true;
        return true;
    }
    bool PotionSlot(RE::Actor *, RE::AlchemyItem *potion, RE::ExtraDataList *extra)
    {
        Check(potion == expectedPotion && extra == expectedExtra, "Potion arguments changed");
        drank = true;
        return true;
    }
    void UpdateSlot(RE::Actor *, float delta)
    {
        updated = delta == .25f;
    }
    __declspec(noinline) void InvokeUpdate(RE::Actor *actor)
    {
        actor->Update(.25f);
    }
    __declspec(noinline) bool Legacy(const RE::Actor *actor)
    {
        return actor->IsDead();
    }
    __declspec(noinline) bool Fixed(const RE::Actor *actor)
    {
        return Wheel::ActorRuntime::IsDead(actor);
    }
} // namespace
int main()
{
    // Each pass switches CommonLib's actual runtime selector, then exercises
    // the engine-shaped fixture. No Skyrim process or real actor required.
    for (const auto version : {REL::Version{1, 5, 97, 0}, REL::Version{1, 6, 1170, 0}, REL::Version{1, 7, 99, 0},
                               REL::Version{1, 7, 104, 0}})
    {
        Check(REL::Module::mock(version), "mock runtime initialization");
        dead = wrongSlot = drank = updated = false;
        std::cout << "Runtime " << version.string() << '\n';
        std::array<std::uintptr_t, 0x200> table;
        table.fill(reinterpret_cast<std::uintptr_t>(&WrongDeathSlot));
        table[0x99] = reinterpret_cast<std::uintptr_t>(&DeathSlot);
        table[0x10F] = reinterpret_cast<std::uintptr_t>(&PotionSlot);
        table[Wheel::RuntimeSupport::playerUpdateSlot] = reinterpret_cast<std::uintptr_t>(&UpdateSlot);
        // Multi-runtime sizeof(Actor) describes a partial layout; reserve native storage.
        alignas(RE::Actor) std::array<std::byte, 0x400> storage{};
        *reinterpret_cast<std::uintptr_t **>(storage.data()) = table.data();
        auto actor = reinterpret_cast<RE::Actor *>(storage.data());
        const bool legacyDead = Legacy(actor);
        std::cout << "Legacy direct IsDead: dead=" << legacyDead << ", wrong-slot=" << wrongSlot << '\n';
        Check(!legacyDead && !wrongSlot, "Upstream direct IsDead dispatch regressed");
        wrongSlot = false;
        Check(!Fixed(actor) && !wrongSlot, "Living actor was rejected or called the wrong slot");
        dead = true;
        Check(Fixed(actor) && !wrongSlot, "Dead actor was not blocked");
        Check(Wheel::ActorRuntime::IsDead(nullptr), "Null actor must be blocked");
        std::byte potionToken{}, extraToken{};
        expectedPotion = reinterpret_cast<RE::AlchemyItem *>(&potionToken);
        expectedExtra = reinterpret_cast<RE::ExtraDataList *>(&extraToken);
        Check(Wheel::ActorRuntime::DrinkPotion(actor, expectedPotion, expectedExtra) && drank,
              "Potion did not call SE/AE slot 0x10F");
        Check(!Wheel::ActorRuntime::DrinkPotion(nullptr, expectedPotion, expectedExtra), "Null actor potion action");
        Check(!Wheel::ActorRuntime::DrinkPotion(actor, nullptr, expectedExtra), "Null potion action");
        InvokeUpdate(actor);
        Check(updated && !wrongSlot, "Player update hook no longer matches upstream Actor::Update slot");
    }
    std::cout << "SE/AE/1.7.99/1.7.104 actor dispatch passed: IsDead=0x99, DrinkPotion=0x10F, Update=0xAD\n";
}
