# MiniMD - project conventions

## Comment style

Wrap prose `//` (and `#` in Python tooling) comments to roughly **120-150 characters per line**,
not the ~80-100 col habit that's otherwise common. When adding or editing a multi-line comment,
check the line width afterward (`awk '{ print length }' <file> | sort -rn | head`) and keep every
line inside that range - not one giant unwrapped line, not narrow 80-90 col blocks either.

This applies to files under `src/`, `tools/`, and the local patches in `vendor/imgui_md/` - not to
untouched vendor/third-party code, which keeps whatever wrap width upstream used.

## Comment content

Keep comments brief and to the point - state the non-obvious WHY, not the WHAT (code already shows
that). Don't repeat what's said elsewhere: no restating the function/variable name in prose, no
duplicating a comment on both a declaration and its usage, no re-explaining something the file
header or an adjacent comment already covers.
