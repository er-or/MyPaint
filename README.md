# MyPaint

A lightweight paint program and image viewer for Windows, written in C++ against
the Win32 API and GDI+. No frameworks, one source file, one small executable.

## Features

- **Drawing tools:** pencil, brush, eraser, fill, smudge, text, colour picker
- **Shapes:** lines, arrows, rectangles, rounded rectangles, ellipses, triangles,
  polygons, stars, hearts, spirals, speech bubbles, check marks and more
- **Selections:** rectangle, lasso and magic wand, with cut, copy and paste
- **Image operations:** rotate, flip, resize, undo and redo
- **Folder browser:** an optional thumbnail strip for flicking through the
  images in a folder (see below)
- **Zoom:** fit to window (shrinks large images, never enlarges small ones),
  fill window, Ctrl + scroll to zoom
- **Dark and light themes**
- **Formats:** opens PNG, JPEG, BMP, GIF, TIFF, WebP (including the first
  frame of animated WebP) and SVG; saves PNG, JPEG, BMP, GIF, TIFF and WebP.
  An SVG is never overwritten: Save on one opens Save As with a PNG name.
- **Open in Inkscape:** SVG files get an "Open in Inkscape" menu item when
  Inkscape is installed

Images load on a background thread, so switching between large files keeps the
window responsive, and zoomed-out views of big images scroll smoothly.

## Folder browser

Hidden by default. Click the **rightmost button in the status bar** to show a
strip of thumbnails of every image in the open file's folder along the bottom
of the window. MyPaint remembers whether it was open, and its height, between
sessions.

| Action | Does |
|---|---|
| Click a thumbnail | Open that image, fitted to the window |
| ← / → | Previous / next image; wraps around from last to first |
| Home / End | First / last image |
| Scroll wheel over the strip | Scroll the strip |
| Delete | Send the open image to the Recycle Bin and open the next one |
| F2 | Rename the open image in place (Enter saves, Esc cancels) |
| Right-click, or the ⋯ button on a thumbnail | Menu for that file (below) |
| Menu key / Shift+F10 | Menu for the open image |
| Drag the strip's top edge | Resize it, from 40 px up to half the screen; thumbnails grow with it |

The thumbnail menu has:

- **Rotate right / left**: turns the file and saves it (lossless for JPEGs
  whose size allows it; not available for SVG, GIF or animated WebP)
- **Copy image / Copy path**: puts the picture or its full path on the clipboard
- **Show in Explorer**: opens the folder with the file selected
- **Open in Inkscape**: for SVG files, when Inkscape is installed
- **Rename / Delete**: as F2 / Delete, but for any thumbnail
- **Properties**: the standard Windows file Properties dialog

A darker gap in the strip marks where the list wraps from the last file to the
first. While the strip is open the Hand tool is selected, so dragging the canvas
pans the image; your previous tool comes back when you close it. Deleted files
go to the Recycle Bin, so they can be restored.

## Building

Requirements:

- Windows 10 or later
- Visual Studio with the **Desktop development with C++** workload
  (the project uses the v145 toolset and C++17)

Steps:

1. Open `MyPaint.sln` in Visual Studio.
2. Choose the **Release | x64** configuration.
3. Build. The program is written to `x64\Release\MyPaint.exe`.

Or from a Developer Command Prompt:

```
msbuild MyPaint.sln /p:Configuration=Release /p:Platform=x64
```

All dependencies are included in the repository; nothing else needs to be
downloaded.

## Project layout

| Path | Contents |
|---|---|
| `MyPaint.cpp` | The whole application |
| `MyPaint.rc`, `resource.h` | Menus, dialogs and the icon |
| `app.manifest` | DPI awareness and visual styles |
| `nanosvg.h`, `nanosvgrast.h` | SVG parser and rasteriser |
| `third_party/libwebp-1.4.0-windows-x64/` | WebP headers and libraries |
| `_dev/` | Source artwork for the app and menu icons |
| `make_icon.ps1` | Builds `MyPaint.ico` from `_dev/mypaint_icon.svg` (and `_dev/mypaint_icon_small.svg` for 32 px and below) |

## Third-party code

- [NanoSVG](https://github.com/memononen/nanosvg) by Mikko Mononen, zlib licence
  (see the header files)
- [libwebp](https://github.com/webmproject/libwebp) by Google, BSD licence
  (see `third_party/libwebp-1.4.0-windows-x64/COPYING`)
- Menu and toolbar icons based on
  [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons)
  by Microsoft, MIT licence
