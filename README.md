# MiniMD

A small, native markdown viewer. Dear ImGui + GLFW + OpenGL3, built with premake5. Windows is the primary target; the premake scripts and vendor build already handle Linux (X11) too, it just hasn't been exercised yet.

## Why this stack

- **GLFW + OpenGL3** is the lightest, most portable windowing/renderer combo Dear ImGui supports: no platform SDK dependency (unlike a Win32+DX11 backend), no extra runtime weight (unlike SDL), and OpenGL3 needs no shader/pipeline setup of its own - ImGui's backend handles that internally.
- **imgui_md + MD4C** (mekhontsev/imgui_md on top of mity/md4c) renders actual CommonMark/GFM: tables, ordered lists, strikethrough, underline, fenced code, basic inline HTML - not just a pseudo-markdown subset. MD4C does the parsing (small, dependency-free C parser); imgui_md turns its parse callbacks into ImGui draw calls.

## Layout

```
premake5.lua            workspace + configuration
premake/imgui.lua        builds vendor/imgui as a static lib
premake/glfw.lua         builds vendor/glfw as a static lib
premake/imgui_md.lua     builds vendor/imgui_md + vendor/md4c as a static lib
src/                     the app itself (MiniMD project)
vendor/                  submodules (see vendor/README.md)
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

With no argument it shows a built-in sample. You can also drag-and-drop a `.md` file onto the window, or type a path into the box in the menu bar.

## Known limitations / next steps

- No native "Open File" dialog yet - path is typed in, dragged in, or passed as argv[1]. Adding one (e.g. nativefiledialog-extended) would be the next vendor addition.
- No custom fonts - `MarkdownView::get_font()` always returns the default font, so headings/bold text don't actually look bigger or bolder even though imgui_md tracks heading level/bold state as it parses. Wiring up real `ImFont*`s per level (see `vendor/imgui_md/README.md`'s example) is the next step.
- No image loading - `MarkdownView::get_image()` returns false, so `![...]` images are skipped rather than shown as a placeholder.
- Code blocks/spans render as plain text, not monospace - imgui_md doesn't swap fonts for `MD_TEXT_CODE` by default; would need a monospace `ImFont*` wired in similarly to headings.
- Table cell alignment is whatever imgui_md defaults to (left-aligned, header row highlighted) - column alignment markers (`:--`, `--:`) in the source aren't applied to layout.
- The GLFW premake script targets the pre-3.4 source layout (tag `3.3.9`). Bumping to GLFW 3.4+ requires updating `premake/glfw.lua`'s file list for the new platform-abstraction sources (`platform.c`, `null_*.c`).
- Text selection (click-drag over the rendered view, Ctrl+C to copy) has no select-all shortcut and doesn't auto-scroll when you drag past the top/bottom edge. Selecting across a table also follows draw order (row by row, left to right within a row), which can read a little oddly if you select a whole multi-column table at once.

### Local patches to vendor/imgui_md

`vendor/imgui_md` isn't pristine upstream - it has three local fixes on top of whatever commit the submodule points at. If you re-clone the submodule fresh from upstream, reapply all three (or better: commit them inside the submodule itself and push to your fork, so they travel with the pointer instead of living as an uncommitted diff):

1. **Compile fix**: `get_image()`'s default implementation assigns `ImFontAtlas::TexID` (now `ImTextureRef` since imgui 1.92) straight into an `ImTextureID` field, which won't compile against current imgui. Changed that line to `nfo.texture_id = ImGui::GetIO().Fonts->TexID.GetTexID();`

2. **Table column auto-fit fix**: the original `BLOCK_TABLE`/`BLOCK_TR`/`BLOCK_TD` hand-tracked cursor X positions instead of using Dear ImGui's real table system - column width was purely a side effect of the *header* row's unwrapped text, with zero input from body content, so a short header next to long content wrapped that content character-by-character into a squished column. Rewrote those three functions plus `render_text()` to use `ImGui::BeginTable`/`TableNextRow`/`TableNextColumn` with `ImGuiTableFlags_SizingFixedFit`, which auto-fits each column to its widest cell.

   Getting there took two attempts. `SizingFixedFit` needs to see each cell's *true* content width to fit against, but `render_text()` word-wraps cell text before submitting it - originally to the column's own current width, which is circular (the column is still being sized *from* that same content) and gets stuck narrow forever. Fixed by capturing the surrounding page width in `BLOCK_TABLE` *before* the table starts, and having `render_text()` wrap table-cell text to that fixed, table-independent cap (`m_table_wrap_width`) instead of the live column width. That breaks the feedback loop while still capping any single absurdly long cell at the page width rather than blowing out the whole table.

3. **`text_run()` hook, for click-drag text selection**: added a new protected virtual, `text_run(str, str_end, min, max)`, called from `render_text()` once per already-word-wrapped chunk, right before its glyphs are drawn (empty default implementation, so upstream behavior is unchanged unless something overrides it). `MarkdownView` overrides it to record `[str,str_end)` - which points directly into the buffer passed to `print()`, not a copy - together with its on-screen rect, into a per-frame list. That list is what makes rich-text selection possible without a parallel text representation: mouse hit-testing walks it to turn a click position into a byte offset, and copy-to-clipboard walks it again to slice the selected byte range straight out of the original document. The same hook doubles as the paint point for the selection highlight itself (drawn *before* the glyphs, matching the draw order ImGui's own `InputText` uses for its selection background).
