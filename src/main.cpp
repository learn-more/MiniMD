#include <cstdio>

#ifdef _WIN32
// Must come before glfw3.h - both define APIENTRY, and GLFW only guards its own
// definition with #if !defined(APIENTRY), so Windows.h has to go first.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "MarkdownView.h"
#include "res/icon_data.h"

// We're a WindowedApp (no console), so stderr has nowhere to go on Windows - fprintf to it is
// silently discarded. Send diagnostics to the debugger (visible in VS's Output window / DebugView)
// and, for anything fatal, pop a message box since the user has no other way to see why the app
// didn't start. Linux keeps a real terminal, so stderr there is fine as-is.
static void ReportError(const char* message, bool fatal)
{
#ifdef _WIN32
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    if (fatal)
        MessageBoxA(nullptr, message, "MiniMD", MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s\n", message);
#endif
}

static void GlfwErrorCallback(int error, const char* description)
{
    char buf[512];
    std::snprintf(buf, sizeof(buf), "GLFW error %d: %s", error, description);
    ReportError(buf, false);
}

static void DropCallback(GLFWwindow* window, int count, const char** paths)
{
    if (count <= 0)
        return;

    auto* view = static_cast<MarkdownView*>(glfwGetWindowUserPointer(window));
    if (view)
        view->LoadFile(paths[0]);
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
    {
        ReportError("glfwInit() failed - see debug output for the GLFW error that preceded this.", true);
        return 1;
    }

    // OpenGL 3.0 context, GLSL 130 - the lowest common denominator ImGui's OpenGL3 backend supports well, keeps us compatible with older GPUs/drivers.
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "MiniMD - Markdown Viewer", nullptr, nullptr);
    if (!window)
    {
        ReportError("glfwCreateWindow() failed - see debug output for the GLFW error that preceded this.", true);
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Cross-platform window/taskbar icon from the pixel data baked in res/icon_data.h (see
    // tools/make_icon.py). GLFWimage::pixels is non-const in the struct but glfwSetWindowIcon
    // only reads it, so the const_cast here is safe.
    GLFWimage icon;
    icon.width = AppIcon::kWidth;
    icon.height = AppIcon::kHeight;
    icon.pixels = const_cast<unsigned char*>(AppIcon::kPixelsRGBA);
    glfwSetWindowIcon(window, 1, &icon);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    MarkdownView view;
    glfwSetWindowUserPointer(window, &view);
    glfwSetDropCallback(window, DropCallback);

    if (argc > 1)
        view.LoadFile(argv[1]);
    else
        view.LoadDefaultSample();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // One full-viewport window hosting the menu bar + markdown content.
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_MenuBar;

        ImGui::Begin("MiniMD", nullptr, flags);
        view.RenderMenuBar();
        view.Render();
        ImGui::End();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
