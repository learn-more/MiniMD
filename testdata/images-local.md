# Local images

Image references using paths relative to this file, in two different formats. MiniMD currently has no image loading implemented (`MarkdownView::get_image()` always returns `false`), so all of these are expected to be skipped entirely - no broken-image placeholder, no crash, just no image drawn - rather than actually displaying anything.

## PNG, no title

![a red square with a label](images/local-red.png)

## JPEG, with a title

![a blue-green gradient](images/local-gradient.jpg "gradient test image")

## Reference-style image

![gradient again][gradient-ref]

[gradient-ref]: images/local-gradient.jpg "same gradient, via reference syntax"

## Image inside a link (common "clickable image" pattern)

[![red square, wrapped in a link](images/local-red.png)](https://github.com/mekhontsev/imgui_md)

## Image with a relative path one level up (should NOT resolve - this repo's images live in testdata/images, not testdata/../images - included to confirm a bad path fails quietly rather than doing something worse)

![intentionally broken path](../images/local-red.png)
