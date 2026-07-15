#include "AboutView.h"

#include "PlatformUtil.h"

namespace
{
    const char kAboutHeader[] =
        "# MiniMD\n"
        "A lightweight markdown viewer built with **Dear ImGui**.\n\n"
        "[https://learn-more.github.io/MiniMD/](https://learn-more.github.io/MiniMD/)\n\n"
        "by [Mark Jansen](https://github.com/learn-more)\n\n";

    // Kept in sync with vendor/README.md's third-party list. Printed as its own block (rather than appended to
    // kAboutHeader) so Render() can put a Separator() between the two - splitting the header from the credits list.
    const char kAboutLibraries[] =
        "## Third-party libraries\n"
        "- [Dear ImGui](https://github.com/ocornut/imgui)\n"
        "- [GLFW](https://github.com/glfw/glfw)\n"
        "- [imgui_md](https://github.com/mekhontsev/imgui_md) "
        "(fork: [learn-more/imgui_md](https://github.com/learn-more/imgui_md))\n"
        "- [MD4C](https://github.com/mity/md4c)\n"
        "- [stb_image](https://github.com/nothings/stb)\n\n";
}

void AboutView::Render()
{
    print(kAboutHeader, kAboutHeader + sizeof(kAboutHeader) - 1);
    ImGui::Separator();
    ImGui::Spacing();
    print(kAboutLibraries, kAboutLibraries + sizeof(kAboutLibraries) - 1);
}

void AboutView::SetFonts(ImFont* body, const std::array<ImFont*, 6>& headings)
{
    m_bodyFont = body;
    m_headingFonts = headings;
}

ImFont* AboutView::get_font() const
{
    // Mirrors MarkdownView::get_font() - see there. m_bodyFont isn't returned here (unlike MarkdownView, which
    // swaps it into io.FontDefault in SetFonts()): the About popup never touches FontDefault, so plain body text
    // falls back to nullptr/"whatever's current" same as the base class default.
    bool inHeading = m_hlevel >= 1 && m_hlevel <= m_headingFonts.size();
    return inHeading ? m_headingFonts[m_hlevel - 1] : nullptr;
}

void AboutView::open_url() const
{
    OpenExternalUrl(m_href);
}
