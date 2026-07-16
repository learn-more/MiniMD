#include "MarkdownView.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cfloat>
#include <cstdio>
#include <cstdlib>

#include "PlatformUtil.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <shellapi.h>
    #include <shlobj.h>
    #include <GL/gl.h>
#elif defined(__linux__)
    #include <sys/stat.h>
    #include <climits>
    #include <GL/gl.h>
    #if defined(DEBUG)
        #include <unistd.h>
    #endif
#endif

// Local-only image decode for get_image() - no network code, so http(s):// references in markdown are left unsupported and silently skipped
// (get_image() checks for "://" before ever touching the filesystem). This is the one translation unit that compiles stb_image's implementation.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace
{
    const char kReleasesUrl[] = "https://github.com/learn-more/MiniMD/releases";

    // Per-user config directory, created if missing: %APPDATA%\MiniMD on Windows, $XDG_CONFIG_HOME/minimd (or ~/.config/minimd) on Linux. Empty if
    // it can't be determined - callers treat that as "persistence unavailable, carry on without it" rather than an error.
    std::string GetConfigDir()
    {
#if defined(_WIN32)
        const char* appdata = std::getenv("APPDATA");
        if (!appdata || !*appdata)
            return {};
        std::string dir = std::string(appdata) + "\\MiniMD";
        CreateDirectoryA(dir.c_str(), nullptr); // ignore failure - already existing is fine
        return dir;
#elif defined(__linux__)
        std::string base;
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg && *xdg)
        {
            base = xdg;
        }
        else
        {
            const char* home = std::getenv("HOME");
            if (!home || !*home)
                return {};
            base = std::string(home) + "/.config";
        }
        mkdir(base.c_str(), 0755);
        std::string dir = base + "/minimd";
        mkdir(dir.c_str(), 0755);
        return dir;
#else
        return {};
#endif
    }

    std::string GetRecentFilesPath()
    {
        std::string dir = GetConfigDir();
        return dir.empty() ? std::string() : dir + "/recent.txt";
    }

    // Recent-files entries are stored as absolute paths so they still resolve on a later run regardless of what the process's working directory
    // happens to be at that point.
    std::string ToAbsolutePath(const std::string& path)
    {
#if defined(_WIN32)
        char buf[MAX_PATH];
        DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, buf, nullptr);
        return (len == 0 || len >= MAX_PATH) ? path : std::string(buf, len);
#elif defined(__linux__)
        char buf[PATH_MAX];
        char* r = realpath(path.c_str(), buf);
        return r ? std::string(buf) : path;
#else
        return path;
#endif
    }
}

#if defined(DEBUG)
namespace
{
    // Directory containing the running executable - used to find testdata/ regardless of the process's current working directory (which differs
    // depending on whether it was launched via the VS debugger, double-clicked from bin/<cfg>/MiniMD/, or run from a shell elsewhere).
    std::string GetExecutableDir()
    {
#if defined(_WIN32)
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
            return {};
        std::string path(buf, len);
#elif defined(__linux__)
        char buf[4096];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len <= 0)
            return {};
        std::string path(buf, static_cast<size_t>(len));
#else
        std::string path;
#endif
        size_t slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    // The exe always lands at <repo_root>/bin/<cfg>-<system>-<arch>/MiniMD/ (see src/premake5.lua's targetdir), so this fixed number of ".." hops
    // reaches testdata/ regardless of machine or launch method.
    std::string GetTestDataDir()
    {
        std::string exeDir = GetExecutableDir();
        return exeDir.empty() ? std::string() : exeDir + "/../../../testdata";
    }
}
#endif

namespace
{
    // Encodes one Unicode codepoint as UTF-8 into out[0..4) and returns the byte length written. Used by render_entity() below - ImGui text
    // functions all expect UTF-8, but decoded entities/numeric character references start out as a single codepoint.
    int EncodeUtf8(unsigned cp, char out[4])
    {
        if (cp <= 0x7F)
        {
            out[0] = (char)cp;
            return 1;
        }
        if (cp <= 0x7FF)
        {
            out[0] = (char)(0xC0 | (cp >> 6));
            out[1] = (char)(0x80 | (cp & 0x3F));
            return 2;
        }
        if (cp <= 0xFFFF)
        {
            out[0] = (char)(0xE0 | (cp >> 12));
            out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[2] = (char)(0x80 | (cp & 0x3F));
            return 3;
        }
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }

    struct NamedEntity
    {
        const char* name;
        unsigned codepoint;
    };

    // Not the full ~2100-entry HTML5 named character reference table - just CommonMark's own required &amp;/&lt;/
    // &gt;/&quot;/&apos; plus the handful of others (punctuation, arrows, currency) likely to actually show up typed
    // literally in someone's markdown source. Anything else falls through to the numeric-reference path below, or is
    // left as literal text if it's neither.
    constexpr NamedEntity kNamedEntities[] = {
        {"amp", '&'},      {"lt", '<'},        {"gt", '>'},        {"quot", '"'},      {"apos", '\''},
        {"nbsp", 0xA0},    {"copy", 0xA9},     {"reg", 0xAE},      {"trade", 0x2122},  {"mdash", 0x2014},
        {"ndash", 0x2013}, {"hellip", 0x2026}, {"ldquo", 0x201C},  {"rdquo", 0x201D},  {"lsquo", 0x2018},
        {"rsquo", 0x2019}, {"deg", 0xB0},      {"plusmn", 0xB1},   {"times", 0xD7},    {"divide", 0xF7},
        {"euro", 0x20AC},  {"pound", 0xA3},    {"yen", 0xA5},      {"cent", 0xA2},     {"sect", 0xA7},
        {"para", 0xB6},    {"middot", 0xB7},   {"laquo", 0xAB},    {"raquo", 0xBB},    {"larr", 0x2190},
        {"rarr", 0x2192},  {"uarr", 0x2191},   {"darr", 0x2193},   {"harr", 0x2194},   {"bull", 0x2022},
        {"dagger", 0x2020},{"Dagger", 0x2021},
    };
}

