#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "MarkdownView.h"
#include "res/icon_data.h"
#include "res/font_inter_data.h"

// We're a WindowedApp (no console), so stderr has nowhere to go on Windows - fprintf to it is silently discarded. Send diagnostics to the debugger
// (visible in VS's Output window / DebugView) and, for anything fatal, pop a message box since the user has no other way to see why the app didn't
// start. Linux keeps a real terminal, so stderr there is fine as-is.
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

    // OpenGL 3.0 context, GLSL 130 - the lowest common denominator ImGui's OpenGL3 backend supports well, keeps us compatible with older
    // GPUs/drivers.
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

    // Cross-platform window/taskbar icon from the pixel data baked in res/icon_data.h (see tools/make_icon.py). GLFWimage::pixels is non-const in
    // the struct but glfwSetWindowIcon only reads it, so the const_cast here is safe.
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

    // Body + one larger font per heading level, baked at fixed pixel sizes (rather than relying on ImGui's own scaling) so text stays crisp - see
    // MarkdownView::ZoomIn/Out for the separate FontGlobalScale-based zoom that stretches these at render time.
    static const float kBodySize = 16.0f;
    static const float kHeadingSizes[6] = { 32.0f, 27.0f, 23.0f, 21.0f, 18.0f, 17.0f };

    // One compressed TTF blob per weight/style - regular, bold, italic, bold-italic - each baked at every size (body + 6 headings) so **bold** and
    // *italic* spans get a real font instead of just reusing the regular glyphs (see MarkdownView::get_font()).
    auto loadWeights = [&](const char* regular, const char* bold, const char* italic, const char* boldItalic, float size) -> MarkdownView::Weights
    {
        MarkdownView::Weights w;
        w.regular = io.Fonts->AddFontFromMemoryCompressedBase85TTF(regular, size);
        w.bold = io.Fonts->AddFontFromMemoryCompressedBase85TTF(bold, size);
        w.italic = io.Fonts->AddFontFromMemoryCompressedBase85TTF(italic, size);
        w.boldItalic = io.Fonts->AddFontFromMemoryCompressedBase85TTF(boldItalic, size);
        return w;
    };

    auto loadFontSet = [&](const char* regular, const char* bold, const char* italic, const char* boldItalic) -> MarkdownView::FontSet
    {
        MarkdownView::FontSet set;
        set.body = loadWeights(regular, bold, italic, boldItalic, kBodySize);
        for (size_t i = 0; i < set.headings.size(); ++i)
            set.headings[i] = loadWeights(regular, bold, italic, boldItalic, kHeadingSizes[i]);
        return set;
    };

    MarkdownView::FontSet fontSet = loadFontSet(
        AppFonts::kInterRegularCompressedDataBase85, AppFonts::kInterBoldCompressedDataBase85,
        AppFonts::kInterItalicCompressedDataBase85, AppFonts::kInterBoldItalicCompressedDataBase85);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    MarkdownView view;
    view.SetFonts(fontSet);
    glfwSetWindowUserPointer(window, &view);
    glfwSetDropCallback(window, DropCallback);

    if (argc > 1)
        view.LoadFile(argv[1]);
    else
        view.LoadDefaultSample();

    std::string lastTitle;

    while (!glfwWindowShouldClose(window) && !view.WantsQuit())
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // One full-viewport window hosting the markdown content and its right-click context menu.
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("MiniMD", nullptr, flags);
        view.Render();
        ImGui::End();

        // The ImGui window itself has no title bar (see flags above) - this drives the actual OS window/taskbar title instead. Only pushed to GLFW
        // when it changes rather than every frame, since glfwSetWindowTitle() isn't free.
        std::string title = view.GetWindowTitle();
        if (title != lastTitle)
        {
            glfwSetWindowTitle(window, title.c_str());
            lastTitle = title;
        }

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
