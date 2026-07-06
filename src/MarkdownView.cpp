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
}

ImFont* MarkdownView::get_font() const
{
    // No custom fonts loaded (yet) - default font for everything. imgui_md still tracks m_hlevel / m_is_strong / m_is_table_header as it parses, so plugging in real heading/bold ImFont*s here later (per the mekhontsev/imgui_md README example) is enough to make headings look like headings without touching anything else.
    return nullptr;
}

bool MarkdownView::get_image(image_info& nfo) const
{
    // No image loading yet - returning false skips the image entirely rather than drawing a placeholder.
    (void)nfo;
    return false;
}

void MarkdownView::open_url() const
{
    if (m_href.empty())
        return;

#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", m_href.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__linux__)
    std::string cmd = "xdg-open \"" + m_href + "\" >/dev/null 2>&1 &";
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

    // Keep the menu-bar input box in sync when a file is opened via drag-and-drop or the command line, not just via the text box.
    std::memset(m_pathInputBuffer.data(), 0, m_pathInputBuffer.size());
    std::strncpy(m_pathInputBuffer.data(), path.c_str(), m_pathInputBuffer.size() - 1);
}

void MarkdownView::LoadDefaultSample()
{
    m_markdownText =
        "# MiniMD\n\n"
        "A lightweight markdown viewer built with **Dear ImGui**, using "
        "[MD4C](https://github.com/mity/md4c) for parsing.\n\n"
        "## Getting started\n\n"
        "1. Drag and drop a `.md` file onto this window\n"
        "2. Or pass a file path as a command-line argument\n"
        "3. Or type a path in the box above and press Enter\n\n"
        "### Supported\n\n"
        "Feature (what's supported) | Works\n"
        "---|---\n"
        "Headings + emphasis | yes\n"
        "Ordered / unordered lists | yes\n"
        "Strikethrough + underline | yes\n"
        "Inline code spans | yes\n"
        "Tables (this one) | yes\n\n"
        "Links: [Dear ImGui](https://github.com/ocornut/imgui), "
        "[MD4C](https://github.com/mity/md4c), "
        "[imgui_md](https://github.com/mekhontsev/imgui_md)\n";
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
    print(m_markdownText.c_str(), m_markdownText.c_str() + m_markdownText.length());
}
