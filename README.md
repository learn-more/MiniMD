# MiniMD

A small, native markdown viewer. Dear ImGui + GLFW + OpenGL3, built with premake5. Windows is the primary target; the premake scripts and vendor build already handle Linux (X11) too, it just hasn't been exercised yet.

## Why this stack

- **GLFW + OpenGL3** is the lightest, most portable windowing/renderer combo Dear ImGui supports: no platform SDK dependency (unlike a Win32+DX11 backend), no extra runtime weight (unlike SDL), and OpenGL3 needs no shader/pipeline setup of its own - ImGui's backend handles that internally.
- **imgui_md + MD4C** (mekhontsev/imgui_md on top of mity/md4c) renders actual CommonMark/GFM: tables, ordered lists, blockquotes, bold/italic, strikethrough, underline, fenced code, local images - not just a pseudo-markdown subset. MD4C does the parsing (small, dependency-free C parser); imgui_md turns its parse callbacks into ImGui draw calls.

## Layout

```
premake5.lua            workspace + configuration
premake/imgui.lua        builds vendor/imgui as a static lib
premake/glfw.lua         builds vendor/glfw as a static lib
premake/imgui_md.lua     builds vendor/imgui_md + vendor/md4c as a static lib
src/                     the app itself (MiniMD project)
vendor/                  submodules + vendored single-header libs (see vendor/README.md)
```

## First-time setup

The vendor libraries aren't committed - pull them in as submodules once you have this repo on a machine with internet access:

```
git submodule add https://github.com/ocornut/imgui.git vendor/imgui
git submodule add https://github.com/glfw/glfw.git vendor/glfw
cd vendor/glfw && git checkout 3.3.9 && cd ../..
git submodule add https://github.com/mekhontsev/imgui_md.git vendor/imgui_md
git submodule add https://github.com/mity/md4c.git vendor/md4c
git submodule update --init --recursive
```

(If the repo was cloned from somewhere that already has `.gitmodules` committed, just run `git submodule update --init --recursive` instead.)

## Building - Windows

