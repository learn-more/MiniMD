---
layout: default
title: MiniMD
---

# MiniMD

A small, native markdown viewer. Dear ImGui + GLFW + OpenGL3, built with premake5.

## Why MiniMD?
MiniMD renders actual CommonMark/GFM markdown - tables, ordered lists, strikethrough, underline, fenced code, basic inline HTML - in a lightweight native window, no browser or Electron involved.

## Features

* Renders CommonMark/GFM: tables, lists, strikethrough, underline, fenced code, inline HTML
* Drag-and-drop a `.md` file onto the window, or pass one as a command-line argument
* Recent Files menu (last 8, persisted across runs)
* Zoom in/out (Ctrl+=/Ctrl+-/Ctrl+0)
* Text selection with click-drag, copy raw markdown source with Ctrl+C
* Register as a `.md` file handler (Windows, no admin rights needed)
* Windows is the primary target; Linux (X11) build support is in place but not yet exercised

