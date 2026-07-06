# MiniMD

A small, native markdown viewer. Dear ImGui + GLFW + OpenGL3, built with premake5.
Windows is the primary target; the premake scripts and vendor build already
handle Linux (X11) too, it just hasn't been exercised yet.

## Why this stack

- **GLFW + OpenGL3** is the lightest, most portable windowing/renderer combo
  Dear ImGui supports: no platform SDK dependency (unlike a Win32+DX11
  backend), no extra runtime weight (unlike SDL), and OpenGL3 needs no
  shader/pipeline setup of its own - ImGui's backend handles that internally.
- **imgui_markdown** (juliettef/imgui_markdown) is a single header that
  parses and renders markdown directly into ImGui draw calls - no
  intermediate AST, no extra renderer to wire up.

## Layout

```
premake5.lua            workspace + configuration
premake/imgui.lua        builds vendor/imgui as a static lib
premake/glfw.lua         builds vendor/glfw as a static lib
src/                     the app itself (MiniMD project)
vendor/                  submodules (see vendor/README.md)
```

## First-time setup

The vendor libraries aren't committed - pull them in as submodules once you
have this repo on a machine with internet access:

```
git submodule add https://github.com/ocornut/imgui.git vendor/imgui
git submodule add -b 3.3.9 https://github.com/glfw/glfw.git vendor/glfw
git submodule add https://github.com/juliettef/imgui_markdown.git vendor/imgui_markdown
git submodule update --init --recursive
```

(If the repo was cloned from somewhere that already has `.gitmodules`
committed, just run `git submodule update --init --recursive` instead.)

## Building - Windows

Get `premake5.exe` (https://premake.github.io/download) onto your PATH, then
from the repo root:

```
premake5 vs2022
```

Open the generated `MiniMD.sln` and build. The exe lands in
`bin/<config>-windows-x86_64/MiniMD/`.

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

With no argument it shows a built-in sample. You can also drag-and-drop a
`.md` file onto the window, or type a path into the box in the menu bar.

## Known limitations / next steps

- No native "Open File" dialog yet - path is typed in, dragged in, or passed
  as argv[1]. Adding one (e.g. nativefiledialog-extended) would be the next
  vendor addition.
- No custom fonts - headings are rendered with the default font plus a
  separator line rather than a genuinely larger/bolder face. Loading extra
  `ImFont*` sizes and wiring them into `MarkdownView`'s heading config would
  fix this.
- Only H1-H3, emphasis, unordered lists, links, images, and horizontal rules
  are supported - whatever imgui_markdown itself covers. No tables, no code
  blocks/fencing, no ordered lists.
- The GLFW premake script targets the pre-3.4 source layout (tag `3.3.9`).
  Bumping to GLFW 3.4+ requires updating `premake/glfw.lua`'s file list for
  the new platform-abstraction sources (`platform.c`, `null_*.c`).