Get `premake5.exe` (https://premake.github.io/download) onto your PATH, then from the repo root:

```
premake5 vs2022
```

Open the generated `MiniMD.sln` and build. The exe lands in `bin/<config>-windows-x86_64/MiniMD/`.

## Building - Linux (future)

```
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
premake5 gmake2
make -j config=release
```

## Running

```
MiniMD.exe path\to\file.md
```

With no argument it shows a built-in sample. You can also drag-and-drop a `.md` file onto the window, or reopen one via the right-click menu's Recent Files. There's no menu bar or title bar - the OS window title itself shows "MiniMD - `<loaded path>`" (or a fallback when nothing's loaded).

## Right-click menu

There's no menu bar - right-click anywhere over the document for:

- **Reload** - re-reads the currently open file from disk. Disabled when no file is open.
- **Recent Files** - last 8 successfully-opened files, most recent first, persisted across runs to `recent.txt` in a per-user config dir (`%APPDATA%\MiniMD` on Windows, `$XDG_CONFIG_HOME/minimd` or `~/.config/minimd` on Linux). Stored as absolute paths so they still resolve regardless of the process's working directory on a later run. Grayed out when empty; "Clear Recent Files" empties it. Only ever populated by actually opening a file - the welcome sample doesn't add to it.
- **View > Zoom In / Zoom Out / Reset Zoom** - scales `io.FontGlobalScale` between 0.5x and 3.0x in 0.1 steps. Also bound to Ctrl+=/Ctrl+-/Ctrl+0. Current zoom percentage shown at the top of the submenu.
- **Debug > (test file names)** - Debug builds only (gated on the `DEBUG` preprocessor define set by `configurations:Debug` in the root `premake5.lua`). Loads one of the `testdata/*.md` files by locating `testdata/` relative to the running exe's own path (`<repo_root>/bin/<cfg>-<system>-<arch>/MiniMD/../../../testdata`), so it works regardless of the process's working directory. Won't appear in Release builds at all.
- **Options** (Windows only) - opens a small dialog with a single toggle button: "Register as .md handler" (registers MiniMD as an available "Open with" handler for `.md` files under `HKCU\Software\Classes`, no admin rights needed - doesn't force it as the default, Windows 8+ gates that behind user confirmation in Explorer/Settings) or "Unregister .md handler" if already registered.
- **Help > About** - opens a small modal with an author/tool summary and third-party attributions (Dear ImGui, GLFW, imgui_md, MD4C, stb_image), links included. Rendered by a second, separate `imgui_md` instance (`AboutView`) on a fixed, short document, so it can't disturb the loaded file's own state.
- **Help > Check for Updates** - opens the GitHub releases page (`https://github.com/learn-more/MiniMD/releases`) in the default browser. No version check - just a shortcut to the page.
- **Exit** - quits the app (`MarkdownView::WantsQuit()`, checked by `main.cpp`'s loop alongside `glfwWindowShouldClose()` - this class has no GLFW dependency itself).

Copying the current selection's raw markdown source is Ctrl+C (no menu item needed - it works directly off the click-drag selection).

## Known limitations / next steps

- No native "Open File" dialog and no path-entry box - a file's opened by dragging it in, passing it as argv[1], or picking it from Recent Files. Adding a dialog (e.g. nativefiledialog-extended) would be the next vendor addition.
- No remote image loading - `MarkdownView::get_image()` only resolves and decodes local files (via stb_image); `http(s)://` references are silently skipped rather than fetched, same as a broken local path. There's no network code in this app.
- Table cell alignment is whatever imgui_md defaults to (left-aligned, header row highlighted) - column alignment markers (`:--`, `--:`) in the source aren't applied to layout.
- The GLFW premake script targets the pre-3.4 source layout (tag `3.3.9`). Bumping to GLFW 3.4+ requires updating `premake/glfw.lua`'s file list for the new platform-abstraction sources (`platform.c`, `null_*.c`).
- Text selection (click-drag over the rendered view, Ctrl+C to copy) has no select-all shortcut and doesn't auto-scroll when you drag past the top/bottom edge. Selecting across a table also follows draw order (row by row, left to right within a row), which can read a little oddly if you select a whole multi-column table at once.

### Local patches to vendor/imgui_md

`vendor/imgui_md` isn't pristine upstream, but the local diff is kept deliberately small: wherever imgui_md already exposes a `protected virtual` for the behavior MiniMD needs to change, that's overridden from `MarkdownView`/`AboutView` instead of editing the vendored `.cpp` directly, and where no such hook existed one was added (empty/no-op default, so upstream behavior is unchanged unless something overrides it) rather than hand-patching the base class body. What's left, committed inside the submodule itself so it travels with the pointer:

1. **Compile fix**: `get_image()`'s default implementation assigns `ImFontAtlas::TexID` (now `ImTextureRef` since imgui 1.92) straight into an `ImTextureID` field, which won't compile against current imgui. Changed that line to `nfo.texture_id = ImGui::GetIO().Fonts->TexID.GetTexID();`

2. **`get_table_wrap_width()` hook, for real auto-fit table columns**: the base class's own `BLOCK_TABLE`/`BLOCK_TR`/`BLOCK_TD` hand-track cursor X positions instead of using Dear ImGui's real table system - column width is purely a side effect of the *header* row's unwrapped text, with zero input from body content, so a short header next to long content wraps that content character-by-character into a squished column. `MarkdownView` overrides those three (all already virtual upstream) with `ImGui::BeginTable`/`TableNextRow`/`TableNextColumn` and `ImGuiTableFlags_SizingFixedFit`, which auto-fits each column to its widest cell - see `MarkdownView::BLOCK_TABLE()`.

   The one piece that can't move to `MarkdownView` is `render_text()`'s own word-wrap: it's a private, non-virtual method, and `SizingFixedFit` needs to see each cell's *true* content width to fit against, but wrapping cell text to the column's own current (still-settling) width is circular and gets stuck narrow forever. `get_table_wrap_width()` is the new hook that breaks that loop - `render_text()` calls it (default: the same full content-region width as a non-table run) instead of touching any table-layout state directly, and `MarkdownView`'s override hands back a fixed, table-independent cap captured once in its own `BLOCK_TABLE()` before the table starts.

3. **`text_run()` hook, for click-drag text selection**: called from `render_text()` once per already-word-wrapped chunk, right before its glyphs are drawn. `MarkdownView` overrides it to record `[str,str_end)` - which points directly into the buffer passed to `print()`, not a copy - together with its on-screen rect, into a per-frame list. That list is what makes rich-text selection possible without a parallel text representation: mouse hit-testing walks it to turn a click position into a byte offset, and copy-to-clipboard walks it again to slice the selected byte range straight out of the original document. The same hook doubles as the paint point for the selection highlight itself (drawn *before* the glyphs, matching the draw order ImGui's own `InputText` uses for its selection background).

4. **`render_task_checkbox()` hook, for task list items**: `BLOCK_LI()` needed a small, unavoidable patch (it reads the private list-item stack, so it can't move out entirely) to recognize `- [ ]`/`- [x]` items and call this new hook in place of the usual bullet/number; `MarkdownView` overrides it to draw a checkbox glyph via `ImDrawList` instead of depending on a "☐"/"☑" code point being present in whatever font is loaded (see `MarkdownView::render_task_checkbox()`).

5. **`m_md` moved from `private` to `protected`**, for opting into extra md4c syntax: `MD_FLAG_TASKLISTS`/`MD_FLAG_PERMISSIVEAUTOLINKS` (task list items, bare URLs becoming links) aren't in the base class's own default flag set. A virtual hook can't fix this up - a virtual call made from the base class's own constructor (where those default flags are set) would still resolve to the base class, not any override, since the derived part of the object doesn't exist yet at that point - so `MarkdownView`'s own constructor just ORs the extra flags into `m_md.flags` directly, after base construction has already finished setting the defaults.

6. **Blockquote rendering**: the original `BLOCK_QUOTE` was an empty stub - `> quoted text` rendered as an indistinguishable, unindented paragraph. `MarkdownView` overrides it to indent, dim the text (`ImGuiCol_TextDisabled`), and draw a left bar spanning the quote's full height once its content (and therefore bottom Y) is known - each nesting level gets its own bar, drawn at the midpoint of its own indent gutter, so nested blockquotes stack correctly.
