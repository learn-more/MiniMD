#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "imgui_md.h"

// Owns the loaded markdown text, and parses/renders it via MD4C (through imgui_md). No menu bar - everything (reload, zoom, debug test files,
// .md registration, exit) lives in a right-click context menu over the document itself.
class MarkdownView : public imgui_md
{
public:
    MarkdownView();
    ~MarkdownView();

    void LoadFile(const std::string& path);
    void LoadDefaultSample();

    void Render();

    // Body font plus one larger ImFont* per heading level (index 0 = h1 ... 5 = h6), all built once in main.cpp after the font atlas exists. Only
    // one weight is embedded per size - **bold** and *italic* spans reuse these same glyphs and get faked at render time instead (see
    // vendor/imgui_md's render_text()), rather than baking 3 more weights just for occasional emphasis in a markdown viewer.
    struct FontSet
    {
        ImFont* body = nullptr;
        std::array<ImFont*, 6> headings{};
    };

    // Handed in once at startup, after the fonts are built into the atlas in main.cpp.
    void SetFonts(const FontSet& fonts);

    // Window title text - "MiniMD - <loaded path>", or a fallback when nothing's loaded. main.cpp polls this once per frame and only calls
    // glfwSetWindowTitle() when it actually changes.
    std::string GetWindowTitle() const;

    // Set by the right-click menu's Exit item; main.cpp's loop checks this alongside glfwWindowShouldClose() so this class doesn't need to know
    // about GLFW/windowing at all.
    bool WantsQuit() const { return m_quitRequested; }

protected:
    // imgui_md overrides - see vendor/imgui_md/imgui_md.h for the full set.
    ImFont* get_font() const override;
    bool get_image(image_info& nfo) const override;
    void open_url() const override;
    void text_run(const char* str, const char* str_end, const ImVec2& min, const ImVec2& max) override;
    bool render_entity(const char* str, const char* str_end) override;
    void BLOCK_CODE(const MD_BLOCK_CODE_DETAIL* d, bool e) override;
    void SPAN_CODE(bool e) override;
    void BLOCK_TD(const MD_BLOCK_TD_DETAIL* d, bool e) override;

private:
    std::string m_markdownText;
    std::string m_currentPath;
    // Directory m_currentPath lives in - relative image paths resolve against this, see ResolveImagePath().
    std::string m_currentDir;
    std::array<ImFont*, 6> m_headingFonts{};
    ImFont* m_bodyFont = nullptr;

    // Set for the duration of a fenced/indented code block - distinguishes a code *block* line (full-width, square-cornered background band,
    // see text_run()) from an inline code *span* (tight rounded pill) sharing the same underlying m_is_code flag. No monospace font is swapped
    // in for either - see BLOCK_CODE()/SPAN_CODE() in the .cpp for why.
    bool m_inCodeBlock = false;

    // Table cell alignment (":---"/"---:"/":-:" column specs). Real ImGui tables auto-fit column width to content and
    // don't expose a per-cell content-alignment flag, so BLOCK_TD() shifts the cell's already-drawn vertices left/
    // right afterward instead - see its definition in the .cpp for the full explanation. Reset per cell in BLOCK_TD().
    MD_ALIGN m_cellAlign = MD_ALIGN_DEFAULT;
    float m_cellColWidth = 0.0f;
    int m_cellVtxStart = 0;

    // One entry per contiguous, already-wrapped chunk of literal text drawn by the last Render() call. [begin,end) point directly into
    // m_markdownText - see text_run(). Used both for hit-testing mouse clicks/drags into a character offset, and for reconstructing the selected
    // text on copy.
    struct TextRun
    {
        const char* begin;
        const char* end;
        ImVec2 min;
        ImVec2 max;
        // Font/size this run was actually drawn with (headings use a larger font than body text - see get_font()). HitTest() runs before
        // print() each frame (see Render()), so by the time it needs to re-measure glyph widths the real per-run font is long since popped off
        // ImGui's font stack - CalcTextSize() at that point would silently measure with whatever font happens to be current instead, which is
        // wrong for any run drawn in a non-default font. Recorded per run here so HitTest() can measure with ImFont::CalcTextSizeA() instead.
        ImFont* font;
        float fontSize;
    };
    std::vector<TextRun> m_runs;        // committed at the end of the previous Render() call
    std::vector<TextRun> m_pendingRuns; // filled *during* the current Render()/print() call

    // Selection anchors are raw pointers into m_markdownText (nullptr = no selection). Order is "anchor = where the drag started", "head = where the
    // mouse is now/ended up" - not necessarily begin <= end, that's normalized where needed via std::min/max.
    const char* m_selAnchor = nullptr;
    const char* m_selHead = nullptr;
    bool m_selecting = false;

    bool HasSelection() const;
    const char* HitTest(const ImVec2& screenPos) const;
    std::string BuildSelectionText() const;
    void UpdateSelectionInput();
    void ResetSelection();
    void CopySelectionToClipboard() const;

    // Local image loading (see get_image()). Remote (http/https) image references are left unsupported and silently skipped, same as a path that
    // fails to resolve at all - no network code in this app. GLuint is always an unsigned int per the GL spec, spelled out that way here rather than
    // pulling a GL header into this header just for the typedef.
    struct CachedImage
    {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
        // false = load was attempted and failed; cached too, so a bad path/corrupt file isn't retried every frame.
        bool valid = false;
    };
    // Keyed by resolved filesystem path. Declared mutable because get_image() is const (an imgui_md interface requirement) but still needs to
    // populate this cache lazily on first use of each image.
    mutable std::unordered_map<std::string, CachedImage> m_imageCache;
    std::string ResolveImagePath(const std::string& href) const;
    CachedImage LoadImageFile(const std::string& path) const;
    void ClearImageCache();

    // Set whenever a new document is loaded so Render() can snap the window's scroll position back to the top on the next frame - otherwise a
    // shorter document loaded while scrolled down in a longer one just shows blank space (or the wrong section) until the user scrolls manually.
    bool m_scrollToTop = true;

    bool m_quitRequested = false;

    // Recent files, most-recently-opened first, persisted to a small text file in a per-user config dir (%APPDATA%\MiniMD or ~/.config/minimd - see
    // GetConfigDir() in the .cpp) so the list survives across runs. Only ever populated by successful LoadFile() calls.
    static constexpr size_t kMaxRecentFiles = 8;
    std::vector<std::string> m_recentFiles;
    void LoadRecentFiles();
    void SaveRecentFiles() const;
    void AddRecentFile(const std::string& path);

    // Mirrors io.FontGlobalScale - kept here rather than poking IO directly from the menu so zoom in/out/reset can clamp and share one code path.
    // Reapplied every frame at the top of Render(), so it affects the context menu and the document alike.
    float m_fontScale = 1.0f;
    void ZoomIn();
    void ZoomOut();
    void ResetZoom();
    void UpdateZoomInput();

    // Right-click popup over the document: Reload / Recent Files / View (zoom) / Debug (DEBUG builds only) / Options / Exit. Also owns the Options
    // dialog (opened via m_showOptionsDialog since a modal can't reliably be opened mid-popup on the same frame it's requested).
    void RenderContextMenu();
    bool m_showOptionsDialog = false;

#if defined(_WIN32)
    // Registers MiniMD as an available "Open with" handler for .md files (HKCU, no admin rights needed). Windows 8+ won't let an app silently become
    // the *default* handler - that's gated behind user confirmation in Explorer/Settings - so this just makes MiniMD show up as a choice there.
    void RegisterFileAssociation();
    void UnregisterFileAssociation();
    bool IsFileAssociationRegistered() const;
#endif
};
