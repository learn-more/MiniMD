-- Builds the markdown renderer: imgui_md (turns MD4C's parse events into ImGui draw calls) + MD4C itself (the actual CommonMark/GitHub-flavoured parser - this is what gives us tables, ordered lists, strikethrough, underline, fenced code blocks and basic inline HTML, none of which the original imgui_markdown backend supported).

project "ImGuiMd"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "../vendor/imgui_md/imgui_md.h",
        "../vendor/imgui_md/imgui_md.cpp",
        "../vendor/md4c/src/md4c.h",
        "../vendor/md4c/src/md4c.c",
    }

    includedirs
    {
        "../vendor/imgui",
        "../vendor/imgui_md",
        "../vendor/md4c/src",
    }

    filter "system:windows"
        systemversion "latest"

    filter "system:linux"
        pic "on"

    filter {}
