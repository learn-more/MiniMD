# Local images

Image references using paths relative to this file, in two different formats. MiniMD loads local images (`MarkdownView::get_image()` resolves the path against this file's directory and decodes it via stb_image), so all but the last of these should actually display. The last one is a deliberately broken path - no broken-image placeholder, no crash, just no image drawn.

## PNG, no title 🖼️ ✅

![a red square with a label](images/local-red.png)

## JPEG, with a title 🖼️ ✅

![a blue-green gradient](images/local-gradient.jpg "gradient test image")

## Reference-style image 🖼️ ✅

![gradient again][gradient-ref]

[gradient-ref]: images/local-gradient.jpg "same gradient, via reference syntax"

## Image inside a link (common "clickable image" pattern) 🖼️🔗 ✅

[![red square, wrapped in a link](images/local-red.png)](https://github.com/mekhontsev/imgui_md)

## Image with a relative path one level up (should NOT resolve - this repo's images live in testdata/images, not testdata/../images - included to confirm a bad path fails quietly rather than doing something worse) ❌

![intentionally broken path](../images/local-red.png)
