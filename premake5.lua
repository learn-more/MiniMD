-- MiniMD workspace - C++ markdown viewer built on Dear ImGui + GLFW + OpenGL3. Primary target: Windows. Linux support is wired in but not the current focus.

workspace "MiniMD"
    architecture "x86_64"
    startproject "MiniMD"
    configurations { "Debug", "Release" }
    flags { "MultiProcessorCompile" }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "size"
        linktimeoptimization "on"

    filter {}

-- Shared output directory pattern, referenced by every included project script.
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Generate src/Version.h at configure time
if os.host() == "windows" then
    os.execute('"' .. _MAIN_SCRIPT_DIR .. '/tools/gen_version.cmd"')
else
    os.execute("sh " .. _MAIN_SCRIPT_DIR .. "/tools/gen_version.sh")
end

group "Vendor"
    include "premake/imgui.lua"
    include "premake/glfw.lua"
    include "premake/imgui_md.lua"
group ""

include "src"
