#include "MarkdownView.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cfloat>
#include <cstdio>
#include <cstdlib>

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

MarkdownView::MarkdownView()
{
    LoadRecentFiles();
}

MarkdownView::~MarkdownView()
{
    ClearImageCache();
}

void MarkdownView::SetFonts(const FontSet& fonts)
{
    m_headingFonts = fonts.headings;
    m_bodyFont = fonts.body;

    // Plain body text has no per-run font of its own (get_font() returns nullptr for it, which PushFont() takes to mean "whatever's current") - so
    // the body's regular weight is swapped in via io.FontDefault instead, same as the rest of the frame (menus, dialogs) not just the document.
    ImGui::GetIO().FontDefault = fonts.body;
}

ImFont* MarkdownView::get_font() const
{
    // m_hlevel is 1-6 inside a heading, 0 otherwise (see imgui_md.h). Only the size varies by heading level - bold/italic reuse this same font and
    // are faked at render time instead (see vendor/imgui_md's render_text()), so m_is_strong/m_is_em don't factor in here at all. Plain body text
    // falls back to nullptr so PushFont() leaves whatever's current (io.FontDefault, set in SetFonts()) alone.
    bool inHeading = m_hlevel >= 1 && m_hlevel <= m_headingFonts.size();
    return inHeading ? m_headingFonts[m_hlevel - 1] : nullptr;
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
    nfo.col_tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    nfo.col_border = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    return true;
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
        "A lightweight markdown viewer built with **Dear ImGui**, using "
        "[MD4C](https://github.com/mity/md4c) for parsing.\n\n"
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
        "Links: [Dear ImGui](https://github.com/ocornut/imgui), "
        "[MD4C](https://github.com/mity/md4c), "
        "[imgui_md](https://github.com/mekhontsev/imgui_md)\n";
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

        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            m_quitRequested = true;

        ImGui::EndPopup();
    }

#if defined(_WIN32)
    // Opened via a flag rather than calling OpenPopup() directly from the MenuItem above - MenuItem closes the popup stack it lives in on the same
    // frame it's clicked, and opening a brand-new popup into a stack that's mid-close doesn't reliably work. Deferring one frame sidesteps that.
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
    print(m_markdownText.c_str(), m_markdownText.c_str() + m_markdownText.length());
    m_runs.swap(m_pendingRuns);
}
