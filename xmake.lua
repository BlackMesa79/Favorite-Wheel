set_project("FavoriteWheel")
set_version("0.9.0")
set_languages("cxx23")
set_encodings("utf-8")
set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)
set_config("tests", false)
add_rules("mode.debug", "mode.release", "mode.releasedbg")
includes("extern/CommonLibVR")

option("deploy_dir")
    set_default("H:/Games/Dev Skyrim/mods/36 - FavoriteWheel")
    set_showmenu(true)
    set_description("MO2 mod root for automatic deployment; empty disables deployment")
option_end()

rule("favoritewheel.version")
    on_load(function(target)
        local version = target:version()
        local major, minor, patch = version:match("^(%d+)%.(%d+)%.(%d+)$")
        assert(major, "FavoriteWheel requires a major.minor.patch version")
        target:add("defines", 'FAVORITEWHEEL_VERSION_STRING="' .. version .. '"',
            "FAVORITEWHEEL_VERSION_MAJOR=" .. major, "FAVORITEWHEEL_VERSION_MINOR=" .. minor,
            "FAVORITEWHEEL_VERSION_PATCH=" .. patch)
    end)
rule_end()

target("wheel-imgui")
    set_kind("static")
    add_files("extern/imgui/imgui.cpp", "extern/imgui/imgui_draw.cpp", "extern/imgui/imgui_tables.cpp", "extern/imgui/imgui_widgets.cpp", "extern/imgui/backends/imgui_impl_dx11.cpp")
    add_includedirs("extern/imgui", "extern/imgui/backends", {public = true})
    add_syslinks("d3d11", "dxgi", "d3dcompiler")

target("FavoriteWheel")
    set_kind("shared")
    add_rules("favoritewheel.version")
    add_rules("commonlibsse-ng.plugin", {name = "FavoriteWheel", author = "BlackMesa79", description = "Categorized vanilla favorites wheel"})
    add_deps("commonlibsse-ng", "wheel-imgui")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    set_pcxxheader("include/PCH.h")
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "user32", "windowscodecs", "ole32")
    after_build(function(target)
        local moddir = get_config("deploy_dir")
        if not moddir or moddir == "" then return end
        local pluginsdir = path.join(moddir, "SKSE", "Plugins")
        os.mkdir(pluginsdir)
        os.cp(target:targetfile(), path.join(pluginsdir, "FavoriteWheel.dll"))
        -- Main settings use built-in defaults; Apply creates the player-owned INI.
        -- Never deploy the source checkout's example configuration.
        local themesdir = path.join(pluginsdir, "FavoriteWheel", "Themes")
        local backupsdir = path.join(pluginsdir, "FavoriteWheel", "ThemeBackups", "0.6.0")
        local function theme_backup(file, move)
            os.mkdir(backupsdir)
            local backup = path.join(backupsdir, path.filename(file))
            local index = 1
            while os.isfile(backup) do
                backup = path.join(backupsdir, path.filename(file) .. "." .. index)
                index = index + 1
            end
            if move then os.mv(file, backup) else os.cp(file, backup) end
        end
        -- Only the four retired built-in filenames are archived; custom IDs stay selectable.
        for _, name in ipairs({"ellipse.ini", "frost.ini", "engraved.ini", "minimal.ini"}) do
            local file = path.join(themesdir, name)
            if os.isfile(file) then theme_backup(file, true) end
        end
        -- Upgrade an untouched original Classic. Keep any locally customized Classic.
        local classic = path.join(themesdir, "classic.ini")
        if os.isfile(classic) then
            local legacy = {Name="Classic Gold", Font="", Accent="E7C98BFF", Text="EDEEEAFF", Muted="9EA8B5FF",
                Sector="17202CF0", Empty="0F161FBE", Hover="6B5B3DEB", Panel="151C24FC", Background="000000FF",
                Border="70604BFF", BorderWidth="1", CornerRadius="10", Ornament="0.65", Relief="0.5",
                TextShadow="0.32", IconScale="1", TitleScale="1", LabelScale="1", HoverDuration="0.10", PageDuration="0.12"}
            local untouched, seen = true, {}
            for line in io.lines(classic) do
                local key, value = line:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
                if key then
                    if legacy[key] ~= value or seen[key] then untouched = false end
                    seen[key] = true
                elseif line:find("%S") and not line:match("^%s*[;#]") then untouched = false end
            end
            for _, key in ipairs({"Name", "Font", "Accent", "Text", "Muted", "Sector", "Empty", "Hover", "Panel", "Background", "Border"}) do
                if not seen[key] then untouched = false end
            end
            if untouched then
                theme_backup(classic, false)
                os.cp("assets/Themes/classic.ini", classic)
            end
        end
        for _, kind in ipairs({"Languages", "Themes"}) do
            local destination = path.join(pluginsdir, "FavoriteWheel", kind)
            os.mkdir(destination)
            for _, source in ipairs(os.files(path.join("assets", kind, "*.ini"))) do
                local file = path.join(destination, path.filename(source))
                -- Keep locally customized language/theme files on subsequent deployments.
                if not os.isfile(file) then os.cp(source, file) end
                if kind == "Languages" or kind == "Themes" then
                    local current = io.readfile(file)
                    if kind == "Languages" then
                        local migrate = import("scripts.language_migrations", {rootdir = os.projectdir()})
                        local updated, changed = migrate.main(current, io.readfile(source))
                        if changed > 0 then
                            local backup = file .. ".before-controls-migration"
                            if not os.isfile(backup) then os.cp(file, backup) end
                            io.writefile(file, updated)
                            current = updated
                            print("Updated %d legacy hints in %s", changed, path.filename(file))
                        end
                    end
                    local additions = {}
                    for line in io.lines(source) do
                        local key = line:match("^([%w_]+)=")
                        local skin_key = key == "FrameStyle" or key == "SurfaceTexture" or key == "MaterialStrength" or key == "MaterialZoom"
                        if key and (kind == "Languages" or skin_key) and not ("\n" .. current):find("\n%s*" .. key .. "%s*=") then
                            current = current .. "\n" .. line
                            table.insert(additions, line)
                        end
                    end
                    if #additions > 0 then
                        -- Append bytes only; retain custom values, BOM and line endings.
                        local writer = io.open(file, "ab")
                        writer:write("\n" .. table.concat(additions, "\n") .. "\n")
                        writer:close()
                    end
                end
            end
        end
        local texturesdir = path.join(pluginsdir, "FavoriteWheel", "Themes", "Textures")
        os.mkdir(texturesdir)
        for _, source in ipairs(os.files("assets/Themes/Textures/*.png")) do
            local file = path.join(texturesdir, path.filename(source))
            if not os.isfile(file) then os.cp(source, file) end
        end
        os.mkdir(path.join(moddir, "docs"))
        os.mkdir(path.join(moddir, "licenses"))
        for _, file in ipairs({"README.md", "LICENSE", "THIRD_PARTY_NOTICES.md"}) do
            os.cp(file, path.join(moddir, file))
        end
        os.cp("docs/*.md", path.join(moddir, "docs"))
        os.cp("docs/CommonLibSSE-NG-REVISION.txt", path.join(moddir, "docs"))
        os.cp("licenses/*.txt", path.join(moddir, "licenses"))
        cprint("FavoriteWheel deployed to %s (existing INI preserved)", moddir)
    end)

