#pragma once

#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_md.h"

// Owns the loaded markdown text, and parses/renders it via MD4C (through imgui_md). Also draws a small menu bar with a path input box so a file can be opened without a native file dialog.
class MarkdownView : public imgui_md
{
public:
    MarkdownView();

    void LoadFile(const std::string& path);
    void LoadDefaultSample();

    void RenderMenuBar();
    void Render();

protected:
    // imgui_md overrides - see vendor/imgui_md/imgui_md.h for the full set.
    ImFont* get_font() const override;
    bool get_image(image_info& nfo) const override;
    void open_url() const override;

private:
    std::string m_markdownText;
    std::string m_currentPath;
    std::vector<char> m_pathInputBuffer;

    // ImGui's InputText only reads m_pathInputBuffer back out when the widget itself becomes
    // active (e.g. the user clicks into it) - external writes to the buffer (drag-and-drop, argv,
    // this same box's own callback) don't get picked up otherwise. Bumping this and folding it into
    // the widget's ID string each time a file loads forces ImGui to treat it as a brand-new widget,
    // which does read from the buffer.
    int m_pathBoxGeneration = 0;

    // Set whenever a new document is loaded so Render() can snap the window's scroll position back
    // to the top on the next frame - otherwise a shorter document loaded while scrolled down in a
    // longer one just shows blank space (or the wrong section) until the user scrolls manually.
    bool m_scrollToTop = true;
};