MarkdownView::MarkdownView()
{
    // MD_FLAG_TASKLISTS is already on by default (see imgui_md.h's set_flag() doc comment).
    set_flag(MD_FLAG_PERMISSIVEAUTOLINKS, true);

    LoadRecentFiles();
}

MarkdownView::~MarkdownView()
{
    ClearImageCache();
}

void MarkdownView::SetFonts(const FontSet& fonts)
{
    m_fonts = fonts;

    // Plain, regular-weight body text has no per-run font of its own (get_font() returns nullptr for it, which PushFont() takes to mean
    // "whatever's current") - so the body's regular weight is swapped in via io.FontDefault instead, same as the rest of the frame (menus,
    // dialogs) not just the document.
    ImGui::GetIO().FontDefault = fonts.regular.body;

    // About dialog shares the regular-weight heading/body fonts rather than falling back to imgui_md's font-less default - its fixed credits
    // text has no bold/italic spans, so it never needs the other three FontStyles - see AboutView::get_font().
    m_aboutView.SetFonts(fonts.regular.body, fonts.regular.headings);
}

// Fonts here are still separate pre-baked ImFont* per size (see FontStyle/FontSet in MarkdownView.h), not imgui_md's newer
// dynamic-sizing model - so the returned size is just each font's own LegacySize (the size it was baked/AddFont()'d at).
MarkdownView::MdSizedFont MarkdownView::get_font() const
{
    // Code spans/blocks always render in the dedicated monospace font, regardless of any surrounding emphasis - see BLOCK_CODE()/SPAN_CODE(),
    // which push/pop it directly rather than routing through here.
    if (m_is_code)
        return { m_fonts.mono, m_fonts.mono ? m_fonts.mono->LegacySize : 0.0f };

    const FontStyle* style = &m_fonts.regular;
    if (m_is_strong && m_is_em)
        style = &m_fonts.boldItalic;
    else if (m_is_strong)
        style = &m_fonts.bold;
    else if (m_is_em)
        style = &m_fonts.italic;

    // m_hlevel is 1-6 inside a heading, 0 otherwise (see imgui_md.h) - only the size varies by heading level, the weight/slant selection above
    // applies the same way whether or not this run is also a heading.
    bool inHeading = m_hlevel >= 1 && m_hlevel <= style->headings.size();
    if (inHeading)
    {
        ImFont* font = style->headings[m_hlevel - 1];
        return { font, font ? font->LegacySize : 0.0f };
    }

    // Plain regular-weight, non-heading body text falls back to nullptr so PushFont() leaves whatever's current (io.FontDefault, set in
    // SetFonts()) alone instead of pushing a redundant duplicate of that same font.
    if (style == &m_fonts.regular)
        return { nullptr, 0.0f };
    return { style->body, style->body ? style->body->LegacySize : 0.0f };
}

void MarkdownView::BLOCK_CODE(const MD_BLOCK_CODE_DETAIL* d, bool e)
{
    if (e)
    {
        ImGui::NewLine();
        if (d->lang.size > 0)
            ImGui::TextDisabled("%.*s", (int)d->lang.size, d->lang.text);
        ImGui::Indent();
        ImGui::PushFont(m_fonts.mono);
    }

    m_is_code = e;
    m_inCodeBlock = e;

    if (!e)
    {
        ImGui::PopFont();
        ImGui::Unindent();
        ImGui::NewLine();
    }
}

void MarkdownView::SPAN_CODE(bool e)
{
    m_is_code = e;
    if (e)
    {
        // Mono renders at a smaller pixel size than body (see main.cpp) to match its visually larger glyphs at equal size.
        // The smaller mono glyphs would sit noticeably higher than the body text surrounding them instead of sharing its baseline.
        // Shifting the cursor down by the ascent difference before drawing puts the two baselines back in line.
        const float bodyAscent = ImGui::GetFontBaked()->Ascent;
        ImGui::PushFont(m_fonts.mono);
        const float monoAscent = ImGui::GetFontBaked()->Ascent;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (bodyAscent - monoAscent));
    }
    else
    {
        ImGui::PopFont();
    }
}

