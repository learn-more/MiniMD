#pragma once

#include <array>

#include "imgui_md.h"

// Renders the fixed About/credits document shown in the About modal (see MarkdownView::RenderContextMenu()) -
// reuses imgui_md instead of a bespoke text widget, the same rendering path as the main document view. It's a
// short, fixed, image-free, table-free, non-selectable document, so nothing beyond get_font() (for heading
// hierarchy) and open_url() (to make its links clickable) needs overriding - every other base-class default is
// fine as-is.
class AboutView : public imgui_md
{
public:
    void Render();

    // Shares MarkdownView's already-built heading/body fonts rather than loading a font of its own - see
    // MarkdownView::FontSet. Takes the fonts directly (not the FontSet struct) so this header doesn't have to
    // depend on MarkdownView.h, which itself includes this header.
    void SetFonts(ImFont* body, const std::array<ImFont*, 6>& headings);

protected:
    MdSizedFont get_font() const override;
    void open_url() const override;

private:
    ImFont* m_bodyFont = nullptr;
    std::array<ImFont*, 6> m_headingFonts{};
};
