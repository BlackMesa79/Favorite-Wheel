#pragma once
#include "Favorites.h"
#include "Settings.h"
#include "NameEditor.h"
#include "FaceLightClient.h"
#include <vector>
namespace Wheel {
    inline constexpr const char* menuName = "FavoriteWheelMenu";
    struct View {
        bool functions=false;
        bool faceLightAvailable=false;
        FaceLight::Section functionSection=FaceLight::Section::Outfits;
        int outfitDialog=0; // 1 new name, 2 rename, 3 manage, 4 overwrite confirmation, 5 delete confirmation
        std::uint32_t outfitId=0;
        NameEditor outfitName;
        std::string inventoryGlyphs;
        bool open = false;
        bool animateClose = false;
        bool settingsOpen = false, capturingKey = false, captureSwitch=false, saveError = false;
        Settings config;
        Category category = Category::Weapons;
        int page = 0;
        float x = 0, y = 0;
        std::vector<Item> items;
    };
    View Snapshot();
    bool IsOpen();
    bool InstallWheel();
    void SetGameActive(bool active);
    void Cancel(bool effects=false);
    bool InstallRenderer();
    bool RendererReady();
    void DrawWheel(const View& view,float opacity=1.f,float expansion=1.f);
    void ResetVisualFeedback(); // Render thread only; new open/session must not reuse old hover state.
    void SetViewport(float width, float height);
}
