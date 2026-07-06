#pragma once

#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_md.h"

// Owns the loaded markdown text, and parses/renders it via MD4C (through
// imgui_md). Also draws a small menu bar with a path input box so a file
// can be opened without a native file dialog.
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
};
