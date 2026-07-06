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
        -- WindowedApp -> /SUBSYSTEM:WINDOWS, whose default CRT startup calls WinMain().
        -- We use a plain main(argc, argv), so point the linker at the console-style CRT
        -- startup instead - keeps the windowed subsystem (no console) but with our entry point.
        entrypoint "mainCRTStartup"
        -- Embeds res/icon.ico as the GLFW_ICON resource - GLFW's Win32 backend looks it up by
        -- that exact name and uses it for the window/taskbar icon; it also becomes the exe's
        -- icon in Explorer. Linux has no equivalent packaging step (see README).
        files { "res/app.rc" }

    filter "system:linux"
        links { "GL", "X11", "Xrandr", "Xinerama", "Xcursor", "Xi", "dl", "pthread" }

    filter {}
