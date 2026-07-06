project "MiniMD"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "**.h",
        "**.cpp",
    }

    includedirs
    {
        ".",
        "../vendor/imgui",
        "../vendor/imgui/backends",
        "../vendor/imgui_markdown",
        "../vendor/glfw/include",
    }

    links { "ImGui", "GLFW" }

    filter "system:windows"
        systemversion "latest"
        links { "opengl32" }
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "system:linux"
        links { "GL", "X11", "Xrandr", "Xinerama", "Xcursor", "Xi", "dl", "pthread" }

    -- Console subsystem in Debug so stdout/stderr (GLFW errors, asserts) are visible.
    filter "configurations:Debug"
        kind "ConsoleApp"

    filter {}