void MarkdownView::BLOCK_QUOTE(bool e)
{
    if (e)
    {
        ImGui::NewLine();
        ImGui::Indent();
        m_quoteStack.push_back(ImGui::GetCursorScreenPos());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        // The quote frame provides the inter-block gap before its first child itself (via the NewLine() above),
        // so suppress the dispatcher's own gap for that first child - otherwise it lands between the position
        // just captured above and the first child's actual text, leaving the bar's top floating above the text.
        m_skip_next_block_gap = true;
        return;
    }

    ImGui::PopStyleColor();

    // Left bar spans from the top of the quote (captured on enter, before any content) down to the cursor's
    // current position now that all of the quote's content has been laid out - drawn at the midpoint of the
    // indent gutter so it sits between the outer margin and the indented text.
    ImVec2 top = m_quoteStack.back();
    m_quoteStack.pop_back();

    // Cursor Y at this point is the TOP of the last rendered line, not its bottom - render_text() ends each
    // paragraph with SameLine(0,0) rather than a real NewLine(), so nothing ever advances past it. Extend by
    // one line height, or the bar falls short and the last line's text hangs below where the bar ends.
    float bottomY = ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight();
    float barX = top.x - ImGui::GetStyle().IndentSpacing * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(barX, top.y), ImVec2(barX, bottomY),
        ImGui::GetColorU32(ImGuiCol_TextDisabled), 2.0f);

    ImGui::Unindent();
    ImGui::NewLine();
}

// A real ImGui::Checkbox() would look interactive, but there's nowhere to persist a click back to the source
// file in a read-only viewer - so this is plain ImDrawList output instead: a square outline, plus a checkmark
// stroke if the item is marked done. Drawn as vector shapes rather than a "☐"/"☑" glyph so it doesn't
// depend on those code points being present in whatever font is baked into the atlas.
void MarkdownView::render_task_marker(bool checked)
{
    const float sz = ImGui::GetFontSize() * 0.8f;
    const float lineHeight = ImGui::GetTextLineHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.y += (lineHeight - sz) * 0.5f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddRect(p, ImVec2(p.x + sz, p.y + sz), col, 2.0f, 1.5f, 0); // rounding, thickness, flags
    if (checked)
    {
        dl->AddLine(ImVec2(p.x + sz * 0.2f, p.y + sz * 0.5f), ImVec2(p.x + sz * 0.42f, p.y + sz * 0.75f), col, 1.5f);
        dl->AddLine(ImVec2(p.x + sz * 0.42f, p.y + sz * 0.75f), ImVec2(p.x + sz * 0.8f, p.y + sz * 0.2f), col, 1.5f);
    }

    ImGui::Dummy(ImVec2(sz, lineHeight));
}

// Swaps imgui_md's base BLOCK_TABLE/BLOCK_TR/BLOCK_TD (hand-tracked cursor positions, see vendor/imgui_md/imgui_md.cpp)
// for real ImGui tables, which auto-fit each column's width to its widest cell instead of only ever sizing off the
// header row. ImGuiTableFlags_SizingFixedFit does the auto-fit; NoHostExtendX keeps the table from stretching its
// last column to fill whatever space is left in the window instead of stopping at its own content width.
void MarkdownView::BLOCK_TABLE(const MD_BLOCK_TABLE_DETAIL* d, bool e)
{
    if (e)
    {
        // Whatever came before the table (e.g. a heading) may have left the cursor mid-line - always start the table on its own fresh line.
        ImGui::NewLine();

        // Captured *before* entering the table (once, before any column exists) rather than read from the current column's width inside
        // get_table_wrap_width() - a value that depends on "whatever the column already measured" can never grow past its first, likely-
        // too-small guess, which is exactly the feedback loop real auto-fit columns need to avoid.
        m_tableWrapWidth = ImGui::GetContentRegionAvail().x;

        ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
        if (m_table_border)
            flags |= ImGuiTableFlags_Borders;

        std::string id = "table" + std::to_string(m_nextTableId++);
        ImGui::BeginTable(id.c_str(), (int)d->col_count, flags);
    }
    else
    {
        ImGui::EndTable();
        ImGui::NewLine();
    }
}

void MarkdownView::BLOCK_TR(bool e)
{
    if (e)
        ImGui::TableNextRow();
}

float MarkdownView::get_table_wrap_width() const
{
    return m_tableWrapWidth;
}

// Real ImGui tables (ImGuiTableFlags_SizingFixedFit, see BLOCK_TABLE() above) auto-fit each column's width to its content and don't expose
// a per-cell content-alignment flag - TableSetupColumn()'s flags control sorting/resizing, not where text sits within the cell. Column setup
// also has to happen before the first TableNextRow() (see BLOCK_TR() above), before per-cell detail (and therefore alignment) is even known.
// So instead of fighting the table API, this shifts the cell's own already-drawn vertices left/right after the fact: capture the vertex-
// buffer range this cell's content lands in, then on leave, measure its actual drawn width from that range's bounding box and nudge every
// vertex in it by however much slack is left in the (already-known, previously-fit) column width - the same "grab the vertices just emitted
// and move them" trick render_text() uses for italics.
void MarkdownView::BLOCK_TD(const MD_BLOCK_TD_DETAIL* d, bool e)
{
    if (e)
    {
        ImGui::TableNextColumn();
        m_cellAlign = d ? d->align : MD_ALIGN_DEFAULT;
        m_cellColWidth = ImGui::GetContentRegionAvail().x;
        m_cellVtxStart = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        return;
    }

    if (m_cellAlign != MD_ALIGN_CENTER && m_cellAlign != MD_ALIGN_RIGHT)
        return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    int vtxEnd = dl->VtxBuffer.Size;
    if (vtxEnd <= m_cellVtxStart)
        return; // empty cell - nothing to shift

    // Plain comparisons, not std::min/max - Windows.h's own min/max macros (still in scope, see the includes at the
    // top of this file) would shadow the std:: names right here otherwise.
    float minX = FLT_MAX, maxX = -FLT_MAX;
    for (int i = m_cellVtxStart; i < vtxEnd; ++i)
    {
        float x = dl->VtxBuffer[i].pos.x;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
    }

    float slack = m_cellColWidth - (maxX - minX);
    if (slack <= 0.0f)
        return; // this cell is (one of) the widest in its column - no room to shift

    float shift = m_cellAlign == MD_ALIGN_CENTER ? slack * 0.5f : slack;
    for (int i = m_cellVtxStart; i < vtxEnd; ++i)
        dl->VtxBuffer[i].pos.x += shift;
}

