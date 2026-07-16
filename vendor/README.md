Third-party libraries. Nothing here is written by hand except this file - see the top-level README for the exact commands to populate the submodules.

- `imgui/` - Dear ImGui (https://github.com/ocornut/imgui) - submodule
- `glfw/` - GLFW (https://github.com/glfw/glfw), pinned to tag `3.3.9` - submodule
- `imgui_md/` - imgui_md (https://github.com/pthom/imgui_md, `imgui_bundle` branch) - renders MD4C's parse events into ImGui draw calls - submodule
- `md4c/` - MD4C (https://github.com/mity/md4c) - the actual CommonMark / GitHub-flavoured markdown parser imgui_md sits on top of - submodule
- `stb/` - `stb_image.h` (https://github.com/nothings/stb) - public domain, vendored directly as a single file (not a submodule) rather than pulling in the whole stb repo for one header. Decodes local image files for `MarkdownView::get_image()`.
