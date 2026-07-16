# CommonMark / GFM feature test

A test document covering as much of CommonMark plus the GFM extensions imgui_md/MD4C enables (tables, strikethrough, underline, superscript, subscript, highlight) as practical, for eyeballing MiniMD's rendering. Not every feature listed will necessarily render correctly - that's the point of having it in one place. The "Extended syntax" section near the end also covers md4c flags MiniMD does *not* enable, so their literal (unrendered) fallback is visible too.

## Headings

# H1
## H2
### H3
#### H4
##### H5
###### H6

Setext H1
=========

Setext H2
---------

## Paragraphs and line breaks

This is a normal paragraph that should wrap onto multiple lines once it gets long enough to hit the edge of the window, wrapping being one of the more basic things a markdown viewer needs to get right.

This line ends with two trailing spaces for a hard break.  
This should start on a new line because of that hard break above.

This line has no trailing spaces or backslash,
so it's a soft break and (per CommonMark) should usually just render as a space/continuation rather than a hard line break.

This line ends with a backslash for a hard break.\
This should also start on a new line.

## Emphasis

*italic with asterisks*

_italic with underscore - imgui_md's MD_FLAG_UNDERLINE makes this render as underline, not italic_

**bold with asterisks**

__bold with underscore__

***bold italic with asterisks***

~~strikethrough (GFM)~~

Combined: **bold with _underline-ish_ inside**, and ~~strikethrough with **bold** inside~~.

## Blockquotes

> A single-level blockquote.
>
> A second paragraph in the same blockquote.

> A blockquote
>
> > with a nested blockquote inside it.

## Lists

Unordered, three marker styles (should all produce the same kind of list):

- Item using a hyphen
* Item using an asterisk
+ Item using a plus

Unordered with nesting:

- Top level item one
  - Nested item one
  - Nested item two
    - Doubly-nested item
- Top level item two

Ordered:

1. First item
2. Second item
3. Third item

Ordered, nested with unordered inside:

1. First item
   1. Nested ordered item
   2. Another nested ordered item
      - Unordered item nested inside an ordered list
2. Second item

Ordered list where the numbers in the source don't count up (CommonMark renumbers automatically):

1. First
1. Second
1. Third

GFM task list (task-list extension is enabled, so these should render as checkbox glyphs, unchecked/checked):

- [ ] Unchecked task
- [x] Checked task

## Horizontal rules

Three different marker styles, each on its own line:

---

***

___

## Links

[Inline link](https://github.com/mity/md4c)

[Inline link with a title](https://github.com/mekhontsev/imgui_md "imgui_md on GitHub")

Reference-style link: [Dear ImGui][imgui-ref], defined at the bottom of the document.

Autolink: <https://github.com/ocornut/imgui>

Permissive (bare, no angle brackets) autolinks: https://github.com/mity/md4c, www.github.com, and someone@example.com should all render as clickable links.

[imgui-ref]: https://github.com/ocornut/imgui "Dear ImGui"

## Code

Inline `code span` in the middle of a sentence, and `` a span containing a literal backtick: ` ``.

Indented code block (four spaces), should render as a literal block:

    function add(a, b) {
        return a + b;
    }

Fenced code block with a language tag:

```cpp
int add(int a, int b)
{
    return a + b;
}
```

Fenced code block with no language tag:

```
plain fenced block, no syntax highlighting expected either way
```

## Tables (GFM)

Default alignment (no colons):

Column A | Column B | Column C
---|---|---
short | a somewhat longer cell | 3
**bold cell** | ~~strikethrough cell~~ | `code cell`

Explicit alignment (left / center / right) - each column should now render left/center/right-aligned per the `:---`/`:--:`/`---:` markers below:

Left | Center | Right
:----|:------:|-----:
a | b | c
long left cell | mid | 42

## Inline HTML

Line break via HTML tag:<br>this should be on a new line.

Horizontal rule via HTML tag:

<hr>

Underline via HTML tag: <u>this text is underlined</u>, back to normal.

Non-breaking space via entity: one&nbsp;&nbsp;&nbsp;&nbsp;two (four non-breaking spaces between "one" and "two").

Other character references - named (&copy; &mdash; &hellip; &rarr;), decimal (&#169;), and hex (&#x2192;) - should all decode to their actual characters, not show up as literal `&...;` text.

`<div>` with a class, in case a subclass overrides `html_div()` to do something with it:

<div class="red">

Text inside a `<div class="red">...</div>` block.

</div>

An unrecognized raw HTML tag, which MiniMD can't render (no HTML engine) and should now be dropped rather than shown as literal tag text: some <span class="unsupported">inline HTML</span> here.

## Backslash escapes

Escaped characters that would otherwise be markdown syntax: \*not italic\*, \[not a link\](not-a-url), \# not a heading, \`not code\`.

## Emoji

Plain: 🎉 🚀 💡 📌 🔥 ⭐ 👍 😀 ✅ ❌ ⚠️

Inside emphasis: **🎉 bold with emoji**, *🚀 italic with emoji*, ~~❌ strikethrough with emoji~~.

## Extended syntax

### Superscript, subscript, highlight (native syntax)

E=mc^2^ is ==famous==, and water is H~2~O.

### Superscript, subscript, highlight, keyboard key (inline HTML)

Same result via raw HTML tags instead of the `^^`/`~~`/`==` syntax: E=mc<sup>2</sup> is <mark>famous</mark>, water is H<sub>2</sub>O. Press <kbd>Ctrl</kbd>+<kbd>C</kbd> to copy a selection.

### Admonitions (GitHub-style, hand-rolled from blockquotes)

Not an MD4C parser feature (`MD_FLAG_ADMONITIONS`/`MD_BLOCK_ADMONITION` are unused) - MiniMD detects the `[!KIND]` marker itself as the first text in a blockquote:

> [!NOTE]
> Useful information the user should know, even when skimming content.

> [!TIP]
> Helpful advice for doing things better.

> [!IMPORTANT]
> Key information users need to know to achieve their goal.

> [!WARNING]
> Urgent info that needs immediate user attention to avoid problems.

> [!CAUTION]
> Advises about risks or negative outcomes of certain actions.

### Collapsible sections

<details>
<summary>Click to expand</summary>

Hidden content, only laid out while expanded.

</details>

<details open>
<summary>Starts expanded (has the <code>open</code> attribute)</summary>

Visible immediately.

</details>

### Not enabled - expect literal, unrendered markup below

These md4c extensions exist in the vendored parser but MiniMD never turns their flag on, so the syntax should show up as plain text rather than being parsed:

- Wiki links (`MD_FLAG_WIKILINKS`): [[Some Page]]
- Footnote reference (`MD_FLAG_FOOTNOTES`): a claim needing a citation[^1]
- Spoiler (`MD_FLAG_SPOILERS`): ||a hidden spoiler||
- LaTeX math (`MD_FLAG_LATEXMATHSPANS`): inline $x^2+y^2=z^2$ and display $$\int_0^1 x^2\,dx$$

[^1]: This footnote definition should also render as literal text, not a collected reference at the bottom of the document.

## Images

See `images-local.md` and `images-local-remote.md` for image-specific test cases. Local images load via stb_image; remote (`http(s)://`) references are unsupported (no network code) and fall back to rendering their alt text as a link instead of showing nothing.

A single inline image reference for completeness: ![a small red square](images/local-red.png "local test image")