// The base class (vendor/imgui_md) only special-cases "&nbsp;" and leaves every other character reference - named
// ("&copy;"), decimal ("&#169;") or hex ("&#xA9;") - as literal, un-decoded text. This overrides it entirely (rather
// than extending the base version) to also resolve numeric references generically and a modest table of common named
// ones; anything still unrecognized falls back to literal text same as before, via the `return false` path in text().
bool MarkdownView::render_entity(const char* str, const char* str_end)
{
    size_t len = str_end - str;
    if (len < 3 || str[0] != '&' || str[len - 1] != ';')
        return false;

    unsigned cp = 0;
    if (str[1] == '#')
    {
        bool hex = len > 3 && (str[2] == 'x' || str[2] == 'X');
        const char* digits = str + (hex ? 3 : 2);
        const char* digitsEnd = str_end - 1;
        if (digits >= digitsEnd)
            return false;

        for (const char* p = digits; p < digitsEnd; ++p)
        {
            char c = *p;
            int v;
            if (c >= '0' && c <= '9')
                v = c - '0';
            else if (hex && c >= 'a' && c <= 'f')
                v = c - 'a' + 10;
            else if (hex && c >= 'A' && c <= 'F')
                v = c - 'A' + 10;
            else
                return false;
            cp = cp * (hex ? 16u : 10u) + (unsigned)v;
        }
        if (cp == 0 || cp > 0x10FFFF)
            cp = 0xFFFD; // U+FFFD replacement character, for a null or out-of-range reference
    }
    else
    {
        std::string name(str + 1, len - 2);
        bool found = false;
        for (const NamedEntity& entity : kNamedEntities)
        {
            if (name == entity.name)
            {
                cp = entity.codepoint;
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    char utf8[4];
    int n = EncodeUtf8(cp, utf8);
    ImGui::TextUnformatted(utf8, utf8 + n);
    ImGui::SameLine(0.0f, 0.0f);
    return true;
}

// Base class leaves this a no-op (see vendor/imgui_md/imgui_md.cpp). render_text() ends every run with SameLine(0.0f, 0.0f) - zero
// spacing - so two runs joined by a soft line break in the source (a plain newline inside a paragraph) glue together with no space
// at all instead of collapsing to the single space CommonMark calls for.
void MarkdownView::soft_break()
{
    ImGui::TextUnformatted(" ");
    ImGui::SameLine(0.0f, 0.0f);
}

// m_href isn't a filesystem path as-is: it's whatever's between (parens) in the markdown, so relative references (the common case) need resolving
// against the loaded file's own directory, not the process's current working directory (which could be anything - see LoadFile()).
std::string MarkdownView::ResolveImagePath(const std::string& href) const
{
    bool isAbsolute = !href.empty() &&
        (href[0] == '/' || href[0] == '\\' || (href.size() > 1 && href[1] == ':'));
    if (isAbsolute || m_currentDir.empty())
        return href;
    return m_currentDir + "/" + href;
}

MarkdownView::CachedImage MarkdownView::LoadImageFile(const std::string& path) const
{
    int width = 0, height = 0, channels = 0;
    // Force 4 components (RGBA) regardless of source format so the GL upload below never has to branch on channel count.
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels)
        return CachedImage{}; // valid=false - bad path, unsupported format, corrupt file, etc.

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    return CachedImage{ texture, width, height, true };
}

void MarkdownView::ClearImageCache()
{
    for (const auto& [path, img] : m_imageCache)
    {
        (void)path;
        if (img.valid)
        {
            GLuint texture = img.texture;
            glDeleteTextures(1, &texture);
        }
    }
    m_imageCache.clear();
}

bool MarkdownView::get_image(image_info& nfo) const
{
    if (m_href.empty())
        return false;

    // No network code in this app - a remote reference just falls through to "couldn't load", same as a relative path that doesn't resolve to
    // anything.
    if (m_href.find("://") != std::string::npos)
        return false;

    std::string path = ResolveImagePath(m_href);

    auto it = m_imageCache.find(path);
    if (it == m_imageCache.end())
        it = m_imageCache.emplace(path, LoadImageFile(path)).first;

    const CachedImage& img = it->second;
    if (!img.valid)
        return false;

    nfo.texture_id = (ImTextureID)(intptr_t)img.texture;
    nfo.size = ImVec2((float)img.width, (float)img.height);
    nfo.uv0 = ImVec2(0.0f, 0.0f);
    nfo.uv1 = ImVec2(1.0f, 1.0f);
    return true;
}

// Overrides the base class's SPAN_IMG() entirely (rather than extending it) for two reasons: enable the hover tooltip for an image's title
// text (the base class ships that path commented out), and set m_is_image from get_image()'s actual result instead of unconditionally on
// enter - m_is_image only suppresses the span's child text (the alt text) while there's actually an image drawn to look at instead (see
// render_text()'s guard in vendor/imgui_md/imgui_md.cpp). If get_image() fails (bad path, unsupported format, remote URL with no network
// code, ...) it stays false so the alt text renders as a normal (link-styled, since m_href is still set to the image src) fallback instead
// of silently vanishing.
void MarkdownView::SPAN_IMG(const MD_SPAN_IMG_DETAIL* d, bool e)
{
    if (!e)
    {
        m_href.clear();
        m_is_image = false;
        return;
    }

    m_href.assign(d->src.text, d->src.size);

    image_info nfo;
    m_is_image = get_image(nfo);
    if (!m_is_image)
        return;

    const float scale = ImGui::GetIO().FontGlobalScale;
    nfo.size.x *= scale;
    nfo.size.y *= scale;

    ImVec2 const csz = ImGui::GetContentRegionAvail();
    if (nfo.size.x > csz.x)
    {
        const float r = nfo.size.y / nfo.size.x;
        nfo.size.x = csz.x;
        nfo.size.y = csz.x * r;
    }

    // No tint/border: image_info dropped those fields upstream when ImGui::Image() itself did - see ImageWithBg() if either's ever needed.
    ImGui::Image(nfo.texture_id, nfo.size, nfo.uv0, nfo.uv1);

    if (ImGui::IsItemHovered())
    {
        if (d->title.size > 0)
            ImGui::SetTooltip("%.*s", (int)d->title.size, d->title.text);
        if (ImGui::IsMouseReleased(0))
            open_url();
    }
}

void MarkdownView::open_url() const
{
    OpenExternalUrl(m_href);
}

void MarkdownView::LoadFile(const std::string& path)
{
    // Old images are resolved against the *previous* file's directory - drop them before anything else, whether or not this load actually succeeds.
    ClearImageCache();

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        m_markdownText = "**Error:** could not open file `" + path + "`";
        m_currentPath.clear();
        m_currentDir.clear();
        m_scrollToTop = true;
        ResetSelection();
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    m_markdownText = ss.str();

    // Strip UTF-8 BOM - otherwise it sits before the first "#" and stops the heading rule from matching.
    if (m_markdownText.compare(0, 3, "\xEF\xBB\xBF") == 0)
        m_markdownText.erase(0, 3);

    m_currentPath = path;
    size_t slash = path.find_last_of("\\/");
    m_currentDir = slash == std::string::npos ? std::string() : path.substr(0, slash);
    AddRecentFile(path);

    m_scrollToTop = true;
    ResetSelection();
}

void MarkdownView::LoadDefaultSample()
{
    ClearImageCache();
    m_currentDir.clear();
    m_markdownText =
        "# MiniMD\n\n"
        "A lightweight markdown viewer built with **Dear ImGui**.\n\n"
        "## Getting started\n\n"
        "1. Drag and drop a `.md` file onto this window\n"
        "2. Or pass a file path as a command-line argument\n"
        "3. Or right-click > Recent Files to reopen one\n\n"
        "### Supported\n\n"
        "Feature (what's supported) | Works\n"
        "---|---\n"
        "Headings + **bold**/*italic* | yes\n"
        "Ordered / unordered lists | yes\n"
        "Blockquotes | yes\n"
        "Strikethrough + underline | yes\n"
        "Inline code spans | yes\n"
        "Tables (this one) | yes\n"
        "Local images | yes\n\n"
        "See right-click > Help > About for project links and full credits.\n";
    m_currentPath.clear();
    m_scrollToTop = true;
    ResetSelection();
}

void MarkdownView::LoadRecentFiles()
{
    m_recentFiles.clear();
    std::string path = GetRecentFilesPath();
    if (path.empty())
        return;

    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line))
    {
        if (!line.empty())
            m_recentFiles.push_back(line);
    }
}

