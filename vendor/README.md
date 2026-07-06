Third-party libraries, vendored as git submodules. Nothing here is written by
hand except this file - see the top-level README for the exact commands to
populate this directory.

- `imgui/` - Dear ImGui (https://github.com/ocornut/imgui)
- `glfw/` - GLFW (https://github.com/glfw/glfw), pinned to tag `3.3.9`
- `imgui_md/` - imgui_md (https://github.com/mekhontsev/imgui_md) - renders
  MD4C's parse events into ImGui draw calls
- `md4c/` - MD4C (https://github.com/mity/md4c) - the actual CommonMark /
  GitHub-flavoured markdown parser imgui_md sits on top of
