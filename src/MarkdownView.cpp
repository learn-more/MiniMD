#include "MarkdownView.h"

#include <fstream>
#include <sstream>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <shellapi.h>
    #include <shlobj.h>
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
        m_scrollToTop = true;
        ResetSelection();
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    m_markdownText = ss.str();
    m_currentPath = path;

    // Keep the menu-bar input box in sync when a file is opened via drag-and-drop or the command line, not just via the text box.
    std::memset(m_pathInputBuffer.data(), 0, m_pathInputBuffer.size());
    std::strncpy(m_pathInputBuffer.data(), path.c_str(), m_pathInputBuffer.size() - 1);
    ++m_pathBoxGeneration;

    m_scrollToTop = true;
    ResetSelection();
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
    m_scrollToTop = true;
    ResetSelection();
}

void MarkdownView::ResetSelection()
{
    // Selection anchors and recorded runs are raw pointers into m_markdownText - once that
    // string is replaced (new file, sample doc) they'd be dangling, so this must run on every
    // load before the old buffer goes away.
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
    m_pendingRuns.push_back({ str, str_end, min, max });

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

    // Pick the closest run, weighting vertical distance heavily so we don't jump to a
    // horizontally-closer run on the wrong line (e.g. a short heading above a long paragraph).
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

    // Linear scan over UTF-8 codepoint boundaries to find which inter-glyph gap the mouse is
    // closest to. O(run length) per call, only done on click/drag frames, and runs are at most
    // one wrapped line long, so this doesn't need to be cleverer than that.
    const char* prevBoundary = best->begin;
    float prevX = best->min.x;
    const char* s = best->begin;
    while (s < best->end)
    {
        const char* next = s + 1;
        while (next < best->end && (*next & 0xC0) == 0x80)
            ++next;

        float x = best->min.x + ImGui::CalcTextSize(best->begin, next).x;
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

    // Selection anchors are raw pointers into m_markdownText (see HitTest()/text_run()), so the
    // literal slice between them *is* the underlying markdown source for that span - list
    // markers, table pipes, ** emphasis markers and all - not just whatever ended up rendered.
    // That's deliberate: this copies the source you'd paste back into another markdown file, not
    // a plain-text rendering of what's on screen.
    const char* selMin = m_selAnchor < m_selHead ? m_selAnchor : m_selHead;
    const char* selMax = m_selAnchor < m_selHead ? m_selHead : m_selAnchor;
    return std::string(selMin, selMax);
}

void MarkdownView::UpdateSelectionInput()
{
    ImGuiIO& io = ImGui::GetIO();

    // Only start a *new* selection from a click that lands on the document itself, not on the
    // menu bar/path box - but once a drag is already in progress, keep extending it even if the
    // mouse strays over another widget or outside the window.
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

    // Guarded by !IsAnyItemActive() so this doesn't fight the path box's own Ctrl+C handling
    // while it's focused.
    if (HasSelection() && !ImGui::IsAnyItemActive() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
    {
        std::string selected = BuildSelectionText();
        if (!selected.empty())
            ImGui::SetClipboardText(selected.c_str());
    }
}

#if defined(_WIN32)
void MarkdownView::RegisterFileAssociation()
{
    char exePathBuf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
    {
        MessageBoxA(nullptr, "Could not determine MiniMD.exe's own path.", "MiniMD", MB_OK | MB_ICONERROR);
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

    // Per-user registration under HKCU\Software\Classes - no admin rights needed, unlike HKCR/HKLM.
    // We register a ProgID and add it to .md's "Open With" list rather than overwriting .md's
    // default association outright: Windows 8+ hash-protects the actual default (UserChoice)
    // against being set programmatically, so the user still confirms it via Explorer's "Open
    // with" or Settings > Default apps - this just makes MiniMD show up there as a choice.
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
        MessageBoxA(nullptr,
            "MiniMD is now registered for .md files.\n\n"
            "Right-click a .md file and choose \"Open with\" to pick it (tick \"Always\" to make "
            "it the default), or set it under Settings > Apps > Default apps.",
            "MiniMD", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxA(nullptr, "Registration only partially succeeded - see debug output for details.",
            "MiniMD", MB_OK | MB_ICONWARNING);
    }
}
#endif

void MarkdownView::RenderMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
#if defined(_WIN32)
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Register as .md handler"))
                RegisterFileAssociation();
            ImGui::EndMenu();
        }
#endif

        // Fixed-width indicator drawn first, before the path box - it used to sit right after
        // the box via SameLine(), so a long path filled the box edge-to-edge and ran straight
        // into "(loaded)" with no visual gap. Putting it up front means its position never
        // depends on how long the loaded path is. Full path is still available via tooltip
        // in case it's longer than the box.
        if (!m_currentPath.empty())
        {
            ImGui::TextDisabled("[loaded]");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", m_currentPath.c_str());
            ImGui::SameLine();
        }

        ImGui::TextUnformatted("File:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(500.0f);

        // ID includes m_pathBoxGeneration so a LoadFile() triggered from outside this widget
        // (drag-and-drop, argv, LoadDefaultSample) forces ImGui to (re)initialize its internal
        // edit buffer from m_pathInputBuffer instead of keeping whatever it already had cached.
        char boxId[32];
        std::snprintf(boxId, sizeof(boxId), "##path%d", m_pathBoxGeneration);

        if (ImGui::InputText(boxId, m_pathInputBuffer.data(), m_pathInputBuffer.size(),
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            LoadFile(std::string(m_pathInputBuffer.data()));
        }
        if (ImGui::IsItemHovered() && !m_currentPath.empty())
            ImGui::SetTooltip("%s", m_currentPath.c_str());

        ImGui::EndMenuBar();
    }
}

void MarkdownView::Render()
{
    if (m_scrollToTop)
    {
        ImGui::SetScrollY(0.0f);
        m_scrollToTop = false;
    }

    // Uses m_runs as committed by *last* frame's print() call below - one frame of lag on the
    // layout, which is the normal immediate-mode way to do hit-testing (ImGui's own widgets work
    // the same way) and isn't visible in practice since layout is stable frame to frame.
    UpdateSelectionInput();

    m_pendingRuns.clear();
    print(m_markdownText.c_str(), m_markdownText.c_str() + m_markdownText.length());
    m_runs.swap(m_pendingRuns);
}