void MarkdownView::SaveRecentFiles() const
{
    std::string path = GetRecentFilesPath();
    if (path.empty())
        return;

    std::ofstream f(path, std::ios::trunc);
    for (const std::string& p : m_recentFiles)
        f << p << '\n';
}

void MarkdownView::AddRecentFile(const std::string& path)
{
    std::string abs = ToAbsolutePath(path);
    m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), abs), m_recentFiles.end());
    m_recentFiles.insert(m_recentFiles.begin(), abs);
    if (m_recentFiles.size() > kMaxRecentFiles)
        m_recentFiles.resize(kMaxRecentFiles);
    SaveRecentFiles();
}

void MarkdownView::ZoomIn()
{
    m_fontScale = m_fontScale + 0.1f > 3.0f ? 3.0f : m_fontScale + 0.1f;
}

void MarkdownView::ZoomOut()
{
    m_fontScale = m_fontScale - 0.1f < 0.5f ? 0.5f : m_fontScale - 0.1f;
}

void MarkdownView::ResetZoom()
{
    m_fontScale = 1.0f;
}

void MarkdownView::UpdateZoomInput()
{
    if (ImGui::IsAnyItemActive())
        return; // don't fire while e.g. a dialog button/field has focus

    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
        ZoomIn();
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
        ZoomOut();
    if (ImGui::IsKeyPressed(ImGuiKey_0))
        ResetZoom();
}

