-- Builds Dear ImGui (core + GLFW/OpenGL3 backends) as a static library.
-- Source lives entirely in vendor/imgui (git submodule), this file is the
-- only thing in the whole repo that knows how to compile it.

project "ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "../vendor/imgui/imconfig.h",
        "../vendor/imgui/imgui.h",
        "../vendor/imgui/imgui.cpp",
        "../vendor/imgui/imgui_draw.cpp",
        "../vendor/imgui/imgui_internal.h",
        "../vendor/imgui/imgui_tables.cpp",
        "../vendor/imgui/imgui_widgets.cpp",
        "../vendor/imgui/imstb_rectpack.h",
        "../vendor/imgui/imstb_textedit.h",
        "../vendor/imgui/imstb_truetype.h",

        -- Backends: GLFW (windowing/input) + OpenGL3 (rendering).
        -- This is the lightest, most portable combo ImGui supports -
        -- no platform SDK dependency (unlike Win32/DX11) and no extra
        -- runtime like SDL.
        "../vendor/imgui/backends/imgui_impl_glfw.h",
        "../vendor/imgui/backends/imgui_impl_glfw.cpp",
        "../vendor/imgui/backends/imgui_impl_opengl3.h",
        "../vendor/imgui/backends/imgui_impl_opengl3.cpp",
        "../vendor/imgui/backends/imgui_impl_opengl3_loader.h",
    }

    includedirs
    {
        "../vendor/imgui",
        "../vendor/glfw/include",
    }

    filter "system:windows"
        systemversion "latest"

    filter "system:linux"
        pic "on"

    filter {}
