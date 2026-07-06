#include "MarkdownView.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <shellapi.h>
#endif

MarkdownView::MarkdownView()
    : m_pathInputBuffer(512, '\0')
{
    SetupConfig();
}

void MarkdownView::SetupConfig()
{
    m_config.linkCallback     = &MarkdownView::LinkCallback;
    m_config.tooltipCallback  = nullptr;
    m_config.imageCallback    = nullptr;
    m_config.linkIcon         = "";

    // No custom fonts loaded (yet) - use the default font for every heading
    // level, with a separator line under H1/H2 to visually distinguish them.
    // Swap in bigger/bold ImFont* here once custom fonts are added.
    m_config.headingFormats[0] = { nullptr, true };   // H1
    m_config.headingFormats[1] = { nullptr, true };   // H2
    m_config.headingFormats[2] = { nullptr, false };  // H3

    m_config.formatFlags = ImGuiMarkdownFormatFlags_GithubStyle;
}

void MarkdownView::LinkCallback(ImGui::MarkdownLinkCallbackData data)
{
    if (data.isImage)
        return;

    std::string url(data.link, static_cast<size_t>(data.linkLength));

#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__linux__)
    std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
    std::system(cmd.c_str());
#endif
}

void MarkdownView::LoadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_markdownText = "**Error:** could not open file `" + path + "`";
        m_currentPath.clear();
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    m_markdownText = ss.str();
    m_currentPath = path;

    // Keep the menu-bar input box in sync when a file is opened via
    // drag-and-drop or the command line, not just via the text box.
    std::memset(m_pathInputBuffer.data(), 0, m_pathInputBuffer.size());
    std::strncpy(m_pathInputBuffer.data(), path.c_str(), m_pathInputBuffer.size() - 1);
}

void MarkdownView::LoadDefaultSample()
{
    m_markdownText =
        "# MiniMD\n"
        "A lightweight markdown viewer built with **Dear ImGui**.\n\n"
        "## Getting started\n"
        "  * Drag and drop a `.md` file onto this window\n"
        "  * Or pass a file path as a command-line argument\n"
        "  * Or type a path in the box above and press Enter\n\n"
        "### Links\n"
        "  * [Dear ImGui](https://github.com/ocornut/imgui)\n"
        "  * [GLFW](https://www.glfw.org/)\n"
        "  * [imgui_markdown](https://github.com/juliettef/imgui_markdown)\n\n"
        "*Emphasis* and **strong emphasis** both work, as do horizontal rules:\n"
        "***\n";
    m_currentPath.clear();
}

void MarkdownView::RenderMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        ImGui::TextUnformatted("File:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(500.0f);
        if (ImGui::InputText("##path", m_pathInputBuffer.data(), m_pathInputBuffer.size(),
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            LoadFile(std::string(m_pathInputBuffer.data()));
        }

        if (!m_currentPath.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(loaded)");
        }

        ImGui::EndMenuBar();
    }
}

void MarkdownView::Render()
{
    ImGui::Markdown(m_markdownText.c_str(), m_markdownText.length(), m_config);
}