void MarkdownView::ResetSelection()
{
    // Selection anchors and recorded runs are raw pointers into m_markdownText - once that string is replaced (new file, sample doc) they'd be
    // dangling, so this must run on every load before the old buffer goes away.
    m_selAnchor = nullptr;
    m_selHead = nullptr;
    m_selecting = false;
    m_runs.clear();
    m_pendingRuns.clear();
}

bool MarkdownView::HasSelection() const
{
    return m_selAnchor != nullptr && m_selHead != nullptr && m_selAnchor != m_selHead;
}

void MarkdownView::text_run(const char* str, const char* str_end, const ImVec2& min, const ImVec2& max)
{
    m_pendingRuns.push_back({ str, str_end, min, max, ImGui::GetFont(), ImGui::GetFontSize() });

    // Draw a background band/pill behind the glyphs as a second visual cue, before they land: full-width per wrapped
    // line for a code block (m_inCodeBlock), a tight pill around just the span otherwise.
    //
    // Skips zero-width runs: md4c emits each code-block source line as two calls, content then a lone trailing '\n'
    // with no glyphs - without this guard that second call would stack another (alpha-blended) full-width rect over
    // the real one, darkening it relative to the empty space beside it.
    if (m_is_code && max.x > min.x)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col = ImGui::GetColorU32(ImGuiCol_FrameBg);
        if (m_inCodeBlock)
        {
            // Draw at most once per row - see m_codeBandRowY's doc comment for why.
            if (min.y != m_codeBandRowY)
            {
                // Padded by half of ItemSpacing.y rather than a fixed 1px, so consecutive lines' bands meet with no
                // gap: each code line is its own ImGui item, spaced ItemSpacing.y apart.
                float padY = ImGui::GetStyle().ItemSpacing.y * 0.5f;
                float rightX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                dl->AddRectFilled(ImVec2(min.x - 4.0f, min.y - padY), ImVec2(rightX, max.y + padY), col);
                m_codeBandRowY = min.y;
            }
        }
        else
        {
            dl->AddRectFilled(ImVec2(min.x - 2.0f, min.y - 1.0f), ImVec2(max.x + 2.0f, max.y + 1.0f), col, 3.0f);
        }
    }

    if (!HasSelection())
        return;

    const char* selMin = m_selAnchor < m_selHead ? m_selAnchor : m_selHead;
    const char* selMax = m_selAnchor < m_selHead ? m_selHead : m_selAnchor;

    const char* hiBegin = str > selMin ? str : selMin;
    const char* hiEnd = str_end < selMax ? str_end : selMax;
    if (hiBegin >= hiEnd)
        return; // this run doesn't overlap the current selection at all

    ImVec2 hiMin = min;
    ImVec2 hiMax = max;
    if (hiBegin > str)
        hiMin.x += ImGui::CalcTextSize(str, hiBegin).x;
    if (hiEnd < str_end)
        hiMax.x = min.x + ImGui::CalcTextSize(str, hiEnd).x;

    ImGui::GetWindowDrawList()->AddRectFilled(hiMin, hiMax, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
}

const char* MarkdownView::HitTest(const ImVec2& screenPos) const
{
    if (m_runs.empty())
        return nullptr;

    // Pick the closest run, weighting vertical distance heavily so we don't jump to a horizontally-closer run on the wrong line (e.g. a short
    // heading above a long paragraph).
    const TextRun* best = nullptr;
    float bestDist = FLT_MAX;
    for (const TextRun& run : m_runs)
    {
        float dx = screenPos.x < run.min.x ? run.min.x - screenPos.x : (screenPos.x > run.max.x ? screenPos.x - run.max.x : 0.0f);
        float dy = screenPos.y < run.min.y ? run.min.y - screenPos.y : (screenPos.y > run.max.y ? screenPos.y - run.max.y : 0.0f);
        float dist = dy * 1000.0f + dx;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &run;
        }
    }
    if (!best)
        return nullptr;

    if (screenPos.x <= best->min.x)
        return best->begin;
    if (screenPos.x >= best->max.x)
        return best->end;

    // Linear scan over UTF-8 codepoint boundaries to find which inter-glyph gap the mouse is closest to. O(run length) per call, only done on
    // click/drag frames, and runs are at most one wrapped line long, so this doesn't need to be cleverer than that.
    const char* prevBoundary = best->begin;
    float prevX = best->min.x;
    const char* s = best->begin;
    while (s < best->end)
    {
        const char* next = s + 1;
        while (next < best->end && (*next & 0xC0) == 0x80)
            ++next;

        float x = best->min.x + best->font->CalcTextSizeA(best->fontSize, FLT_MAX, 0.0f, best->begin, next).x;
        float mid = (prevX + x) * 0.5f;
        if (screenPos.x < mid)
            return prevBoundary;

        prevBoundary = next;
        prevX = x;
        s = next;
    }
    return best->end;
}

