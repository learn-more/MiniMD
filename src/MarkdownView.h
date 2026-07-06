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
    void text_run(const char* str, const char* str_end, const ImVec2& min, const ImVec2& max) override;

private:
    std::string m_markdownText;
    std::string m_currentPath;
    std::vector<char> m_pathInputBuffer;

    // One entry per contiguous, already-wrapped chunk of literal text drawn by the last Render()
    // call. [begin,end) point directly into m_markdownText - see text_run(). Used both for
    // hit-testing mouse clicks/drags into a character offset, and for reconstructing the
    // selected text on copy.
    struct TextRun
    {
        const char* begin;
        const char* end;
        ImVec2 min;
        ImVec2 max;
    };
    std::vector<TextRun> m_runs;        // committed at the end of the previous Render() call
    std::vector<TextRun> m_pendingRuns; // filled *during* the current Render()/print() call

    // Selection anchors are raw pointers into m_markdownText (nullptr = no selection). Order is
    // "anchor = where the drag started", "head = where the mouse is now/ended up" - not
    // necessarily begin <= end, that's normalized where needed via std::min/max.
    const char* m_selAnchor = nullptr;
    const char* m_selHead = nullptr;
    bool m_selecting = false;

    bool HasSelection() const;
    const char* HitTest(const ImVec2& screenPos) const;
    std::string BuildSelectionText() const;
    void UpdateSelectionInput();
    void ResetSelection();

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

#if defined(_WIN32)
    // Registers MiniMD as an available "Open with" handler for .md files (HKCU, no admin rights
    // needed). Windows 8+ won't let an app silently become the *default* handler - that's
    // gated behind user confirmation in Explorer/Settings - so this just makes MiniMD show up
    // as a choice there.
    void RegisterFileAssociation();
#endif
};
