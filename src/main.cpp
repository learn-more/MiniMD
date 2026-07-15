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

// Renders and presents one frame. Pulled out of the main loop so it can also be invoked from RefreshCallback() below - on Windows, a live
// border-drag enters a modal loop inside glfwPollEvents() that doesn't return until the drag ends, which would otherwise leave the window
// showing a stale/blank frame for the whole drag. GLFW_REFRESH_CALLBACK fires from inside that modal loop whenever the OS asks the window to
// repaint (e.g. once per size change while dragging), giving us a chance to draw a fresh frame despite glfwPollEvents() not having returned.
static void RenderFrame(GLFWwindow* window)
{
    auto* view = static_cast<MarkdownView*>(glfwGetWindowUserPointer(window));
    if (!view)
        return;

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
    view->Render();
    ImGui::End();

    // The ImGui window itself has no title bar (see flags above) - this drives the actual OS window/taskbar title instead. Only pushed to GLFW
    // when it changes rather than every frame, since glfwSetWindowTitle() isn't free. Kept as a function-local static rather than a main()
    // local since this function is now also called from RefreshCallback(), which has no access to main()'s stack.
    static std::string lastTitle;
    std::string title = view->GetWindowTitle();
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

static void RefreshCallback(GLFWwindow* window)
{
    RenderFrame(window);
}

namespace
{
#if defined(_WIN32)
    // %WINDIR%\Fonts\ - where the system TTFs loaded by name below (Segoe UI, Consolas) live. Falls back to the conventional
    // "C:\Windows\Fonts\" on the vanishingly unlikely chance GetWindowsDirectoryA() itself fails.
    std::string GetWindowsFontsDir()
    {
        char buf[MAX_PATH];
        UINT len = GetWindowsDirectoryA(buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
            return "C:\\Windows\\Fonts\\";
        return std::string(buf, len) + "\\Fonts\\";
    }
#endif

    // Loads one weight/size of a system font by file path. Falls back to ImGui's built-in, no-file-dependency default font at the same pixel
    // size if the path doesn't resolve - e.g. a stripped-down Windows install missing one of these faces, or this binary run on a platform
    // where the paths below don't apply - so a missing font file degrades the look rather than crashing the app on startup.
    ImFont* LoadSystemFont(ImGuiIO& io, const std::string& path, float size)
    {
        ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
        if (font)
            return font;

        ImFontConfig cfg;
        cfg.SizePixels = size;
        return io.Fonts->AddFontDefault(&cfg);
    }
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
    io.IniFilename = nullptr; // no imgui.ini - no persisted window layout state to manage
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // Body + one larger font per heading level, baked at fixed pixel sizes (rather than relying on ImGui's own scaling) so text stays crisp - see
    // MarkdownView::ZoomIn/Out for the separate FontGlobalScale-based zoom that stretches these at render time.
    static const float kBodySize = 18.0f;
    static const float kHeadingSizes[6] = { 36.0f, 30.0f, 26.0f, 24.0f, 20.0f, 19.0f };

    // Consolas renders visibly larger than Segoe UI at the same nominal pixel size (its cap-height/em ratio is bigger), so code spans/blocks
    // get their own, smaller size rather than reusing kBodySize - this keeps code visually matched to surrounding body text instead of towering
    // over it.
    static const float kMonoSize = 15.0f;

    // Loaded straight from the OS's own font files rather than embedding a font in the binary. Segoe UI is Windows' own UI typeface and ships on
    // every Windows install in real regular/bold/italic/bold-italic weights, so markdown emphasis no longer needs to be faked at render time
    // (compare vendor/imgui_md's render_text() and MarkdownView::get_font(), which now just picks one of the four FontStyles below per run).
    // Consolas supplies a dedicated monospace face for code spans/blocks (see MarkdownView::BLOCK_CODE()/SPAN_CODE()) at body size only - code
    // never appears at heading size, so it gets no heading-sized variants.
#if defined(_WIN32)
    const std::string fontDir = GetWindowsFontsDir();
    const std::string regularPath = fontDir + "segoeui.ttf";
    const std::string boldPath = fontDir + "segoeuib.ttf";
    const std::string italicPath = fontDir + "segoeuii.ttf";
    const std::string boldItalicPath = fontDir + "segoeuiz.ttf";
    const std::string monoPath = fontDir + "consola.ttf";
#else
    // Non-Windows builds (Linux support is scaffolded in the premake scripts but not yet exercised - see README) have no fixed system font path
    // to rely on; LoadSystemFont() falls back to ImGui's built-in default font for every weight/size below instead of failing to start.
    const std::string regularPath, boldPath, italicPath, boldItalicPath, monoPath;
#endif

    auto loadStyle = [&](const std::string& path) -> MarkdownView::FontStyle
    {
        MarkdownView::FontStyle style;
        style.body = LoadSystemFont(io, path, kBodySize);
        for (size_t i = 0; i < style.headings.size(); ++i)
            style.headings[i] = LoadSystemFont(io, path, kHeadingSizes[i]);
        return style;
    };

    MarkdownView::FontSet fontSet;
    fontSet.regular = loadStyle(regularPath);
    fontSet.bold = loadStyle(boldPath);
    fontSet.italic = loadStyle(italicPath);
    fontSet.boldItalic = loadStyle(boldItalicPath);
    fontSet.mono = LoadSystemFont(io, monoPath, kMonoSize);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    MarkdownView view;
    view.SetFonts(fontSet);
    glfwSetWindowUserPointer(window, &view);
    glfwSetDropCallback(window, DropCallback);
    glfwSetWindowRefreshCallback(window, RefreshCallback);

    if (argc > 1)
        view.LoadFile(argv[1]);
    else
        view.LoadDefaultSample();

    while (!glfwWindowShouldClose(window) && !view.WantsQuit())
    {
        glfwPollEvents();
        RenderFrame(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