std::string MarkdownView::BuildSelectionText() const
{
    if (!HasSelection())
        return {};

    // Selection anchors are raw pointers into m_markdownText (see HitTest()/text_run()), so the literal slice between them *is* the underlying
    // markdown source for that span - list markers, table pipes, ** emphasis markers and all - not just whatever ended up rendered. That's
    // deliberate: this copies the source you'd paste back into another markdown file, not a plain-text rendering of what's on screen.
    const char* selMin = m_selAnchor < m_selHead ? m_selAnchor : m_selHead;
    const char* selMax = m_selAnchor < m_selHead ? m_selHead : m_selAnchor;
    return std::string(selMin, selMax);
}

void MarkdownView::UpdateSelectionInput()
{
    ImGuiIO& io = ImGui::GetIO();

    // Only start a *new* selection from a click that lands on the document itself, not on some other widget (context menu, dialog) - but once a drag
    // is already in progress, keep extending it even if the mouse strays over another widget or outside the window.
    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    bool canStart = hovered && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();

    if (canStart && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const char* hit = HitTest(io.MousePos);
        m_selAnchor = hit;
        m_selHead = hit;
        m_selecting = (hit != nullptr);
    }
    else if (m_selecting && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const char* hit = HitTest(io.MousePos);
        if (hit)
            m_selHead = hit;
    }
    else if (m_selecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_selecting = false;
    }

    // Guarded by !IsAnyItemActive() so this doesn't fight some other focused widget's own Ctrl+C handling (e.g. if a future dialog adds a text
    // field).
    if (HasSelection() && !ImGui::IsAnyItemActive() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
        CopySelectionToClipboard();
}

void MarkdownView::CopySelectionToClipboard() const
{
    std::string selected = BuildSelectionText();
    if (!selected.empty())
        ImGui::SetClipboardText(selected.c_str());
}

#if defined(_WIN32)
void MarkdownView::RegisterFileAssociation()
{
    char exePathBuf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
    {
        m_registerErrorMessage = "Could not determine MiniMD.exe's own path.";
        m_showRegisterErrorDialog = true;
        return;
    }
    std::string exePath(exePathBuf, len);

    auto setValue = [](HKEY root, const std::string& subKey, const char* valueName, const std::string& value) -> bool
    {
        HKEY key;
        if (RegCreateKeyExA(root, subKey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return false;
        LONG result = RegSetValueExA(key, valueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>(value.size() + 1));
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    };

    // Per-user registration under HKCU\Software\Classes - no admin rights needed, unlike HKCR/HKLM. We register a ProgID and add it to .md's "Open
    // With" list rather than overwriting .md's default association outright: Windows 8+ hash-protects the actual default (UserChoice) against being
    // set programmatically, so the user still confirms it via Explorer's "Open with" or Settings > Default apps - this just makes MiniMD show up
    // there as a choice.
    const std::string progId = "Software\\Classes\\MiniMD.md";
    const std::string command = "\"" + exePath + "\" \"%1\"";

    bool ok = true;
    if (!setValue(HKEY_CURRENT_USER, progId, nullptr, "MiniMD Markdown Document"))
        ok = false;
    if (!setValue(HKEY_CURRENT_USER, progId + "\\DefaultIcon", nullptr, exePath + ",0"))
        ok = false;
    if (!setValue(HKEY_CURRENT_USER, progId + "\\shell\\open\\command", nullptr, command))
        ok = false;
    if (!setValue(HKEY_CURRENT_USER, "Software\\Classes\\.md\\OpenWithProgids", "MiniMD.md", ""))
        ok = false;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (ok)
    {
        m_showRegisterSuccessDialog = true;
    }
    else
    {
        m_registerErrorMessage = "Registration only partially succeeded - see debug output for details.";
        m_showRegisterErrorDialog = true;
    }
}

bool MarkdownView::IsFileAssociationRegistered() const
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\MiniMD.md", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;
    RegCloseKey(key);
    return true;
}

void MarkdownView::UnregisterFileAssociation()
{
    // Mirror image of RegisterFileAssociation(): drop the ProgID (and everything under it) plus its entry in .md's "Open With" list. Doesn't touch
    // OpenWithProgids itself since other apps may have their own entries there.
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Classes\\MiniMD.md");

    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\.md\\OpenWithProgids", 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS)
    {
        RegDeleteValueA(key, "MiniMD.md");
        RegCloseKey(key);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
#endif

std::string MarkdownView::GetWindowTitle() const
{
    return m_currentPath.empty() ? "MiniMD - Markdown Viewer" : "MiniMD - " + m_currentPath;
}

void MarkdownView::RenderContextMenu()
{
    if (ImGui::BeginPopupContextWindow("ContextMenu"))
    {
        if (ImGui::MenuItem("Reload", nullptr, false, !m_currentPath.empty()))
            LoadFile(m_currentPath);

        if (ImGui::BeginMenu("Recent Files", !m_recentFiles.empty()))
        {
            for (size_t i = 0; i < m_recentFiles.size(); ++i)
            {
                const std::string& full = m_recentFiles[i];
                size_t slash = full.find_last_of("\\/");
                std::string filename = slash == std::string::npos ? full : full.substr(slash + 1);
                std::string label = filename + "##recent" + std::to_string(i);

                if (ImGui::MenuItem(label.c_str()))
                    LoadFile(full);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", full.c_str());
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Clear Recent Files"))
            {
                m_recentFiles.clear();
                SaveRecentFiles();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("View"))
        {
            char zoomLabel[32];
            std::snprintf(zoomLabel, sizeof(zoomLabel), "Zoom: %d%%", (int)(m_fontScale * 100.0f + 0.5f));
            ImGui::TextDisabled("%s", zoomLabel);
            ImGui::Separator();

            if (ImGui::MenuItem("Zoom In", "Ctrl+="))
                ZoomIn();
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-"))
                ZoomOut();
            if (ImGui::MenuItem("Reset Zoom", "Ctrl+0"))
                ResetZoom();
            ImGui::EndMenu();
        }

#if defined(DEBUG)
        if (ImGui::BeginMenu("Debug"))
        {
            static const char* kTestFiles[] = {
                "commonmark-features.md",
                "images-local.md",
                "images-local-remote.md",
            };

            ImGui::TextDisabled("Load test file:");
            ImGui::Separator();

            std::string dir = GetTestDataDir();
            if (dir.empty())
            {
                ImGui::TextDisabled("(couldn't locate testdata/)");
            }
            else
            {
                for (const char* name : kTestFiles)
                {
                    if (ImGui::MenuItem(name))
                        LoadFile(dir + "/" + name);
                }
            }
            ImGui::EndMenu();
        }
#endif

#if defined(_WIN32)
        if (ImGui::MenuItem("Options"))
            m_showOptionsDialog = true;
#endif

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Check for Updates"))
                OpenExternalUrl(kReleasesUrl);
            if (ImGui::MenuItem("About"))
                m_showAboutDialog = true;
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            m_quitRequested = true;

        ImGui::EndPopup();
    }

    // Opened via a flag rather than calling OpenPopup() directly from the MenuItem above - MenuItem closes the popup stack it lives in on the same
    // frame it's clicked, and opening a brand-new popup into a stack that's mid-close doesn't reliably work. Deferring one frame sidesteps that.
    if (m_showAboutDialog)
    {
        ImGui::OpenPopup("About");
        m_showAboutDialog = false;
    }

    // Width fixed up front (rather than ImGuiWindowFlags_AlwaysAutoResize) so imgui_md's word-wrap - which wraps to
    // the window's own current content width - has a stable value to wrap against from the very first frame;
    // height still auto-fits the (fixed) content underneath that fixed width. NoResize since there's nothing
    // useful to resize into - it's a short, fixed document.
    // Extra WindowPadding (over ImGui's default) so the credits text isn't flush against the popup's edges - the
    // default padding reads fine for the main document view's own scrollable window, but this modal is small
    // enough that it shows.
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_NoResize))
    {
        m_aboutView.Render();

        ImGui::Spacing();
        // Right-aligned rather than left, so it lines up under the credits text's own right edge instead of
        // floating at the window's left margin.
        float buttonWidth = ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth);
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

#if defined(_WIN32)
    // Deferred one frame for the same reason as the About popup above.
    if (m_showOptionsDialog)
    {
        ImGui::OpenPopup("Options");
        m_showOptionsDialog = false;
    }

    if (ImGui::BeginPopupModal("Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        bool registered = IsFileAssociationRegistered();
        if (ImGui::Button(registered ? "Unregister .md handler" : "Register as .md handler"))
        {
            if (registered)
                UnregisterFileAssociation();
            else
                RegisterFileAssociation();
        }

        ImGui::SameLine();
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // Deferred one frame for the same reason as the Options/About popups above - RegisterFileAssociation() runs
    // from inside the Options popup's own Button(), and OpenPopup() doesn't reliably stack a new popup onto one
    // that's mid-close on the same frame.
    if (m_showRegisterSuccessDialog)
    {
        ImGui::OpenPopup("Registered");
        m_showRegisterSuccessDialog = false;
    }

    if (ImGui::BeginPopupModal("Registered", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("MiniMD is now registered for .md files.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Right-click a .md file and choose \"Open with\" to pick it (tick \"Always\" to make");
        ImGui::TextUnformatted("it the default), or set it under Settings > Apps > Default apps.");

        ImGui::Spacing();
        float buttonWidth = ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth);
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (m_showRegisterErrorDialog)
    {
        ImGui::OpenPopup("Registration failed");
        m_showRegisterErrorDialog = false;
    }

    if (ImGui::BeginPopupModal("Registration failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(m_registerErrorMessage.c_str());

        ImGui::Spacing();
        float buttonWidth = ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth);
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
#endif
}

void MarkdownView::Render()
{
    // Reapplied every frame rather than just on a zoom action, since it's cheap and this keeps it a straight mirror of m_fontScale regardless of
    // what else might touch IO.
    ImGui::GetIO().FontGlobalScale = m_fontScale;
    UpdateZoomInput();
    RenderContextMenu();

    if (m_scrollToTop)
    {
        ImGui::SetScrollY(0.0f);
        m_scrollToTop = false;
    }

    // Uses m_runs as committed by *last* frame's print() call below - one frame of lag on the layout, which is the normal immediate-mode way to do
    // hit-testing (ImGui's own widgets work the same way) and isn't visible in practice since layout is stable frame to frame.
    UpdateSelectionInput();

    m_pendingRuns.clear();
    // Reset so each table in the document (and each frame's re-render of it) gets a stable, table-scoped ID rather than one that keeps
    // climbing forever - see m_nextTableId's own comment and BLOCK_TABLE().
    m_nextTableId = 0;
    print(m_markdownText.c_str(), m_markdownText.c_str() + m_markdownText.length());
    m_runs.swap(m_pendingRuns);
}
