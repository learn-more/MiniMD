-- Builds GLFW itself from source (vendor/glfw submodule) as a static library.
-- Pin the submodule to tag 3.3.9 - this file list matches GLFW's classic
-- (pre-3.4 platform-abstraction) source layout. If you bump the submodule
-- to 3.4+, GLFW added a runtime backend-selection layer (platform.c and
-- null_*.c are now always required) - update this file's `files` list
-- accordingly or the link will fail with unresolved externals.

project "GLFW"
    kind "StaticLib"
    language "C"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "../vendor/glfw/include/GLFW/glfw3.h",
        "../vendor/glfw/include/GLFW/glfw3native.h",
        "../vendor/glfw/src/context.c",
        "../vendor/glfw/src/init.c",
        "../vendor/glfw/src/input.c",
        "../vendor/glfw/src/monitor.c",
        "../vendor/glfw/src/vulkan.c",
        "../vendor/glfw/src/window.c",
    }

    filter "system:windows"
        systemversion "latest"
        files
        {
            "../vendor/glfw/src/win32_init.c",
            "../vendor/glfw/src/win32_joystick.c",
            "../vendor/glfw/src/win32_monitor.c",
            "../vendor/glfw/src/win32_time.c",
            "../vendor/glfw/src/win32_thread.c",
            "../vendor/glfw/src/win32_window.c",
            "../vendor/glfw/src/wgl_context.c",
            "../vendor/glfw/src/egl_context.c",
            "../vendor/glfw/src/osmesa_context.c",
        }
        defines { "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS" }

    -- Linux target (future): X11 backend. Requires libx11-dev, libxrandr-dev,
    -- libxinerama-dev, libxcursor-dev, libxi-dev on the build machine.
    filter "system:linux"
        pic "on"
        files
        {
            "../vendor/glfw/src/x11_init.c",
            "../vendor/glfw/src/x11_monitor.c",
            "../vendor/glfw/src/x11_window.c",
            "../vendor/glfw/src/xkb_unicode.c",
            "../vendor/glfw/src/posix_time.c",
            "../vendor/glfw/src/posix_thread.c",
            "../vendor/glfw/src/posix_poll.c",
            "../vendor/glfw/src/glx_context.c",
            "../vendor/glfw/src/egl_context.c",
            "../vendor/glfw/src/osmesa_context.c",
            "../vendor/glfw/src/linux_joystick.c",
        }
        defines { "_GLFW_X11" }

    filter {}
