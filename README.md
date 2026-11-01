# MyPaint

A lightweight paint program and image viewer for Windows, written in C++ against
the Win32 API and GDI+. No frameworks, one source file, one small executable.

## Features

- **Drawing tools:** pencil, brush, eraser, fill, smudge, text, colour picker
- **Shapes:** lines, arrows, rectangles, rounded rectangles, ellipses, triangles,
  polygons, stars, hearts, spirals, speech bubbles, check marks and more
- **Selections:** rectangle, lasso and magic wand, with cut, copy and paste;
  Ctrl+C with nothing selected copies the whole image
- **Image operations:** rotate, flip, resize, crop to selection (Ctrl+Shift+X),
  undo and redo
- **Opening files:** File → Open, drag and drop onto the window, or
  File → Open Recent (the last 10 images)
- **Folder browser:** a thumbnail strip for flicking through the images in a
  folder, with Ctrl+F to search by file name (see below)
- **Zoom:** fit to window (shrinks large images, never enlarges small ones),
  fill window, Ctrl + scroll; keyboard Ctrl+= / Ctrl+− to zoom, Ctrl+0 to fit,
  Ctrl+1 for 100%
- **Full screen and slideshow (View menu):** F11 shows the image on black;
  F5 starts a slideshow (3, 5 or 10 seconds per image). The arrow keys and
  scroll wheel step through the folder, Space pauses, Esc returns; moving the
  mouse shows a control bar. A folder browser search limits both to its matches.
- **Dark and light themes** (Ctrl+Shift+D)
- **Formats:** opens PNG, JPEG, BMP, GIF, TIFF, WebP (including the first
  frame of animated WebP) and SVG; saves PNG, JPEG, BMP, GIF, TIFF and WebP.
  HEIC/HEIF (phone photos) and AVIF also open, using Windows' own decoders
  from Microsoft's free Store extensions (HEIF Image Extensions, plus HEVC
  Video Extensions for most phone photos; AV1 Video Extension for AVIF).
  Without them, MyPaint explains what to install.
  An SVG is never overwritten: Save on one opens Save As with a PNG name.
- **Photo orientation:** JPEG and TIFF photos tagged with which way is up
  (EXIF Orientation, as phones and cameras write it) open upright, in the
  window and in the folder browser's thumbnails
- **Save Options (File menu):** JPEG, PNG and WebP tabs, each showing the
  estimated size of the open image. JPEG and WebP: quality (default 100);
  WebP also Lossless. PNG: full colour or 256 colours. PNG and WebP: keep
  transparency (on by default). Saving never asks; it just uses these
  settings. PNGs with no transparent areas are saved without a transparency
  layer, which makes them about a quarter smaller.
- **Open in Inkscape:** SVG files get an "Open in Inkscape" menu item when
  Inkscape is installed

Images load on a background thread, so switching between large files keeps the
window responsive, and zoomed-out views of big images scroll smoothly.

## Folder browser

A strip of thumbnails of every image in the open file's folder, along the
bottom of the window. Click the **rightmost button in the status bar** to show
or hide it.

It opens by default when MyPaint starts with an image (double-clicking one in
Explorer, for example) and stays closed when MyPaint starts on its own. Each of
those remembers your choice separately, so closing it after opening a photo
doesn't affect starting blank, and vice versa. The strip's height is
remembered too.

| Action | Does |
|---|---|
| Click a thumbnail | Open that image, fitted to the window |
| ← / → | Previous / next image; wraps around from last to first |
| Home / End | First / last image |
| Scroll wheel over the strip | Scroll the strip |
| Delete | Send the open image to the Recycle Bin and open the next one |
| F2 | Rename the open image in place (Enter saves, Esc cancels) |
| Ctrl+F | Search: a box at the left end of the strip; only images whose names contain the text are shown (Enter opens the first match, Esc or ✕ closes the search) |
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
- **Sort by**: name (with numbers in order, as in Explorer: "photo 2" before
  "photo 10"), date taken (the photo's own date, else date modified), date
  modified, size or type, ascending or descending. Also available by
  right-clicking empty space in the strip, and from View → Folder Browser
  Sort. MyPaint remembers the choice, and the arrow keys, Delete and the
  slideshow follow it.

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
