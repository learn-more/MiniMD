#pragma once

#include <string>
#include <vector>

#include "imgui_markdown.h"

// Owns the loaded markdown text and renders it into the current ImGui
// window using imgui_markdown. Also draws a small menu bar with a path
// input box so a file can be opened without a native file dialog.
class MarkdownView
{
public:
    MarkdownView();

    void LoadFile(const std::string& path);
    void LoadDefaultSample();

    void RenderMenuBar();
    void Render();

private:
    void SetupConfig();

    static void LinkCallback(ImGui::MarkdownLinkCallbackData data);

    std::string m_markdownText;
    std::string m_currentPath;
    std::vector<char> m_pathInputBuffer;
    ImGui::MarkdownConfig m_config;
};
