set_project("FavoriteWheel")
set_version("0.4.8")
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

target("wheel-imgui")
    set_kind("static")
    add_files("extern/imgui/imgui.cpp", "extern/imgui/imgui_draw.cpp", "extern/imgui/imgui_tables.cpp", "extern/imgui/imgui_widgets.cpp", "extern/imgui/backends/imgui_impl_dx11.cpp")
    add_includedirs("extern/imgui", "extern/imgui/backends", {public = true})
    add_syslinks("d3d11", "dxgi", "d3dcompiler")

target("FavoriteWheel")
    set_kind("shared")
    add_rules("commonlibsse-ng.plugin", {name = "FavoriteWheel", author = "BlackMesa79", description = "Categorized vanilla favorites wheel"})
    add_deps("commonlibsse-ng", "wheel-imgui")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    set_pcxxheader("include/PCH.h")
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "user32")
    after_build(function(target)
        local moddir = get_config("deploy_dir")
        if not moddir or moddir == "" then return end
        local pluginsdir = path.join(moddir, "SKSE", "Plugins")
        os.mkdir(pluginsdir)
        os.cp(target:targetfile(), path.join(pluginsdir, "FavoriteWheel.dll"))
        -- Preserve the user's settings on subsequent builds.
        local inifile = path.join(pluginsdir, "FavoriteWheel.ini")
        if not os.isfile(inifile) then os.cp("FavoriteWheel.ini", inifile) end
        for _, kind in ipairs({"Languages", "Themes"}) do
            local destination = path.join(pluginsdir, "FavoriteWheel", kind)
            os.mkdir(destination)
            for _, source in ipairs(os.files(path.join("assets", kind, "*.ini"))) do
                local file = path.join(destination, path.filename(source))
                -- Keep locally customized language/theme files on subsequent deployments.
                if not os.isfile(file) then os.cp(source, file) end
                if kind == "Languages" then
                    local current = io.readfile(file)
                    for line in io.lines(source) do
                        local key = line:match("^([%w_]+)=")
                        if key and not ("\n" .. current):find("\n%s*" .. key .. "%s*=") then
                            current = current .. "\n" .. line
                        end
                    end
                    io.writefile(file, current)
                end
            end
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
    add_files("tests/WheelPreview.cpp", "src/Draw.cpp", "src/WheelFonts.cpp", "src/UIResources.cpp", "src/Settings.cpp", "src/ItemInfo.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("d3d11", "dxgi", "d3dcompiler")

target("SettingsTests")
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_files("tests/SettingsTests.cpp", "src/Settings.cpp", "src/UIResources.cpp")
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_syslinks("user32")

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
