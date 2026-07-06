#include <cstdio>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "MarkdownView.h"

static void GlfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
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
        return 1;

    // OpenGL 3.0 context, GLSL 130 - the lowest common denominator ImGui's
    // OpenGL3 backend supports well, keeps us compatible with older GPUs/drivers.
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "MiniMD - Markdown Viewer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

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
