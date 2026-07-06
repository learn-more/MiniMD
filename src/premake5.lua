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
        "../vendor/imgui_md",
        "../vendor/md4c/src",
        "../vendor/glfw/include",
    }

    links { "ImGui", "GLFW", "ImGuiMd" }

    filter "system:windows"
        systemversion "latest"
        links { "opengl32" }
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "system:linux"
        links { "GL", "X11", "Xrandr", "Xinerama", "Xcursor", "Xi", "dl", "pthread" }

    filter {}