target("WheelLogicTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/WheelLogicTests.cpp")
    add_includedirs("include")

target("GameSessionTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/GameSessionTests.cpp")
    add_includedirs("include")

target("HandEquipTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/HandEquipTests.cpp")
    add_includedirs("include")

target("ActorRuntimeTests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsse-ng")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_defines("ENABLE_COMMONLIBSSE_TESTING")
    add_files("tests/ActorRuntimeTests.cpp")

target("WheelPreview")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_deps("wheel-imgui")
    add_files("tests/WheelPreview.cpp", "src/Draw.cpp", "src/WheelFonts.cpp", "src/UIResources.cpp", "src/Settings.cpp", "src/ItemInfo.cpp", "src/SkinTextures.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "windowscodecs", "ole32")

target("SectorMeshTests")
    set_kind("binary")
    set_default(false)
    add_deps("wheel-imgui")
    add_files("tests/SectorMeshTests.cpp")
    add_includedirs("include")

target("LayoutSkinTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/LayoutSkinTests.cpp")
    add_includedirs("include")

target("SettingsTests")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_files("tests/SettingsTests.cpp", "src/Settings.cpp", "src/UIResources.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("user32")

target("SettingsVfsTests")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_files("tests/SettingsVfsTests.cpp", "src/Settings.cpp", "src/UIResources.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("user32")

target("SettingsPersistenceTests")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_files("tests/SettingsPersistenceTests.cpp", "src/Settings.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")

target("CombatCycleTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/CombatCycleTests.cpp")
    add_includedirs("include")

target("CombatPresetTests")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_files("tests/CombatPresetTests.cpp")
    add_includedirs("include")

target("OutfitTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/OutfitTests.cpp")
    add_includedirs("include")

target("NameEditorTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/NameEditorTests.cpp")
    add_includedirs("include")
target("FaceLightClientTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/FaceLightClientTests.cpp")
    add_includedirs("include")

target("RuntimeLayoutTests")
    set_kind("binary")
    add_rules("favoritewheel.version")
    set_default(false)
    set_rundir(os.projectdir())
    add_deps("commonlibsse-ng")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", "ENABLE_COMMONLIBSSE_TESTING")
    add_files("tests/RuntimeLayoutTests.cpp")

target("ItemInfoTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/ItemInfoTests.cpp", "src/ItemInfo.cpp")

target("InventoryTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/InventoryTests.cpp")

target("TimeTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/TimeTests.cpp")

target("SkinTextureTests")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_files("tests/SkinTextureTests.cpp", "src/SkinTextures.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("d3d11", "dxgi", "windowscodecs", "ole32")

target("InventoryOrderTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/InventoryOrderTests.cpp")
