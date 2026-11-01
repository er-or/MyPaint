// MyPaint.cpp
// A Win32 GDI+ paint application — MS-Paint-style clone.
// Single .cpp file; links: gdiplus.lib comctl32.lib comdlg32.lib
//                           msimg32.lib ole32.lib user32.lib gdi32.lib shell32.lib

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <objbase.h>

// GDI+
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// libwebp
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/demux.h>   // animated WebP (first frame)
#pragma comment(lib, "libwebpdemux.lib")

// nanosvg (header-only SVG parser + rasterizer)
#pragma warning(push)
#pragma warning(disable: 4996 4244)  // suppress deprecated + double-to-float for nanosvg
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"
#pragma warning(pop)
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#include <dwmapi.h>
#include <uxtheme.h>
#include <d2d1_3.h>
#include <d3d11.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

using namespace Gdiplus;

#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <memory>
#include <thread>
#include <strsafe.h>

using std::max;
using std::min;

#include "resource.h"

// ============================================================
//  Constants
// ============================================================
static const wchar_t* APP_CLASS  = L"MyPaintWndClass";
static const wchar_t* APP_TITLE  = L"MyPaint";

static const int DEFAULT_CANVAS_W  = 800;
static const int DEFAULT_CANVAS_H  = 600;
static const int TOOLBAR_HEIGHT    = 42;
static const int COLORSTRIP_H      = 28;   // height of colour-swatch strip
static const int PALETTE_CELL      = 34;   // px per cell in More palette
static const int PALETTE_COLS      = 4;    // columns in More palette
static const int PALETTE_PAD       = 4;    // border padding around grid
static const int STATUS_HEIGHT     = 22;
static const int MAX_UNDO          = 20;
static const int SCROLL_PAD        = 200;  // pixels of scroll margin around canvas
static const float DEFAULT_BRUSH_SZ = 8.0f;
static const float MIN_BRUSH_SZ     = 0.5f;
static const float MAX_BRUSH_SZ     = 100.0f;
static const int   DEFAULT_ERASER_SZ = 40;
static const int   MIN_ERASER_SZ     = 1;
static const int   MAX_ERASER_SZ     = 500;
static const int   DEFAULT_SMUDGE_SZ = 20;
static const int   MIN_SMUDGE_SZ     = 1;
static const int   MAX_SMUDGE_SZ     = 500;

// Colour-strip layout constants
static const int CS_FG_X           = 4;    // foreground swatch left edge
static const int CS_FG_W           = 30;
static const int CS_FG_H           = 20;
static const int CS_BG_OX          = 14;   // bg swatch offset from FG_X
static const int CS_CUSTOM_X       = 60;   // first custom-colour swatch
static const int CS_SWATCH_W       = 14;
static const int CS_SWATCH_H       = 14;
static const int CS_SWATCH_GAP     = 1;
static const int CS_CUSTOM_COUNT   = 16;
static const int CS_COLEDIT_X      = CS_CUSTOM_X + CS_CUSTOM_COUNT * (CS_SWATCH_W + CS_SWATCH_GAP) + 3;
static const int CS_COLEDIT_W      = 22;
static const int CS_COLEDIT_H      = 18;
static const int CS_RESET_X        = CS_COLEDIT_X + CS_COLEDIT_W + 2;
static const int CS_RESET_W        = 22;
static const int CS_RESET_H        = 18;
static const int CS_HIT_RESET      = 20;  // HitTest return value for reset button

// Alpha (opacity) section — right after custom colours / reset button
static const int CS_ALPHA_SEP_X  = CS_RESET_X + CS_RESET_W + 4;
static const int CS_ALPHA_LBL_X  = CS_ALPHA_SEP_X + 8;
static const int CS_ALPHA_LBL_W  = 36;   // "Opacity"
static const int CS_ALPHA_EDIT_X = CS_ALPHA_LBL_X + CS_ALPHA_LBL_W + 2;
static const int CS_ALPHA_EDIT_W = 34;
static const int CS_ALPHA_EDIT_H = 18;

// Line Width section (all x values are relative to left edge of main window)
static const int CS_LW_SEP_L_X    = CS_ALPHA_EDIT_X + CS_ALPHA_EDIT_W + 4;   // left separator
static const int CS_LW_LABEL_X    = CS_LW_SEP_L_X + 6;             // label (2=sep + 4=pad)
static const int CS_LW_LABEL_W    = 53;
static const int CS_LW_PREV_X     = CS_LW_LABEL_X + CS_LW_LABEL_W + 5;
static const int CS_LW_PREV_W     = 18;
static const int CS_LW_EDIT_X     = CS_LW_PREV_X + CS_LW_PREV_W + 2;
static const int CS_LW_EDIT_W     = 34;
static const int CS_LW_EDIT_H     = 18;
static const int CS_LW_MINUS_X    = CS_LW_EDIT_X + CS_LW_EDIT_W + 1;
static const int CS_LW_BTN_W      = 20;
static const int CS_LW_BTN_H      = 18;
static const int CS_LW_PLUS_X     = CS_LW_MINUS_X + CS_LW_BTN_W;
static const int CS_LW_ER_SQ_X    = CS_LW_PLUS_X + CS_LW_BTN_W + 2;   // eraser square button
static const int CS_LW_ER_RD_X    = CS_LW_ER_SQ_X + CS_LW_BTN_W;      // eraser round button
static const int CS_LW_SEP_R_X    = CS_LW_ER_RD_X + CS_LW_BTN_W + 4;  // right separator

// Wand tool section (replaces eraser-shape buttons with Contiguous checkbox)
static const int CS_WD_CONTIG_X   = CS_LW_ER_SQ_X;                       // checkbox left
static const int CS_WD_CONTIG_W   = 80;                                    // "Contiguous" + box
static const int CS_WD_SEP_R_X    = CS_WD_CONTIG_X + CS_WD_CONTIG_W + 4; // right separator (wand)

// Text font toolbar section (shown in colour strip when text tool active)
static const int CS_TF_FACE_X  = CS_LW_SEP_L_X + 6;  // font face combobox left
static const int CS_TF_FACE_W  = 140;
static const int CS_TF_FACE_H  = 18;                  // edit portion height in strip
static const int CS_TF_SIZE_X  = CS_TF_FACE_X + CS_TF_FACE_W + 4;
static const int CS_TF_SIZE_W  = 40;
static const int CS_TF_B_X     = CS_TF_SIZE_X + CS_TF_SIZE_W + 6;
static const int CS_TF_BTN_W   = 22;
static const int CS_TF_BTN_H   = 18;
static const int CS_TF_I_X     = CS_TF_B_X   + CS_TF_BTN_W + 1;
static const int CS_TF_U_X     = CS_TF_I_X   + CS_TF_BTN_W + 1;
static const int CS_TF_SEP_R_X = CS_TF_U_X   + CS_TF_BTN_W + 4;

// Zoom controls (positioned in the status bar via LayoutChildren)
static const int ZOOM_EDIT_W      = 42;   // fits "1600%"
static const int ZOOM_BTN_W       = 20;
static const int ZOOM_BTN_H       = 18;
static const int ZOOM_ZONE_W      = 194;  // total reserved width at right of status bar
static const int FOLDER_BROWSER_H     = 140;  // default folder browser height
static const int FOLDER_BROWSER_MIN_H = 40;   // drag limits: this (room for a preview), and half the screen
static const int FOLDER_LABEL_H       = 16;   // file name line under each thumbnail
static const int FOLDER_GRIP_H        = 5;    // top edge band that resizes the strip
static const int FOLDER_SEARCH_W      = 200;  // search panel at the left end of the strip (Ctrl+F)
static int g_slideSeconds = 5;                // slideshow interval: 3, 5 or 10
static int g_folderBrowserH = FOLDER_BROWSER_H;  // chosen by dragging the strip's top edge
static int g_folderCellW    = 112;  // cell width; follows the strip height
static int g_folderThumbMax = 100;  // size thumbnails are made at; follows the strip height

// ============================================================
//  Tool enumeration
// ============================================================
enum Tool {
    TOOL_PENCIL = 0,
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_FILL,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_TEXT,
    TOOL_SELECT,    // rectangular selection
    TOOL_RRECT,
    TOOL_TRIANGLE,
    TOOL_ARROW_L,
    TOOL_ARROW_R,
    TOOL_ARROW_U,
    TOOL_ARROW_D,
    TOOL_ARROW,
    TOOL_STICKMAN,
    TOOL_SPEECH,
    TOOL_COMPUTER,
    TOOL_STAR,
    TOOL_HEART,
    TOOL_LIGHTNING,
    TOOL_MUSTACHE,
    TOOL_DIAMOND,
    TOOL_PENTAGON,
    TOOL_HEXAGON_H,
    TOOL_HEXAGON_V,
    TOOL_OCTAGON,
    TOOL_SPIRAL,
    TOOL_SPIRAL_R,
    TOOL_DROPPER,
    TOOL_WAND,
    TOOL_LASSO,
    TOOL_SMUDGE,
    TOOL_CROSS,
    TOOL_XMARK,
    TOOL_CHECK,
    TOOL_HAND,
    TOOL_COUNT
};

enum BrushStyle {
    BRUSH_ROUND = 0,
    BRUSH_CALLIGRAPHY,
    BRUSH_AIRBRUSH,
    BRUSH_MARKER,
    BRUSH_CRAYON,
    BRUSH_OIL,
    BRUSH_COUNT
};

// ============================================================
//  Application state (globals)
// ============================================================

// Window handles
static HWND  g_hwndMain      = NULL;
static HWND  g_hwndToolbar   = NULL;
static HWND  g_hwndCanvas    = NULL;
static HWND  g_hwndStatus    = NULL;
static HIMAGELIST g_hToolImageList = NULL;
static HMENU g_hMenuMain     = NULL;  // loaded at startup; submenus shown as toolbar dropdowns
static HWND  g_hwndBrushSz   = NULL;   // brush-size edit box

// Custom tool cursors (created once at startup, valid for app lifetime)
static HCURSOR g_hCurPencil  = NULL;
static HCURSOR g_hCurBrush[BRUSH_COUNT] = {};
static COLORREF g_lastBrushCursorColor = (COLORREF)-1; // sentinel: -1 means not yet built
static HCURSOR g_hCurDropper = NULL;
static HCURSOR g_hCurWand    = NULL;
static HCURSOR g_hCurFill    = NULL;  // recreated on every foreground colour change
static HCURSOR g_hCurLasso   = NULL;
static HCURSOR g_hCurRotate    = NULL;
static HCURSOR g_hCurCornerRot = NULL;  // ctrl+corner rotate: white fill, black outline
static HCURSOR g_hCurSmudge   = NULL;
static Bitmap* g_pBmpRotHandle = nullptr;

static const int   kRotHandSz  = 24;    // handle icon pixel size
static const float kRotHandOff = 28.f;  // px above bounding-box top (local frame)
static const float kRotHandRad = 13.f;  // hit-test radius px

// Lasso tool state
static std::vector<PointF> g_lassoPoints;

// Status bar pane icons (Fluent Icons)
static HICON g_hIcoStatusBorder = NULL;  // selection size pane
static HICON g_hIcoStatusCursor = NULL;  // mouse position pane
static HICON g_hIcoStatusCrop   = NULL;  // canvas size pane
// File-menu icon bitmaps — 32bpp premultiplied-alpha DIBs set via MIIM_BITMAP.
// Regenerated by SetFileMenuBitmaps() whenever dark/light mode changes.
static HBITMAP g_hbmFileMenuIcons[8]  = {};
static HBITMAP g_hbmEditMenuIcons[6]  = {};  // Undo, Redo, Cut, Copy, Paste, SelectAll
static HBITMAP g_hbmImageMenuIcons[6] = {};  // RotateRight, RotateLeft, FlipH, FlipV, Crop, Resize
static HBITMAP g_hbmViewMenuIcons[9] = {};   // one per kViewMenuEntries item

// Pre-rendered opaque bitmaps for status bar icons — used in WM_DRAWITEM via BitBlt
// so the icon draw is a single atomic GDI operation and never flickers.
static HBITMAP g_hbmStatusCursor = NULL;
static HBITMAP g_hbmStatusBorder = NULL;
static HBITMAP g_hbmStatusCrop   = NULL;
static HICON g_hIcoColorEdit    = NULL;  // colour strip "Edit Colors" button
static HICON g_hIcoZoomOut      = NULL;  // zoom-out button
static HICON g_hIcoZoomIn       = NULL;  // zoom-in button

// Right-aligned toolbar overlay buttons (dark mode toggle + help)
static HWND  g_hwndBtnDark   = NULL;
static HWND  g_hwndBtnHelp   = NULL;
static HICON g_hIcoMoon      = NULL;  // bright-mode icon for dark-mode button
static HICON g_hIcoSun       = NULL;  // dark-mode icon for dark-mode button
static HICON g_hIcoHelpBtn   = NULL;  // help button icon
static int   g_tbSepX        = -1;    // x coord of vertical separator in toolbar client space
static bool  g_btnDarkHot    = false; // mouse is over the dark-mode toggle button
static bool  g_btnHelpHot    = false; // mouse is over the help button
static bool  g_btnZoomInHot    = false;
static bool  g_btnZoomOutHot   = false;
static bool  g_btnZoomFitHot   = false;
static bool  g_zoomFitMode     = false;   // true = fitted to window
static bool  g_btnFolderBrowserHot    = false;
static bool  g_showFolderBrowser     = false;
static Tool  g_toolBeforeFolderBrowser = TOOL_PENCIL;  // restored when folder browser is hidden
static float g_zoomBeforeFolderBrowser = 1.0f;         // restored when folder browser is hidden
static bool  g_fitModeBeforeFolderBrowser = false;     // likewise
static int   g_sbFolderSepX        = -1;            // separator line X in status bar client coords

// Hand-tool pan state
static POINT g_handDragStart = {};
static int   g_handScrollX0  = 0;
static int   g_handScrollY0  = 0;
static bool  g_btnColorEditHot = false;
static bool  g_btnResetHot     = false;
static bool  g_btnSzMinusHot   = false;
static bool  g_btnSzPlusHot    = false;
static bool  g_btnEraserSqHot  = false;
static bool  g_btnEraserRdHot  = false;
static bool  g_btnSmudgeSqHot  = false;
static bool  g_btnSmudgeRdHot  = false;

// Status bar pane text storage (for owner-draw)
static wchar_t g_statusPaneText[6][128] = {};

// GDI+
static ULONG_PTR  g_gdiplusToken = 0;
static Bitmap*    g_pCanvas      = nullptr;  // backing store
static Bitmap*    g_pPreview     = nullptr;  // in-progress shape overlay
static bool       g_previewUsed  = false;    // g_pPreview may hold something; when false painting skips it
static Bitmap*    g_zoomViewCache = nullptr; // g_pCanvas pre-shrunk for zoom < 1, reused while scrolling
static Bitmap*    g_zoomViewSrc   = nullptr; // canvas it was made from
static unsigned   g_zoomViewVersion = 0;     // g_canvasVersion it was made at
static Graphics*  g_pGfxCanvas   = nullptr;  // Graphics on canvas

// Drawing state
static Tool      g_currentTool  = TOOL_PENCIL;
static BrushStyle g_brushType    = BRUSH_ROUND;
static COLORREF  g_fgColor      = RGB(0, 0, 0);
static COLORREF  g_bgColor      = RGB(255, 255, 255);
static float     g_brushSize    = DEFAULT_BRUSH_SZ;
static bool     g_drawing      = false;
static POINT    g_ptStart      = {0, 0};
static POINT    g_ptLast       = {0, 0};
static bool     g_modified     = false;

// Canvas scroll / positioning
static int  g_canvasOffX = 0;
static int  g_canvasOffY = 0;

// Current file
static wchar_t g_currentFile[MAX_PATH] = L"";

// Undo / redo stacks
static std::vector<Bitmap*> g_undoStack;
static std::vector<Bitmap*> g_redoStack;

// Custom colours for ChooseColor
static COLORREF g_customColors[16] = {};

// ─── Selection state ────────────────────────────────────────
static bool  g_hasSelection  = false;
static RECT  g_selRect       = {};   // canvas-pixel coords, always normalised

// ─── Magic wand / pixel mask ────────────────────────────────
static int               g_wandTolerance   = 32;    // per-channel tolerance 0–255
static int               g_fillTolerance   = 5;     // per-channel tolerance 0–255
static bool              g_wandContiguous  = true;  // flood-fill vs. whole-canvas match
static std::vector<BYTE> g_selMask;                 // 1=selected, size = g_selMaskW * canvas_height
static UINT              g_selMaskW        = 0;     // canvas width when mask was captured
static bool              g_hasSelMask      = false; // true when mask (not plain rect) is active

// ─── Marching ants (wand selection animation) ────────────────
struct BndRun { int a, b1, b2; };              // a=fixed coord, [b1,b2)=span
static std::vector<BndRun> g_selHEdges;        // horizontal boundary edges: y=a, x=[b1,b2)
static std::vector<BndRun> g_selVEdges;        // vertical boundary edges:   x=a, y=[b1,b2)
static int                 g_marchAntPhase = 0;
static UINT_PTR            g_marchAntTimer = 0;

// ─── Colour strip extras ────────────────────────────────────
static HWND  g_hwndColorTip  = NULL;   // tooltip for reset button
static HFONT g_hSymbolFont   = NULL;   // Segoe UI Symbol for ↺ glyph
static HFONT g_hEditFont     = NULL;   // smaller font for line-width edit box
static HWND  g_hwndReset     = NULL;   // real themed reset button
static HWND  g_hwndColorEdit = NULL;   // "Edit Colors" button
static bool  g_showCustomColors = true;  // hidden when window is too narrow
static bool  g_strokeActive  = true;   // true = stroke (FG) active, false = fill (BG)
static HWND  g_hwndSzMinus   = NULL;   // "-" button in colour strip
static HWND  g_hwndSzPlus    = NULL;   // "+" button in colour strip
static HWND  g_hwndEraserSq  = NULL;   // square eraser shape button
static HWND  g_hwndEraserRd  = NULL;   // round eraser shape button
static HWND  g_hwndSmudgeSq  = NULL;   // square smudge shape button
static HWND  g_hwndSmudgeRd  = NULL;   // round smudge shape button
static HWND  g_hwndWandContig = NULL;  // "Contiguous" checkbox (wand tool)

// Help-dialog Ctrl+F search bar state (used in AboutDlgProc and ApplyDarkMode)
static bool  s_searchVisible  = false;
static HWND  s_hwndSearchEdit = NULL;
static HWND  s_hwndFindNext   = NULL;
static HWND  s_hwndFindClose  = NULL;
static int   s_searchLastPos  = 0;
static int   g_eraserSize    = 40;     // eraser size in canvas pixels (separate from brush size)
static bool  g_eraserRound   = false;  // false=square, true=round
static POINT g_eraserCurPos  = {-1, -1}; // canvas-client coords of eraser cursor
static bool  g_eraserCurVis  = false;  // true when eraser cursor overlay should draw
static int   g_smudgeSize    = 20;     // smudge size in canvas pixels
static bool  g_smudgeRound   = true;   // false=square, true=round
static POINT g_smudgeCurPos  = {-1, -1};
static bool  g_smudgeCurVis  = false;  // true when smudge cursor overlay should draw

// ─── Saved window placement (loaded before first paint) ─────
static int   g_savedWinX     = INT_MIN;  // INT_MIN = not set
static int   g_savedWinY     = INT_MIN;
static int   g_savedWinW     = 0;
static int   g_savedWinH     = 0;

// ─── Floating paste state ───────────────────────────────────
static bool    g_isPasting       = false;
static Bitmap* g_pasteBitmap     = nullptr;
static POINT   g_pastePos        = {0, 0};  // canvas coords (top-left of display rect)
static int     g_pasteSizeW      = 0;       // display width in canvas pixels
static int     g_pasteSizeH      = 0;       // display height in canvas pixels
static float   g_pasteAngle      = 0.0f;   // rotation in degrees
static int     g_pasteDragMode   = 0;       // 0=none 1=move 4-11=handle 20=rotate
static POINT   g_pasteDragStart  = {};      // client coords at drag start
static int     g_pasteDragCorner = 0;       // corner hit at drag start (for rot/resize switch)
static POINT   g_pasteOrigPos    = {};      // g_pastePos snapshot at drag start
static int     g_pasteOrigSizeW  = 0;       // g_pasteSizeW snapshot at drag start
static int     g_pasteOrigSizeH  = 0;       // g_pasteSizeH snapshot at drag start
static float   g_pasteRotAng0    = 0.0f;   // mouse angle at rotate-drag start
static float   g_pasteShapeAng0  = 0.0f;   // g_pasteAngle at rotate-drag start
static bool    g_pasteDidResize  = false;   // undo already saved by auto-enlarge in DoPaste
static int     g_numOverflowed  = 0;       // how many extended-shape buttons are currently in More
static int     g_numToolsOverflowed = 0;   // how many basic-tool buttons are currently in Tools palette
static UINT    g_cfARGB          = 0;       // private clipboard format for lossless ARGB round-trip
static HWND    g_hwndPalette     = NULL;    // More palette popup (NULL when hidden)
static int     g_paletteHot      = -1;     // hovered cell index (-1 = none)
static UINT    g_paletteCmds[24] = {};     // command IDs for each palette cell
static int     g_paletteImgs[24] = {};     // image-list indices for each palette cell
static int     g_paletteCount    = 0;      // number of populated cells

// ─── Zoom ────────────────────────────────────────────────────
static float g_zoom            = 1.0f;
static bool  g_isSvgFile       = false;
static Bitmap* g_svgDisplayBitmap = nullptr;  // re-rasterized at g_zoom for crisp SVG display
static HWND  g_hwndZoomOut     = NULL;
static HWND  g_hwndZoomIn      = NULL;
static HWND  g_hwndZoomFit     = NULL;
static HWND  g_hwndZoomEdit    = NULL;
static HWND  g_hwndFolderBrowserBtn = NULL;
static HWND  g_hwndFolderBrowser   = NULL;
static HWND  g_hwndFocusBeforeDeactivate = NULL;   // child to refocus on reactivation

// stale: thumb was made for another strip height and is redone when visible
struct FolderEntry { std::wstring path; Bitmap* thumb = nullptr; bool failed = false; FILETIME modified = {}; bool stale = false; };
static std::vector<FolderEntry> g_folderEntries;
static std::vector<FolderEntry> g_folderHiddenEntries;   // left out by the search; kept for their thumbnails
static int   g_folderCurrentIdx   = -1;
static std::wstring g_folderFilter;     // search text: only file names containing it are listed
static int   g_folderTotalCount   = 0;  // images in the folder, before the search filter
static bool  g_folderSearchOpen   = false;
static HWND  g_hwndFolderSearch     = NULL;   // search panel at the left end of the strip
static HWND  g_hwndFolderSearchEdit = NULL;
static int   g_folderScrollX = 0;
static bool  g_updatingZoomEdit = false;

// ─── Alpha (foreground opacity) ──────────────────────────────
static int   g_fgAlpha            = 100;  // 0-100 percentage
static HWND  g_hwndAlphaEdit      = NULL;
static bool  g_updatingAlphaEdit  = false;

// ─── Background fill alpha (0 = transparent/no fill by default) ───
static int   g_bgAlpha            = 100;  // 0-100 percentage

// ─── Dark mode ────────────────────────────────────────────────
static bool  g_darkMode           = false;
static bool  g_startedWithFile = false;         // an image was passed on the command line
static bool  g_folderBrowserFromFile  = true;   // open the folder browser when started with an image
static bool  g_folderBrowserFromBlank = false;  // ... and when started without one

// Save settings per format, set in File > Save Options
static const int DEFAULT_SAVE_QUALITY = 100;
static int  g_jpegQuality    = DEFAULT_SAVE_QUALITY;   // 1..100
static int  g_webpQuality    = DEFAULT_SAVE_QUALITY;   // 1..100, ignored when lossless
static bool g_webpLossless   = false;
static bool g_webpKeepAlpha  = true;   // false: flatten transparency onto white
static bool g_pngPalette     = false;  // 256-colour PNG instead of full colour
static bool g_pngKeepAlpha   = true;   // false: flatten transparency onto white

// Recently opened or saved images, newest first; shown in File > Open Recent
static const int MAX_RECENT_FILES = 10;
static std::vector<std::wstring> g_recentFiles;
static void AddRecentFile(const wchar_t* path)
{
    if (!path || !path[0]) return;
    for (size_t i = 0; i < g_recentFiles.size(); i++)
        if (_wcsicmp(g_recentFiles[i].c_str(), path) == 0) { g_recentFiles.erase(g_recentFiles.begin() + i); break; }
    g_recentFiles.insert(g_recentFiles.begin(), path);
    if ((int)g_recentFiles.size() > MAX_RECENT_FILES) g_recentFiles.resize(MAX_RECENT_FILES);
}
static HBRUSH g_hDarkBgBrush      = NULL; // reused for WM_CTLCOLOREDIT
static HHOOK  g_hMenuCBTHook      = NULL; // WH_CBT hook — strips WS_EX_DLGMODALFRAME from popup menus

// ─── Modeless help window ─────────────────────────────────────
static HWND  g_hwndHelp           = NULL;

// ─── Checkerboard brush (canvas transparency indicator) ───────
static Bitmap*       g_pCheckBmp   = nullptr;
static TextureBrush* g_pCheckBrush = nullptr;

// ─── Startup centering ────────────────────────────────────────
static bool  g_pendingCenter   = false;

// ─── Selection float state ────────────────────────────────────
static bool    g_selFloating     = false;
static Bitmap* g_selFloatBitmap  = nullptr;
static RECT    g_selFloatOrigRect = {};
static float   g_selFloatAngle   = 0.0f;  // rotation in degrees; 0 = none
static float   g_selRotMouseAng0 = 0.0f;  // mouse angle at rotate-drag start
static float   g_selRotFloatAng0 = 0.0f;  // g_selFloatAngle at rotate-drag start
static int     g_selDragCorner   = 0;     // corner hit (2/4/6/8) at drag start; 0=non-corner
// g_selDragMode == 20 means rotate

// ─── Dropper tool state ──────────────────────────────────────
static POINT   g_dropperCur      = {};  // canvas coords of current drag end

// ─── Text tool state ─────────────────────────────────────────
struct TextDlgData { wchar_t text[256]; int fontSize; };
static TextDlgData g_textDlgData    = { {}, 24 };

// Inline text input state
static bool         g_textInline     = false;    // actively typing text on canvas
static std::wstring g_textStr;                   // accumulated text
static int          g_textCursorIdx  = 0;        // caret position within g_textStr
static bool         g_textCaretVis   = true;     // caret blink state
static UINT_PTR     g_textBlinkTimer = 0;

// Text font settings (persist across strokes)
static wchar_t      g_textFontName[LF_FACESIZE] = L"Arial";
static int          g_textFontSize               = 24;
static int          g_textFontStyle              = 0;  // bits: 1=bold 2=italic 4=underline

// Text font control HWNDs (in colour strip, visible when text tool active)
static HWND         g_hwndFontFace  = NULL;
static HWND         g_hwndFontSize  = NULL;
static HWND         g_hwndFontBold  = NULL;
static HWND         g_hwndFontItal  = NULL;
static HWND         g_hwndFontUnder = NULL;
static HFONT        g_hFontBold     = NULL;
static HFONT        g_hFontItal     = NULL;
static HFONT        g_hFontUndr     = NULL;

// ─── Shape post-draw editing state ──────────────────────────
static bool    g_shapeEditing       = false;
static Tool    g_shapeEditTool      = TOOL_PENCIL;
static POINT   g_shapeEditPt1       = {};   // corner 1 (canvas coords)
static POINT   g_shapeEditPt2       = {};   // corner 2 (canvas coords)
static int     g_shapeEditDrag      = 0;    // 0=none 1=move 4=NW..11=W 20=rotate
static POINT   g_shapeEditDragStart = {};   // client coords at drag start
static POINT   g_shapeEditOrigPt1   = {};   // pt1 at drag start
static POINT   g_shapeEditOrigPt2   = {};   // pt2 at drag start
static float   g_shapeEditAngle     = 0.0f; // accumulated rotation in degrees
static float   g_shapeEditRotAng0   = 0.0f; // mouse-to-center angle at rotate-drag start
static float   g_shapeEditShapeAng0 = 0.0f; // g_shapeEditAngle at rotate-drag start
static int     g_shapeEditCorner    = 0;    // corner hit (4/6/8/10) at drag start; 0=non-corner
static bool    g_shapeEditFlipH     = false; // shape content mirrored horizontally
static bool    g_shapeEditFlipV     = false; // shape content mirrored vertically

// ─── Bezier arrow editing state ─────────────────────────────
static bool    g_bezEditing  = false;
static PointF  g_bezPts[4]   = {};          // P0 P1 P2 P3 (canvas coords)
static int     g_bezDragMode = 0;           // 0=none 1=CP1 2=CP2 3=whole 4=NW..11=W 20=rotate
static PointF  g_bezDragOff  = {};          // offset P0→click for whole-drag
static PointF  g_bezDragOrigPts[4] = {};    // snapshot of g_bezPts at resize/rotate-drag start
static float   g_bezRotAng0  = 0.0f;        // mouse-to-center angle at bezier rotate-drag start
static int     g_bezCorner   = 0;           // corner hit (4/6/8/10) at drag start; 0=non-corner
static float   g_bezAngle    = 0.0f;        // accumulated rotation angle (degrees) since last reset
static float   g_bezAngle0   = 0.0f;        // g_bezAngle at start of current rotation drag
static REAL    g_bezRefL = 0, g_bezRefT = 0, g_bezRefR = 0, g_bezRefB = 0; // bbox in local frame (kept current)
static REAL    g_bezDragRefL = 0, g_bezDragRefT = 0, g_bezDragRefR = 0, g_bezDragRefB = 0; // ref bbox snapshot at resize drag start
static REAL    g_bezDragPivX = 0, g_bezDragPivY = 0; // pivot (ref bbox centre) at CP-drag start, kept fixed to avoid drift

// ─── Scroll state ────────────────────────────────────────────
static int  g_scrollX = 0;
static int  g_scrollY = 0;

// ─── Canvas edge-drag resize ─────────────────────────────────
static int   g_canvasResizeDir   = 0;   // 0=none; bitmask: 1=E 2=S 4=W 8=N (e.g. SE=3, NW=12, SW=6, NE=9)
static POINT g_canvasResizeStart = {};
static POINT g_canvasResizeCur   = {};   // current mouse pos during resize drag
static int   g_canvasResizeOrigW = 0;
static int   g_canvasResizeOrigH = 0;

// ─── Selection drag state ────────────────────────────────────
static int   g_selDragMode     = 0;  // 0=none 1=move 2=NW 3=N 4=NE 5=E 6=SE 7=S 8=SW 9=W
static POINT g_selDragStart    = {};
static RECT  g_selDragOrigRect = {};

// ============================================================
//  Forward declarations
// ============================================================
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CanvasWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK TextDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ResizeDlgProc(HWND, UINT, WPARAM, LPARAM);

// Canvas helpers
static void CreateCanvas(int w, int h, bool white = true);
static void DestroyCanvas();
static void PushUndo();
static void PopUndo();
static void InvalidateCanvas();
static void InvalidateCanvasView();
static void UpdateStatusBar(int x, int y);
static void WandFloodFill(int seedX, int seedY);
static void RefreshEditingPreview();
static void UpdateTitleBar();

// File ops
static void DoNew();
static void DoOpen();
static void DoSave();
static void LoadImageFromPath(const wchar_t* file);
static void InstallCanvasImage(Bitmap* bmp, const wchar_t* file, bool isSVG, bool detached, float svgScale, bool callerFits = false);
static void DoSaveAs();
static bool GetSaveEncoder(const wchar_t* filename, CLSID& clsid);
static bool GetEncoderClsid(const wchar_t* format, CLSID* pClsid);

// Drawing primitives
static void DrawFreehand(int x, int y, bool isEraser);
static void FloodFill(int x, int y);
static void DoTextTool(int cx, int cy);

// Preview
static void ClearPreview();
static void RenderPreview(int mx, int my);
static void CommitPreview();

// Edit ops
static void DoSelectAll();
static void DoCut();
static void DoCopy();
static void DoPaste();
static void DoRedo();
static void CommitPaste();
static void CancelPaste();
static void CommitSelectionFloat();
static void CancelSelectionFloat();

// Image ops
static void DoResizeCanvas();
static void DoRotateCanvas(bool clockwise);
static void DoFlipCanvas(bool horizontal);
static void SetEditMenuIcons();
static void SetImageMenuIcons();
static void SetViewMenuIcons();

// Colour helpers
static Color    GdipColorFromCOLORREF(COLORREF cr, BYTE alpha = 255);
static void     ChooseActiveColor();

// Persistent settings (registry) + colour defaults
static void ResetCustomColors();
static void LoadSettings();
static void SaveSettings();

// Bezier arrow
static void DrawBezierArrow(Graphics& gfx);
static void CommitBezier();
static void CancelBezier();

// Zoom
static void ZoomIn(int pivotX = -1, int pivotY = -1);
static void ZoomOut(int pivotX = -1, int pivotY = -1);
static void ApplyZoom(float newZoom, int pivotX = -1, int pivotY = -1);
static void ZoomToFit();
static void ZoomToCover();
static void UpdateZoomEdit();
static void RebuildSvgDisplayBitmap();

// Folder browser
static Bitmap* LoadSvgToBitmap(const wchar_t* path, float scale = 1.0f, float* pScaleUsed = nullptr);
static float   g_svgCanvasScale = 1.0f;   // canvas pixels per SVG unit (< 1 for oversized SVGs)
static void    BuildFolderList();
static void    FreeFolderThumbs();
static void    NavigateToFile(int idx);
static void    ToggleFolderBrowser();
static void    OpenFolderSearch();
static void    EnterFullscreen(bool play);
static void    ExitFullscreen();
static void    CloseFolderSearch();
static LRESULT CALLBACK FolderSearchWndProc(HWND, UINT, WPARAM, LPARAM);
static void    CenterFolderBrowserOnCurrent();
static void    LayoutChildren(HWND hwndMain);
static void    LayoutCanvasAndFolderBrowser(HWND hwndMain);
static void UpdateAlphaEdit();
static void ClampAlphaEdit();
static void ApplyDarkMode();
static void SetFileMenuIcons();
static void DoPrint();
static HBITMAP MakeBitmapFromSvg(const char* d, int sz, Color fillColor,
                                  float coordSz, Color bgColor);
static HICON MakeHIconFromSvg(const char* d, int sz, Color fillColor);
static HIMAGELIST BuildToolImageList(bool dark);
static void StopMarchingAnts();

// Toolbar / layout
static void SetTool(Tool t);
static LRESULT CALLBACK ToolbarSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK OverlayBtnSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK PaletteWndProc(HWND, UINT, WPARAM, LPARAM);
static void ClosePalette();
static void ShowPalette();
static void ShowToolsPalette();
static LRESULT CALLBACK StatusBarSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK DarkEditSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK WandContigSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK DarkMenuSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static LRESULT CALLBACK MenuCBTProc(int, WPARAM, LPARAM);
static void RebuildStatusIcons();
static void UpdateBrushSizeEdit();
static void UpdateLWSectionForTool();
static void UpdateScrollBars();
static void CenterScrollOnCanvas();
static int     HitTestCanvasResize(int cx, int cy);
static HCURSOR ResizeCursorForHandle(int hit, float angleDeg, bool isShapeEdit);
static int  HitTestSelection(int cx, int cy);
static int  HitTestBezierResize(REAL canX, REAL canY); // 0=none 4=NW..11=W
static void AddSvgPath(GraphicsPath& gp, const char* d, float scale);
static void RecreateFillCursor();
static void RecreateBrushCursors();
static int  HitTestPaste(int cx, int cy);              // 0=none 1=move 4=NW..11=W
static int  HitTestShapeEdit(int cx, int cy);          // 0=none 1=move 4=NW..11=W
static void CommitShape();
static void CancelShape();
static void CommitTextInline();
static void CancelTextInline();
static bool IsTextBoxInterior(float canX, float canY);
static void PlaceCaretAtCanvasPoint(float canX, float canY);
static POINT ClientToCanvas(HWND hwnd, int cx, int cy);
static void PaintColorStrip(HDC hdc, int stripX, int stripY, int stripW);
static int  HitTestColorStrip(int x, int y, int stripY);  // 0=none 1=FG 2=BG 3+=custom(i-3)

// ============================================================
//  Utility
// ============================================================
static Color GdipColorFromCOLORREF(COLORREF cr, BYTE alpha)
{
    return Color(alpha, GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

// Convert the 0-100 g_fgAlpha / g_bgAlpha percentages to 0-255 bytes for GDI+
static inline BYTE FgAlphaByte() { return (BYTE)(g_fgAlpha * 255 / 100); }
static inline BYTE BgAlphaByte() { return (BYTE)(g_bgAlpha * 255 / 100); }

// ============================================================
//  Canvas helpers
// ============================================================
static void CreateCanvas(int w, int h, bool white)
{
    DestroyCanvas();
    g_pCanvas  = new Bitmap(w, h, PixelFormat32bppARGB);
    g_pPreview = new Bitmap(w, h, PixelFormat32bppARGB);
    g_pGfxCanvas = Graphics::FromImage(g_pCanvas);
    g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    g_pGfxCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
    if (white) {
        // Legacy path — kept for file-open operations that need an opaque base
        SolidBrush wb(Color(255, 255, 255, 255));
        g_pGfxCanvas->FillRectangle(&wb, 0, 0, w, h);
    }
    // else: canvas starts fully transparent (pixels = 0x00000000)
    Graphics gfxPrev(g_pPreview);
    gfxPrev.Clear(Color(0, 0, 0, 0));
    g_previewUsed = false;
}

static void DestroyCanvas()
{
    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;    g_pCanvas    = nullptr;
    delete g_pPreview;   g_pPreview   = nullptr;
}

// An open SVG is displayed from a fresh render of the file. Once the canvas is
// edited that render is out of date, so show the canvas itself from then on.
static void LeaveSvgDisplay()
{
    if (!g_isSvgFile) return;
    g_isSvgFile = false;
    delete g_svgDisplayBitmap; g_svgDisplayBitmap = nullptr;
}

static void PushUndo()
{
    if (!g_pCanvas) return;
    LeaveSvgDisplay();   // every edit starts here
    // Clear redo — new drawing action invalidates the redo history
    for (auto* b : g_redoStack) delete b;
    g_redoStack.clear();
    Bitmap* snap = g_pCanvas->Clone(0, 0,
        (INT)g_pCanvas->GetWidth(), (INT)g_pCanvas->GetHeight(),
        PixelFormat32bppARGB);
    g_undoStack.push_back(snap);
    while ((int)g_undoStack.size() > MAX_UNDO) {
        delete g_undoStack.front();
        g_undoStack.erase(g_undoStack.begin());
    }
}

static void PopUndo()
{
    if (g_undoStack.empty()) { MessageBeep(MB_ICONEXCLAMATION); return; }
    // Discard any in-flight state that references the current canvas pixels
    if (g_shapeEditing) { ClearPreview(); g_shapeEditing = false; }
    if (g_isPasting)   CancelPaste();
    if (g_selFloating) { delete g_selFloatBitmap; g_selFloatBitmap = nullptr; g_selFloating = false; }
    g_selDragMode = 0;
    // Save current state to redo before reverting
    if (g_pCanvas) {
        Bitmap* cur = g_pCanvas->Clone(0, 0,
            (INT)g_pCanvas->GetWidth(), (INT)g_pCanvas->GetHeight(),
            PixelFormat32bppARGB);
        if (cur) {
            g_redoStack.push_back(cur);
            while ((int)g_redoStack.size() > MAX_UNDO) {
                delete g_redoStack.front();
                g_redoStack.erase(g_redoStack.begin());
            }
        }
    }
    Bitmap* prev = g_undoStack.back();
    g_undoStack.pop_back();

    int w = (int)prev->GetWidth();
    int h = (int)prev->GetHeight();
    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;
    g_pCanvas = prev;
    if (!g_pPreview ||
        (int)g_pPreview->GetWidth()  != w ||
        (int)g_pPreview->GetHeight() != h)
    {
        delete g_pPreview;
        g_pPreview = new Bitmap(w, h, PixelFormat32bppARGB);
    }
    Graphics gp(g_pPreview); gp.Clear(Color(0,0,0,0));
    g_pGfxCanvas = Graphics::FromImage(g_pCanvas);
    g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    g_pGfxCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
    StopMarchingAnts(); g_hasSelMask = false;
    g_hasSelection = false;
    InvalidateCanvas();
    g_modified = true;
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    UpdateScrollBars();
}

static void DoRedo()
{
    if (g_redoStack.empty()) { MessageBeep(MB_ICONEXCLAMATION); return; }
    // Push current state to undo (without touching the redo stack)
    if (g_pCanvas) {
        Bitmap* snap = g_pCanvas->Clone(0, 0,
            (INT)g_pCanvas->GetWidth(), (INT)g_pCanvas->GetHeight(),
            PixelFormat32bppARGB);
        if (snap) {
            g_undoStack.push_back(snap);
            while ((int)g_undoStack.size() > MAX_UNDO) {
                delete g_undoStack.front();
                g_undoStack.erase(g_undoStack.begin());
            }
        }
    }
    Bitmap* next = g_redoStack.back();
    g_redoStack.pop_back();
    int w = (int)next->GetWidth(), h = (int)next->GetHeight();
    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;
    g_pCanvas = next;
    if (!g_pPreview || (int)g_pPreview->GetWidth() != w || (int)g_pPreview->GetHeight() != h) {
        delete g_pPreview;
        g_pPreview = new Bitmap(w, h, PixelFormat32bppARGB);
    }
    { Graphics gp(g_pPreview); gp.Clear(Color(0,0,0,0)); }
    g_pGfxCanvas = Graphics::FromImage(g_pCanvas);
    g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    g_pGfxCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
    StopMarchingAnts(); g_hasSelMask = false;
    g_hasSelection = false;
    g_modified = true;
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    InvalidateCanvas();
    UpdateScrollBars();
}

// Every canvas change repaints through InvalidateCanvas, which bumps this so
// the zoomed-out view cache is rebuilt. Repaints that only move or overlay the
// view (scrolling, zooming, animations) use InvalidateCanvasView and keep it.
static unsigned g_canvasVersion = 0;
static HWND     g_hwndFullscreen = NULL;   // full screen view (View menu), shows the canvas

static void InvalidateCanvas()
{
    g_canvasVersion++;
    if (g_hwndCanvas) InvalidateRect(g_hwndCanvas, NULL, FALSE);
    if (g_hwndFullscreen) InvalidateRect(g_hwndFullscreen, NULL, FALSE);
}

static void InvalidateCanvasView()
{
    if (g_hwndCanvas) InvalidateRect(g_hwndCanvas, NULL, FALSE);
}

// File being loaded in the background; the title shows it straight away
static std::wstring g_titleLoadingPath;

static void UpdateTitleBar()
{
    wchar_t title[MAX_PATH + 64];
    bool loading = !g_titleLoadingPath.empty();
    const wchar_t* name = loading ? g_titleLoadingPath.c_str()
                        : (g_currentFile[0] != L'\0') ? g_currentFile : L"Untitled";
    const wchar_t* slash = wcsrchr(name, L'\\');
    if (!slash) slash = wcsrchr(name, L'/');
    if (slash) name = slash + 1;
    StringCchPrintf(title, ARRAYSIZE(title), L"%s%s - MyPaint",
        g_modified && !loading ? L"*" : L"", name);
    SetWindowText(g_hwndMain, title);
}

static POINT ClientToCanvas(HWND /*hwnd*/, int cx, int cy)
{
    POINT pt;
    pt.x = (LONG)((cx - g_canvasOffX) / g_zoom);
    pt.y = (LONG)((cy - g_canvasOffY) / g_zoom);
    if (g_pCanvas) {
        pt.x = max(0L, min(pt.x, (LONG)g_pCanvas->GetWidth()  - 1));
        pt.y = max(0L, min(pt.y, (LONG)g_pCanvas->GetHeight() - 1));
    }
    return pt;
}

// ============================================================
//  Status bar
// ============================================================
static void SetStatusText(int pane, const wchar_t* text)
{
    if (pane < 0 || pane >= 6 || !g_hwndStatus) return;
    const wchar_t* t = text ? text : L"";
    // Skip the SB_SETTEXT (and the repaint it triggers) when nothing has changed.
    // This prevents spurious erase+redraw cycles on every WM_MOUSEMOVE.
    if (wcscmp(g_statusPaneText[pane], t) == 0) return;
    StringCchCopyW(g_statusPaneText[pane], 128, t);
    SendMessage(g_hwndStatus, SB_SETTEXT, (WPARAM)(pane | SBT_OWNERDRAW), (LPARAM)(LPWSTR)g_statusPaneText[pane]);
}

static void UpdateStatusBar(int canvasX, int canvasY)
{
    if (!g_hwndStatus || !g_pCanvas) return;
    wchar_t buf[64];
    StringCchPrintf(buf, 64, L" %d x %d",
        (int)g_pCanvas->GetWidth(), (int)g_pCanvas->GetHeight());
    SetStatusText(STATUS_PANE_SIZE, buf);
    StringCchPrintf(buf, 64, L" X: %d  Y: %d", canvasX, canvasY);
    SetStatusText(STATUS_PANE_POS, buf);
    if (g_hasSelection) {
        int sw = g_selRect.right  - g_selRect.left;
        int sh = g_selRect.bottom - g_selRect.top;
        StringCchPrintf(buf, 64, L" %d,%d  %d\u00D7%d",
            g_selRect.left, g_selRect.top, sw, sh);
        SetStatusText(STATUS_PANE_SEL, buf);
    } else {
        SetStatusText(STATUS_PANE_SEL, L"");
    }
    if (g_currentTool == TOOL_FILL) {
        BYTE fr = GetRValue(g_fgColor), fg = GetGValue(g_fgColor), fb = GetBValue(g_fgColor);
        StringCchPrintf(buf, 64, L" RGB(%d,%d,%d)", (int)fr, (int)fg, (int)fb);
        SetStatusText(STATUS_PANE_COLOR, buf);
        StringCchPrintf(buf, 64, L" #%02X%02X%02X", (int)fr, (int)fg, (int)fb);
        SetStatusText(STATUS_PANE_HEX, buf);
    } else if (canvasX >= 0 && canvasY >= 0 &&
        canvasX < (int)g_pCanvas->GetWidth() &&
        canvasY < (int)g_pCanvas->GetHeight())
    {
        Color c;
        g_pCanvas->GetPixel(canvasX, canvasY, &c);
        StringCchPrintf(buf, 64, L" RGB(%d,%d,%d)", (int)c.GetR(), (int)c.GetG(), (int)c.GetB());
        SetStatusText(STATUS_PANE_COLOR, buf);
        StringCchPrintf(buf, 64, L" #%02X%02X%02X", (int)c.GetR(), (int)c.GetG(), (int)c.GetB());
        SetStatusText(STATUS_PANE_HEX, buf);
    }
}

// ============================================================
//  Encoder helpers
// ============================================================
static bool GetEncoderClsid(const wchar_t* format, CLSID* pClsid)
{
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    ImageCodecInfo* pInfo = reinterpret_cast<ImageCodecInfo*>(buf.data());
    GetImageEncoders(num, size, pInfo);
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(pInfo[i].MimeType, format) == 0) {
            *pClsid = pInfo[i].Clsid;
            return true;
        }
    }
    return false;
}

static bool GetSaveEncoder(const wchar_t* filename, CLSID& clsid)
{
    const wchar_t* dot = wcsrchr(filename, L'.');
    if (!dot) dot = L".png";
    wchar_t ext[16] = {};
    StringCchCopy(ext, 16, dot + 1);
    for (int i = 0; ext[i]; ++i) ext[i] = (wchar_t)towlower(ext[i]);
    const wchar_t* mime = L"image/png";
    if (wcscmp(ext, L"bmp") == 0)                       mime = L"image/bmp";
    else if (wcscmp(ext, L"jpg") == 0 ||
             wcscmp(ext, L"jpeg") == 0)                 mime = L"image/jpeg";
    else if (wcscmp(ext, L"gif") == 0)                  mime = L"image/gif";
    else if (wcscmp(ext, L"tif") == 0 ||
             wcscmp(ext, L"tiff") == 0)                 mime = L"image/tiff";
    return GetEncoderClsid(mime, &clsid);
}

// ============================================================
//  File operations
// ============================================================
static bool PromptSaveIfNeeded()
{
    if (!g_modified) return true;
    const wchar_t* name = (g_currentFile[0] != L'\0') ? g_currentFile : L"Untitled";
    const wchar_t* slash = wcsrchr(name, L'\\');
    if (!slash) slash = wcsrchr(name, L'/');
    if (slash) name = slash + 1;
    wchar_t msg[MAX_PATH + 64];
    StringCchPrintf(msg, ARRAYSIZE(msg), L"Save changes to \"%s\"?", name);
    int r = MessageBox(g_hwndMain, msg, APP_TITLE, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return false;
    if (r == IDYES) DoSave();
    return true;
}

static void DoNew()
{
    if (!PromptSaveIfNeeded()) return;
    for (auto* b : g_undoStack) delete b; g_undoStack.clear();
    for (auto* b : g_redoStack) delete b; g_redoStack.clear();
    CancelPaste();
    g_hasSelection = false;
    CreateCanvas(DEFAULT_CANVAS_W, DEFAULT_CANVAS_H, false);
    CenterScrollOnCanvas();
    g_currentFile[0] = L'\0';
    g_modified = false;
    InvalidateCanvas();
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
}

// ─── Decoded pixel buffers ───────────────────────────────────────────────────
// The Decode* functions below use no GDI+, so they can run on a background
// thread: GDI+ serialises its calls, and decoding through it would block the
// UI thread's painting for the whole decode.

struct DecodedImage {
    int w = 0, h = 0;
    bool premultiplied = false;
    float scale = 1.0f;       // SVG only: raster scale actually used
    std::vector<BYTE> px;     // BGRA, stride w * 4
};

// Largest SVG rasterisation (~128 MB per 32-bit copy). A bigger request is
// rendered at a reduced scale instead.
static const double MAX_SVG_RASTER_PIXELS = 32.0 * 1024 * 1024;

static Bitmap* MakeBitmapFromPixels(const DecodedImage& img)
{
    PixelFormat fmt = img.premultiplied ? PixelFormat32bppPARGB : PixelFormat32bppARGB;
    Bitmap* bmp = new Bitmap(img.w, img.h, fmt);
    if (bmp->GetLastStatus() != Ok) { delete bmp; return nullptr; }
    Rect rc(0, 0, img.w, img.h); BitmapData bd;
    if (bmp->LockBits(&rc, ImageLockModeWrite, fmt, &bd) != Ok) { delete bmp; return nullptr; }
    for (int y = 0; y < img.h; y++)
        memcpy((BYTE*)bd.Scan0 + y * bd.Stride, img.px.data() + (size_t)y * img.w * 4, (size_t)img.w * 4);
    bmp->UnlockBits(&bd);
    return bmp;
}

// ─── EXIF orientation ────────────────────────────────────────
// Phones and cameras often store a photo as the sensor saw it and add an EXIF
// Orientation tag (1-8) saying how to turn it for display. Decoders hand back
// the stored pixels, so MyPaint applies the tag itself. HEIC/AVIF are left
// alone: they carry their own rotation, which Windows' decoder handles.
static bool UsesExifOrientation(const wchar_t* path)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    if (!dot) return false;
    for (auto e : { L"jpg", L"jpeg", L"jpe", L"jfif", L"tif", L"tiff" }) if (_wcsicmp(dot + 1, e) == 0) return true;
    return false;
}

// The Orientation tag of a GDI+ image, 1 (upright) when there is none
static int GetExifOrientation(Image* img)
{
    UINT size = img->GetPropertyItemSize(PropertyTagOrientation);
    if (size < sizeof(PropertyItem) + sizeof(USHORT)) return 1;
    std::vector<BYTE> buf(size);
    auto* item = (PropertyItem*)buf.data();
    if (img->GetPropertyItem(PropertyTagOrientation, size, item) != Ok || item->type != PropertyTagTypeShort) return 1;
    int o = *(USHORT*)item->value;
    return (o >= 1 && o <= 8) ? o : 1;
}

// The turn that displays an image tagged o upright
static RotateFlipType ExifOrientationToRotateFlip(int o)
{
    switch (o) {
    case 2: return RotateNoneFlipX;
    case 3: return Rotate180FlipNone;
    case 4: return Rotate180FlipX;      // = flip vertically
    case 5: return Rotate90FlipX;       // transpose
    case 6: return Rotate90FlipNone;
    case 7: return Rotate270FlipX;      // transverse
    case 8: return Rotate270FlipNone;
    }
    return RotateNoneFlipNone;
}

// Turns a GDI+ bitmap loaded from path upright, if its tag says so
static void ApplyExifOrientation(Bitmap* bmp, const wchar_t* path)
{
    if (!bmp || !UsesExifOrientation(path)) return;
    int o = GetExifOrientation(bmp);
    if (o != 1) bmp->RotateFlip(ExifOrientationToRotateFlip(o));
}

// Decodes the formats Windows Imaging Component handles (PNG, JPEG, BMP,
// GIF, TIFF, ...); first frame only, like Bitmap::FromFile. COM must be
// initialised on the calling thread.
// pHr receives the failure, e.g. WINCODEC_ERR_COMPONENTNOTFOUND when Windows
// has no decoder for the format (HEIC/AVIF without their Store extensions).
static bool DecodeWicPixels(const wchar_t* file, DecodedImage& out, HRESULT* pHr = nullptr)
{
    IWICImagingFactory*    fac   = nullptr;
    IWICBitmapDecoder*     dec   = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapFlipRotator* turn  = nullptr;
    IWICFormatConverter*   conv  = nullptr;
    UINT w = 0, h = 0;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&fac));
    if (SUCCEEDED(hr)) hr = fac->CreateDecoderFromFilename(file, nullptr, GENERIC_READ,
                                                           WICDecodeMetadataCacheOnDemand, &dec);
    if (SUCCEEDED(hr)) hr = dec->GetFrame(0, &frame);
    // EXIF Orientation: turn the photo upright on the way through
    IWICBitmapSource* src = frame;
    if (SUCCEEDED(hr) && UsesExifOrientation(file)) {
        int o = 1;
        IWICMetadataQueryReader* qr = nullptr;
        if (SUCCEEDED(frame->GetMetadataQueryReader(&qr))) {
            const wchar_t* dot = wcsrchr(file, L'.');
            bool tiff = dot && (_wcsicmp(dot + 1, L"tif") == 0 || _wcsicmp(dot + 1, L"tiff") == 0);
            PROPVARIANT v; PropVariantInit(&v);
            if (SUCCEEDED(qr->GetMetadataByName(tiff ? L"/ifd/{ushort=274}" : L"/app1/ifd/{ushort=274}", &v)) &&
                v.vt == VT_UI2 && v.uiVal >= 1 && v.uiVal <= 8)
                o = v.uiVal;
            PropVariantClear(&v);
            qr->Release();
        }
        // WIC flips before it turns (GDI+ turns first), so 5 and 7 pair the
        // opposite turn with the flip compared with ExifOrientationToRotateFlip
        static const WICBitmapTransformOptions kTurn[9] = {
            WICBitmapTransformRotate0, WICBitmapTransformRotate0, WICBitmapTransformFlipHorizontal,
            WICBitmapTransformRotate180, WICBitmapTransformFlipVertical,
            (WICBitmapTransformOptions)(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal),
            WICBitmapTransformRotate90,
            (WICBitmapTransformOptions)(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal),
            WICBitmapTransformRotate270 };
        if (o != 1 && SUCCEEDED(fac->CreateBitmapFlipRotator(&turn)) &&
            SUCCEEDED(turn->Initialize(frame, kTurn[o])))
            src = turn;
    }
    if (SUCCEEDED(hr)) hr = fac->CreateFormatConverter(&conv);
    if (SUCCEEDED(hr)) hr = conv->Initialize(src, GUID_WICPixelFormat32bppBGRA,
                                             WICBitmapDitherTypeNone, nullptr, 0.0,
                                             WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) hr = conv->GetSize(&w, &h);
    if (SUCCEEDED(hr) && (w == 0 || h == 0 || w > 32767 || h > 32767)) hr = E_FAIL;
    if (SUCCEEDED(hr)) {
        out.w = (int)w; out.h = (int)h; out.premultiplied = false;
        out.px.resize((size_t)w * h * 4);
        hr = conv->CopyPixels(nullptr, w * 4, (UINT)out.px.size(), out.px.data());
    }
    if (conv)  conv->Release();
    if (turn)  turn->Release();
    if (frame) frame->Release();
    if (dec)   dec->Release();
    if (fac)   fac->Release();
    if (pHr) *pHr = hr;
    return SUCCEEDED(hr);
}

static Bitmap* LoadBitmapViaWic(const wchar_t* path)
{
    DecodedImage img;
    return DecodeWicPixels(path, img) ? MakeBitmapFromPixels(img) : nullptr;
}

// HEIC/HEIF and AVIF open only through codecs Windows installs from the Store
static bool IsStoreCodecPath(const wchar_t* path)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    if (!dot) return false;
    for (auto e : { L"heic", L"heif", L"hif", L"avif" }) if (_wcsicmp(dot + 1, e) == 0) return true;
    return false;
}

// Explains a HEIC/AVIF that wouldn't open and offers the free Store extension
static void ShowStoreCodecHelp(const wchar_t* path, HRESULT hr)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    bool avif = dot && _wcsicmp(dot + 1, L"avif") == 0;
    std::wstring msg = PathFindFileNameW(path);
    const wchar_t* storeUrl;
    if (avif) {
        msg += L"\n\nOpening AVIF images needs Microsoft's free \"AV1 Video Extension\" "
               L"from the Microsoft Store.";
        storeUrl = L"ms-windows-store://pdp/?ProductId=9MVZQVXJBQ9V";
    } else if (hr == WINCODEC_ERR_COMPONENTNOTFOUND || hr == WINCODEC_ERR_COMPONENTINITIALIZEFAILURE) {
        // Windows 10's own HEIF reader fails to start without the extension
        msg += L"\n\nOpening HEIC images (the photo format of iPhones and many other phones) "
               L"needs Microsoft's free \"HEIF Image Extensions\" from the Microsoft Store. "
               L"Most phone photos also need \"HEVC Video Extensions\".";
        storeUrl = L"ms-windows-store://pdp/?ProductId=9PMMSR1CGPWG";
    } else {
        msg += L"\n\nWindows found a HEIF decoder but couldn't read this image. Most phone "
               L"photos also need Microsoft's \"HEVC Video Extensions\" from the Microsoft Store.";
        storeUrl = L"ms-windows-store://pdp/?ProductId=9NMZLZ57R3T7";
    }
    msg += L"\n\nOpen the Microsoft Store page now?";
    if (MessageBox(g_hwndMain, msg.c_str(), APP_TITLE, MB_YESNO | MB_ICONINFORMATION) == IDYES &&
        (INT_PTR)ShellExecuteW(g_hwndMain, NULL, storeUrl, NULL, NULL, SW_SHOWNORMAL) <= 32)
        MessageBox(g_hwndMain, L"The Microsoft Store couldn't be opened on this PC.", APP_TITLE, MB_ICONWARNING);
}

// ─── WebP helpers ────────────────────────────────────────────────────────────

static bool DecodeWebPPixels(const wchar_t* file, DecodedImage& out,
                             HRESULT* pHrOut = nullptr, const wchar_t** pStepOut = nullptr)
{
    auto fail = [&](HRESULT hr, const wchar_t* step) -> bool {
        if (pHrOut)   *pHrOut   = hr;
        if (pStepOut) *pStepOut = step;
        return false;
    };
    if (pHrOut)   *pHrOut   = S_OK;
    if (pStepOut) *pStepOut = nullptr;

    // Read file into memory
    FILE* f = nullptr;
    if (_wfopen_s(&f, file, L"rb") != 0 || !f)
        return fail(HRESULT_FROM_WIN32(GetLastError()), L"fopen");
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data((size_t)fileSize);
    fread(data.data(), 1, (size_t)fileSize, f);
    fclose(f);

    // Validate and decode — BGRA output matches GDI+ PixelFormat32bppARGB byte order
    WebPBitstreamFeatures feat;
    if (WebPGetFeatures(data.data(), data.size(), &feat) != VP8_STATUS_OK)
        return fail(E_FAIL, L"WebPGetFeatures (not a valid WebP file)");

    if (feat.has_animation) {
        // Animated: open the first frame, composited onto the full canvas
        WebPAnimDecoderOptions opts;
        if (!WebPAnimDecoderOptionsInit(&opts)) return fail(E_FAIL, L"WebPAnimDecoderOptionsInit");
        opts.color_mode = MODE_BGRA;
        WebPData wd = { data.data(), data.size() };
        WebPAnimDecoder* dec = WebPAnimDecoderNew(&wd, &opts);
        if (!dec) return fail(E_FAIL, L"WebPAnimDecoderNew (damaged animated WebP)");
        WebPAnimInfo info;
        uint8_t* frame = nullptr;
        int ts = 0;
        bool ok = WebPAnimDecoderGetInfo(dec, &info) &&
                  WebPAnimDecoderGetNext(dec, &frame, &ts) && frame;
        if (ok) {
            out.w = (int)info.canvas_width; out.h = (int)info.canvas_height; out.premultiplied = false;
            out.px.assign(frame, frame + (size_t)out.w * out.h * 4);   // owned by the decoder
        }
        WebPAnimDecoderDelete(dec);
        return ok ? true : fail(E_FAIL, L"WebPAnimDecoderGetNext (damaged animated WebP)");
    }

    int w = 0, h = 0;
    uint8_t* pixels = WebPDecodeBGRA(data.data(), data.size(), &w, &h);
    if (!pixels)
        return fail(E_FAIL, L"WebPDecodeBGRA (damaged or unsupported WebP)");

    out.w = w; out.h = h; out.premultiplied = false;
    out.px.assign(pixels, pixels + (size_t)w * h * 4);
    WebPFree(pixels);
    return true;
}

static Bitmap* LoadWebP(const wchar_t* file, HRESULT* pHrOut = nullptr, const wchar_t** pStepOut = nullptr)
{
    DecodedImage img;
    if (!DecodeWebPPixels(file, img, pHrOut, pStepOut)) return nullptr;
    return MakeBitmapFromPixels(img);
}

static Bitmap* FlattenOntoWhite(Bitmap* bmp);

// Encodes a bitmap as WebP into out. quality 1..100 (ignored when lossless);
// keepAlpha false flattens transparency onto white first.
static bool EncodeWebP(Bitmap* bmp, int quality, bool lossless, std::vector<uint8_t>& out, bool keepAlpha = true)
{
    std::unique_ptr<Bitmap> flat;
    if (!keepAlpha) {
        flat.reset(FlattenOntoWhite(bmp));
        if (!flat) return false;
        bmp = flat.get();
    }
    int w = (int)bmp->GetWidth(), h = (int)bmp->GetHeight();

    Rect rc(0, 0, w, h);
    BitmapData bd;
    if (bmp->LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok)
        return false;

    // Encode — GDI+ PixelFormat32bppARGB byte order is BGRA, matching WebPEncodeBGRA
    uint8_t* output = nullptr;
    const int stride = std::abs(bd.Stride);
    size_t outputSize = lossless
        ? WebPEncodeLosslessBGRA((const uint8_t*)bd.Scan0, w, h, stride, &output)
        : WebPEncodeBGRA((const uint8_t*)bd.Scan0, w, h, stride, (float)quality, &output);
    bmp->UnlockBits(&bd);

    if (!outputSize || !output) return false;
    out.assign(output, output + outputSize);
    WebPFree(output);
    return true;
}

// Saves as WebP with the current save settings
static bool SaveWebPBitmap(Bitmap* bmp, const wchar_t* file)
{
    std::vector<uint8_t> data;
    if (!EncodeWebP(bmp, g_webpQuality, g_webpLossless, data, g_webpKeepAlpha)) return false;
    FILE* f = nullptr;
    bool ok = _wfopen_s(&f, file, L"wb") == 0 && f;
    if (ok) {
        ok = (fwrite(data.data(), 1, data.size(), f) == data.size());
        fclose(f);
    }
    return ok;
}

// Flattens transparency onto white, as a 24-bit bitmap: needed for JPEG (which
// has none; GDI+ would turn transparent pixels black) and for PNGs saved
// without transparency. Caller deletes the result.
static Bitmap* FlattenOntoWhite(Bitmap* bmp)
{
    int w = (int)bmp->GetWidth(), h = (int)bmp->GetHeight();
    Bitmap* flat = new Bitmap(w, h, PixelFormat24bppRGB);
    if (flat->GetLastStatus() != Ok) { delete flat; return nullptr; }
    Graphics g(flat);
    g.Clear(Color(255, 255, 255, 255));
    g.DrawImage(bmp, 0, 0, w, h);
    return flat;
}

// Saves a bitmap as JPEG at quality 1..100, to a file or (if file is null) a stream
static bool SaveJpeg(Bitmap* bmp, int quality, const wchar_t* file, IStream* stream = nullptr)
{
    CLSID enc;
    if (!GetEncoderClsid(L"image/jpeg", &enc)) return false;
    Bitmap* flat = FlattenOntoWhite(bmp);
    if (!flat) return false;
    ULONG q = (ULONG)max(1, min(100, quality));
    EncoderParameters ep;
    ep.Count = 1;
    ep.Parameter[0].Guid           = EncoderQuality;
    ep.Parameter[0].NumberOfValues = 1;
    ep.Parameter[0].Type           = EncoderParameterValueTypeLong;
    ep.Parameter[0].Value          = &q;
    bool ok = (file ? flat->Save(file, &enc, &ep) : flat->Save(stream, &enc, &ep)) == Ok;
    delete flat;
    return ok;
}

// True if any pixel is less than fully opaque
static bool HasTransparency(Bitmap* bmp)
{
    int w = (int)bmp->GetWidth(), h = (int)bmp->GetHeight();
    Rect rc(0, 0, w, h);
    BitmapData bd;
    if (bmp->LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) return true;
    bool found = false;
    for (int y = 0; y < h && !found; y++) {
        const BYTE* row = (const BYTE*)bd.Scan0 + (size_t)y * bd.Stride;
        for (int x = 0; x < w; x++) if (row[x * 4 + 3] != 255) { found = true; break; }
    }
    bmp->UnlockBits(&bd);
    return found;
}

// Encodes an 8-bit palette PNG (up to 256 colours) with WIC, which picks the
// palette and dithers. With transparent, one palette entry is transparent and
// pixels under half opacity use it.
static bool EncodePngIndexed(Bitmap* bmp, bool transparent, IStream* out)
{
    int w = (int)bmp->GetWidth(), h = (int)bmp->GetHeight();
    Rect rc(0, 0, w, h);
    BitmapData bd;
    if (bmp->LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) return false;

    bool ok = false;
    IWICImagingFactory* fac = nullptr;
    IWICBitmap* src = nullptr;
    IWICPalette* pal = nullptr;
    IWICFormatConverter* conv = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    WICPixelFormatGUID pf = GUID_WICPixelFormat8bppIndexed;
    // GDI+ 32bppARGB is straight-alpha BGRA in memory, WIC's 32bppBGRA
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) &&
        SUCCEEDED(fac->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppBGRA, (UINT)bd.Stride,
                                              (UINT)bd.Stride * h, (BYTE*)bd.Scan0, &src)) &&   // copies the pixels
        SUCCEEDED(fac->CreatePalette(&pal)) &&
        SUCCEEDED(pal->InitializeFromBitmap(src, 256, transparent)) &&
        SUCCEEDED(fac->CreateFormatConverter(&conv)) &&
        SUCCEEDED(conv->Initialize(src, GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeErrorDiffusion,
                                   pal, transparent ? 50.0 : 0.0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(fac->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) &&
        SUCCEEDED(enc->Initialize(out, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(enc->CreateNewFrame(&frame, &props)) &&
        SUCCEEDED(frame->Initialize(props)) &&
        SUCCEEDED(frame->SetSize(w, h)) &&
        SUCCEEDED(frame->SetPixelFormat(&pf)) && pf == GUID_WICPixelFormat8bppIndexed &&
        SUCCEEDED(frame->SetPalette(pal)) &&
        SUCCEEDED(frame->WriteSource(conv, nullptr)) &&
        SUCCEEDED(frame->Commit()) &&
        SUCCEEDED(enc->Commit()))
        ok = true;
    bmp->UnlockBits(&bd);
    if (props) props->Release();
    if (frame) frame->Release();
    if (enc)   enc->Release();
    if (conv)  conv->Release();
    if (pal)   pal->Release();
    if (src)   src->Release();
    if (fac)   fac->Release();
    return ok;
}

// Encodes a PNG with the given settings into out. Images with no transparency
// (or with keepAlpha off) are saved 24-bit, which is about a quarter smaller.
static bool EncodePng(Bitmap* bmp, bool palette, bool keepAlpha, IStream* out)
{
    bool alpha = keepAlpha && HasTransparency(bmp);
    Bitmap* flat = alpha ? nullptr : FlattenOntoWhite(bmp);
    if (!alpha && !flat) return false;
    Bitmap* src = flat ? flat : bmp;
    bool ok;
    if (palette) {
        ok = EncodePngIndexed(src, alpha, out);
    } else {
        CLSID enc;
        ok = GetEncoderClsid(L"image/png", &enc) && src->Save(out, &enc, nullptr) == Ok;
    }
    delete flat;
    return ok;
}

// Writes the whole of an in-memory stream to a file
static bool WriteStreamToFile(IStream* st, const wchar_t* file)
{
    HGLOBAL hg = NULL;
    STATSTG ss = {};
    if (FAILED(GetHGlobalFromStream(st, &hg)) || FAILED(st->Stat(&ss, STATFLAG_NONAME))) return false;
    const void* p = GlobalLock(hg);
    if (!p) return false;
    FILE* f = nullptr;
    bool ok = _wfopen_s(&f, file, L"wb") == 0 && f;
    if (ok) {
        size_t n = (size_t)ss.cbSize.QuadPart;
        ok = fwrite(p, 1, n, f) == n;
        fclose(f);
    }
    GlobalUnlock(hg);
    return ok;
}

static bool SavePngFile(Bitmap* bmp, const wchar_t* file)
{
    IStream* st = nullptr;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &st)) || !st) return false;
    bool ok = EncodePng(bmp, g_pngPalette, g_pngKeepAlpha, st) && WriteStreamToFile(st, file);
    st->Release();
    return ok;
}

static bool SaveWebP(const wchar_t* file)
{
    return g_pCanvas && SaveWebPBitmap(g_pCanvas, file);
}

// Rasterise an SVG file to a pixel buffer.
// Tries Direct2D first (full SVG spec: clipPath, use, etc.); falls back to NanoSVG.
static bool DecodeSvgPixels(const wchar_t* path, float scale, DecodedImage& out)
{
    // Read file bytes via Win32 (handles Unicode paths)
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(hFile, nullptr);
    std::vector<char> buf(sz + 1, '\0');
    DWORD rd = 0;
    ReadFile(hFile, buf.data(), sz, &rd, nullptr);
    CloseHandle(hFile);
    if (rd != sz) return false;

    // Get SVG native dimensions via NanoSVG (fast, no rasterisation).
    // nsvgParse mutates its input buffer, so work on a copy.
    std::vector<char> dimBuf = buf;
    NSVGimage* img = nsvgParse(dimBuf.data(), "px", 96.0f);
    if (!img) return false;
    double natW = img->width, natH = img->height;
    nsvgDelete(img);
    if (!(natW > 0 && natH > 0 && natW < 1e7 && natH < 1e7)) return false;
    // Files can declare huge sizes (e.g. 15100 x 14980, ~900 MB per copy);
    // render those smaller rather than exhausting memory.
    double px = natW * scale * natH * scale;
    if (px > MAX_SVG_RASTER_PIXELS) scale *= (float)sqrt(MAX_SVG_RASTER_PIXELS / px);
    int w = max(1, (int)(natW * scale));
    int h = max(1, (int)(natH * scale));
    out.scale = scale;

    // ── Attempt Direct2D rendering (supports clipPath, <use>, filters, etc.) ──
    bool d2dOk = false;
    {
        ID3D11Device* d3dDev = nullptr;
        if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                D3D11_SDK_VERSION, &d3dDev, nullptr, nullptr)))
        {
            IDXGIDevice* dxgiDev = nullptr;
            d3dDev->QueryInterface(&dxgiDev);

            ID2D1Factory3* d2dFac = nullptr;
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                __uuidof(ID2D1Factory3), nullptr, (void**)&d2dFac);

            ID2D1Device2* d2dDev = nullptr;
            if (d2dFac) d2dFac->CreateDevice(dxgiDev, &d2dDev);

            ID2D1DeviceContext* dcBase = nullptr;
            if (d2dDev) d2dDev->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dcBase);

            ID2D1DeviceContext5* dc = nullptr;
            if (dcBase) dcBase->QueryInterface(&dc);

            ID2D1Bitmap1* target = nullptr;
            if (dc) {
                D2D1_BITMAP_PROPERTIES1 bp = {};
                bp.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM,
                                   D2D1_ALPHA_MODE_PREMULTIPLIED };
                bp.dpiX = bp.dpiY = 96.0f;
                bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
                dc->CreateBitmap({ (UINT32)w, (UINT32)h }, nullptr, 0, &bp, &target);
                dc->SetTarget(target);
            }

            ID2D1SvgDocument* svgDoc = nullptr;
            if (dc && target) {
                IStream* stream = SHCreateMemStream(
                    (const BYTE*)buf.data(), sz);
                dc->CreateSvgDocument(stream,
                    D2D1::SizeF((float)w, (float)h), &svgDoc);
                if (stream) stream->Release();
            }
            // A root width/height fixes the drawing at that size, ignoring the
            // w x h viewport, so an enlarged render came out small in the corner.
            // Give the root a viewBox in its natural units (if it lacks one) and
            // drop width/height so the drawing scales to fill the viewport.
            if (svgDoc) {
                ID2D1SvgElement* root = nullptr;
                svgDoc->GetRoot(&root);
                if (root) {
                    if (!root->IsAttributeSpecified(L"viewBox")) {
                        D2D1_SVG_VIEWBOX vb = { 0.0f, 0.0f, (float)natW, (float)natH };
                        root->SetAttributeValue(L"viewBox", D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX, &vb, sizeof(vb));
                    }
                    root->RemoveAttribute(L"width");
                    root->RemoveAttribute(L"height");
                    root->Release();
                }
            }

            if (dc && target && svgDoc) {
                dc->BeginDraw();
                dc->Clear(D2D1::ColorF(0, 0, 0, 0));
                dc->DrawSvgDocument(svgDoc);
                dc->EndDraw();

                // Read pixels back via D3D staging texture
                IDXGISurface* surf = nullptr;
                target->GetSurface(&surf);
                ID3D11Texture2D* srcTex = nullptr;
                if (surf) surf->QueryInterface(&srcTex);

                if (srcTex) {
                    D3D11_TEXTURE2D_DESC desc = {};
                    srcTex->GetDesc(&desc);
                    desc.Usage          = D3D11_USAGE_STAGING;
                    desc.BindFlags      = 0;
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    desc.MiscFlags      = 0;
                    ID3D11Texture2D* staging = nullptr;
                    d3dDev->CreateTexture2D(&desc, nullptr, &staging);

                    if (staging) {
                        ID3D11DeviceContext* ctx = nullptr;
                        d3dDev->GetImmediateContext(&ctx);
                        ctx->CopyResource(staging, srcTex);
                        D3D11_MAPPED_SUBRESOURCE mapped = {};
                        if (SUCCEEDED(ctx->Map(staging, 0,
                                D3D11_MAP_READ, 0, &mapped)))
                        {
                            // D2D produces premultiplied BGRA — keep it as PARGB
                            // so GDI+ alpha compositing stays correct
                            out.w = w; out.h = h; out.premultiplied = true;
                            out.px.resize((size_t)w * h * 4);
                            for (int y = 0; y < h; y++)
                                memcpy(out.px.data() + (size_t)y * w * 4,
                                       (BYTE*)mapped.pData + y * mapped.RowPitch,
                                       (size_t)w * 4);
                            d2dOk = true;
                            ctx->Unmap(staging, 0);
                        }
                        ctx->Release();
                        staging->Release();
                    }
                    srcTex->Release();
                }
                if (surf) surf->Release();
            }

            if (svgDoc)  svgDoc->Release();
            if (target)  target->Release();
            if (dc)      dc->Release();
            if (dcBase)  dcBase->Release();
            if (d2dDev)  d2dDev->Release();
            if (d2dFac)  d2dFac->Release();
            if (dxgiDev) dxgiDev->Release();
            d3dDev->Release();
        }
    }
    if (d2dOk) return true;

    // ── Fallback: NanoSVG ──
    std::vector<char> fallbackBuf = buf;
    NSVGimage* img2 = nsvgParse(fallbackBuf.data(), "px", 96.0f);
    if (!img2) return false;
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) { nsvgDelete(img2); return false; }
    out.w = w; out.h = h; out.premultiplied = false;
    out.px.assign((size_t)w * h * 4, 0);
    nsvgRasterize(rast, img2, 0.0f, 0.0f, scale, out.px.data(), w, h, w * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(img2);
    for (size_t i = 0; i < (size_t)w * h; i++) std::swap(out.px[i*4+0], out.px[i*4+2]);
    return true;
}

// Rasterise an SVG file to a GDI+ Bitmap. Returns nullptr on failure.
// Caller owns the returned Bitmap. pScaleUsed receives the scale actually
// rendered, which is smaller than requested for very large results.
static Bitmap* LoadSvgToBitmap(const wchar_t* path, float scale, float* pScaleUsed)
{
    DecodedImage img;
    try {
        if (!DecodeSvgPixels(path, scale, img)) return nullptr;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
    if (pScaleUsed) *pScaleUsed = img.scale;
    return MakeBitmapFromPixels(img);
}

// ─────────────────────────────────────────────────────────────────────────────

static void LoadImageFromPath(const wchar_t* file)
{
    const wchar_t* dot = wcsrchr(file, L'.');
    bool isWebP = dot && _wcsicmp(dot + 1, L"webp") == 0;
    bool isSVG  = dot && _wcsicmp(dot + 1, L"svg")  == 0;

    Bitmap* bmp = nullptr;
    float svgScale = 1.0f;
    if (isSVG) {
        bmp = LoadSvgToBitmap(file, 1.0f, &svgScale);
        if (!bmp) {
            MessageBox(g_hwndMain, L"Failed to open SVG image.", APP_TITLE, MB_ICONERROR);
            return;
        }
    } else {
        HRESULT hrWebP = S_OK;
        const wchar_t* webpStep = nullptr;
        bmp = isWebP ? LoadWebP(file, &hrWebP, &webpStep) : Bitmap::FromFile(file);
        if (!isWebP && bmp && bmp->GetLastStatus() == Ok) ApplyExifOrientation(bmp, file);
        if (!isWebP && (!bmp || bmp->GetLastStatus() != Ok)) {
            // GDI+ can't read HEIC, AVIF and some others; Windows' WIC codecs can
            delete bmp;
            DecodedImage img;
            HRESULT hrWic = S_OK;
            bmp = DecodeWicPixels(file, img, &hrWic) ? MakeBitmapFromPixels(img) : nullptr;
            if (!bmp && IsStoreCodecPath(file)) { ShowStoreCodecHelp(file, hrWic); return; }
        }
        if (!bmp || (!isWebP && bmp->GetLastStatus() != Ok)) {
            wchar_t msg[512];
            if (isWebP)
                StringCchPrintf(msg, ARRAYSIZE(msg),
                    L"Failed to open WebP image.\nStep: %s\nHRESULT: 0x%08X",
                    webpStep ? webpStep : L"(unknown)", (unsigned)hrWebP);
            else
                StringCchPrintf(msg, ARRAYSIZE(msg),
                    L"Failed to open image.\nGDI+ status: %d",
                    bmp ? (int)bmp->GetLastStatus() : -1);
            delete bmp;
            MessageBox(g_hwndMain, msg, APP_TITLE, MB_ICONERROR);
            return;
        }
    }
    InstallCanvasImage(bmp, file, isSVG, false, svgScale);
}

// Makes a decoded image the current document. Takes ownership of bmp.
// detached: bmp was built from a pixel buffer (no file association), so an
// ARGB one can become the canvas as-is.
// callerFits: the caller applies its own zoom right after, so skip laying the
// image out at the old zoom (which flashes scrollbars and renders an SVG twice).
static void InstallCanvasImage(Bitmap* bmp, const wchar_t* file, bool isSVG, bool detached, float svgScale, bool callerFits)
{
    // Build the new canvas and preview before releasing the current document,
    // so running out of memory leaves the old image open instead of crashing.
    // Draw into a fresh Bitmap so g_pCanvas has no internal reference to the
    // source file — Bitmap::FromFile keeps the file associated through Clone(),
    // which would cause Save() to fail when writing back to the same path.
    Bitmap* canvas;
    if (detached && bmp->GetPixelFormat() == PixelFormat32bppARGB) {
        canvas = bmp;
    } else {
        canvas = new Bitmap((INT)bmp->GetWidth(), (INT)bmp->GetHeight(), PixelFormat32bppARGB);
        // Pass explicit size: DrawImage(x, y) scales by the source DPI, so a
        // 72-DPI file would be drawn 33% larger and cropped.
        if (canvas->GetLastStatus() == Ok) {
            Graphics g(canvas);
            g.DrawImage(bmp, 0, 0, (INT)bmp->GetWidth(), (INT)bmp->GetHeight());
        }
        delete bmp;
    }
    Bitmap* preview = nullptr;
    if (canvas->GetLastStatus() == Ok)
        preview = new Bitmap((INT)canvas->GetWidth(), (INT)canvas->GetHeight(), PixelFormat32bppARGB);
    if (!preview || preview->GetLastStatus() != Ok) {
        delete preview;
        delete canvas;
        MessageBox(g_hwndMain, L"Not enough memory to open this image.", APP_TITLE, MB_ICONERROR);
        return;
    }

    delete g_svgDisplayBitmap; g_svgDisplayBitmap = nullptr;
    g_isSvgFile = isSVG;
    g_svgCanvasScale = isSVG ? svgScale : 1.0f;
    for (auto* b : g_undoStack) delete b; g_undoStack.clear();
    for (auto* b : g_redoStack) delete b; g_redoStack.clear();
    CancelPaste();
    g_hasSelection = false;
    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;
    delete g_pPreview;
    g_pCanvas = canvas;
    g_pPreview = preview;
    { Graphics gp(g_pPreview); gp.Clear(Color(0,0,0,0)); }
    g_previewUsed = false;
    g_pGfxCanvas = Graphics::FromImage(g_pCanvas);
    g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    g_pGfxCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
    StringCchCopy(g_currentFile, MAX_PATH, file);
    AddRecentFile(file);
    g_modified = false;
    if (!callerFits) {
        if (g_isSvgFile) RebuildSvgDisplayBitmap();
        CenterScrollOnCanvas();
    }
    InvalidateCanvas();
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    // Refresh folder browser: rescan directory so current selection stays in sync
    if (g_showFolderBrowser) {
        BuildFolderList();
        if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
    }
}

// ─── Background image loading ────────────────────────────────────────────────
// Opening a file decodes it on a worker thread while the canvas shows a
// spinner instead of the old image. One decode runs at a time: a request made
// meanwhile is queued, and only the newest queued one is kept.

static constexpr UINT     WM_IMAGE_LOADED    = WM_APP + 0x40;   // lParam: AsyncImageLoad*
static constexpr UINT_PTR LOAD_SPINNER_TIMER = 0x5350;
static bool g_imageLoading = false;   // canvas shows the spinner
static int  g_spinnerAngle = 0;

struct AsyncImageLoad {
    std::wstring path;
    bool fromStrip = false;   // opened from the folder browser
    bool ok = false;
    HRESULT hr = S_OK;        // WIC failure, to explain missing HEIC/AVIF codecs
    DecodedImage img;
};
static bool         g_loadThreadBusy = false;
static bool         g_hasQueuedLoad  = false;
static std::wstring g_queuedLoadPath;
static bool         g_queuedLoadFromStrip = false;

static bool IsSvgPath(const wchar_t* p)
{
    const wchar_t* dot = wcsrchr(p, L'.');
    return dot && _wcsicmp(dot + 1, L"svg") == 0;
}

static void StartLoadThread(const std::wstring& path, bool fromStrip)
{
    auto* req = new AsyncImageLoad();
    req->path = path;
    req->fromStrip = fromStrip;
    g_loadThreadBusy = true;
    HWND target = g_hwndMain;
    std::thread([req, target] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const wchar_t* p   = req->path.c_str();
        const wchar_t* dot = wcsrchr(p, L'.');
        try {
            if (IsSvgPath(p))
                req->ok = DecodeSvgPixels(p, 1.0f, req->img);
            else if (dot && _wcsicmp(dot + 1, L"webp") == 0)
                req->ok = DecodeWebPPixels(p, req->img);
            else
                req->ok = DecodeWicPixels(p, req->img, &req->hr);
        } catch (const std::bad_alloc&) {
            // An uncaught exception here would terminate the app
            req->ok = false;
            req->img = DecodedImage();
        }
        CoUninitialize();
        if (!PostMessage(target, WM_IMAGE_LOADED, 0, (LPARAM)req)) delete req;
    }).detach();
}

static void BeginImageLoad(const wchar_t* file, bool fromStrip)
{
    if (!g_imageLoading) {
        g_imageLoading = true;
        g_spinnerAngle = 0;
        SetTimer(g_hwndCanvas, LOAD_SPINNER_TIMER, 30, NULL);
        InvalidateCanvas();
    }
    g_titleLoadingPath = file;
    UpdateTitleBar();
    if (g_loadThreadBusy) {
        g_hasQueuedLoad = true;
        g_queuedLoadPath = file;
        g_queuedLoadFromStrip = fromStrip;
        return;
    }
    StartLoadThread(file, fromStrip);
}

static void FinishImageLoad(AsyncImageLoad* r)
{
    std::unique_ptr<AsyncImageLoad> req(r);
    g_loadThreadBusy = false;
    if (g_hasQueuedLoad) {
        // A newer request arrived while this one decoded: drop this result
        g_hasQueuedLoad = false;
        StartLoadThread(g_queuedLoadPath, g_queuedLoadFromStrip);
        return;
    }
    g_imageLoading = false;
    g_titleLoadingPath.clear();
    KillTimer(g_hwndCanvas, LOAD_SPINNER_TIMER);
    InvalidateCanvas();
    // Repaint the strip so its paused thumbnail loading resumes
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, FALSE);

    Bitmap* bmp = req->ok ? MakeBitmapFromPixels(req->img) : nullptr;
    std::vector<BYTE>().swap(req->img.px);
    if (bmp)
        InstallCanvasImage(bmp, req->path.c_str(), IsSvgPath(req->path.c_str()), true, req->img.scale, req->fromStrip);
    else if (IsStoreCodecPath(req->path.c_str()))
        ShowStoreCodecHelp(req->path.c_str(), req->hr);   // GDI+ can't read these either
    else
        LoadImageFromPath(req->path.c_str());   // GDI+ fallback; reports the error if this fails too
    UpdateTitleBar();   // back to the open file's name if the load failed

    if (!req->fromStrip) return;
    if (_wcsicmp(g_currentFile, req->path.c_str()) == 0) {
        if (g_zoomFitMode) ZoomToFit();
        else               ZoomToCover();
        if (g_showFolderBrowser && g_currentTool != TOOL_HAND) SetTool(TOOL_HAND);
    }
    // On failure the highlight stays on the file that wouldn't open (the
    // canvas keeps the previous image), so the arrow keys carry on past it
    // rather than retrying it every time.
    CenterFolderBrowserOnCurrent();
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
}

static void DoOpen()
{
    if (!PromptSaveIfNeeded()) return;

    // Derive initial directory from the currently open file
    wchar_t initDir[MAX_PATH] = {};
    if (g_currentFile[0]) {
        StringCchCopy(initDir, MAX_PATH, g_currentFile);
        PathRemoveFileSpecW(initDir);
    }

    wchar_t file[MAX_PATH] = {};
    OPENFILENAME ofn = {};
    ofn.lStructSize    = sizeof(ofn);
    ofn.hwndOwner      = g_hwndMain;
    ofn.lpstrFilter    = L"Image Files\0*.png;*.bmp;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.webp;*.svg;*.heic;*.heif;*.hif;*.avif\0All Files\0*.*\0";
    ofn.lpstrFile      = file;
    ofn.nMaxFile       = MAX_PATH;
    ofn.lpstrInitialDir = initDir[0] ? initDir : nullptr;
    ofn.Flags          = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle     = L"Open Image";
    if (!GetOpenFileName(&ofn)) return;
    BeginImageLoad(file, false);
}

// Formats SaveToFile can write. Anything else (an .svg above all) would get
// PNG data under the wrong extension, destroying the original file.
static bool IsSaveableImagePath(const wchar_t* path)
{
    static const wchar_t* k[] = { L"png", L"jpg", L"jpeg", L"bmp", L"gif", L"tif", L"tiff", L"webp" };
    const wchar_t* dot = wcsrchr(path, L'.');
    if (dot) for (auto e : k) if (_wcsicmp(dot + 1, e) == 0) return true;
    return false;
}

static void SaveToFile(const wchar_t* filepath)
{
    if (!IsSaveableImagePath(filepath)) {
        MessageBox(g_hwndMain,
                   L"MyPaint can save PNG, JPEG, BMP, GIF, TIFF and WebP images.\n"
                   L"Please choose one of those file types.",
                   APP_TITLE, MB_ICONWARNING);
        return;
    }
    const wchar_t* dot = wcsrchr(filepath, L'.');
    bool isWebP = dot && _wcsicmp(dot + 1, L"webp") == 0;

    bool isJpeg = dot && (_wcsicmp(dot + 1, L"jpg") == 0 || _wcsicmp(dot + 1, L"jpeg") == 0);
    // Large images can take seconds (lossless WebP above all)
    HCURSOR oldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    bool ok;
    if (isWebP) {
        ok = SaveWebP(filepath);
    } else if (isJpeg) {
        ok = SaveJpeg(g_pCanvas, g_jpegQuality, filepath);
    } else if (dot && _wcsicmp(dot + 1, L"png") == 0) {
        ok = SavePngFile(g_pCanvas, filepath);
    } else {
        CLSID enc;
        if (!GetSaveEncoder(filepath, enc)) GetEncoderClsid(L"image/png", &enc);
        ok = (g_pCanvas->Save(filepath, &enc) == Ok);
    }
    SetCursor(oldCursor);

    if (!ok) {
        MessageBox(g_hwndMain, L"Failed to save file.", APP_TITLE, MB_ICONERROR);
    } else {
        StringCchCopy(g_currentFile, MAX_PATH, filepath);
        AddRecentFile(filepath);
        g_modified = false;
        if (g_isSvgFile) {
            // Now a raster file: stop re-rendering the display from SVG source
            g_isSvgFile = false;
            g_svgCanvasScale = 1.0f;
            delete g_svgDisplayBitmap; g_svgDisplayBitmap = nullptr;
            InvalidateCanvas();
        }
        UpdateTitleBar();
        // Refresh the strip so the new file shows up
        if (g_showFolderBrowser) {
            BuildFolderList();
            if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
        }
    }
}

static void DoSave()
{
    // An SVG (or any format MyPaint can't write) goes through Save As, so the
    // original is never overwritten with raster data
    if (g_currentFile[0] == L'\0' || !IsSaveableImagePath(g_currentFile)) { DoSaveAs(); return; }
    SaveToFile(g_currentFile);
}

// ─── File > Save Options ────────────────────────────────────────────────────
// One page per format with settings (JPEG, PNG, WebP), picked with a row of
// tab buttons. Saving itself never asks: it uses whatever is set here.

enum { SAVEOPT_JPEG, SAVEOPT_PNG, SAVEOPT_WEBP };

struct SaveOptionsData {
    int     tab;
    int     jpegQuality, webpQuality;
    bool    webpLossless, webpKeepAlpha, pngPalette, pngKeepAlpha;
    Bitmap* sample;       // the open image, scaled down when big, for size estimates
    double  sampleScale;  // image pixels per sample pixel
};
static SaveOptionsData g_saveOpt;
static int g_saveOptLastTab = SAVEOPT_JPEG;
static const UINT_PTR SAVEOPT_ESTIMATE_TIMER = 1;

static void SaveOptionsEstimate(HWND hDlg)
{
    SaveOptionsData& d = g_saveOpt;
    if (!d.sample) return;
    unsigned long long bytes = 0;
    if (d.tab == SAVEOPT_WEBP) {
        std::vector<uint8_t> data;
        if (EncodeWebP(d.sample, d.webpQuality, d.webpLossless, data, d.webpKeepAlpha)) bytes = data.size();
    } else {
        IStream* st = nullptr;
        if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &st)) && st) {
            bool ok = d.tab == SAVEOPT_JPEG ? SaveJpeg(d.sample, d.jpegQuality, nullptr, st)
                                            : EncodePng(d.sample, d.pngPalette, d.pngKeepAlpha, st);
            STATSTG ss = {};
            if (ok && SUCCEEDED(st->Stat(&ss, STATFLAG_NONAME))) bytes = ss.cbSize.QuadPart;
            st->Release();
        }
    }
    bytes = (unsigned long long)(bytes * d.sampleScale);
    wchar_t text[96];
    const wchar_t* about = d.sampleScale > 1.0 ? L"about " : L"";
    if (!bytes)                    StringCchCopy(text, ARRAYSIZE(text), L"Estimated size for this image: unknown");
    else if (bytes < 1024 * 1024)  StringCchPrintf(text, ARRAYSIZE(text), L"Estimated size for this image: %s%llu KB", about, (bytes + 512) / 1024);
    else                           StringCchPrintf(text, ARRAYSIZE(text), L"Estimated size for this image: %s%.1f MB", about, bytes / (1024.0 * 1024.0));
    SetDlgItemTextW(hDlg, IDC_SAVEOPT_SIZE, text);
}

static INT_PTR CALLBACK SaveOptionsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool s_syncing = false;
    SaveOptionsData& d = g_saveOpt;
    auto scheduleEstimate = [&]() {
        SetDlgItemTextW(hDlg, IDC_SAVEOPT_SIZE, L"Estimated size for this image: …");
        SetTimer(hDlg, SAVEOPT_ESTIMATE_TIMER, 250, NULL);   // after the slider settles
    };
    auto show = [&](int id, bool on) { ShowWindow(GetDlgItem(hDlg, id), on ? SW_SHOW : SW_HIDE); };
    // Shows the current tab's controls and loads its values into them
    auto showPage = [&]() {
        bool jpeg = d.tab == SAVEOPT_JPEG, png = d.tab == SAVEOPT_PNG, webp = d.tab == SAVEOPT_WEBP;
        for (int id : { IDC_SAVEOPT_QLABEL, IDC_SAVEOPT_SLIDER, IDC_SAVEOPT_QUALITY }) show(id, !png);
        show(IDC_SAVEOPT_HINT, jpeg);
        show(IDC_SAVEOPT_LOSSLESS, webp);
        show(IDC_SAVEOPT_WEBP_ALPHA, webp);
        for (int id : { IDC_SAVEOPT_PNG_FULL, IDC_SAVEOPT_PNG_256, IDC_SAVEOPT_PNG_ALPHA, IDC_SAVEOPT_PNG_HINT }) show(id, png);
        if (!png) {
            int q = jpeg ? d.jpegQuality : d.webpQuality;
            s_syncing = true;
            SendDlgItemMessage(hDlg, IDC_SAVEOPT_SLIDER, TBM_SETPOS, TRUE, q);
            SetDlgItemInt(hDlg, IDC_SAVEOPT_QUALITY, q, FALSE);
            s_syncing = false;
            BOOL qOn = !(webp && d.webpLossless);
            EnableWindow(GetDlgItem(hDlg, IDC_SAVEOPT_SLIDER), qOn);
            EnableWindow(GetDlgItem(hDlg, IDC_SAVEOPT_QUALITY), qOn);
        }
        CheckDlgButton(hDlg, IDC_SAVEOPT_LOSSLESS,  d.webpLossless ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_SAVEOPT_WEBP_ALPHA, d.webpKeepAlpha ? BST_CHECKED : BST_UNCHECKED);
        CheckRadioButton(hDlg, IDC_SAVEOPT_PNG_FULL, IDC_SAVEOPT_PNG_256,
                         d.pngPalette ? IDC_SAVEOPT_PNG_256 : IDC_SAVEOPT_PNG_FULL);
        CheckDlgButton(hDlg, IDC_SAVEOPT_PNG_ALPHA, d.pngKeepAlpha ? BST_CHECKED : BST_UNCHECKED);
        for (int id : { IDC_SAVEOPT_TAB_JPEG, IDC_SAVEOPT_TAB_PNG, IDC_SAVEOPT_TAB_WEBP })
            InvalidateRect(GetDlgItem(hDlg, id), NULL, FALSE);
        scheduleEstimate();
    };
    auto setQuality = [&](int q) {
        if (d.tab == SAVEOPT_JPEG) d.jpegQuality = q; else d.webpQuality = q;
    };

    switch (msg) {
    case WM_INITDIALOG: {
        SendDlgItemMessage(hDlg, IDC_SAVEOPT_SLIDER, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
        SendDlgItemMessage(hDlg, IDC_SAVEOPT_SLIDER, TBM_SETPAGESIZE, 0, 10);
        SendDlgItemMessage(hDlg, IDC_SAVEOPT_QUALITY, EM_LIMITTEXT, 3, 0);
        if (g_darkMode) {
            // Unthemed check and radio buttons so WM_CTLCOLORSTATIC can colour their text
            for (int id : { IDC_SAVEOPT_LOSSLESS, IDC_SAVEOPT_WEBP_ALPHA, IDC_SAVEOPT_PNG_FULL, IDC_SAVEOPT_PNG_256, IDC_SAVEOPT_PNG_ALPHA })
                SetWindowTheme(GetDlgItem(hDlg, id), L"", L"");
            for (int id : { IDOK, IDCANCEL, IDC_SAVEOPT_DEFAULTS })
                SetWindowTheme(GetDlgItem(hDlg, id), L"DarkMode_Explorer", NULL);
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hDlg, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        }
        showPage();
        return TRUE;
    }
    case WM_DRAWITEM: {
        // Tab buttons: the selected one in the accent colour
        auto* dis = (DRAWITEMSTRUCT*)lParam;
        int tab = dis->CtlID == IDC_SAVEOPT_TAB_JPEG ? SAVEOPT_JPEG
                : dis->CtlID == IDC_SAVEOPT_TAB_PNG  ? SAVEOPT_PNG
                : dis->CtlID == IDC_SAVEOPT_TAB_WEBP ? SAVEOPT_WEBP : -1;
        if (tab < 0) break;
        bool sel = tab == d.tab;
        COLORREF bg = sel ? (g_darkMode ? RGB(0, 84, 153) : RGB(0, 120, 215))
                          : (g_darkMode ? RGB(60, 60, 64) : RGB(225, 225, 225));
        HBRUSH br = CreateSolidBrush(bg);
        FillRect(dis->hDC, &dis->rcItem, br);
        DeleteObject(br);
        wchar_t text[16];
        GetWindowTextW(dis->hwndItem, text, 16);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, sel ? RGB(255, 255, 255) : (g_darkMode ? RGB(220, 220, 220) : RGB(30, 30, 30)));
        DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        // Focus outline only once the keyboard is in use, as Windows does for
        // its own controls (ODS_NOFOCUSRECT is set while focus cues are hidden)
        if ((dis->itemState & ODS_FOCUS) && !(dis->itemState & ODS_NOFOCUSRECT)) {
            RECT fr = dis->rcItem; InflateRect(&fr, -2, -2);
            DrawFocusRect(dis->hDC, &fr);
        }
        return TRUE;
    }
    case WM_PAINT: {
        // Frame around the page, joined to the tab row
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);
        RECT fr = { 7, 22, 243, 108 };
        MapDialogRect(hDlg, &fr);
        HPEN pen = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(90, 90, 96) : RGB(180, 180, 180));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, fr.left, fr.top, fr.right, fr.bottom);
        SelectObject(hdc, oldBr); SelectObject(hdc, oldPen);
        DeleteObject(pen);
        EndPaint(hDlg, &ps);
        return TRUE;
    }
    case WM_HSCROLL:
        if ((HWND)lParam == GetDlgItem(hDlg, IDC_SAVEOPT_SLIDER)) {
            int q = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            setQuality(q);
            s_syncing = true;
            SetDlgItemInt(hDlg, IDC_SAVEOPT_QUALITY, q, FALSE);
            s_syncing = false;
            scheduleEstimate();
        }
        return TRUE;
    case WM_TIMER:
        if (wParam == SAVEOPT_ESTIMATE_TIMER) {
            KillTimer(hDlg, SAVEOPT_ESTIMATE_TIMER);
            HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
            SaveOptionsEstimate(hDlg);
            SetCursor(old);
        }
        return TRUE;
    case WM_COMMAND: {
        WORD id = LOWORD(wParam), code = HIWORD(wParam);
        switch (id) {
        case IDC_SAVEOPT_TAB_JPEG: d.tab = SAVEOPT_JPEG; showPage(); return TRUE;
        case IDC_SAVEOPT_TAB_PNG:  d.tab = SAVEOPT_PNG;  showPage(); return TRUE;
        case IDC_SAVEOPT_TAB_WEBP: d.tab = SAVEOPT_WEBP; showPage(); return TRUE;
        case IDC_SAVEOPT_QUALITY:
            if (code == EN_CHANGE && !s_syncing) {
                BOOL ok = FALSE;
                int q = (int)GetDlgItemInt(hDlg, IDC_SAVEOPT_QUALITY, &ok, FALSE);
                if (ok && q >= 1 && q <= 100) {
                    setQuality(q);
                    SendDlgItemMessage(hDlg, IDC_SAVEOPT_SLIDER, TBM_SETPOS, TRUE, q);
                    scheduleEstimate();
                }
            }
            return TRUE;
        case IDC_SAVEOPT_LOSSLESS:
            d.webpLossless = IsDlgButtonChecked(hDlg, IDC_SAVEOPT_LOSSLESS) == BST_CHECKED;
            showPage();
            return TRUE;
        case IDC_SAVEOPT_WEBP_ALPHA:
            d.webpKeepAlpha = IsDlgButtonChecked(hDlg, IDC_SAVEOPT_WEBP_ALPHA) == BST_CHECKED;
            scheduleEstimate();
            return TRUE;
        case IDC_SAVEOPT_PNG_FULL:
        case IDC_SAVEOPT_PNG_256:
            d.pngPalette = IsDlgButtonChecked(hDlg, IDC_SAVEOPT_PNG_256) == BST_CHECKED;
            scheduleEstimate();
            return TRUE;
        case IDC_SAVEOPT_PNG_ALPHA:
            d.pngKeepAlpha = IsDlgButtonChecked(hDlg, IDC_SAVEOPT_PNG_ALPHA) == BST_CHECKED;
            scheduleEstimate();
            return TRUE;
        case IDC_SAVEOPT_DEFAULTS:
            d.jpegQuality  = d.webpQuality = DEFAULT_SAVE_QUALITY;
            d.webpLossless = false;
            d.webpKeepAlpha = true;
            d.pngPalette   = false;
            d.pngKeepAlpha = true;
            showPage();
            return TRUE;
        case IDOK:
        case IDCANCEL:
            KillTimer(hDlg, SAVEOPT_ESTIMATE_TIMER);
            EndDialog(hDlg, id);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
        if (g_darkMode && g_hDarkBgBrush) {
            SetTextColor((HDC)wParam, RGB(220, 220, 220));
            SetBkColor((HDC)wParam, RGB(45, 45, 48));
            return (INT_PTR)g_hDarkBgBrush;
        }
        break;
    }
    return FALSE;
}

// File > Save Options
static void DoSaveOptions()
{
    if (!g_pCanvas) return;
    SaveOptionsData& d = g_saveOpt;
    d.tab          = g_saveOptLastTab;
    d.jpegQuality  = g_jpegQuality;
    d.webpQuality  = g_webpQuality;
    d.webpLossless = g_webpLossless;
    d.webpKeepAlpha = g_webpKeepAlpha;
    d.pngPalette   = g_pngPalette;
    d.pngKeepAlpha = g_pngKeepAlpha;
    // Estimate from a copy of at most ~4 megapixels so the dialog stays quick
    int w = (int)g_pCanvas->GetWidth(), h = (int)g_pCanvas->GetHeight();
    double px = (double)w * h, maxPx = 4.0 * 1024 * 1024;
    double f = px > maxPx ? sqrt(maxPx / px) : 1.0;
    int sw = max(1, (int)(w * f)), sh = max(1, (int)(h * f));
    d.sample = new Bitmap(sw, sh, PixelFormat32bppARGB);
    {
        Graphics g(d.sample);
        g.SetCompositingMode(CompositingModeSourceCopy);   // keep transparency as is
        g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
        g.DrawImage(g_pCanvas, 0, 0, sw, sh);
    }
    d.sampleScale = px / ((double)sw * sh);
    if (!g_hDarkBgBrush && g_darkMode) g_hDarkBgBrush = CreateSolidBrush(RGB(45, 45, 48));
    INT_PTR r = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SAVE_OPTIONS), g_hwndMain, SaveOptionsDlgProc);
    delete d.sample; d.sample = nullptr;
    g_saveOptLastTab = d.tab;
    if (r != IDOK) return;
    g_jpegQuality  = d.jpegQuality;
    g_webpQuality  = d.webpQuality;
    g_webpLossless = d.webpLossless;
    g_webpKeepAlpha = d.webpKeepAlpha;
    g_pngPalette   = d.pngPalette;
    g_pngKeepAlpha = d.pngKeepAlpha;
}

static void DoSaveAs()
{
    wchar_t file[MAX_PATH] = {};
    if (g_currentFile[0] != L'\0') StringCchCopy(file, MAX_PATH, g_currentFile);
    // Suggest a PNG beside an SVG (or other unwritable file) rather than its own name
    if (file[0] && !IsSaveableImagePath(file)) PathRenameExtensionW(file, L".png");
    // Open in the image's own folder: given a full path in lpstrFile, the
    // dialog may still start in the last folder used instead
    wchar_t initDir[MAX_PATH] = {};
    if (file[0]) {
        StringCchCopy(initDir, MAX_PATH, file);
        PathRemoveFileSpecW(initDir);
        StringCchCopy(file, MAX_PATH, PathFindFileNameW(g_currentFile));
        if (!IsSaveableImagePath(file)) PathRenameExtensionW(file, L".png");
    }
    OPENFILENAME ofn = {};
    ofn.lpstrInitialDir = initDir[0] ? initDir : nullptr;
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = g_hwndMain;
    ofn.lpstrFilter  = L"PNG Image\0*.png\0JPEG Image\0*.jpg;*.jpeg\0WebP Image\0*.webp\0"
                       L"GIF Image\0*.gif\0TIFF Image\0*.tif;*.tiff\0BMP Image\0*.bmp\0All Files\0*.*\0";
    // Default filter/extension to match the currently open file type
    const wchar_t* curDot = wcsrchr(g_currentFile, L'.');
    DWORD filterIndex = 1;
    const wchar_t* defExt = L"png";
    if (curDot) {
        const wchar_t* e = curDot + 1;
        if      (_wcsicmp(e, L"jpg") == 0 || _wcsicmp(e, L"jpeg") == 0) { filterIndex = 2; defExt = L"jpg"; }
        else if (_wcsicmp(e, L"webp") == 0)                             { filterIndex = 3; defExt = L"webp"; }
        else if (_wcsicmp(e, L"gif") == 0)                              { filterIndex = 4; defExt = L"gif"; }
        else if (_wcsicmp(e, L"tif") == 0 || _wcsicmp(e, L"tiff") == 0) { filterIndex = 5; defExt = L"tif"; }
        else if (_wcsicmp(e, L"bmp") == 0)                              { filterIndex = 6; defExt = L"bmp"; }
    }
    ofn.nFilterIndex = filterIndex;
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrDefExt  = defExt;
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = L"Save Image As";
    if (!GetSaveFileName(&ofn)) return;
    SaveToFile(file);   // with the settings from File > Save Options
}

static void DoPrint()
{
    if (!g_pCanvas) return;
    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner   = g_hwndMain;
    pd.Flags       = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
    if (!PrintDlgW(&pd)) return;
    HDC hPrn = pd.hDC;
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
    if (!hPrn) return;
    int cw = (int)g_pCanvas->GetWidth();
    int ch = (int)g_pCanvas->GetHeight();
    int pw = GetDeviceCaps(hPrn, HORZRES);
    int ph = GetDeviceCaps(hPrn, VERTRES);
    float scale = min((float)pw / cw, (float)ph / ch);
    int dw = (int)(cw * scale), dh = (int)(ch * scale);
    int dx = (pw - dw) / 2,    dy = (ph - dh) / 2;
    DOCINFOW di = { sizeof(di), L"MyPaint" };
    if (StartDocW(hPrn, &di) > 0) {
        StartPage(hPrn);
        Graphics g(hPrn);
        g.DrawImage(g_pCanvas, dx, dy, dw, dh);
        EndPage(hPrn);
        EndDoc(hPrn);
    }
    DeleteDC(hPrn);
}

// ============================================================
//  Persistent settings  (HKCU\Software\MyPaint)
// ============================================================
static const wchar_t* REG_KEY = L"Software\\MyPaint";

// Default 16-colour palette (used on first run and on reset)
static const COLORREF s_defaultCustomColors[CS_CUSTOM_COUNT] = {
    RGB(  0,   0,   0),   //  0%  Black
    RGB(128, 128, 128),   // 50%  Dk. Gray
    RGB(128,   0,   0),   // 50%  Red
    RGB(  0,   0, 128),   // 50%  Blue
    RGB(  0, 128,   0),   // 50%  Green
    RGB(128,  64,   0),   // 50%  Brown
    RGB(128,   0, 128),   // 50%  Purple
    RGB(204, 204, 204),   // 80%  Lt. Gray
    RGB(  0, 102, 204),   // 80%  Lt. Blue
    RGB(  0, 204,   0),   // 80%  Lt. Green
    RGB(255, 128,   0),   // 80%  Orange
    RGB(  0, 204, 204),   // 80%  Cyan
    RGB(210, 180, 140),   // 90%  Tan
    RGB(255, 128, 192),   // 90%  Pink
    RGB(255, 255,   0),   // 90%  Yellow
    RGB(255, 255, 255),   // 100% White
};

static void ResetCustomColors()
{
    memcpy(g_customColors, s_defaultCustomColors, sizeof(g_customColors));
    g_fgColor = RGB(0, 0, 0);
    g_bgColor = RGB(255, 255, 255);
    g_fgAlpha = 100;
    g_bgAlpha = 100;
    g_strokeActive = true;
    UpdateAlphaEdit();
    RecreateFillCursor();
    if (g_hwndMain) InvalidateRect(g_hwndMain, NULL, FALSE);
}

static void LoadSettings()
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        ResetCustomColors();   // first run — apply built-in defaults
        return;
    }

    for (int i = 0; i < CS_CUSTOM_COUNT; ++i) {
        wchar_t name[32];
        StringCchPrintf(name, 32, L"CustomColor%d", i);
        DWORD val = 0, sz = sizeof(val), type = REG_DWORD;
        if (RegQueryValueExW(hKey, name, NULL, &type, (BYTE*)&val, &sz) == ERROR_SUCCESS)
            g_customColors[i] = (COLORREF)val;
    }

    // Helper lambda to read one DWORD value
    auto qv = [&](const wchar_t* name, DWORD* out) -> bool {
        DWORD sz = sizeof(DWORD), tp = REG_DWORD;
        return RegQueryValueExW(hKey, name, NULL, &tp, (BYTE*)out, &sz) == ERROR_SUCCESS;
    };

    // FG / BG colours
    DWORD val = 0;
    if (qv(L"FgColor", &val)) g_fgColor = (COLORREF)val;
    if (qv(L"BgColor", &val)) g_bgColor = (COLORREF)val;
    if (qv(L"BgAlpha", &val)) { g_bgAlpha = (int)val; if (g_bgAlpha < 0) g_bgAlpha = 0; if (g_bgAlpha > 100) g_bgAlpha = 100; }

    if (qv(L"DarkMode", &val)) g_darkMode = (val != 0);
    if (qv(L"FolderBrowserFromFile", &val))  g_folderBrowserFromFile  = (val != 0);
    if (qv(L"FolderBrowserFromBlank", &val)) g_folderBrowserFromBlank = (val != 0);
    if (qv(L"SlideshowSeconds", &val) && (val == 3 || val == 5 || val == 10)) g_slideSeconds = (int)val;
    if (qv(L"FolderBrowserH", &val) && val <= 4000)
        g_folderBrowserH = max(FOLDER_BROWSER_MIN_H, (int)val);

    // Qualities saved before version 2 were the old default of 90, not a
    // choice, so they give way to the new default of 100
    DWORD saveOptVer = 0;
    qv(L"SaveOptionsVersion", &saveOptVer);
    if (saveOptVer >= 2) {
        if (qv(L"JpegQuality", &val) && val >= 1 && val <= 100) g_jpegQuality = (int)val;
        if (qv(L"WebpQuality", &val) && val >= 1 && val <= 100) g_webpQuality = (int)val;
    }
    if (qv(L"WebpLossless", &val)) g_webpLossless = (val != 0);
    if (qv(L"WebpKeepAlpha", &val)) g_webpKeepAlpha = (val != 0);
    if (qv(L"PngPalette", &val))   g_pngPalette   = (val != 0);
    if (qv(L"PngKeepAlpha", &val)) g_pngKeepAlpha = (val != 0);

    g_recentFiles.clear();
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        wchar_t name[32], path[MAX_PATH];
        StringCchPrintf(name, 32, L"RecentFile%d", i);
        DWORD sz = sizeof(path), tp = 0;
        if (RegQueryValueExW(hKey, name, NULL, &tp, (BYTE*)path, &sz) == ERROR_SUCCESS && tp == REG_SZ) {
            path[MAX_PATH - 1] = L'\0';
            if (path[0]) g_recentFiles.push_back(path);
        }
    }

    // Line width — stored as tenths (e.g. 8.5 → 85)
    if (qv(L"LineWidth", &val)) {
        float lw = (float)val / 10.0f;
        if (lw >= MIN_BRUSH_SZ && lw <= MAX_BRUSH_SZ)
            g_brushSize = lw;
    }
    if (qv(L"EraserSize", &val)) {
        int es = (int)val;
        if (es >= MIN_ERASER_SZ && es <= MAX_ERASER_SZ) g_eraserSize = es;
    }
    if (qv(L"EraserRound", &val)) g_eraserRound = (val != 0);
    if (qv(L"SmudgeSize", &val)) {
        int ss = (int)val;
        if (ss >= MIN_SMUDGE_SZ && ss <= MAX_SMUDGE_SZ) g_smudgeSize = ss;
    }
    if (qv(L"SmudgeRound", &val)) g_smudgeRound = (val != 0);
    if (qv(L"FillTolerance", &val)) {
        int ft = (int)val;
        if (ft >= 0 && ft <= 255) g_fillTolerance = ft;
    }

    // Window placement
    DWORD wx = 0, wy = 0, ww = 0, wh = 0;
    if (qv(L"WindowX", &wx) && qv(L"WindowY", &wy) &&
        qv(L"WindowW", &ww) && qv(L"WindowH", &wh))
    {
        g_savedWinX = (int)wx;  g_savedWinY = (int)wy;
        g_savedWinW = (int)ww;  g_savedWinH = (int)wh;
    }

    RegCloseKey(hKey);
}

static void SaveSettings()
{
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &hKey, &disp) != ERROR_SUCCESS)
        return;

    for (int i = 0; i < CS_CUSTOM_COUNT; ++i) {
        wchar_t name[32];
        StringCchPrintf(name, 32, L"CustomColor%d", i);
        DWORD val = (DWORD)g_customColors[i];
        RegSetValueExW(hKey, name, 0, REG_DWORD, (BYTE*)&val, sizeof(val));
    }

    // Helper lambda to write one DWORD value
    auto sv = [&](const wchar_t* name, DWORD val) {
        RegSetValueExW(hKey, name, 0, REG_DWORD, (BYTE*)&val, sizeof(val));
    };

    // FG / BG colours
    sv(L"FgColor",  (DWORD)g_fgColor);
    sv(L"BgColor",  (DWORD)g_bgColor);
    sv(L"BgAlpha",  (DWORD)g_bgAlpha);

    sv(L"DarkMode", (DWORD)(g_darkMode ? 1 : 0));
    // The folder browser state is remembered separately for starting with an
    // image and starting blank; this session updates the one it started as
    if (g_startedWithFile) g_folderBrowserFromFile  = g_showFolderBrowser;
    else                   g_folderBrowserFromBlank = g_showFolderBrowser;
    sv(L"FolderBrowserFromFile",  (DWORD)(g_folderBrowserFromFile  ? 1 : 0));
    sv(L"FolderBrowserFromBlank", (DWORD)(g_folderBrowserFromBlank ? 1 : 0));
    RegDeleteValueW(hKey, L"FolderBrowser");   // replaced by the two values above
    sv(L"FolderBrowserH", (DWORD)g_folderBrowserH);
    sv(L"SlideshowSeconds", (DWORD)g_slideSeconds);
    sv(L"JpegQuality",    (DWORD)g_jpegQuality);
    sv(L"WebpQuality",    (DWORD)g_webpQuality);
    sv(L"WebpLossless",   (DWORD)(g_webpLossless ? 1 : 0));
    sv(L"WebpKeepAlpha",  (DWORD)(g_webpKeepAlpha ? 1 : 0));
    sv(L"PngPalette",     (DWORD)(g_pngPalette ? 1 : 0));
    sv(L"PngKeepAlpha",   (DWORD)(g_pngKeepAlpha ? 1 : 0));
    sv(L"SaveOptionsVersion", 2);
    RegDeleteValueW(hKey, L"AskSaveOptions");   // from the old Save As popup

    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        wchar_t name[32];
        StringCchPrintf(name, 32, L"RecentFile%d", i);
        if (i < (int)g_recentFiles.size()) {
            const std::wstring& f = g_recentFiles[i];
            RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE*)f.c_str(), (DWORD)((f.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, name);
        }
    }

    // Line width — stored as tenths to preserve one decimal place
    sv(L"LineWidth", (DWORD)(g_brushSize * 10.0f + 0.5f));
    sv(L"EraserSize",  (DWORD)g_eraserSize);
    sv(L"EraserRound", (DWORD)(g_eraserRound ? 1 : 0));
    sv(L"SmudgeSize",  (DWORD)g_smudgeSize);
    sv(L"SmudgeRound", (DWORD)(g_smudgeRound ? 1 : 0));
    sv(L"FillTolerance", (DWORD)g_fillTolerance);

    // Window placement — use GetWindowPlacement so we save the restored rect
    // even if the window is currently maximised
    WINDOWPLACEMENT wp = {}; wp.length = sizeof(wp);
    if (GetWindowPlacement(g_hwndMain, &wp)) {
        const RECT& r = wp.rcNormalPosition;
        sv(L"WindowX", (DWORD)r.left);
        sv(L"WindowY", (DWORD)r.top);
        sv(L"WindowW", (DWORD)(r.right  - r.left));
        sv(L"WindowH", (DWORD)(r.bottom - r.top));
    }

    RegCloseKey(hKey);
}

// ============================================================
//  Colour choosers
// ============================================================
static void SetActiveColorFromPick(BYTE r, BYTE g2, BYTE b, BYTE a)
{
    COLORREF col = RGB(r, g2, b);
    int alpha = (int)(a * 100 / 255);
    if (g_strokeActive) { g_fgColor = col; g_fgAlpha = alpha; RecreateFillCursor(); RecreateBrushCursors(); }
    else                { g_bgColor = col; g_bgAlpha = alpha; }
    UpdateAlphaEdit();
    RefreshEditingPreview();
    SetTool(TOOL_FILL);
    InvalidateRect(g_hwndMain, NULL, FALSE);
}

static void ChooseActiveColor()
{
    COLORREF& color = g_strokeActive ? g_fgColor : g_bgColor;
    CHOOSECOLOR cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = g_hwndMain;
    cc.rgbResult    = color;
    cc.lpCustColors = g_customColors;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc)) {
        color = cc.rgbResult;
        if (!g_strokeActive) g_bgAlpha = 100;
        else { RecreateFillCursor(); RecreateBrushCursors(); }
        UpdateAlphaEdit();
        RefreshEditingPreview();
        InvalidateRect(g_hwndMain, NULL, FALSE);
    }
}

// Build (or rebuild) the fill-bucket cursor with the current foreground colour.
// The lower bucket half and the drop are filled with g_fgColor; the rest is
// white-filled with a black outline, matching the other custom cursors.
static void RecreateFillCursor()
{
    if (!g_hwndMain) return;   // called before window is ready (e.g. from LoadSettings)
    if (g_hCurFill) { DestroyCursor(g_hCurFill); g_hCurFill = NULL; }

    // Fluent ic_fluent_paint_bucket_20_regular split into three rendering layers:
    //   fillOuterD  — subpath 1: outer silhouette  → white fill
    //   fillColorD  — subpaths 3+4: lower bucket half + drop  → g_fgColor fill
    //   fillFullD   — all 5 subpaths  → black stroke (outline + detail)
    static const char* fillOuterD =
        "M8.99997 2.5C8.99997 2.22386 8.77611 2 8.49997 2C8.22383 2 7.99997 2.22386"
        " 7.99997 2.5V3.9996C7.79524 4.0718 7.60306 4.18978 7.43931 4.35352L2.35352 9.43931"
        "C1.76774 10.0251 1.76774 10.9748 2.35352 11.5606L6.68931 15.8964"
        "C7.2751 16.4822 8.22484 16.4822 8.81063 15.8964L13.8964 10.8106"
        "C14.4822 10.2248 14.4822 9.2751 13.8964 8.68931L9.56063 4.35352"
        "C9.39688 4.18978 9.2047 4.0718 8.99997 3.9996V2.5Z";
    static const char* fillColorD =
        "M7.39642 15.1893L3.20711 11H12.2928L8.10352 15.1893"
        "C7.90826 15.3846 7.59168 15.3846 7.39642 15.1893Z"
        "M16.016 11.3922C15.7855 10.997 15.2145 10.997 14.984 11.3922L13.3316 14.2248"
        "C12.3554 15.8983 13.5626 18 15.5 18C17.4374 18 18.6445 15.8983 17.6683 14.2248"
        "L16.016 11.3922Z";
    static const char* fillFullD =
        "M8.99997 2.5C8.99997 2.22386 8.77611 2 8.49997 2C8.22383 2 7.99997 2.22386"
        " 7.99997 2.5V3.9996C7.79524 4.0718 7.60306 4.18978 7.43931 4.35352L2.35352 9.43931"
        "C1.76774 10.0251 1.76774 10.9748 2.35352 11.5606L6.68931 15.8964"
        "C7.2751 16.4822 8.22484 16.4822 8.81063 15.8964L13.8964 10.8106"
        "C14.4822 10.2248 14.4822 9.2751 13.8964 8.68931L9.56063 4.35352"
        "C9.39688 4.18978 9.2047 4.0718 8.99997 3.9996V2.5Z"
        "M7.99997 5.20708V6.5C7.99997 6.77614 8.22383 7 8.49997 7C8.77611 7 8.99997 6.77614"
        " 8.99997 6.5V5.20708L13.1893 9.39642C13.3529 9.56002 13.3794 9.80878 13.2689 10"
        "H3.20705L7.99997 5.20708Z"
        "M7.39642 15.1893L3.20711 11H12.2928L8.10352 15.1893"
        "C7.90826 15.3846 7.59168 15.3846 7.39642 15.1893Z"
        "M16.016 11.3922C15.7855 10.997 15.2145 10.997 14.984 11.3922L13.3316 14.2248"
        "C12.3554 15.8983 13.5626 18 15.5 18C17.4374 18 18.6445 15.8983 17.6683 14.2248"
        "L16.016 11.3922Z"
        "M14.1954 14.7287L15.5 12.4923L16.8045 14.7287C17.3919 15.7355 16.6656 17 15.5 17"
        "C14.3343 17 13.6081 15.7355 14.1954 14.7287Z";

    const int fsz = 32;
    const float fScale = (float)fsz / 20.0f;
    Bitmap fbmp(fsz, fsz, PixelFormat32bppARGB);
    {
        Graphics fg(&fbmp);
        fg.SetSmoothingMode(SmoothingModeAntiAlias);
        fg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        fg.Clear(Color(0, 0, 0, 0));
        // White fill — outer silhouette
        GraphicsPath fpOuter(FillModeWinding);
        AddSvgPath(fpOuter, fillOuterD, fScale);
        SolidBrush whiteBr(Color(255, 255, 255, 255));
        fg.FillPath(&whiteBr, &fpOuter);
        // Foreground colour fill — lower bucket half + drop
        GraphicsPath fpColor(FillModeWinding);
        AddSvgPath(fpColor, fillColorD, fScale);
        SolidBrush fgBr(Color(255, GetRValue(g_fgColor), GetGValue(g_fgColor), GetBValue(g_fgColor)));
        fg.FillPath(&fgBr, &fpColor);
        // Black stroke — all subpaths for outline and inner detail
        GraphicsPath fpFull(FillModeWinding);
        AddSvgPath(fpFull, fillFullD, fScale);
        Pen blackPen(Color(255, 0, 0, 0), 1.0f);
        fg.DrawPath(&blackPen, &fpFull);
    }
    BITMAPV5HEADER fbmi = {};
    fbmi.bV5Size = sizeof(fbmi); fbmi.bV5Width = fsz; fbmi.bV5Height = -fsz;
    fbmi.bV5Planes = 1; fbmi.bV5BitCount = 32; fbmi.bV5Compression = BI_BITFIELDS;
    fbmi.bV5RedMask = 0x00FF0000; fbmi.bV5GreenMask = 0x0000FF00;
    fbmi.bV5BlueMask = 0x000000FF; fbmi.bV5AlphaMask = 0xFF000000;
    void* fpv = nullptr;
    HDC fhdc = GetDC(NULL);
    HBITMAP fhbmC = CreateDIBSection(fhdc, (BITMAPINFO*)&fbmi, DIB_RGB_COLORS, &fpv, NULL, 0);
    ReleaseDC(NULL, fhdc);
    if (fhbmC && fpv) {
        BitmapData fbd; Rect frc(0, 0, fsz, fsz);
        fbmp.LockBits(&frc, ImageLockModeRead, PixelFormat32bppARGB, &fbd);
        BYTE* src = (BYTE*)fbd.Scan0; BYTE* dst = (BYTE*)fpv;
        for (int i = 0; i < fsz*fsz*4; i += 4) {
            BYTE a = src[i+3];
            dst[i+0]=(BYTE)((int)src[i+0]*a/255);
            dst[i+1]=(BYTE)((int)src[i+1]*a/255);
            dst[i+2]=(BYTE)((int)src[i+2]*a/255);
            dst[i+3]=a;
        }
        fbmp.UnlockBits(&fbd);
    }
    HBITMAP fhbmM = CreateBitmap(fsz, fsz, 1, 1, NULL);
    HDC fhdcM = CreateCompatibleDC(NULL);
    HBITMAP fhOld = (HBITMAP)SelectObject(fhdcM, fhbmM);
    RECT fmr = {0,0,fsz,fsz}; FillRect(fhdcM, &fmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SelectObject(fhdcM, fhOld); DeleteDC(fhdcM);
    // Hotspot at the tip of the drop (bottom of the teardrop)
    ICONINFO fii = {}; fii.fIcon = FALSE; fii.xHotspot = 25; fii.yHotspot = 29;
    fii.hbmMask = fhbmM; fii.hbmColor = fhbmC;
    g_hCurFill = (HCURSOR)CreateIconIndirect(&fii);
    DeleteObject(fhbmM); if (fhbmC) DeleteObject(fhbmC);
}

static void RecreateBrushCursors()
{
    if (!g_hwndMain) return;
    for (int i = 0; i < BRUSH_COUNT; i++) {
        if (g_hCurBrush[i]) { DestroyCursor(g_hCurBrush[i]); g_hCurBrush[i] = NULL; }
    }
    struct BrushCursorDef {
        const char* outerD;
        const char* tipD;    // filled fg color; NULL = none
        const char* fullD;
        float coordSz;
        int hotX, hotY;
        bool strokeOnly;
        BYTE outerFgBlend;   // 0=white fill, 1-255=blend fg into white (255=full fg)
    };
    static const char* kRoundOuter =
        "M10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17"
        "C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3Z";
    static const char* kRoundFull =
        "M10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17"
        "C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3Z"
        "M2 10C2 5.58172 5.58172 2 10 2C14.4183 2 18 5.58172 18 10"
        "C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10Z";
    // Calligraphy cursor paths from calligraphy_brush.svg (coords ~29-1002; coordSz 1024 → 24px)
    static const char* kCalligBodyD =  // path 1: pen body → filled white
        "M348.596,604.309 830.20829,64.215005"
        "c31.51663,-35.343667 93.81847,-23.060479 124.98992,4.586787"
        "27.60598,24.484898 46.87249,89.358608 11.46696,120.403138"
        "L417.398,670.817"
        "c-10.62816,-25.74673 -35.2774,-51.83393 -68.802,-66.508z";
    static const char* kCalligTipD =   // path 2: brush tip → filled fg color
        "M220.54763,617.85771"
        "c38.07589,-31.9548 87.03891,-34.00561 128.04837,-13.54871"
        "30.25341,15.09142 59.75112,36.58885 68.802,66.508"
        "20.36842,67.33112 7.57313,131.35468 -33.46673,177.7238"
        "C295.14639,948.8549 162.40004,968.67841 29.190128,987.59933"
        "101.16928,958.49339 169.37111,894.6818 171.08658,777.59258"
        "c0.9762,-66.62991 7.90095,-124.85598 49.46105,-159.73487z";
    static const char* kCalligD =      // all paths → black outline
        "M348.596,604.309 830.20829,64.215005"
        "c31.51663,-35.343667 93.81847,-23.060479 124.98992,4.586787"
        "27.60598,24.484898 46.87249,89.358608 11.46696,120.403138"
        "L417.398,670.817"
        "c-10.62816,-25.74673 -35.2774,-51.83393 -68.802,-66.508z"
        "M220.54763,617.85771"
        "c38.07589,-31.9548 87.03891,-34.00561 128.04837,-13.54871"
        "30.25341,15.09142 59.75112,36.58885 68.802,66.508"
        "20.36842,67.33112 7.57313,131.35468 -33.46673,177.7238"
        "C295.14639,948.8549 162.40004,968.67841 29.190128,987.59933"
        "101.16928,958.49339 169.37111,894.6818 171.08658,777.59258"
        "c0.9762,-66.62991 7.90095,-124.85598 49.46105,-159.73487z";
    static const char* kRoundTipD = kRoundOuter;  // whole disc = fg color
    static const char* kAirbrushD =
        "M9.5 4C9.776 4 10 3.776 10 3.5C10 3.224 9.776 3 9.5 3C9.224 3 9 3.224 9 3.5C9 3.776 9.224 4 9.5 4Z"
        "M11 2.5C11 2.224 11.224 2 11.5 2C11.776 2 12 2.224 12 2.5C12 2.776 11.776 3 11.5 3C11.224 3 11 2.776 11 2.5Z"
        "M11 4.5C11 4.224 11.224 4 11.5 4C11.776 4 12 4.224 12 4.5C12 4.776 11.776 5 11.5 5C11.224 5 11 4.776 11 4.5Z"
        "M13 1.5C13 1.224 13.224 1 13.5 1C13.776 1 14 1.224 14 1.5C14 1.776 13.776 2 13.5 2C13.224 2 13 1.776 13 1.5Z"
        "M13 3.5C13 3.224 13.224 3 13.5 3C13.776 3 14 3.224 14 3.5C14 3.776 13.776 4 13.5 4C13.224 4 13 3.776 13 3.5Z"
        "M13 5.5C13 5.224 13.224 5 13.5 5C13.776 5 14 5.224 14 5.5C14 5.776 13.776 6 13.5 6C13.224 6 13 5.776 13 5.5Z"
        "M9.975 6.372C9.981 6.394 9.985 6.413 9.988 6.436C9.98924 6.4451 9.99068 6.45368 9.99311 6.46225"
        "C9.99656 6.4744 10 6.48651 10 6.5V13.25C10 14.216 9.216 15 8.25 15H4.75C3.784 15 3 14.216 3 13.25"
        "V6.5C3 6.48651 3.00344 6.4744 3.00689 6.46225C3.00932 6.45368 3.01176 6.4451 3.013 6.436"
        "C3.016 6.413 3.02 6.394 3.026 6.372C3.05 6.286 3.086 6.207 3.147 6.146L5.001 4.293V2.5"
        "C5.001 2.224 5.225 2 5.501 2H7.5C7.776 2 8 2.224 8 2.5V4.292L9.854 6.146"
        "C9.915 6.207 9.952 6.287 9.975 6.372Z"
        "M7.001 3H6.001V4H7.001V3Z"
        "M7.294 5H5.708L4.708 6H8.294L7.294 5Z"
        "M8.251 14C8.665 14 9.001 13.664 9.001 13.25V7H4.001V13.25C4.001 13.664 4.337 14 4.751 14H8.251Z";
    // Marker cursor paths from marker.svg (viewBox 0-34; group transform applied to abs coords)
    static const char* kMarkerBodyD =  // paths 1+2: barrel + grip → filled white
        "M8.081973,18.76509l19.08188,-16.75355"
        "c0.73506,-0.64537 1.52003,-0.48784 1.91238,-0.0831"
        "l3.94946,4.07419"
        "c0.54909,0.56644 0.42553,1.26969 -0.16629,1.95394"
        "L16.354973,27.03909Z"
        "M8.081973,18.76509l8.273,8.274 -3.11819,3.24232"
        "c-2.54969,-1.01438 -4.558723,-0.82884 -6.11081,0.37368"
        "l-2.786,-2.66"
        "c1.099875,-1.5035 1.81694,-3.592 0.498993,-6.19464z";
    static const char* kMarkerD =      // all 3 paths → black outline
        "M8.081973,18.76509l19.08188,-16.75355"
        "c0.73506,-0.64537 1.52003,-0.48784 1.91238,-0.0831"
        "l3.94946,4.07419"
        "c0.54909,0.56644 0.42553,1.26969 -0.16629,1.95394"
        "L16.354973,27.03909Z"
        "M8.081973,18.76509l8.273,8.274 -3.11819,3.24232"
        "c-2.54969,-1.01438 -4.558723,-0.82884 -6.11081,0.37368"
        "l-2.786,-2.66"
        "c1.099875,-1.5035 1.81694,-3.592 0.498993,-6.19464z"
        "M0.764776,31.44547 4.339973,27.99509"
        "l2.786,2.66 -1.912848,1.87129z";
    static const char* kAirbrushOuterD =   // can body outer contour only (for white fill; inner detail subpaths are CCW and would punch holes)
        "M9.975 6.372C9.981 6.394 9.985 6.413 9.988 6.436C9.98924 6.4451 9.99068 6.45368 9.99311 6.46225"
        "C9.99656 6.4744 10 6.48651 10 6.5V13.25C10 14.216 9.216 15 8.25 15H4.75C3.784 15 3 14.216 3 13.25"
        "V6.5C3 6.48651 3.00344 6.4744 3.00689 6.46225C3.00932 6.45368 3.01176 6.4451 3.013 6.436"
        "C3.016 6.413 3.02 6.394 3.026 6.372C3.05 6.286 3.086 6.207 3.147 6.146L5.001 4.293V2.5"
        "C5.001 2.224 5.225 2 5.501 2H7.5C7.776 2 8 2.224 8 2.5V4.292L9.854 6.146"
        "C9.915 6.207 9.952 6.287 9.975 6.372Z";
    static const char* kAirbrushTipD =   // spray dots = fg color
        "M9.5 4C9.776 4 10 3.776 10 3.5C10 3.224 9.776 3 9.5 3C9.224 3 9 3.224 9 3.5C9 3.776 9.224 4 9.5 4Z"
        "M11 2.5C11 2.224 11.224 2 11.5 2C11.776 2 12 2.224 12 2.5C12 2.776 11.776 3 11.5 3C11.224 3 11 2.776 11 2.5Z"
        "M11 4.5C11 4.224 11.224 4 11.5 4C11.776 4 12 4.224 12 4.5C12 4.776 11.776 5 11.5 5C11.224 5 11 4.776 11 4.5Z"
        "M13 1.5C13 1.224 13.224 1 13.5 1C13.776 1 14 1.224 14 1.5C14 1.776 13.776 2 13.5 2C13.224 2 13 1.776 13 1.5Z"
        "M13 3.5C13 3.224 13.224 3 13.5 3C13.776 3 14 3.224 14 3.5C14 3.776 13.776 4 13.5 4C13.224 4 13 3.776 13 3.5Z"
        "M13 5.5C13 5.224 13.224 5 13.5 5C13.776 5 14 5.224 14 5.5C14 5.776 13.776 6 13.5 6C13.224 6 13 5.776 13 5.5Z";
    static const char* kMarkerTipD =   // path 3: nib → filled fg color
        "M0.764776,31.44547 4.339973,27.99509"
        "l2.786,2.66 -1.912848,1.87129z";
    // Crayon cursor paths from crayon.svg (viewBox coords ~250-900; coordSz 1000 → 24px)
    static const char* kCrayonBodyD =  // path 1: main body → filled white
        "M368.09,541.241 677.98302,170.66893"
        "c8.52732,-9.14849 28.96779,-12.74641 41.12022,-2.31558"
        "l158.58073,132.61428"
        "c6.85144,6.14182 8.98609,19.68002 1.09664,27.98803"
        "L563.028,705.218Z";
    static const char* kCrayonTipD =   // paths 2+3: collar + tip → filled fg color
        "M301.63764,620.98322 368.09,541.241 563.028,705.218 493.32646,787.6821"
        "c-7.14576,8.10743 -21.74017,5.92628 -26.49246,1.35385"
        "L301.28858,648.95906"
        "c-8.02573,-7.7517 -6.2332,-20.98998 0.34906,-27.97584z"
        "M250.92383,832.16582 317.57134,663.19601 449.97404,774.1533 287.14911,862.72121Z";
    static const char* kCrayonD =      // all 3 paths → black outline
        "M368.09,541.241 677.98302,170.66893"
        "c8.52732,-9.14849 28.96779,-12.74641 41.12022,-2.31558"
        "l158.58073,132.61428"
        "c6.85144,6.14182 8.98609,19.68002 1.09664,27.98803"
        "L563.028,705.218Z"
        "M301.63764,620.98322 368.09,541.241 563.028,705.218 493.32646,787.6821"
        "c-7.14576,8.10743 -21.74017,5.92628 -26.49246,1.35385"
        "L301.28858,648.95906"
        "c-8.02573,-7.7517 -6.2332,-20.98998 0.34906,-27.97584z"
        "M250.92383,832.16582 317.57134,663.19601 449.97404,774.1533 287.14911,862.72121Z";
    static const char* kOilD =
        "m 8.701,10.054 c 1.55576,-1.9748837 4.517049,-5.9148881 8.231555,-7.9374307"
        " 0.79921,-0.4351685 1.841804,0.0048 1.293459,1.1758718"
        " C 16.578856,6.8102046 14.039646,9.4716048 10.965,12.111"
        " 11.05214,11.097396 9.8474097,9.9906905 8.701,10.054 Z"
        "M 6.0073487,14.010218 C 6.7970108,12.649905 7.6342376,11.414313 8.701,10.054"
        " c 1.2770287,0.02284 2.162993,0.871655 2.264,2.057"
        " C 9.7129891,13.200043 8.4497388,14.153127 7.0882557,15.132699"
        " 6.9416934,14.63823 6.583428,14.234492 6.0073487,14.010218 Z"
        "M 2.2033874,18.105193 C 2.7586121,17.517989 3.1868836,17.075397 3.4713746,16.379899"
        " 3.7657063,15.660342 3.716033,15.048017 4.074188,14.613032"
        " 4.7429538,13.800805 5.3719059,13.89656 6.0489221,14.051791"
        " c 0.4919039,0.112788 0.9052619,0.61516 1.0393337,1.101694"
        " 0.21699,0.787436 -0.1024805,1.509112 -0.6443868,2.120241"
        " -0.5241445,0.591097 -1.3395783,0.772625 -2.1202409,0.893827"
        " -0.693192,0.107623 -2.1202407,-0.06236 -2.1202407,-0.06236 z";
    static const char* kOilTipD =
        "M 2.2033874,18.105193 C 2.7586121,17.517989 3.1868836,17.075397 3.4713746,16.379899"
        " 3.7657063,15.660342 3.716033,15.048017 4.074188,14.613032"
        " 4.7429538,13.800805 5.3719059,13.89656 6.0489221,14.051791"
        " c 0.4919039,0.112788 0.9052619,0.61516 1.0393337,1.101694"
        " 0.21699,0.787436 -0.1024805,1.509112 -0.6443868,2.120241"
        " -0.5241445,0.591097 -1.3395783,0.772625 -2.1202409,0.893827"
        " -0.693192,0.107623 -2.1202407,-0.06236 -2.1202407,-0.06236 z";
    static const BrushCursorDef kDefs[BRUSH_COUNT] = {
        { kRoundOuter,     kRoundTipD,    kRoundFull,   20.0f, 12, 12, false,   0 },
        { kCalligBodyD,    kCalligTipD,   kCalligD,   1024.0f,  1, 23, false,   0 },
        { kAirbrushOuterD, kAirbrushTipD, kAirbrushD,   16.0f,  5, 19, false,   0 },
        { kMarkerBodyD,    kMarkerTipD,   kMarkerD,     34.0f,  1, 22, false,   0 },
        { kCrayonBodyD,    kCrayonTipD,   kCrayonD,   1000.0f,  6, 20, false, 100 }, // ~40% fg → faded crayon body
        { kOilD,           kOilTipD,      kOilD,        20.0f,  4, 20, false,   0 },
    };
    const int bsz = 24;
    Color fgCol(255, GetRValue(g_fgColor), GetGValue(g_fgColor), GetBValue(g_fgColor));
    for (int bi = 0; bi < BRUSH_COUNT; ++bi) {
        const auto& def = kDefs[bi];
        float scale = (float)bsz / def.coordSz;
        Bitmap bbmp(bsz, bsz, PixelFormat32bppARGB);
        {
            Graphics bg(&bbmp);
            bg.SetSmoothingMode(SmoothingModeAntiAlias);
            bg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            bg.Clear(Color(0, 0, 0, 0));
            GraphicsPath bpOuter(FillModeWinding);
            AddSvgPath(bpOuter, def.outerD, scale);
            if (!def.strokeOnly) {
                Color outerClr(255, 255, 255, 255);
                if (def.outerFgBlend) {
                    int t = def.outerFgBlend;
                    outerClr = Color(255,
                        (BYTE)(255 + t * ((int)GetRValue(g_fgColor) - 255) / 255),
                        (BYTE)(255 + t * ((int)GetGValue(g_fgColor) - 255) / 255),
                        (BYTE)(255 + t * ((int)GetBValue(g_fgColor) - 255) / 255));
                }
                SolidBrush outerBr(outerClr);
                bg.FillPath(&outerBr, &bpOuter);
            }
            if (def.tipD) {
                GraphicsPath bpTip(FillModeWinding);
                AddSvgPath(bpTip, def.tipD, scale);
                SolidBrush tipBr(fgCol);
                bg.FillPath(&tipBr, &bpTip);
            }
            GraphicsPath bpFull(FillModeWinding);
            AddSvgPath(bpFull, def.fullD, scale);
            Pen blackPen(Color(255, 0, 0, 0), 1.0f);
            bg.DrawPath(&blackPen, &bpFull);
        }
        BITMAPV5HEADER bbmi = {};
        bbmi.bV5Size = sizeof(bbmi); bbmi.bV5Width = bsz; bbmi.bV5Height = -bsz;
        bbmi.bV5Planes = 1; bbmi.bV5BitCount = 32; bbmi.bV5Compression = BI_BITFIELDS;
        bbmi.bV5RedMask = 0x00FF0000; bbmi.bV5GreenMask = 0x0000FF00;
        bbmi.bV5BlueMask = 0x000000FF; bbmi.bV5AlphaMask = 0xFF000000;
        void* bpv = nullptr;
        HDC bhdc = GetDC(NULL);
        HBITMAP bhbmC = CreateDIBSection(bhdc, (BITMAPINFO*)&bbmi, DIB_RGB_COLORS, &bpv, NULL, 0);
        ReleaseDC(NULL, bhdc);
        if (bhbmC && bpv) {
            BitmapData bbd; Rect brc(0, 0, bsz, bsz);
            bbmp.LockBits(&brc, ImageLockModeRead, PixelFormat32bppARGB, &bbd);
            BYTE* src = (BYTE*)bbd.Scan0; BYTE* dst = (BYTE*)bpv;
            for (int i = 0; i < bsz*bsz*4; i += 4) {
                BYTE a = src[i+3];
                dst[i+0]=(BYTE)((int)src[i+0]*a/255);
                dst[i+1]=(BYTE)((int)src[i+1]*a/255);
                dst[i+2]=(BYTE)((int)src[i+2]*a/255);
                dst[i+3]=a;
            }
            bbmp.UnlockBits(&bbd);
        }
        HBITMAP bhbmM = CreateBitmap(bsz, bsz, 1, 1, NULL);
        HDC bhdcM = CreateCompatibleDC(NULL);
        HBITMAP bhOld = (HBITMAP)SelectObject(bhdcM, bhbmM);
        RECT bmr = {0,0,bsz,bsz}; FillRect(bhdcM, &bmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(bhdcM, bhOld); DeleteDC(bhdcM);
        ICONINFO bii = {};
        bii.fIcon    = FALSE;
        bii.xHotspot = (DWORD)def.hotX;
        bii.yHotspot = (DWORD)def.hotY;
        bii.hbmMask  = bhbmM;
        bii.hbmColor = bhbmC;
        g_hCurBrush[bi] = (HCURSOR)CreateIconIndirect(&bii);
        DeleteObject(bhbmM); if (bhbmC) DeleteObject(bhbmC);
    }
    g_lastBrushCursorColor = g_fgColor;  // mark which color these cursors were built for
    // Force immediate cursor refresh if the brush cursor is currently displayed
    if (g_currentTool == TOOL_BRUSH) {
        HCURSOR hc = g_hCurBrush[g_brushType];
        SetCursor(hc ? hc : LoadCursor(NULL, IDC_CROSS));
    }
}

// ============================================================
//  Zoom
// ============================================================
static const float s_zoomSteps[] = {
    0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f,
    1.00f, 1.10f, 1.20f, 1.30f, 1.40f, 1.50f, 1.60f, 1.70f, 1.80f, 1.90f,
    2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f, 6.00f, 7.00f, 8.00f,
    9.00f, 10.0f
};

static void RebuildSvgDisplayBitmap()
{
    delete g_svgDisplayBitmap; g_svgDisplayBitmap = nullptr;
    if (g_isSvgFile && g_currentFile[0] != L'\0')
        g_svgDisplayBitmap = LoadSvgToBitmap(g_currentFile, g_zoom * g_svgCanvasScale);
}

static void ApplyZoom(float newZoom, int pivotX, int pivotY)
{
    newZoom = max(0.10f, min(10.0f, newZoom));
    if (!g_hwndCanvas || !g_pCanvas) { g_zoom = newZoom; return; }
    RECT rc; GetClientRect(g_hwndCanvas, &rc);
    float cliW = (float)rc.right, cliH = (float)rc.bottom;
    // Anchor: use supplied pivot (mouse pos) or viewport centre
    float anchorX = (pivotX >= 0) ? (float)pivotX : cliW / 2.0f;
    float anchorY = (pivotY >= 0) ? (float)pivotY : cliH / 2.0f;
    // Canvas coordinate under the anchor before zoom change
    float canX = (anchorX - g_canvasOffX) / g_zoom;
    float canY = (anchorY - g_canvasOffY) / g_zoom;
    g_zoom = newZoom;
    RebuildSvgDisplayBitmap();
    int scaledW = max(1, (int)(g_pCanvas->GetWidth()  * g_zoom));
    int scaledH = max(1, (int)(g_pCanvas->GetHeight() * g_zoom));
    if (cliW - (float)scaledW >= 2.0f * SCROLL_PAD)
        g_scrollX = 0;
    else
        g_scrollX = max(0, (int)(SCROLL_PAD - anchorX + canX * g_zoom));
    if (cliH - (float)scaledH >= 2.0f * SCROLL_PAD)
        g_scrollY = 0;
    else
        g_scrollY = max(0, (int)(SCROLL_PAD - anchorY + canY * g_zoom));
    UpdateScrollBars();
    UpdateZoomEdit();
    InvalidateCanvasView();
}

static void ZoomIn(int pivotX, int pivotY)
{
    for (float s : s_zoomSteps)
        if (g_zoom < s - 0.001f) { ApplyZoom(s, pivotX, pivotY); return; }
}

static void ZoomOut(int pivotX, int pivotY)
{
    for (int i = (int)(sizeof(s_zoomSteps)/sizeof(s_zoomSteps[0])) - 1; i >= 0; --i)
        if (g_zoom > s_zoomSteps[i] + 0.001f) { ApplyZoom(s_zoomSteps[i], pivotX, pivotY); return; }
}

static void ZoomToFit()
{
    if (!g_hwndCanvas || !g_pCanvas) return;
    RECT rc; GetClientRect(g_hwndCanvas, &rc);
    int vpW = rc.right, vpH = rc.bottom;
    int imgW = (int)g_pCanvas->GetWidth();
    int imgH = (int)g_pCanvas->GetHeight();
    if (imgW <= 0 || imgH <= 0 || vpW <= 0 || vpH <= 0) return;
    float fitZoom = min((float)(vpW - 8) / imgW, (float)(vpH - 8) / imgH);
    // Shrink to fit but never enlarge: a small image shows at its real size
    // (nearest-neighbour zoom would only make it blocky). SVGs re-render
    // sharply at any size, so they still grow to fill the window.
    if (!g_isSvgFile) fitZoom = min(fitZoom, 1.0f);
    fitZoom = max(0.10f, min(10.0f, fitZoom));
    g_zoom = fitZoom;
    RebuildSvgDisplayBitmap();
    int scaledW = max(1, (int)(imgW * g_zoom));
    int scaledH = max(1, (int)(imgH * g_zoom));
    // Center the image in the viewport
    g_scrollX = max(0, SCROLL_PAD - (vpW - scaledW) / 2);
    g_scrollY = max(0, SCROLL_PAD - (vpH - scaledH) / 2);
    UpdateScrollBars();
    UpdateZoomEdit();
    InvalidateCanvasView();
    g_zoomFitMode = true;
    if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
}

static void ZoomToCover()
{
    if (!g_hwndCanvas || !g_pCanvas) return;
    RECT rc; GetClientRect(g_hwndCanvas, &rc);
    int vpW = rc.right, vpH = rc.bottom;
    int imgW = (int)g_pCanvas->GetWidth();
    int imgH = (int)g_pCanvas->GetHeight();
    if (imgW <= 0 || imgH <= 0 || vpW <= 0 || vpH <= 0) return;
    // Cover: scale so image fills viewport (both axes), crop any overhang
    float coverZoom = max((float)vpW / imgW, (float)vpH / imgH);
    coverZoom = max(0.10f, min(10.0f, coverZoom));
    g_zoom = coverZoom;
    RebuildSvgDisplayBitmap();
    int scaledW = max(1, (int)(imgW * g_zoom));
    int scaledH = max(1, (int)(imgH * g_zoom));
    // Center the image (excess hangs off the edge and is scrollable)
    g_scrollX = max(0, SCROLL_PAD - (vpW - scaledW) / 2);
    g_scrollY = max(0, SCROLL_PAD - (vpH - scaledH) / 2);
    UpdateScrollBars();
    UpdateZoomEdit();
    InvalidateCanvasView();
    g_zoomFitMode = false;
    if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
}

// ============================================================
//  Slide panel helpers
// ============================================================

static bool IsImageExtension(const wchar_t* ext)
{
    static const wchar_t* k[] = {
        L"png", L"jpg", L"jpeg", L"bmp", L"gif",
        L"tif", L"tiff", L"webp", L"svg",
        L"heic", L"heif", L"hif", L"avif"   // need Windows' Store codecs
    };
    for (auto e : k) if (_wcsicmp(ext, e) == 0) return true;
    return false;
}

static void FreeFolderThumbs()
{
    for (auto& e : g_folderEntries) { delete e.thumb; e.thumb = nullptr; }
    for (auto& e : g_folderHiddenEntries) delete e.thumb;
    g_folderEntries.clear();
    g_folderHiddenEntries.clear();
    g_folderCurrentIdx   = -1;
    g_folderScrollX = 0;
}

static void BuildFolderList()
{
    // Keep the old entries so thumbnails can be reused below: this runs on
    // every image switch, and the folder usually hasn't changed.
    std::vector<FolderEntry> old;
    old.swap(g_folderEntries);
    old.insert(old.end(), g_folderHiddenEntries.begin(), g_folderHiddenEntries.end());
    g_folderHiddenEntries.clear();
    g_folderCurrentIdx   = -1;
    g_folderScrollX = 0;

    wchar_t dir[MAX_PATH] = {};
    if (g_currentFile[0]) {
        StringCchCopy(dir, MAX_PATH, g_currentFile);
        PathRemoveFileSpecW(dir);
    } else {
        GetCurrentDirectoryW(MAX_PATH, dir);
    }

    wchar_t pattern[MAX_PATH];
    StringCchCopy(pattern, MAX_PATH, dir);
    PathAppendW(pattern, L"*.*");

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            const wchar_t* dot = wcsrchr(fd.cFileName, L'.');
            if (!dot || !IsImageExtension(dot + 1)) continue;
            wchar_t full[MAX_PATH];
            StringCchCopy(full, MAX_PATH, dir);
            PathAppendW(full, fd.cFileName);
            g_folderEntries.push_back({ full, nullptr, false, fd.ftLastWriteTime });
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    auto byPath = [](const FolderEntry& a, const FolderEntry& b) {
        return _wcsicmp(a.path.c_str(), b.path.c_str()) < 0;
    };
    std::sort(g_folderEntries.begin(), g_folderEntries.end(), byPath);
    std::sort(old.begin(), old.end(), byPath);

    // Carry over thumbnails for files that are unchanged since they were made
    // (old is sorted the same way); edited or new files get a fresh one.
    for (auto& e : g_folderEntries) {
        auto it = std::lower_bound(old.begin(), old.end(), e, byPath);
        if (it != old.end() && _wcsicmp(it->path.c_str(), e.path.c_str()) == 0 &&
            CompareFileTime(&it->modified, &e.modified) == 0) {
            e.thumb  = it->thumb;  it->thumb = nullptr;
            e.failed = it->failed;
            e.stale  = it->stale;
        }
    }
    for (auto& e : old) delete e.thumb;

    // The search keeps only names containing its text; the rest are set aside
    // with their thumbnails, so they are back at once when the text changes
    g_folderTotalCount = (int)g_folderEntries.size();
    if (!g_folderFilter.empty()) {
        std::vector<FolderEntry> shown;
        for (auto& e : g_folderEntries)
            (StrStrIW(PathFindFileNameW(e.path.c_str()), g_folderFilter.c_str()) ? shown : g_folderHiddenEntries).push_back(e);
        g_folderEntries.swap(shown);
    }
    if (g_folderSearchOpen && g_hwndFolderSearch) InvalidateRect(g_hwndFolderSearch, NULL, FALSE);   // its count

    if (g_currentFile[0]) {
        for (int i = 0; i < (int)g_folderEntries.size(); i++) {
            if (_wcsicmp(g_folderEntries[i].path.c_str(), g_currentFile) == 0) {
                g_folderCurrentIdx = i;
                break;
            }
        }
    }

    CenterFolderBrowserOnCurrent();
}

// Folder strip layout. A list wider than the panel wraps around endlessly,
// with a FOLDER_WRAP_GAP_W gap marking where the last file meets the first;
// g_folderScrollX is then kept in [0, period). A list that fits is drawn
// once, centered, and does not scroll.
static const int FOLDER_WRAP_GAP_W = 16;

// Strip geometry for a panel panelH tall. Cells widen as the strip grows so
// thumbnails keep their shape (112 is the narrowest that fits a file name).
static int  FolderImgAreaH(int panelH) { return panelH - FOLDER_LABEL_H - 4; }
static int  FolderCellWFor(int panelH) { return max(112, FolderImgAreaH(panelH) * 14 / 15); }
static int  FolderThumbSizeFor(int panelH) { return min(FolderCellWFor(panelH) - 12, FolderImgAreaH(panelH) - 4); }

// Limits a strip height to FOLDER_BROWSER_MIN_H .. half the screen, leaving
// the canvas some room in a small window.
static int ClampFolderBrowserH(int h)
{
    int maxH = 600;
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(g_hwndMain, MONITOR_DEFAULTTONEAREST), &mi))
        maxH = (mi.rcWork.bottom - mi.rcWork.top) / 2;
    if (g_hwndMain) {
        RECT rc; GetClientRect(g_hwndMain, &rc);
        int statusH = 0;
        if (g_hwndStatus) { RECT sr; GetWindowRect(g_hwndStatus, &sr); statusH = sr.bottom - sr.top; }
        maxH = min(maxH, (int)rc.bottom - TOOLBAR_HEIGHT - COLORSTRIP_H - statusH - 80);
    }
    return max(FOLDER_BROWSER_MIN_H, min(h, maxH));
}

static int  FolderStripPeriod() { return (int)g_folderEntries.size() * g_folderCellW + FOLDER_WRAP_GAP_W; }
static bool FolderStripWraps(int panelW) { return (int)g_folderEntries.size() * g_folderCellW > panelW; }

static int WrapFolderScroll(int x)
{
    int p = FolderStripPeriod();
    return ((x % p) + p) % p;
}

// Calls f(i, cx) for every visible cell, where cx is the cell's left edge
// in client coordinates; i == -1 is the wrap gap.
template <typename F>
static void ForEachVisibleFolderCell(int panelW, F f)
{
    int n = (int)g_folderEntries.size();
    if (n == 0) return;
    if (!FolderStripWraps(panelW)) {
        int ox = (panelW - n * g_folderCellW) / 2;
        for (int i = 0; i < n; i++) f(i, ox + i * g_folderCellW);
        return;
    }
    for (int ox = -WrapFolderScroll(g_folderScrollX); ox < panelW; ox += FolderStripPeriod()) {
        for (int i = 0; i < n; i++) {
            int cx = ox + i * g_folderCellW;
            if (cx + g_folderCellW > 0 && cx < panelW) f(i, cx);
        }
        int gx = ox + n * g_folderCellW;
        if (gx + FOLDER_WRAP_GAP_W > 0 && gx < panelW) f(-1, gx);
    }
}

// Index of the file under client x, or -1 for the wrap gap / empty space.
static int FolderHitTest(int x, int panelW)
{
    int hit = -1;
    ForEachVisibleFolderCell(panelW, [&](int i, int cx) {
        if (i >= 0 && x >= cx && x < cx + g_folderCellW) hit = i;
    });
    return hit;
}

// Scroll the folder browser so the current entry is centered.
static void CenterFolderBrowserOnCurrent()
{
    if (g_folderCurrentIdx < 0 || !g_hwndFolderBrowser) return;
    RECT rc; GetClientRect(g_hwndFolderBrowser, &rc);
    int panelW   = rc.right;
    if (!FolderStripWraps(panelW)) { g_folderScrollX = 0; return; }
    int cellMid  = g_folderCurrentIdx * g_folderCellW + g_folderCellW / 2;
    g_folderScrollX = WrapFolderScroll(cellMid - panelW / 2);
}

static Bitmap* MakeFolderThumb(const wchar_t* path)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    Bitmap* src = nullptr;
    if (dot && _wcsicmp(dot+1, L"webp") == 0) {
        src = LoadWebP(path);
    } else if (dot && _wcsicmp(dot+1, L"svg") == 0) {
        // Render SVG at small size directly to avoid large rasterization
        HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD sz = GetFileSize(hFile, nullptr);
            std::vector<char> buf(sz + 1, '\0');
            DWORD rd = 0;
            ReadFile(hFile, buf.data(), sz, &rd, nullptr);
            CloseHandle(hFile);
            NSVGimage* img = nsvgParse(buf.data(), "px", 96.0f);
            if (img) {
                float svgW = max(1.0f, img->width), svgH = max(1.0f, img->height);
                float sc = min((float)g_folderThumbMax / svgW, (float)g_folderThumbMax / svgH);
                if (sc > 1.0f) sc = 1.0f;
                int tw = max(1, (int)(svgW * sc)), th = max(1, (int)(svgH * sc));
                NSVGrasterizer* rast = nsvgCreateRasterizer();
                if (rast) {
                    std::vector<unsigned char> px(tw * th * 4, 0);
                    nsvgRasterize(rast, img, 0, 0, sc, px.data(), tw, th, tw * 4);
                    nsvgDeleteRasterizer(rast);
                    for (int i = 0; i < tw * th; i++) std::swap(px[i*4+0], px[i*4+2]);
                    src = new Bitmap(tw, th, PixelFormat32bppARGB);
                    Rect r(0,0,tw,th); BitmapData bd;
                    if (src->LockBits(&r, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                        for (int y = 0; y < th; y++)
                            memcpy((BYTE*)bd.Scan0 + y*bd.Stride, px.data()+y*tw*4, tw*4);
                        src->UnlockBits(&bd);
                    }
                }
                nsvgDelete(img);
            }
        }
        return src; // already thumbnail-sized, return directly
    } else {
        src = Bitmap::FromFile(path);
        if (src && src->GetLastStatus() != Ok) { delete src; src = nullptr; }
        ApplyExifOrientation(src, path);
        if (!src) src = LoadBitmapViaWic(path);   // HEIC, AVIF and others GDI+ can't read
    }
    if (!src) return nullptr;

    int sw = (int)src->GetWidth(), sh = (int)src->GetHeight();
    if (sw <= 0 || sh <= 0) { delete src; return nullptr; }
    float scale = min((float)g_folderThumbMax / sw, (float)g_folderThumbMax / sh);
    if (scale > 1.0f) scale = 1.0f;
    int tw = max(1, (int)(sw * scale)), th = max(1, (int)(sh * scale));
    Bitmap* thumb = new Bitmap(tw, th, PixelFormat32bppARGB);
    {
        Graphics g(thumb);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(src, 0, 0, tw, th);
    }
    delete src;
    return thumb;
}

static void NavigateToFile(int idx)
{
    if (idx < 0 || idx >= (int)g_folderEntries.size()) return;
    // Already open, or already loading: the highlight moves at the start of a load
    if (idx == g_folderCurrentIdx) return;
    if (!PromptSaveIfNeeded()) return;
    // Move the highlight now; FinishImageLoad zooms the new image and puts
    // the highlight back if the load fails.
    g_folderCurrentIdx = idx;
    CenterFolderBrowserOnCurrent();
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, FALSE);
    BeginImageLoad(g_folderEntries[idx].path.c_str(), true);
}

// Sends a file to the Recycle Bin. False if it failed or the user cancelled.
static bool RecycleFile(const wchar_t* path)
{
    // pFrom is a list of paths ending in a double null. Without FOF_NOCONFIRMATION
    // Windows asks first if the user has turned on delete confirmation, or if
    // the file can't go to the Recycle Bin and would be deleted for good.
    wchar_t from[MAX_PATH + 1] = {};
    StringCchCopy(from, MAX_PATH, path);
    SHFILEOPSTRUCTW op = {};
    op.hwnd   = g_hwndMain;
    op.wFunc  = FO_DELETE;
    op.pFrom  = from;
    op.fFlags = FOF_ALLOWUNDO | FOF_SILENT;
    if (SHFileOperationW(&op) != 0 || op.fAnyOperationsAborted) {
        if (!op.fAnyOperationsAborted)
            MessageBox(g_hwndMain, L"Could not delete this file.", APP_TITLE, MB_ICONERROR);
        return false;
    }
    return true;
}

// Moves the open file to the Recycle Bin and opens the next image in its
// folder, or a blank canvas when none is left.
static void DoDeleteFile()
{
    if (!g_currentFile[0] || g_imageLoading) return;
    if (g_modified &&
        MessageBox(g_hwndMain, L"This image has unsaved changes. Delete the file anyway?",
                   APP_TITLE, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;

    if (!RecycleFile(g_currentFile)) return;
    g_modified = false;

    // The deleted file's place in the rescanned list is where the next one sits
    std::wstring deleted = g_currentFile;
    BuildFolderList();
    int n = (int)g_folderEntries.size();
    if (n == 0 && !g_folderFilter.empty() && g_folderTotalCount > 0) {
        // That was the last match: show the whole folder again rather than a blank image
        g_folderFilter.clear();
        if (g_hwndFolderSearchEdit) SetWindowTextW(g_hwndFolderSearchEdit, L"");
        BuildFolderList();
        n = (int)g_folderEntries.size();
    }
    if (n == 0) {
        DoNew();
        if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
        return;
    }
    int idx = 0;
    while (idx < n && _wcsicmp(g_folderEntries[idx].path.c_str(), deleted.c_str()) < 0) idx++;
    if (idx == n) idx = 0;
    g_folderCurrentIdx = idx;
    CenterFolderBrowserOnCurrent();
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
    BeginImageLoad(g_folderEntries[idx].path.c_str(), g_showFolderBrowser);
}

// True for a WebP whose extended header has the animation flag set
static bool IsAnimatedWebP(const wchar_t* path)
{
    BYTE hdr[21] = {};
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") != 0 || !f) return false;
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    return n == sizeof(hdr) && memcmp(hdr + 12, "VP8X", 4) == 0 && (hdr[20] & 0x02);
}

// Formats that can be rotated and written back without losing anything else:
// SVG would need its markup rewritten, and saving a GIF or an animated WebP
// keeps only one frame.
static bool CanRotateFileInPlace(const wchar_t* path)
{
    static const wchar_t* k[] = { L"png", L"jpg", L"jpeg", L"bmp", L"tif", L"tiff", L"webp" };
    const wchar_t* dot = wcsrchr(path, L'.');
    if (!dot) return false;
    for (auto e : k)
        if (_wcsicmp(dot + 1, e) == 0)
            return _wcsicmp(e, L"webp") != 0 || !IsAnimatedWebP(path);
    return false;
}

// Rotates an image file by 90° and saves it over itself, via a temporary file
// so a failure leaves the original alone. JPEGs are rotated without
// re-encoding when their size allows it (GDI+ needs whole JPEG blocks).
static bool RotateImageFileOnDisk(const wchar_t* path, bool clockwise)
{
    if (!CanRotateFileInPlace(path)) return false;
    std::wstring tmp = std::wstring(path) + L".rotating";
    const wchar_t* dot = wcsrchr(path, L'.');
    bool isWebP = _wcsicmp(dot + 1, L"webp") == 0;
    bool isJpeg = _wcsicmp(dot + 1, L"jpg") == 0 || _wcsicmp(dot + 1, L"jpeg") == 0;
    RotateFlipType rot = clockwise ? Rotate90FlipNone : Rotate270FlipNone;
    bool ok = false;
    if (isWebP) {
        if (Bitmap* bmp = LoadWebP(path)) {
            ok = bmp->RotateFlip(rot) == Ok && SaveWebPBitmap(bmp, tmp.c_str());
            delete bmp;
        }
    } else {
        Image* img = Image::FromFile(path);   // keeps the file open until deleted
        CLSID enc;
        if (img && img->GetLastStatus() == Ok && GetSaveEncoder(path, enc)) {
            // The stored pixels turn but the Orientation tag is kept, and is
            // applied after them. Plain turns still come out right; a mirroring
            // tag must swap axes (flip horizontal <-> vertical, 5 <-> 7).
            if (UsesExifOrientation(path)) {
                static const USHORT kSwapped[9] = { 1, 1, 4, 3, 2, 7, 6, 5, 8 };
                int o = GetExifOrientation(img);
                if (kSwapped[o] != o) {
                    USHORT v = kSwapped[o];
                    PropertyItem item;
                    item.id = PropertyTagOrientation; item.length = sizeof(v);
                    item.type = PropertyTagTypeShort; item.value = &v;
                    img->SetPropertyItem(&item);
                }
            }
            EncoderParameters ep;
            ep.Count = 1;
            ep.Parameter[0].NumberOfValues = 1;
            ep.Parameter[0].Type = EncoderParameterValueTypeLong;
            ULONG value;
            if (isJpeg) {
                value = clockwise ? EncoderValueTransformRotate90 : EncoderValueTransformRotate270;
                ep.Parameter[0].Guid  = EncoderTransformation;
                ep.Parameter[0].Value = &value;
                ok = img->Save(tmp.c_str(), &enc, &ep) == Ok;
            }
            if (!ok && img->RotateFlip(rot) == Ok) {
                if (isJpeg) {
                    value = 95;
                    ep.Parameter[0].Guid  = EncoderQuality;
                    ep.Parameter[0].Value = &value;
                    ok = img->Save(tmp.c_str(), &enc, &ep) == Ok;
                } else {
                    ok = img->Save(tmp.c_str(), &enc, NULL) == Ok;
                }
            }
        }
        delete img;
    }
    if (ok) ok = MoveFileExW(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING) != 0;
    if (!ok) DeleteFileW(tmp.c_str());
    return ok;
}

// Rotates a strip file on disk; the open image is reloaded to show it.
static void RotateFolderFile(const std::wstring& path, bool clockwise)
{
    bool isCur = _wcsicmp(path.c_str(), g_currentFile) == 0;
    if (isCur && !PromptSaveIfNeeded()) return;
    if (!RotateImageFileOnDisk(path.c_str(), clockwise)) {
        MessageBox(g_hwndMain, L"Could not rotate this file.", APP_TITLE, MB_ICONERROR);
        return;
    }
    BuildFolderList();   // the new modified time gives the file a fresh thumbnail
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
    if (isCur) BeginImageLoad(path.c_str(), true);
}

// Deletes any file in the folder strip; the open file moves on to the next one.
static void DeleteFolderFile(const std::wstring& path)
{
    if (_wcsicmp(path.c_str(), g_currentFile) == 0) { DoDeleteFile(); return; }
    if (!RecycleFile(path.c_str())) return;
    BuildFolderList();
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
}

static void ToggleFolderBrowser()
{
    g_showFolderBrowser = !g_showFolderBrowser;
    if (g_showFolderBrowser) {
        g_toolBeforeFolderBrowser = g_currentTool;
        g_zoomBeforeFolderBrowser = g_zoom;
        g_fitModeBeforeFolderBrowser = g_zoomFitMode;
        BuildFolderList();
        SetTool(TOOL_HAND);
    } else {
        if (g_folderSearchOpen) {
            g_folderSearchOpen = false;
            g_folderFilter.clear();
            SetWindowTextW(g_hwndFolderSearchEdit, L"");
            ShowWindow(g_hwndFolderSearch, SW_HIDE);
        }
        FreeFolderThumbs();
        SetTool(g_toolBeforeFolderBrowser);
    }
    if (g_hwndFolderBrowserBtn) InvalidateRect(g_hwndFolderBrowserBtn, NULL, FALSE);
    if (g_hwndMain) {
        // Only the canvas and strip move: a full LayoutChildren would also redo
        // the status bar and its zoom controls, which flickers. The strip is
        // placed before it is shown, and hidden before the canvas grows over it.
        if (!g_showFolderBrowser && g_hwndFolderBrowser) ShowWindow(g_hwndFolderBrowser, SW_HIDE);
        LayoutCanvasAndFolderBrowser(g_hwndMain);  // canvas resized first so ZoomToFit/ApplyZoom sees correct viewport
        if (g_showFolderBrowser && g_hwndFolderBrowser) ShowWindow(g_hwndFolderBrowser, SW_SHOW);
        if (g_showFolderBrowser || g_fitModeBeforeFolderBrowser)
            ZoomToFit();
        else {
            g_zoomFitMode = false;
            if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
            ApplyZoom(g_zoomBeforeFolderBrowser);
            CenterScrollOnCanvas();
        }
        InvalidateCanvas();
    }
    if (g_showFolderBrowser && g_hwndFolderBrowser)
        SetFocus(g_hwndFolderBrowser);
    else if (g_hwndCanvas)
        SetFocus(g_hwndCanvas);
}

// ============================================================
//  Tool selection
// ============================================================
static const int TOOL_COMMANDS[TOOL_COUNT] = {
    ID_TOOL_PENCIL,
    ID_TOOL_BRUSH,
    ID_TOOL_ERASER,
    ID_TOOL_FILL,
    ID_TOOL_LINE,
    ID_TOOL_RECT,
    ID_TOOL_ELLIPSE,
    ID_TOOL_TEXT,
    ID_TOOL_SELECT,
    ID_TOOL_RRECT,
    ID_TOOL_TRIANGLE,
    ID_TOOL_ARROW_L,
    ID_TOOL_ARROW_R,
    ID_TOOL_ARROW_U,
    ID_TOOL_ARROW_D,
    ID_TOOL_ARROW,
    ID_TOOL_STICKMAN,
    ID_TOOL_SPEECH,
    ID_TOOL_COMPUTER,
    ID_TOOL_STAR,
    ID_TOOL_HEART,
    ID_TOOL_LIGHTNING,
    ID_TOOL_MUSTACHE,
    ID_TOOL_DIAMOND,
    ID_TOOL_PENTAGON,
    ID_TOOL_HEXAGON_H,
    ID_TOOL_HEXAGON_V,
    ID_TOOL_OCTAGON,
    ID_TOOL_SPIRAL,
    ID_TOOL_SPIRAL_R,
    ID_TOOL_DROPPER,
    ID_TOOL_WAND,
    ID_TOOL_LASSO,
    ID_TOOL_SMUDGE,
    ID_TOOL_CROSS,
    ID_TOOL_XMARK,
    ID_TOOL_CHECK,
    ID_TOOL_HAND,   // no toolbar button; activated programmatically
};

// Ordered right-to-left in toolbar (first = rightmost → overflows first)
static const UINT kOverflowCmds[] = {
    ID_TOOL_ARROW, ID_TOOL_ARROW_D, ID_TOOL_ARROW_U, ID_TOOL_ARROW_R, ID_TOOL_ARROW_L,
    ID_TOOL_TRIANGLE, ID_TOOL_RRECT,
    ID_TOOL_ELLIPSE, ID_TOOL_RECT, ID_TOOL_LINE
};
static const wchar_t* const kOverflowNames[] = {
    L"Arrow", L"Down Arrow", L"Up Arrow", L"Right Arrow", L"Left Arrow",
    L"Triangle", L"Rounded Rectangle",
    L"Ellipse", L"Rectangle", L"Line"
};
static const int kOverflowCount = 10;

// Tools permanently accessible only via the More dropdown (never on toolbar)
static const UINT kMorePermanentCmds[] = {
    ID_TOOL_DIAMOND, ID_TOOL_PENTAGON, ID_TOOL_HEXAGON_H, ID_TOOL_HEXAGON_V, ID_TOOL_OCTAGON,
    ID_TOOL_STICKMAN, ID_TOOL_SPEECH, ID_TOOL_COMPUTER, ID_TOOL_STAR,
    ID_TOOL_HEART, ID_TOOL_LIGHTNING, ID_TOOL_MUSTACHE, ID_TOOL_SPIRAL, ID_TOOL_SPIRAL_R,
    ID_TOOL_CROSS, ID_TOOL_XMARK, ID_TOOL_CHECK
};
static const wchar_t* const kMorePermanentNames[] = {
    L"Diamond", L"Pentagon", L"Hexagon (flat top/bottom)", L"Hexagon (flat left/right)", L"Octagon",
    L"Stick Person", L"Speech Bubble", L"Computer", L"Star",
    L"Heart", L"Lightning", L"Mustache && Glasses", L"Spiral", L"Reverse Spiral",
    L"Plus / Cross", L"X Mark", L"Checkmark"
};
static const int kMorePermanentCount = 17;

// Tools permanently accessible only via the Tools dropdown (never on toolbar)
static const UINT kToolsPermanentCmds[]            = { ID_TOOL_LASSO, ID_TOOL_SMUDGE };
static const wchar_t* const kToolsPermanentNames[] = { L"Lasso", L"Smudge" };
static const int kToolsPermanentImgIdx[]           = { 32, 43 };  // 43 = smudge finger icon
static const int kToolsPermanentCount              = 2;

// Image-list indices parallel to the command arrays above (for menu item bitmaps)
static const int kOverflowImgIdx[]      = {15, 14, 13, 12, 11, 10, 9, 6, 5, 4};   // arrow..line
static const int kMorePermanentImgIdx[] = {35, 36, 37, 38, 39, 16, 17, 18, 19, 20, 21, 22, 33, 34, 40, 41, 42};  // diamond..check

// Basic-tool overflow: right-to-left (select is rightmost → overflows first)
static const UINT kToolsOverflowCmds[] = {
    ID_TOOL_SELECT, ID_TOOL_TEXT, ID_TOOL_WAND,
    ID_TOOL_FILL, ID_TOOL_DROPPER, ID_TOOL_ERASER, ID_TOOL_BRUSH, ID_TOOL_PENCIL
};
static const wchar_t* const kToolsOverflowNames[] = {
    L"Select", L"Text", L"Magic Wand",
    L"Fill", L"Color Picker", L"Eraser", L"Brush", L"Pencil"
};
static const int kToolsOverflowImgIdx[] = {8, 7, 31, 3, 23, 2, 1, 0};
static const int kToolsOverflowCount = 8;

static void SetTool(Tool t)
{
    // Commit any in-progress bezier, shape, text, paste or floating selection when switching tools
    if (g_textInline) CommitTextInline();
    if (g_shapeEditing) CommitShape();
    if (g_bezEditing) CommitBezier();
    if (g_isPasting && t != TOOL_SELECT) CommitPaste();
    if (g_selFloating && t != TOOL_SELECT) CommitSelectionFloat();
    g_currentTool = t;
    for (int i = 0; i < TOOL_COUNT; ++i) {
        SendMessage(g_hwndToolbar, TB_CHECKBUTTON,
            TOOL_COMMANDS[i], MAKELONG((i == (int)t) ? TRUE : FALSE, 0));
    }
    // Check the More button when the active tool lives in the More dropdown
    if (g_hwndToolbar) {
        bool inMore = false;
        for (int i = 0; i < kMorePermanentCount && !inMore; i++)
            if (TOOL_COMMANDS[(int)t] == kMorePermanentCmds[i]) inMore = true;
        for (int i = 0; i < g_numOverflowed && !inMore; i++)
            if (TOOL_COMMANDS[(int)t] == kOverflowCmds[i]) inMore = true;
        SendMessage(g_hwndToolbar, TB_CHECKBUTTON, ID_MENU_MORE, MAKELONG(inMore, 0));
    }
    if (g_hwndToolbar) {
        bool inTools = false;
        for (int i = 0; i < g_numToolsOverflowed && !inTools; i++)
            if (TOOL_COMMANDS[(int)t] == kToolsOverflowCmds[i]) inTools = true;
        for (int i = 0; i < kToolsPermanentCount && !inTools; i++)
            if (TOOL_COMMANDS[(int)t] == kToolsPermanentCmds[i]) inTools = true;
        SendMessage(g_hwndToolbar, TB_CHECKBUTTON, ID_MENU_TOOLS, MAKELONG(inTools, 0));
    }
    if (t != TOOL_ERASER) g_eraserCurVis = false;
    if (t != TOOL_SMUDGE) g_smudgeCurVis = false;
    UpdateLWSectionForTool();
}

// Edit controls repaint on every WM_SETTEXT, even with identical text,
// so skip the call when nothing changed to avoid flicker.
static void SetEditTextIfChanged(HWND hwnd, const wchar_t* text)
{
    wchar_t cur[64];
    GetWindowText(hwnd, cur, ARRAYSIZE(cur));
    if (wcscmp(cur, text) != 0) SetWindowText(hwnd, text);
}

static void UpdateBrushSizeEdit()
{
    if (!g_hwndBrushSz) return;
    wchar_t buf[16];
    if (g_brushSize == floorf(g_brushSize))
        StringCchPrintf(buf, 16, L"%d", (int)g_brushSize);
    else
        StringCchPrintf(buf, 16, L"%.1f", g_brushSize);
    SetWindowText(g_hwndBrushSz, buf);
}

static void UpdateLWSectionForTool()
{
    bool eraser = (g_currentTool == TOOL_ERASER);
    bool smudge = (g_currentTool == TOOL_SMUDGE);
    bool text   = (g_currentTool == TOOL_TEXT);
    bool wand   = (g_currentTool == TOOL_WAND);
    bool fill   = (g_currentTool == TOOL_FILL);
    if (g_hwndBrushSz) {
        if (text) {
            ShowWindow(g_hwndBrushSz, SW_HIDE);
        } else {
            ShowWindow(g_hwndBrushSz, SW_SHOW);
            wchar_t buf[16];
            if (eraser)
                StringCchPrintf(buf, 16, L"%d", g_eraserSize);
            else if (smudge)
                StringCchPrintf(buf, 16, L"%d", g_smudgeSize);
            else if (wand)
                StringCchPrintf(buf, 16, L"%d", g_wandTolerance);
            else if (fill)
                StringCchPrintf(buf, 16, L"%d", g_fillTolerance);
            else if (g_brushSize == floorf(g_brushSize))
                StringCchPrintf(buf, 16, L"%d", (int)g_brushSize);
            else
                StringCchPrintf(buf, 16, L"%.1f", g_brushSize);
            SetEditTextIfChanged(g_hwndBrushSz, buf);
        }
    }
    if (g_hwndSzMinus)   ShowWindow(g_hwndSzMinus,   text ? SW_HIDE : SW_SHOW);
    if (g_hwndSzPlus)    ShowWindow(g_hwndSzPlus,    text ? SW_HIDE : SW_SHOW);
    if (g_hwndEraserSq)  ShowWindow(g_hwndEraserSq,  eraser ? SW_SHOW : SW_HIDE);
    if (g_hwndEraserRd)  ShowWindow(g_hwndEraserRd,  eraser ? SW_SHOW : SW_HIDE);
    if (g_hwndSmudgeSq)  ShowWindow(g_hwndSmudgeSq,  smudge ? SW_SHOW : SW_HIDE);
    if (g_hwndSmudgeRd)  ShowWindow(g_hwndSmudgeRd,  smudge ? SW_SHOW : SW_HIDE);
    if (g_hwndWandContig) ShowWindow(g_hwndWandContig, wand ? SW_SHOW : SW_HIDE);
    if (g_hwndAlphaEdit) ShowWindow(g_hwndAlphaEdit,  wand ? SW_HIDE : SW_SHOW);
    if (g_hwndFontFace)  ShowWindow(g_hwndFontFace,  text ? SW_SHOW : SW_HIDE);
    if (g_hwndFontSize)  ShowWindow(g_hwndFontSize,  text ? SW_SHOW : SW_HIDE);
    if (g_hwndFontBold)  ShowWindow(g_hwndFontBold,  text ? SW_SHOW : SW_HIDE);
    if (g_hwndFontItal)  ShowWindow(g_hwndFontItal,  text ? SW_SHOW : SW_HIDE);
    if (g_hwndFontUnder) ShowWindow(g_hwndFontUnder, text ? SW_SHOW : SW_HIDE);
    if (g_hwndMain) {
        int xShift = g_showCustomColors ? 0 : (CS_CUSTOM_X - CS_ALPHA_SEP_X);
        int rightEdge = text ? CS_TF_SEP_R_X :
                        wand ? CS_WD_SEP_R_X :
                               CS_LW_SEP_R_X;
        // Invalidate the whole section including the alpha area (may appear/disappear)
        RECT rc = { CS_ALPHA_SEP_X + xShift - 2, TOOLBAR_HEIGHT,
                    rightEdge + xShift + 4, TOOLBAR_HEIGHT + COLORSTRIP_H };
        InvalidateRect(g_hwndMain, &rc, FALSE);
    }
}

static void UpdateZoomEdit()
{
    if (!g_hwndZoomEdit) return;
    g_updatingZoomEdit = true;
    wchar_t buf[16];
    float pct = g_zoom * 100.0f;
    if (pct == floorf(pct))
        StringCchPrintf(buf, 16, L"%d", (int)pct);
    else
        StringCchPrintf(buf, 16, L"%.1f", pct);
    SetEditTextIfChanged(g_hwndZoomEdit, buf);
    g_updatingZoomEdit = false;
}

static void UpdateAlphaEdit()
{
    if (!g_hwndAlphaEdit) return;
    g_updatingAlphaEdit = true;
    wchar_t buf[8];
    StringCchPrintf(buf, 8, L"%d", g_strokeActive ? g_fgAlpha : g_bgAlpha);
    SetWindowText(g_hwndAlphaEdit, buf);
    g_updatingAlphaEdit = false;
}

static void ClampAlphaEdit()
{
    if (!g_hwndAlphaEdit) return;
    wchar_t buf[8];
    GetWindowText(g_hwndAlphaEdit, buf, 8);
    int v = _wtoi(buf);
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    if (g_strokeActive) g_fgAlpha = v; else g_bgAlpha = v;
    UpdateAlphaEdit();
    RefreshEditingPreview();
    if (g_hwndMain) InvalidateRect(g_hwndMain, NULL, FALSE);
}

static LRESULT CALLBACK AlphaEditSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        ClampAlphaEdit();
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// SVG paths for button icons that must be recoloured on dark-mode toggle
static const char* kColorEditD =
    "M9.75003 6.5C10.1642 6.5 10.5 6.16421 10.5 5.75C10.5 5.33579 10.1642 5 9.75003 5C9.33582 5 9.00003 5.33579"
    " 9.00003 5.75C9.00003 6.16421 9.33582 6.5 9.75003 6.5ZM12.75 7.5C13.1642 7.5 13.5 7.16421 13.5 6.75C13.5"
    " 6.33579 13.1642 6 12.75 6C12.3358 6 12 6.33579 12 6.75C12 7.16421 12.3358 7.5 12.75 7.5ZM15.25 9C15.25"
    " 9.41421 14.9142 9.75 14.5 9.75C14.0858 9.75 13.75 9.41421 13.75 9C13.75 8.58579 14.0858 8.25 14.5 8.25"
    "C14.9142 8.25 15.25 8.58579 15.25 9ZM14.5 12.75C14.9142 12.75 15.25 12.4142 15.25 12C15.25 11.5858 14.9142"
    " 11.25 14.5 11.25C14.0858 11.25 13.75 11.5858 13.75 12C13.75 12.4142 14.0858 12.75 14.5 12.75ZM13.25 14"
    "C13.25 14.4142 12.9142 14.75 12.5 14.75C12.0858 14.75 11.75 14.4142 11.75 14C11.75 13.5858 12.0858 13.25"
    " 12.5 13.25C12.9142 13.25 13.25 13.5858 13.25 14ZM13.6972 2.99169C10.9426 1.57663 8.1432 1.7124 5.77007"
    " 3.16636C4.55909 3.9083 3.25331 5.46925 2.51605 7.05899C2.14542 7.85816 1.89915 8.70492 1.90238 9.49318"
    "C1.90566 10.2941 2.16983 11.0587 2.84039 11.6053C3.45058 12.1026 3.98165 12.353 4.49574 12.3784C5.01375"
    " 12.404 5.41804 12.1942 5.73429 12.0076C5.80382 11.9666 5.86891 11.927 5.93113 11.8892C6.17332 11.7421"
    " 6.37205 11.6214 6.62049 11.5426C6.90191 11.4534 7.2582 11.4205 7.77579 11.5787C7.96661 11.637 8.08161"
    " 11.7235 8.16212 11.8229C8.24792 11.9289 8.31662 12.0774 8.36788 12.2886C8.41955 12.5016 8.44767 12.7527"
    " 8.46868 13.0491C8.47651 13.1594 8.48379 13.2855 8.49142 13.4176C8.50252 13.6098 8.51437 13.8149 8.52974"
    " 14.0037C8.58435 14.6744 8.69971 15.4401 9.10362 16.1357C9.51764 16.8488 10.2047 17.439 11.307 17.8158"
    "C12.9093 18.3636 14.3731 17.9191 15.5126 17.0169C16.6391 16.125 17.4691 14.7761 17.8842 13.4272C19.1991"
    " 9.15377 17.6728 5.03394 13.6972 2.99169ZM6.29249 4.01905C8.35686 2.75426 10.7844 2.61959 13.2403 3.88119"
    "C16.7473 5.68275 18.1135 9.28161 16.9284 13.1332C16.5624 14.3227 15.8338 15.4871 14.8919 16.2329C13.963"
    " 16.9684 12.8486 17.286 11.6305 16.8696C10.7269 16.5607 10.2467 16.1129 9.96842 15.6336C9.68001 15.1369"
    " 9.57799 14.5556 9.52644 13.9225C9.51101 13.733 9.50132 13.5621 9.49147 13.3884C9.48399 13.2564 9.47642"
    " 13.1229 9.46618 12.9783C9.44424 12.669 9.41175 12.3499 9.33968 12.0529C9.26719 11.7541 9.14902 11.4527"
    " 8.93935 11.1937C8.72439 10.9282 8.43532 10.7346 8.06801 10.6223C7.36648 10.408 6.80266 10.4359 6.31839"
    " 10.5893C5.94331 10.7082 5.62016 10.9061 5.37179 11.0582C5.31992 11.0899 5.2713 11.1197 5.22616 11.1463"
    "C4.94094 11.3146 4.75357 11.39 4.54514 11.3796C4.33279 11.3691 4.00262 11.2625 3.47218 10.8301C3.0866"
    " 10.5158 2.90473 10.0668 2.90237 9.48908C2.89995 8.89865 3.08843 8.20165 3.42324 7.47971C4.09686 6.0272"
    " 5.28471 4.63649 6.29249 4.01905Z";

static const char* kZoomOutD =
    "M11 8C11.2761 8 11.5 8.22386 11.5 8.5C11.5 8.77614 11.2761 9 11 9H6C5.72386 9 5.5 8.77614 5.5 8.5"
    "C5.5 8.22386 5.72386 8 6 8H11ZM8.5 2C12.0899 2 15 4.91015 15 8.5C15 10.115 14.4114 11.5927 13.4365"
    " 12.7295L16.8535 16.1465C17.0487 16.3417 17.0487 16.6583 16.8535 16.8535C16.68 17.0271 16.4107"
    " 17.0461 16.2158 16.9111L16.1465 16.8535L12.7295 13.4365C11.5927 14.4114 10.115 15 8.5 15C4.91015"
    " 15 2 12.0899 2 8.5C2 4.91015 4.91015 2 8.5 2ZM8.5 3C5.46243 3 3 5.46243 3 8.5C3 11.5376 5.46243"
    " 14 8.5 14C9.83877 14 11.0658 13.5216 12.0195 12.7266C12.276 12.5128 12.5128 12.276 12.7266"
    " 12.0195C13.5216 11.0658 14 9.83877 14 8.5C14 5.46243 11.5376 3 8.5 3Z";

// Heart outline SVG (Inkscape, openclipart, public domain).
// Group transform translate(-175.32265,-1696.4765) pre-applied to start point;
// relative 'c' segments are translation-invariant. ViewBox: 241.59736 x 220.05746.
static const char* kHeartD =
    "m 68.12978,12.5021 "
    "c -26.9341,0.2312 -51.58009,21.4767 -55.08178,48.2939 "
    "-3.11346,23.844 7.32949,47.3995 23.96855,64.2142 "
    "27.5148,27.8054 61.22631,49.7939 83.44686,82.5473 "
    "4.39089,-4.6889 9.27818,-12.0655 14.22742,-17.529 "
    "25.22951,-27.8509 58.1653,-48.0133 80.86454,-78.1545 "
    "17.17546,-22.8065 19.10279,-58.1138 -0.53802,-80.4051 "
    "-18.24975,-20.7125 -51.76012,-25.1712 -74.36972,-9.2543 "
    "-8.22594,5.791 -15.27469,13.3707 -19.93251,22.3123 "
    "-10.05314,-19.3195 -30.53412,-32.2142 -52.58534,-32.0248 z";
static const float kHeartVW = 241.59736f;
static const float kHeartVH = 220.05746f;

static const char* kZoomInD =
    "M8.5 5.5C8.77614 5.5 9 5.72386 9 6V8H11C11.2761 8 11.5 8.22386 11.5 8.5C11.5 8.77614 11.2761 9 11"
    " 9H9V11C9 11.2761 8.77614 11.5 8.5 11.5C8.22386 11.5 8 11.2761 8 11V9H6C5.72386 9 5.5 8.77614 5.5"
    " 8.5C5.5 8.22386 5.72386 8 6 8H8V6C8 5.72386 8.22386 5.5 8.5 5.5ZM8.5 2C12.0899 2 15 4.91015 15"
    " 8.5C15 10.115 14.4114 11.5927 13.4365 12.7295L16.8535 16.1465C17.0487 16.3417 17.0487 16.6583"
    " 16.8535 16.8535C16.68 17.0271 16.4107 17.0461 16.2158 16.9111L16.1465 16.8535L12.7295 13.4365"
    "C11.5927 14.4114 10.115 15 8.5 15C4.91015 15 2 12.0899 2 8.5C2 4.91015 4.91015 2 8.5 2ZM8.5 3"
    "C5.46243 3 3 5.46243 3 8.5C3 11.5376 5.46243 14 8.5 14C9.83877 14 11.0658 13.5216 12.0195"
    " 12.7266C12.276 12.5128 12.5128 12.276 12.7266 12.0195C13.5216 11.0658 14 9.83877 14 8.5C14"
    " 5.46243 11.5376 3 8.5 3Z";

// ic_fluent_slide_size_20_regular — used for zoom-fit button when in fit mode (click -> 1:1)
static const char* kFolderSizeD =
    "M2 6.75C2 5.23122 3.23122 4 4.75 4H15.25C16.7688 4 18 5.23122 18 6.75V13.25C18 14.7688 16.7688 16"
    " 15.25 16H4.75C3.23122 16 2 14.7688 2 13.25V6.75ZM4.75 5C3.7835 5 3 5.7835 3 6.75V13.25C3 14.2165"
    " 3.7835 15 4.75 15H15.25C16.2165 15 17 14.2165 17 13.25V6.75C17 5.7835 16.2165 5 15.25 5H4.75ZM11"
    " 6.5C11 6.22386 11.2239 6 11.5 6H13.5C13.7761 6 14 6.22386 14 6.5V8.5C14 8.77614 13.7761 9 13.5 9"
    "C13.2239 9 13 8.77614 13 8.5V7.70711L11.3536 9.35355C11.1583 9.54882 10.8417 9.54882 10.6464"
    " 9.35355C10.4512 9.15829 10.4512 8.84171 10.6464 8.64645L12.2929 7H11.5C11.2239 7 11 6.77614 11"
    " 6.5ZM8.64645 10.6464C8.84171 10.4512 9.15829 10.4512 9.35355 10.6464C9.54882 10.8417 9.54882"
    " 11.1583 9.35355 11.3536L7.70711 13H8.5C8.77614 13 9 13.2239 9 13.5C9 13.7761 8.77614 14 8.5 14H6.5"
    "C6.36739 14 6.24021 13.9473 6.14645 13.8536C6.05268 13.7598 6 13.6326 6 13.5L6 11.5C6 11.2239"
    " 6.22386 11 6.5 11C6.77614 11 7 11.2239 7 11.5V12.2929L8.64645 10.6464Z";

// ic_fluent_page_fit_20_regular — used for zoom-fit button when NOT in fit mode (click -> fit)
static const char* kPageFitD =
    "M5.5 6C4.67157 6 4 6.67157 4 7.5V8.5C4 8.77614 4.22386 9 4.5 9C4.77614 9 5 8.77614 5 8.5V7.5"
    "C5 7.22386 5.22386 7 5.5 7H6.5C6.77614 7 7 6.77614 7 6.5C7 6.22386 6.77614 6 6.5 6H5.5ZM13.5 6"
    "C13.2239 6 13 6.22386 13 6.5C13 6.77614 13.2239 7 13.5 7H14.5C14.7761 7 15 7.22386 15 7.5V8.5"
    "C15 8.77614 15.2239 9 15.5 9C15.7761 9 16 8.77614 16 8.5V7.5C16 6.67157 15.3284 6 14.5 6H13.5Z"
    "M5 11.5C5 11.2239 4.77614 11 4.5 11C4.22386 11 4 11.2239 4 11.5V12.5C4 13.3284 4.67157 14 5.5 14"
    "H6.5C6.77614 14 7 13.7761 7 13.5C7 13.2239 6.77614 13 6.5 13H5.5C5.22386 13 5 12.7761 5 12.5V11.5Z"
    "M16 11.5C16 11.2239 15.7761 11 15.5 11C15.2239 11 15 11.2239 15 11.5V12.5C15 12.7761 14.7761 13"
    " 14.5 13H13.5C13.2239 13 13 13.2239 13 13.5C13 13.7761 13.2239 14 13.5 14H14.5C15.3284 14 16"
    " 13.3284 16 12.5V11.5ZM2 6C2 4.89543 2.89543 4 4 4H16C17.1046 4 18 4.89543 18 6V14C18 15.1046"
    " 17.1046 16 16 16H4C2.89543 16 2 15.1046 2 14V6ZM3 6V14C3 14.5523 3.44772 15 4 15H16C16.5523 15"
    " 17 14.5523 17 14V6C17 5.44772 16.5523 5 16 5H4C3.44772 5 3 5.44772 3 6Z";

// ic_fluent_slide_transition_20_regular — folder browser toggle button
static const char* kFolderBrowserIconD =
    "M4.75 4C3.23122 4 2 5.23122 2 6.75V13.25C2 14.7688 3.23122 16 4.75 16H15.25C16.7688 16 18"
    " 14.7688 18 13.25V6.75C18 5.23122 16.7688 4 15.25 4H4.75ZM12 5V15H9V5H12ZM13 15V5H15.25"
    "C16.2165 5 17 5.7835 17 6.75V13.25C17 14.2165 16.2165 15 15.25 15H13ZM8 15H6V5H8V15ZM5 5V15"
    "H4.75C3.7835 15 3 14.2165 3 13.25V6.75C3 5.7835 3.7835 5 4.75 5H5Z";

// Fluent Icons (20×20 viewBox) for status bar panes
static const char* kStatusBorderD =   // ic_fluent_border_none_20_regular
    "M8 3.5C8 3.22386 8.22386 3 8.5 3H11.5C11.7761 3 12 3.22386 12 3.5C12 3.77614 11.7761 4 11.5 4H8.5"
    "C8.22386 4 8 3.77614 8 3.5Z"
    "M5.53929 4.05336C4.80477 4.22654 4.2266 4.80468 4.05337 5.53918C4.05203 5.56013 4.04935 5.58119"
    " 4.04525 5.60227C3.99256 5.87334 3.72424 6.05433 3.45317 6.00164C3.1821 5.94895 3.00112 5.68063"
    " 3.05381 5.40956C3.05857 5.38505 3.06509 5.36128 3.07323 5.33836C3.3266 4.21278 4.21265 3.3267"
    " 5.33821 3.07328C5.36118 3.06512 5.385 3.05858 5.40956 3.05381C5.68063 3.00112 5.94895 3.1821"
    " 6.00164 3.45317C6.05433 3.72424 5.87334 3.99256 5.60227 4.04525C5.58123 4.04934 5.5602 4.05202"
    " 5.53929 4.05336Z"
    "M5.53917 15.9466C4.80472 15.7734 4.22659 15.1953 4.05337 14.4609C4.05203 14.4399 4.04935 14.4188"
    " 4.04525 14.3977C3.99256 14.1267 3.72424 13.9457 3.45317 13.9984C3.1821 14.0511 3.00112 14.3194"
    " 3.05381 14.5905C3.05857 14.615 3.06509 14.6387 3.07321 14.6616C3.32659 15.7873 4.21272 16.6734"
    " 5.33837 16.9268C5.36129 16.9349 5.38506 16.9414 5.40956 16.9462C5.68063 16.9989 5.94895 16.8179"
    " 6.00164 16.5468C6.05433 16.2758 5.87334 16.0075 5.60227 15.9548C5.58119 15.9507 5.56012 15.948"
    " 5.53917 15.9466Z"
    "M14.5905 3.05381C14.615 3.05858 14.6388 3.06512 14.6618 3.07328C15.7874 3.3267 16.6734 4.21277"
    " 16.9268 5.33832C16.9349 5.36126 16.9414 5.38504 16.9462 5.40956C16.9989 5.68063 16.8179 5.94895"
    " 16.5468 6.00164C16.2758 6.05433 16.0075 5.87334 15.9548 5.60227C15.9507 5.5812 15.948 5.56015"
    " 15.9466 5.53921C15.7734 4.8047 15.1952 4.22654 14.4607 4.05336C14.4398 4.05202 14.4188 4.04934"
    " 14.3977 4.04525C14.1267 3.99256 13.9457 3.72424 13.9984 3.45317C14.0511 3.1821 14.3194 3.00112"
    " 14.5905 3.05381Z"
    "M14.4608 15.9466C15.1953 15.7734 15.7734 15.1953 15.9466 14.4608C15.948 14.4399 15.9507 14.4188"
    " 15.9548 14.3977C16.0075 14.1267 16.2758 13.9457 16.5468 13.9984C16.8179 14.0511 16.9989 14.3194"
    " 16.9462 14.5905C16.9414 14.615 16.9349 14.6387 16.9268 14.6617C16.6734 15.7873 15.7873 16.6734"
    " 14.6617 16.9268C14.6387 16.9349 14.615 16.9414 14.5905 16.9462C14.3194 16.9989 14.0511 16.8179"
    " 13.9984 16.5468C13.9457 16.2758 14.1267 16.0075 14.3977 15.9548C14.4188 15.9507 14.4399 15.948"
    " 14.4608 15.9466Z"
    "M16 11.5C16 11.7761 16.2239 12 16.5 12C16.7761 12 17 11.7761 17 11.5V8.5C17 8.22386 16.7761 8"
    " 16.5 8C16.2239 8 16 8.22386 16 8.5V11.5Z"
    "M3.5 12C3.22386 12 3 11.7761 3 11.5V8.5C3 8.22386 3.22386 8 3.5 8C3.77614 8 4 8.22386 4 8.5V11.5"
    "C4 11.7761 3.77614 12 3.5 12Z"
    "M8.5 16C8.22386 16 8 16.2239 8 16.5C8 16.7761 8.22386 17 8.5 17H11.5C11.7761 17 12 16.7761 12"
    " 16.5C12 16.2239 11.7761 16 11.5 16H8.5Z";

static const char* kStatusCursorD =   // ic_fluent_cursor_20_regular
    "M5 3.05854C5 2.21347 5.98325 1.74939 6.63564 2.28655L17.6418 11.3487C18.3661 11.9451 17.9444"
    " 13.1207 17.0061 13.1207H11.4142C10.9788 13.1207 10.5648 13.3099 10.2799 13.6392L6.75622 17.7117"
    "C6.15025 18.412 5 17.9835 5 17.0574L5 3.05854Z"
    "M17.0061 12.1207L6 3.05854L6 17.0574L9.52369 12.9849C9.99856 12.4361 10.6885 12.1207 11.4142"
    " 12.1207H17.0061Z";

static const char* kStatusCropD =     // ic_fluent_crop_20_regular
    "M6 2.5C6 2.22386 5.77614 2 5.5 2C5.22386 2 5 2.22386 5 2.5V5H2.5C2.22386 5 2 5.22386 2 5.5"
    "C2 5.77614 2.22386 6 2.5 6H5V12.5C5 13.8807 6.11929 15 7.5 15H14V17.5C14 17.7761 14.2239 18"
    " 14.5 18C14.7761 18 15 17.7761 15 17.5V15H17.5C17.7761 15 18 14.7761 18 14.5C18 14.2239 17.7761"
    " 14 17.5 14H7.5C6.67157 14 6 13.3284 6 12.5V2.5Z"
    "M14 7.5V13H15V7.5C15 6.11929 13.8807 5 12.5 5H7V6H12.5C13.3284 6 14 6.67157 14 7.5Z";

// ── SVG paths for dark-mode toggle and help toolbar buttons ──────────────────
// Crescent moon (20×20 coord space, FillModeWinding):
// Moon crescent — Fluent Icons WeatherMoon (viewBox 0 0 24 24)
static const char* kMoonD =
    "M20.026 17.001c-2.762 4.784-8.879 6.423-13.663 3.661"
    "a9.964 9.964 0 0 1-3.234-2.983.75.75 0 0 1 .365-1.131"
    "c3.767-1.348 5.785-2.911 6.956-5.146 1.232-2.353 1.551-4.93.689-8.464"
    "a.75.75 0 0 1 .769-.926 9.961 9.961 0 0 1 4.457 1.327"
    "C21.149 6.1 22.788 12.217 20.025 17Z"
    "m-8.248-4.903c-1.25 2.388-3.31 4.099-6.817 5.499"
    "a8.492 8.492 0 0 0 2.152 1.766 8.501 8.501 0 1 0 8.502-14.725"
    "a8.485 8.485 0 0 0-2.792-1.016c.647 3.384.23 6.044-1.045 8.476Z";
static const float kMoonVB  = 40.f;  // expanded viewBox: -8 -8 40 40
static const float kMoonOff =  8.f;  // -minX = -(-8), shifts path right/down

// Sun — Fluent Icons WeatherSunny (viewBox 0 0 24 24)
static const char* kSunD =
    "M11.996 19.01a.75.75 0 0 1 .743.649l.007.102v1.5a.75.75 0 0 1-1.493.101l-.007-.101v-1.5"
    "a.75.75 0 0 1 .75-.75Z"
    "m6.022-2.072 1.06 1.06a.75.75 0 1 1-1.06 1.061l-1.06-1.06a.75.75 0 0 1 1.06-1.061Z"
    "m-10.983 0a.75.75 0 0 1 0 1.06L5.974 19.06a.75.75 0 0 1-1.06-1.06l1.06-1.061"
    "a.75.75 0 0 1 1.06 0Z"
    "M12 6.475a5.525 5.525 0 1 1 0 11.05 5.525 5.525 0 0 1 0-11.05Z"
    "m0 1.5a4.025 4.025 0 1 0 0 8.05 4.025 4.025 0 0 0 0-8.05Z"
    "m9.25 3.293a.75.75 0 0 1 .102 1.493l-.102.007h-1.5a.75.75 0 0 1-.102-1.493l.102-.007h1.5Z"
    "m-17-.029a.75.75 0 0 1 .102 1.494l-.102.006h-1.5a.75.75 0 0 1-.102-1.493l.102-.007h1.5Z"
    "m1.64-6.37.084.072 1.06 1.06a.75.75 0 0 1-.976 1.134l-.084-.073-1.06-1.06"
    "a.75.75 0 0 1 .976-1.134Z"
    "m13.188.072a.75.75 0 0 1 .073.977l-.073.084-1.06 1.061a.75.75 0 0 1-1.133-.976"
    "l.072-.084 1.06-1.061a.75.75 0 0 1 1.061 0Z"
    "M12 1.99a.75.75 0 0 1 .743.648l.007.102v1.5a.75.75 0 0 1-1.493.101l-.007-.102v-1.5"
    "a.75.75 0 0 1 .75-.75Z";
static const float kSunVB = 24.f;

// Question circle (same data as the Help icon in BuildToolImageList)
static const char* kHelpBtnD =
    "M10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2"
    " 14.4183 2 10C2 5.58172 5.58172 2 10 2ZM10 3C6.13401 3 3 6.13401 3 10C3"
    " 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3Z"
    "M10 13.5C10.4142 13.5 10.75 13.8358 10.75 14.25C10.75 14.6642 10.4142 15 10"
    " 15C9.58579 15 9.25 14.6642 9.25 14.25C9.25 13.8358 9.58579 13.5 10 13.5Z"
    "M10 5.5C11.3807 5.5 12.5 6.61929 12.5 8C12.5 8.72959 12.1848 9.40774 11.6513"
    " 9.8771L11.4967 10.0024L11.2782 10.1655L11.1906 10.2372C11.1348 10.2851"
    " 11.0835 10.3337 11.0346 10.3859C10.6963 10.7464 10.5 11.2422 10.5 12"
    "C10.5 12.2761 10.2761 12.5 10 12.5C9.72386 12.5 9.5 12.2761 9.5 12"
    "C9.5 10.988 9.79312 10.2475 10.3054 9.70162C10.4165 9.5832 10.532 9.47988"
    " 10.6609 9.37874L10.9076 9.19439L11.0256 9.09468C11.325 8.81435 11.5 8.42206"
    " 11.5 8C11.5 7.17157 10.8284 6.5 10 6.5C9.17157 6.5 8.5 7.17157 8.5 8"
    "C8.5 8.27614 8.27614 8.5 8 8.5C7.72386 8.5 7.5 8.27614 7.5 8"
    "C7.5 6.61929 8.61929 5.5 10 5.5Z";

// ── File menu icon SVG paths (all 24×24 viewBox, Fluent UI) ──────────────────
static const char* kFileNewD =
    "M18.5 20a.5.5 0 0 1-.5.5H6a.5.5 0 0 1-.5-.5V4a.5.5 0 0 1 .5-.5h6V8"
    "a2 2 0 0 0 2 2h4.5v10Zm-5-15.379L17.378 8.5H14a.5.5 0 0 1-.5-.5V4.621Z"
    "m5.914 3.793-5.829-5.828c-.026-.026-.058-.046-.085-.07"
    "a2.072 2.072 0 0 0-.219-.18c-.04-.027-.086-.045-.128-.068"
    "-.071-.04-.141-.084-.216-.116a1.977 1.977 0 0 0-.624-.138"
    "C12.266 2.011 12.22 2 12.172 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12"
    "a2 2 0 0 0 2-2V9.828a2 2 0 0 0-.586-1.414Z";
static const char* kFileOpenD =
    "M20 9.502V8.75a2.25 2.25 0 0 0-2.25-2.25h-5.725l-2.38-1.98"
    "A2.25 2.25 0 0 0 8.204 4H4.25A2.25 2.25 0 0 0 2 6.25l-.004 11.5"
    "A2.25 2.25 0 0 0 4.246 20H18.47a1.75 1.75 0 0 0 1.698-1.325l1.75-6.998"
    "a1.75 1.75 0 0 0-1.698-2.175H20ZM4.25 5.5h3.956a.75.75 0 0 1 .48.173"
    "l2.588 2.154a.75.75 0 0 0 .48.173h5.996a.75.75 0 0 1 .75.75v.752"
    "H6.424a2.25 2.25 0 0 0-2.183 1.704l-.744 2.978L3.5 6.25"
    "a.75.75 0 0 1 .75-.75Zm1.447 6.07a.75.75 0 0 1 .727-.568H20.22"
    "a.25.25 0 0 1 .242.31l-1.75 6.999a.25.25 0 0 1-.242.189H4.285"
    "a.25.25 0 0 1-.243-.31l1.655-6.62Z";
static const char* kFileSaveD =
    "M3 5.75A2.75 2.75 0 0 1 5.75 3h9.964a3.25 3.25 0 0 1 2.299.952"
    "l2.035 2.035c.61.61.952 1.437.952 2.299v9.964A2.75 2.75 0 0 1 18.25 21"
    "H5.75A2.75 2.75 0 0 1 3 18.25V5.75ZM5.75 4.5c-.69 0-1.25.56-1.25 1.25"
    "v12.5c0 .69.56 1.25 1.25 1.25H6v-5.25A2.25 2.25 0 0 1 8.25 12h7.5"
    "A2.25 2.25 0 0 1 18 14.25v5.25h.25c.69 0 1.25-.56 1.25-1.25V8.286"
    "c0-.465-.184-.91-.513-1.238l-2.035-2.035a1.75 1.75 0 0 0-.952-.49V7.25"
    "a2.25 2.25 0 0 1-2.25 2.25h-4.5A2.25 2.25 0 0 1 7 7.25V4.5H5.75Z"
    "m10.75 15v-5.25a.75.75 0 0 0-.75-.75h-7.5a.75.75 0 0 0-.75.75v5.25h9Z"
    "m-8-15v2.75c0 .414.336.75.75.75h4.5a.75.75 0 0 0 .75-.75V4.5h-6Z";
// Three sliders (_dev/save_options.svg); every shape winds clockwise so the
// knobs join the bars under non-zero fill
static const char* kFileSaveOptionsD =
    "M3.75,6.25 H20.25 V7.75 H3.75 Z M5.5,7 A2.5,2.5 0 1 1 10.5,7 A2.5,2.5 0 1 1 5.5,7 Z"
    "M3.75,11.25 H20.25 V12.75 H3.75 Z M12.5,12 A2.5,2.5 0 1 1 17.5,12 A2.5,2.5 0 1 1 12.5,12 Z"
    "M3.75,16.25 H20.25 V17.75 H3.75 Z M7.5,17 A2.5,2.5 0 1 1 12.5,17 A2.5,2.5 0 1 1 7.5,17 Z";

// Fluent UI System Icons "Delete" 24 regular (_dev/delete.svg)
static const char* kFileDeleteD =
    "M10 5H14C14 3.89543 13.1046 3 12 3C10.8954 3 10 3.89543 10 5Z"
    "M8.5 5C8.5 3.067 10.067 1.5 12 1.5C13.933 1.5 15.5 3.067 15.5 5H21.25C21.6642 5 22 5.33579 22 5.75"
    "C22 6.16421 21.6642 6.5 21.25 6.5H19.9309L18.7589 18.6112C18.5729 20.5334 16.9575 22 15.0263 22"
    "H8.97369C7.04254 22 5.42715 20.5334 5.24113 18.6112L4.06908 6.5H2.75C2.33579 6.5 2 6.16421 2 5.75"
    "C2 5.33579 2.33579 5 2.75 5H8.5Z"
    "M10.5 9.75C10.5 9.33579 10.1642 9 9.75 9C9.33579 9 9 9.33579 9 9.75V17.25C9 17.6642 9.33579 18 9.75 18"
    "C10.1642 18 10.5 17.6642 10.5 17.25V9.75Z"
    "M14.25 9C14.6642 9 15 9.33579 15 9.75V17.25C15 17.6642 14.6642 18 14.25 18C13.8358 18 13.5 17.6642 13.5 17.25"
    "V9.75C13.5 9.33579 13.8358 9 14.25 9Z"
    "M6.73416 18.4667C6.84577 19.62 7.815 20.5 8.97369 20.5H15.0263C16.185 20.5 17.1542 19.62 17.2658 18.4667"
    "L18.4239 6.5H5.57608L6.73416 18.4667Z";
static const char* kFilePrintD =
    "M15.752 3a2.25 2.25 0 0 1 2.25 2.25v.753h.75"
    "a3.254 3.254 0 0 1 3.252 3.25l.003 5.997"
    "a2.249 2.249 0 0 1-2.248 2.25H18v1.25A2.25 2.25 0 0 1 15.75 21h-7.5"
    "A2.25 2.25 0 0 1 6 18.75V17.5H4.25A2.25 2.25 0 0 1 2 15.25V9.254"
    "a3.25 3.25 0 0 1 3.25-3.25l.749-.001L6 5.25A2.25 2.25 0 0 1 8.25 3h7.502Z"
    "m-.002 10.5h-7.5a.75.75 0 0 0-.75.75v4.5c0 .414.336.75.75.75h7.5"
    "a.75.75 0 0 0 .75-.75v-4.5a.75.75 0 0 0-.75-.75Z"
    "m3.002-5.996H5.25a1.75 1.75 0 0 0-1.75 1.75v5.996"
    "c0 .414.336.75.75.75H6v-1.75A2.25 2.25 0 0 1 8.25 12h7.5"
    "A2.25 2.25 0 0 1 18 14.25V16h1.783a.749.749 0 0 0 .724-.749"
    "l-.003-5.997a1.754 1.754 0 0 0-1.752-1.75Z"
    "m-3-3.004H8.25a.75.75 0 0 0-.75.75l-.001.753h9.003V5.25"
    "a.75.75 0 0 0-.75-.75Z";
// save_edit_or_save_as.svg — two paths, both 24×24 viewBox
static const char* kFileSaveAsFloppyD =
    "M5.75 3A2.75 2.75 0 0 0 3 5.75v12.5A2.75 2.75 0 0 0 5.75 21h4.249"
    "a2.112 2.112 0 0 1 .062-.593l.227-.907H7.5v-5.25a.75.75 0 0 1 .75-.75"
    "h6.603l1.435-1.435A2.258 2.258 0 0 0 15.75 12h-7.5A2.25 2.25 0 0 0 6 14.25"
    "v5.25h-.25c-.69 0-1.25-.56-1.25-1.25V5.75c0-.69.56-1.25 1.25-1.25H7v2.75"
    "A2.25 2.25 0 0 0 9.25 9.5h4.5A2.25 2.25 0 0 0 16 7.25V4.523"
    "c.358.06.692.23.952.49l2.035 2.035c.329.328.513.773.513 1.238v1.721"
    "c.07-.005.142-.007.213-.007h.002c.437 0 .875.087 1.285.261V8.287"
    "a3.25 3.25 0 0 0-.952-2.299l-2.035-2.035A3.25 3.25 0 0 0 15.714 3H5.75Z"
    "M8.5 7.25V4.5h6v2.75a.75.75 0 0 1-.75.75h-4.5a.75.75 0 0 1-.75-.75Z";
static const char* kFileSaveAsPencilD =
    "M19.715 11h-.002c-.585 0-1.17.223-1.615.67l-5.902 5.902"
    "a2.684 2.684 0 0 0-.707 1.247l-.458 1.831a1.087 1.087 0 0 0 1.319 1.318"
    "l1.83-.457a2.684 2.684 0 0 0 1.248-.707l5.902-5.902"
    "A2.285 2.285 0 0 0 19.715 11Z";

// ─── Edit menu icon SVG paths (Fluent UI, 24×24 viewBox) ─────────────────────
static const char* kEditUndoD =
    "M4.75 2a.75.75 0 0 1 .743.648l.007.102v5.69l4.574-4.56a6.41 6.41 0 0 1 8.879-.179"
    "l.186.18a6.41 6.41 0 0 1 0 9.063l-8.846 8.84a.75.75 0 0 1-1.06-1.062l8.845-8.838"
    "a4.91 4.91 0 0 0-6.766-7.112l-.178.17L6.562 9.5h5.688a.75.75 0 0 1 .743.648l.007.102"
    "a.75.75 0 0 1-.648.743L12.25 11h-7.5a.75.75 0 0 1-.743-.648L4 10.25v-7.5"
    "A.75.75 0 0 1 4.75 2Z";
static const char* kEditRedoD =
    "M19.25 2a.75.75 0 0 0-.743.648l-.007.102v5.69l-4.574-4.56a6.41 6.41 0 0 0-8.878-.179"
    "l-.186.18a6.41 6.41 0 0 0 0 9.063l8.845 8.84a.75.75 0 0 0 1.06-1.062l-8.845-8.838"
    "a4.91 4.91 0 0 1 6.766-7.112l.178.17L17.438 9.5H11.75a.75.75 0 0 0-.743.648L11 10.25"
    "c0 .38.282.694.648.743l.102.007h7.5a.75.75 0 0 0 .743-.648L20 10.25v-7.5"
    "a.75.75 0 0 0-.75-.75Z";
static const char* kEditCutD =
    "M12.14 9.341v.002L7.37 2.328a.75.75 0 1 0-1.24.844l5.13 7.545"
    "-2.395 3.743a4 4 0 1 0 1.178.943l2.135-3.337 2.065 3.036"
    "a4 4 0 1 0 1.261-.813l-2.447-3.597.002-.002-.918-1.348Z"
    "m-7.64 8.66a2.5 2.5 0 1 1 5 0 2.5 2.5 0 0 1-5 0Z"
    "m10 0a2.5 2.5 0 1 1 5 0 2.5 2.5 0 0 1-5 0Z";
static const char* kEditCutD2 =
    "m13.938 9.316 3.943-6.162a.75.75 0 1 0-1.263-.808L13.02 7.968l.918 1.348Z";
static const char* kEditCopyD =
    "M5.503 4.627 5.5 6.75v10.504a3.25 3.25 0 0 0 3.25 3.25h8.616"
    "a2.251 2.251 0 0 1-2.122 1.5H8.75A4.75 4.75 0 0 1 4 17.254V6.75"
    "c0-.98.627-1.815 1.503-2.123ZM17.75 2A2.25 2.25 0 0 1 20 4.25v13"
    "a2.25 2.25 0 0 1-2.25 2.25h-9a2.25 2.25 0 0 1-2.25-2.25v-13"
    "A2.25 2.25 0 0 1 8.75 2h9Zm0 1.5h-9a.75.75 0 0 0-.75.75v13"
    "c0 .414.336.75.75.75h9a.75.75 0 0 0 .75-.75v-13a.75.75 0 0 0-.75-.75Z";
// ─── Image menu icon SVG paths (Fluent UI, 24×24 viewBox) ────────────────────
static const char* kImgRotateRightD =
    "M8.985 5c-.084 0-.167.002-.25.005h-.017a6.79 6.79 0 0 0-3.132.883"
    "C4.024 6.79 3 8.295 3 10c0 1.708 1.028 3.216 2.595 4.118.968.556 2.14.882 3.405.882"
    "h.5a.75.75 0 0 0 0-1.5H9a5.401 5.401 0 0 1-2.588-.636C5.256 12.23 4.5 11.184 4.5 10"
    "s.756-2.23 1.912-2.864a5.357 5.357 0 0 1 2.363-.632L9 6.5h6.939l-1.72 1.72"
    "a.75.75 0 0 0 .977 1.133l.084-.073h.001l3-3a.75.75 0 0 0 0-1.06l-3-3-.085-.073"
    "a.75.75 0 0 0-.977 1.132v.001L15.939 5H8.985Z"
    "M3.61 20.067a.5.5 0 0 0 .25.933H20a1 1 0 0 0 1-1v-9.133"
    "a.5.5 0 0 0-.75-.433L3.61 20.067Zm3.974-.567L19.5 12.602V19.5H7.585Z";
static const char* kImgRotateLeftD =
    "M15.015 5c.084 0 .167.002.25.005h.017a6.79 6.79 0 0 1 3.131.883"
    "C19.976 6.79 21 8.295 21 10c0 1.708-1.027 3.216-2.596 4.118"
    "A6.833 6.833 0 0 1 15 15h-.5a.75.75 0 1 1 0-1.5h.5c.963 0 1.856-.235 2.588-.636"
    "C18.744 12.23 19.5 11.184 19.5 10s-.756-2.23-1.912-2.864"
    "a5.357 5.357 0 0 0-2.363-.632L15 6.5H8.061l1.72 1.72"
    "a.75.75 0 0 1-.977 1.133L8.72 9.28h-.001l-3-3a.75.75 0 0 1 0-1.06l3-3"
    " .085-.073a.75.75 0 0 1 .977 1.132L8.062 5H15.015Z"
    "m5.374 15.067a.5.5 0 0 1-.25.933H4a1 1 0 0 1-1-1v-9.133"
    "a.5.5 0 0 1 .75-.433l16.639 9.633Zm-3.974-.567L4.5 12.602V19.5h11.915Z";
static const char* kImgFlipHD =
    "M4.5 17.5a.75.75 0 0 1-.659-1.108l6.25-11.5a.75.75 0 0 1 1.409.358v11.5"
    "a.75.75 0 0 1-.75.75H4.5ZM20.145 17.133a.75.75 0 0 1-.645.367h-6.25"
    "a.75.75 0 0 1-.75-.75V5.25a.75.75 0 0 1 1.409-.358l6.25 11.5"
    "a.75.75 0 0 1-.014.741ZM14 8.201V16h4.239L14 8.2Z";
static const char* kImgFlipVD =
    "M17.133 3.855a.75.75 0 0 1 .367.645v6.25a.75.75 0 0 1-.75.75H5.25"
    "a.75.75 0 0 1-.358-1.409l11.5-6.25a.75.75 0 0 1 .741.014Z"
    "M8.201 10H16V5.761L8.2 10ZM17.5 19.5a.75.75 0 0 1-1.108.659l-11.5-6.25"
    "A.75.75 0 0 1 5.25 12.5h11.5a.75.75 0 0 1 .75.75v6.25Z";
static const char* kImgResizeD =
    "M21.25 13a.75.75 0 0 1 .743.648l.007.102v5a3.25 3.25 0 0 1-3.066 3.245L18.75 22"
    "h-4.668c.536-.385.973-.9 1.265-1.499l3.403-.001a1.75 1.75 0 0 0 1.744-1.607"
    "l.006-.143v-5a.75.75 0 0 1 .75-.75Zm-9.5-4A3.25 3.25 0 0 1 15 12.25v6.5"
    "A3.25 3.25 0 0 1 11.75 22h-6.5A3.25 3.25 0 0 1 2 18.75v-6.5A3.25 3.25 0 0 1 5.25 9h6.5Z"
    "m-4.032 8.353-.102.091L4.663 20.4c.184.066.381.101.587.101h6.5"
    "c.206 0 .403-.035.587-.1l-2.953-2.955a1.25 1.25 0 0 0-1.558-.17l-.108.078Z"
    "M11.75 10.5h-6.5a1.75 1.75 0 0 0-1.75 1.75v6.5c0 .206.036.403.1.587"
    "l2.955-2.953a2.75 2.75 0 0 1 3.752-.129l.138.129 2.954 2.953"
    "c.066-.184.101-.381.101-.587v-6.5a1.75 1.75 0 0 0-1.75-1.75Z"
    "M11 12a1 1 0 1 1 0 2 1 1 0 0 1 0-2Z"
    "m7.75-10a3.25 3.25 0 0 1 3.245 3.066L22 5.25v5a.75.75 0 0 1-1.493.102l-.007-.102v-5"
    "a1.75 1.75 0 0 0-1.606-1.744L18.75 3.5h-5a.75.75 0 0 1-.102-1.493L13.75 2h5Z"
    "m-8.5 0a.75.75 0 0 1 .102 1.493l-.102.007h-5a1.75 1.75 0 0 0-1.744 1.606"
    "L3.5 5.25v3.402c-.6.292-1.114.73-1.5 1.266V5.25a3.25 3.25 0 0 1 3.066-3.245L5.25 2h5Z";

static const char* kPencilSvgD =
    "M17.1813 2.92689C16.029 1.71505 14.1046 1.69077 12.9222 2.87317L3.54735 12.2475"
    "C3.21952 12.5754 2.99198 12.9899 2.89142 13.4424L2.0138 17.3923C1.97672 17.5592"
    " 2.02748 17.7335 2.14838 17.8544C2.26928 17.9753 2.44356 18.026 2.61044 17.9889"
    "L6.53683 17.1157C7.00426 17.0118 7.43237 16.7767 7.77096 16.4381L17.129 7.08003"
    "C18.27 5.939 18.2932 4.09631 17.1813 2.92689Z"
    "M13.6293 3.58029C14.4142 2.79538 15.6917 2.8115 16.4566 3.61596C17.1947 4.39225"
    " 17.1793 5.61548 16.4218 6.37293L15.7506 7.04418L12.958 4.25155L13.6293 3.58029Z"
    "M12.2508 4.95864L15.0435 7.7513L7.06385 15.731C6.8597 15.9352 6.60158 16.0769"
    " 6.31975 16.1396L3.16044 16.8421L3.86762 13.6593C3.92692 13.3924 4.0611 13.148"
    " 4.25444 12.9547L12.2508 4.95864Z";

// File menu entries — command ID, mnemonic text (with &accelerator and \tShortcut)
struct FileMenuEntry { UINT cmdId; const wchar_t* text; };
static const FileMenuEntry kFileMenuEntries[] = {
    { ID_FILE_NEW,    L"&New\tCtrl+N"      },
    { ID_FILE_OPEN,   L"&Open...\tCtrl+O"  },
    { ID_FILE_SAVE,   L"&Save\tCtrl+S"     },
    { ID_FILE_SAVEAS, L"Save &As..."        },
    { ID_FILE_SAVE_OPTIONS, L"Sa&ve Options..." },
    { ID_FILE_PRINT,  L"&Print...\tCtrl+P" },
    { ID_FILE_DELETE, L"&Delete"            },
    { ID_FILE_EXIT,   L"E&xit\tAlt+F4"     },
};

// Draws a file-menu icon at (x, y) in a sz×sz box directly onto hdc using GDI+.
// fillColor is the icon foreground (accounts for hover/grayed/dark state).
static void DrawFileMenuIcon(HDC hdc, UINT cmdId, int x, int y, int sz, Color fillColor)
{
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetClip(Gdiplus::Rect(x, y, sz, sz));
    g.TranslateTransform((REAL)x, (REAL)y);
    SolidBrush br(fillColor);
    float sc = (float)sz / 24.0f;

    if (cmdId == ID_FILE_SAVEAS) {
        GraphicsPath p1(FillModeWinding), p2(FillModeWinding);
        AddSvgPath(p1, kFileSaveAsFloppyD, sc);
        AddSvgPath(p2, kFileSaveAsPencilD, sc);
        g.FillPath(&br, &p1);
        g.FillPath(&br, &p2);
    } else if (cmdId == ID_FILE_EXIT) {
        float m = sz * 0.2f, M = sz * 0.8f;
        Pen xp(fillColor, 1.5f);
        xp.SetStartCap(LineCapRound); xp.SetEndCap(LineCapRound);
        g.DrawLine(&xp, PointF(m, m), PointF(M, M));
        g.DrawLine(&xp, PointF(M, m), PointF(m, M));
    } else {
        const char* d = nullptr;
        switch (cmdId) {
        case ID_FILE_NEW:   d = kFileNewD;   break;
        case ID_FILE_OPEN:  d = kFileOpenD;  break;
        case ID_FILE_SAVE:  d = kFileSaveD;  break;
        case ID_FILE_PRINT: d = kFilePrintD; break;
        case ID_FILE_DELETE: d = kFileDeleteD; break;
        case ID_FILE_SAVE_OPTIONS: d = kFileSaveOptionsD; break;
        }
        if (d) { GraphicsPath path(FillModeWinding); AddSvgPath(path, d, sc); g.FillPath(&br, &path); }
    }
}

// Builds a 32bpp premultiplied-alpha DIB for one file-menu icon and returns its HBITMAP.
// Renders via a GDI+ Bitmap object (not through a DC) so the alpha channel is written
// correctly; then copies pixels to a DIBSECTION with premultiplied alpha for MIIM_BITMAP.
static HBITMAP MakeFileMenuIconBitmap(UINT cmdId, Color fillColor, int sz)
{
    // Draw into a GDI+ Bitmap — alpha is correctly set here unlike DC-based drawing.
    Bitmap bmp(sz, sz, PixelFormat32bppARGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(Color(0, 0, 0, 0));
        SolidBrush br(fillColor);
        float sc = (float)sz / 24.0f;

        if (cmdId == ID_FILE_SAVEAS) {
            GraphicsPath p1(FillModeWinding), p2(FillModeWinding);
            AddSvgPath(p1, kFileSaveAsFloppyD, sc);
            AddSvgPath(p2, kFileSaveAsPencilD, sc);
            g.FillPath(&br, &p1);
            g.FillPath(&br, &p2);
        } else if (cmdId == ID_FILE_EXIT) {
            float m = sz * 0.2f, M = sz * 0.8f;
            Pen xp(fillColor, 1.5f);
            xp.SetStartCap(LineCapRound); xp.SetEndCap(LineCapRound);
            g.DrawLine(&xp, PointF(m, m), PointF(M, M));
            g.DrawLine(&xp, PointF(M, m), PointF(m, M));
        } else {
            const char* d = nullptr;
            switch (cmdId) {
            case ID_FILE_NEW:   d = kFileNewD;   break;
            case ID_FILE_OPEN:  d = kFileOpenD;  break;
            case ID_FILE_SAVE:  d = kFileSaveD;  break;
            case ID_FILE_PRINT: d = kFilePrintD; break;
            case ID_FILE_DELETE: d = kFileDeleteD; break;
            case ID_FILE_SAVE_OPTIONS: d = kFileSaveOptionsD; break;
            }
            if (d) { GraphicsPath path(FillModeWinding); AddSvgPath(path, d, sc); g.FillPath(&br, &path); }
        }
    }

    // Copy straight-alpha pixels into a premultiplied DIBSECTION for MIIM_BITMAP.
    BITMAPV5HEADER bmi = {};
    bmi.bV5Size        = sizeof(bmi);
    bmi.bV5Width       = sz;
    bmi.bV5Height      = -sz;
    bmi.bV5Planes      = 1;
    bmi.bV5BitCount    = 32;
    bmi.bV5Compression = BI_BITFIELDS;
    bmi.bV5RedMask     = 0x00FF0000;
    bmi.bV5GreenMask   = 0x0000FF00;
    bmi.bV5BlueMask    = 0x000000FF;
    bmi.bV5AlphaMask   = 0xFF000000;
    void* pBits = nullptr;
    HBITMAP hbm = CreateDIBSection(NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hbm || !pBits) return NULL;

    BitmapData bd; Rect rc(0, 0, sz, sz);
    if (bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) == Ok) {
        BYTE* src = (BYTE*)bd.Scan0;
        BYTE* dst = (BYTE*)pBits;
        for (int i = 0; i < sz * sz; i++, src += 4, dst += 4) {
            BYTE a = src[3];
            dst[0] = (BYTE)((int)src[0] * a / 255);  // B premul
            dst[1] = (BYTE)((int)src[1] * a / 255);  // G premul
            dst[2] = (BYTE)((int)src[2] * a / 255);  // R premul
            dst[3] = a;
        }
        bmp.UnlockBits(&bd);
    }
    return hbm;
}

// Sets file-menu item icons using MIIM_BITMAP with opaque pre-composited bitmaps,
// matching the approach used by ShowBrushTypePopup.  No MFT_OWNERDRAW is used,
// so user32 never adds WS_EX_DLGMODALFRAME to the popup window.
static void SetFileMenuIcons()
{
    if (!g_hMenuMain) return;
    HMENU hFile = GetSubMenu(g_hMenuMain, 0);
    if (!hFile) return;

    // Free previous bitmaps
    for (int i = 0; i < (int)ARRAYSIZE(g_hbmFileMenuIcons); i++) {
        if (g_hbmFileMenuIcons[i]) { DeleteObject(g_hbmFileMenuIcons[i]); g_hbmFileMenuIcons[i] = NULL; }
    }

    const int sz     = 16;
    COLORREF crBg    = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crIcon  = g_darkMode ? RGB(200, 200, 200) : RGB(45, 45, 45);
    Color    bgClr  (255, GetRValue(crBg),   GetGValue(crBg),   GetBValue(crBg));
    Color    iconClr(255, GetRValue(crIcon), GetGValue(crIcon), GetBValue(crIcon));

    for (int i = 0; i < (int)ARRAYSIZE(kFileMenuEntries); i++) {
        UINT cmdId = kFileMenuEntries[i].cmdId;

        // Render icon onto an opaque background — same technique as brush menu
        Bitmap bmp(sz, sz, PixelFormat32bppRGB);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.Clear(bgClr);
            SolidBrush br(iconClr);
            float sc = (float)sz / 24.0f;

            if (cmdId == ID_FILE_SAVEAS) {
                GraphicsPath p1(FillModeWinding), p2(FillModeWinding);
                AddSvgPath(p1, kFileSaveAsFloppyD, sc);
                AddSvgPath(p2, kFileSaveAsPencilD, sc);
                g.FillPath(&br, &p1);
                g.FillPath(&br, &p2);
            } else if (cmdId == ID_FILE_EXIT) {
                float m = sz * 0.2f, M = sz * 0.8f;
                Pen xp(iconClr, 1.5f);
                xp.SetStartCap(LineCapRound); xp.SetEndCap(LineCapRound);
                g.DrawLine(&xp, PointF(m, m), PointF(M, M));
                g.DrawLine(&xp, PointF(M, m), PointF(m, M));
            } else {
                const char* d = nullptr;
                switch (cmdId) {
                case ID_FILE_NEW:   d = kFileNewD;   break;
                case ID_FILE_OPEN:  d = kFileOpenD;  break;
                case ID_FILE_SAVE:  d = kFileSaveD;  break;
                case ID_FILE_PRINT: d = kFilePrintD; break;
                case ID_FILE_DELETE: d = kFileDeleteD; break;
                case ID_FILE_SAVE_OPTIONS: d = kFileSaveOptionsD; break;
                }
                if (d) { GraphicsPath path(FillModeWinding); AddSvgPath(path, d, sc); g.FillPath(&br, &path); }
            }
        }

        // Copy into a screen-compatible HBITMAP (same as brush menu)
        HDC hdc    = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, sz, sz);
        ReleaseDC(NULL, hdc);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
        Graphics gDC(hdcMem);
        gDC.DrawImage(&bmp, 0, 0, sz, sz);
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        g_hbmFileMenuIcons[i] = hbm;

        // Apply as MIIM_BITMAP; also restore MFT_STRING and item text in case
        // a previous call had set MFT_OWNERDRAW
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask      = MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;
        mii.fType      = MFT_STRING;
        mii.dwTypeData = const_cast<LPWSTR>(kFileMenuEntries[i].text);
        mii.hbmpItem   = hbm;
        SetMenuItemInfoW(hFile, cmdId, FALSE, &mii);
    }

    // Restore separators to plain MFT_SEPARATOR (no MFT_OWNERDRAW). Found by
    // scanning: the SVG-only Inkscape item shifts positions when present.
    for (int pos = 0, n = GetMenuItemCount(hFile); pos < n; pos++) {
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_FTYPE;
        if (GetMenuItemInfoW(hFile, pos, TRUE, &mii) && (mii.fType & MFT_SEPARATOR)) {
            mii.fType = MFT_SEPARATOR;
            SetMenuItemInfoW(hFile, pos, TRUE, &mii);
        }
    }
}

// ─── Open in Inkscape (SVG only) ────────────────────────────────────────────

// inkscape.exe from its App Paths registration (what the Run dialog and
// ShellExecute use) or the default install folder; empty if not installed.
// A local registry read only — nothing leaves the machine.
static std::wstring FindInkscape()
{
    wchar_t buf[MAX_PATH];
    for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }) {
        DWORD sz = sizeof(buf);
        if (RegGetValueW(root, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\inkscape.exe",
                         NULL, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, buf, &sz) == ERROR_SUCCESS) {
            PathUnquoteSpacesW(buf);
            if (PathFileExistsW(buf)) return buf;
        }
    }
    if (ExpandEnvironmentStringsW(L"%ProgramFiles%\\Inkscape\\bin\\inkscape.exe", buf, MAX_PATH) &&
        PathFileExistsW(buf))
        return buf;
    return L"";
}

// A program's own icon as an opaque menu bitmap on the menu background
static HBITMAP MakeExeIconBitmap(const wchar_t* exe, int sz)
{
    HICON hi = NULL;
    if (FAILED(SHDefExtractIconW(exe, 0, 0, &hi, NULL, sz)) || !hi) return NULL;
    COLORREF crBg = g_darkMode ? RGB(43, 43, 43) : GetSysColor(COLOR_MENU);
    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP hb = CreateCompatibleBitmap(screen, sz, sz);
    ReleaseDC(NULL, screen);
    HBITMAP old = (HBITMAP)SelectObject(mem, hb);
    HBRUSH br = CreateSolidBrush(crBg);
    RECT rc = { 0, 0, sz, sz };
    FillRect(mem, &rc, br);
    DeleteObject(br);
    DrawIconEx(mem, 0, 0, hi, sz, sz, 0, NULL, DI_NORMAL);
    SelectObject(mem, old);
    DeleteDC(mem);
    DestroyIcon(hi);
    return hb;
}

static void OpenInInkscape(const std::wstring& path)
{
    std::wstring exe = FindInkscape();
    if (exe.empty()) return;
    std::wstring args = L"\"" + path + L"\"";
    if ((INT_PTR)ShellExecuteW(g_hwndMain, NULL, exe.c_str(), args.c_str(), NULL, SW_SHOWNORMAL) <= 32)
        MessageBox(g_hwndMain, L"Could not start Inkscape.", APP_TITLE, MB_ICONERROR);
}

// History icon (_dev/history.svg): a clock whose rim runs back to an arrow.
// A stroke path, drawn with a 1.5-unit pen in a 24-unit box.
static const char* kHistoryD =
    "M 4,12 A 8,8 0 1 1 6.34,17.66 M 1.5,9.5 L 4,12 L 6.5,9.5 M 12,7.5 V 12 L 15,14";

static HBITMAP MakeHistoryMenuIcon(int sz)
{
    COLORREF crBg   = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crIcon = g_darkMode ? RGB(200, 200, 200) : RGB(45, 45, 45);
    Bitmap bmp(sz, sz, PixelFormat32bppRGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(Color(255, GetRValue(crBg), GetGValue(crBg), GetBValue(crBg)));
        float sc = sz / 24.0f;
        GraphicsPath path;
        AddSvgPath(path, kHistoryD, sc);
        Pen pen(Color(255, GetRValue(crIcon), GetGValue(crIcon), GetBValue(crIcon)), 1.5f * sc);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
        g.DrawPath(&pen, &path);
    }
    HBITMAP hbm = NULL;
    bmp.GetHBITMAP(Color(255, GetRValue(crBg), GetGValue(crBg), GetBValue(crBg)), &hbm);
    return hbm;
}

// Rebuilds the File > Open Recent submenu (placed just after Open) from
// g_recentFiles. Called as the File menu opens.
static void UpdateFileMenuRecent(HMENU hFile)
{
    static HMENU   s_recent = NULL;
    static HBITMAP s_icon   = NULL;
    if (s_recent) {
        for (int pos = 0, n = GetMenuItemCount(hFile); pos < n; pos++)
            if (GetSubMenu(hFile, pos) == s_recent) { RemoveMenu(hFile, pos, MF_BYPOSITION); break; }
        DestroyMenu(s_recent);
    }
    s_recent = CreatePopupMenu();
    for (int i = 0; i < (int)g_recentFiles.size(); i++) {
        // "&1 C:\path\file.png", with & in the path doubled so it isn't a mnemonic
        std::wstring text = L"&" + std::to_wstring((i + 1) % 10) + L"  ";
        for (wchar_t c : g_recentFiles[i]) { if (c == L'&') text += L'&'; text += c; }
        AppendMenuW(s_recent, MF_STRING, ID_FILE_RECENT_FIRST + i, text.c_str());
    }
    if (g_recentFiles.empty()) {
        AppendMenuW(s_recent, MF_STRING | MF_GRAYED, 0, L"(no recent files)");
    } else {
        AppendMenuW(s_recent, MF_SEPARATOR, 0, NULL);
        AppendMenuW(s_recent, MF_STRING, ID_FILE_RECENT_CLEAR, L"&Clear list");
    }
    int pos = 0;
    for (int n = GetMenuItemCount(hFile); pos < n && GetMenuItemID(hFile, pos) != ID_FILE_OPEN; pos++) {}
    InsertMenuW(hFile, pos + 1, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)s_recent, L"Open &Recent");
    // Rebuilt each time so it follows dark mode
    if (s_icon) DeleteObject(s_icon);
    s_icon = MakeHistoryMenuIcon(16);
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask    = MIIM_BITMAP;
    mii.hbmpItem = s_icon;
    SetMenuItemInfoW(hFile, pos + 1, TRUE, &mii);
}

static void OpenRecentFile(int i)
{
    if (i < 0 || i >= (int)g_recentFiles.size()) return;
    std::wstring path = g_recentFiles[i];
    if (!PathFileExistsW(path.c_str())) {
        std::wstring msg = path + L"\n\nThis file no longer exists. It has been removed from the list.";
        MessageBox(g_hwndMain, msg.c_str(), APP_TITLE, MB_ICONWARNING);
        g_recentFiles.erase(g_recentFiles.begin() + i);
        return;
    }
    if (PromptSaveIfNeeded()) BeginImageLoad(path.c_str(), false);
}

// Adds "Open in Inkscape" under Open in the File menu while an SVG is open
// and Inkscape is installed; removes it otherwise. Called as the menu opens.
static void UpdateFileMenuInkscapeItem(HMENU hFile)
{
    static HBITMAP s_icon = NULL;
    RemoveMenu(hFile, ID_FILE_INKSCAPE, MF_BYCOMMAND);
    if (s_icon) { DeleteObject(s_icon); s_icon = NULL; }
    if (!g_isSvgFile || !g_currentFile[0]) return;
    std::wstring exe = FindInkscape();
    if (exe.empty()) return;
    s_icon = MakeExeIconBitmap(exe.c_str(), 16);
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask      = MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;
    mii.fType      = MFT_STRING;
    mii.wID        = ID_FILE_INKSCAPE;
    mii.dwTypeData = const_cast<LPWSTR>(L"Open in &Inkscape");
    mii.hbmpItem   = s_icon;
    int pos = 0;
    for (int n = GetMenuItemCount(hFile); pos < n && GetMenuItemID(hFile, pos) != ID_FILE_OPEN; pos++) {}
    InsertMenuItemW(hFile, pos + 1, TRUE, &mii);
}

// Edit menu entries — command ID and display text
struct EditMenuEntry { UINT cmdId; const wchar_t* text; };
static const EditMenuEntry kEditMenuEntries[] = {
    { ID_EDIT_UNDO,       L"&Undo\tCtrl+Z"        },
    { ID_EDIT_REDO,       L"&Redo\tCtrl+Y"        },
    { ID_EDIT_CUT,        L"Cu&t\tCtrl+X"         },
    { ID_EDIT_COPY,       L"&Copy\tCtrl+C"        },
    { ID_EDIT_PASTE,      L"&Paste\tCtrl+V"       },
    { ID_EDIT_SELECT_ALL, L"Select &All\tCtrl+A"  },
};

// Sets Edit menu item icons using MIIM_BITMAP with opaque pre-composited bitmaps.
static void SetEditMenuIcons()
{
    if (!g_hMenuMain) return;
    HMENU hEdit = GetSubMenu(g_hMenuMain, 1);
    if (!hEdit) return;

    for (int i = 0; i < (int)ARRAYSIZE(g_hbmEditMenuIcons); i++) {
        if (g_hbmEditMenuIcons[i]) { DeleteObject(g_hbmEditMenuIcons[i]); g_hbmEditMenuIcons[i] = NULL; }
    }

    const int sz    = 16;
    COLORREF crBg   = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crIcon = g_darkMode ? RGB(200, 200, 200) : RGB(45, 45, 45);
    Color bgClr  (255, GetRValue(crBg),   GetGValue(crBg),   GetBValue(crBg));
    Color iconClr(255, GetRValue(crIcon), GetGValue(crIcon), GetBValue(crIcon));

    for (int i = 0; i < (int)ARRAYSIZE(kEditMenuEntries); i++) {
        UINT cmdId = kEditMenuEntries[i].cmdId;
        Bitmap bmp(sz, sz, PixelFormat32bppRGB);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.Clear(bgClr);
            float sc = (float)sz / 24.0f;
            SolidBrush br(iconClr);

            if (cmdId == ID_EDIT_CUT) {
                GraphicsPath p1(FillModeWinding), p2(FillModeWinding);
                AddSvgPath(p1, kEditCutD,  sc);
                AddSvgPath(p2, kEditCutD2, sc);
                g.FillPath(&br, &p1);
                g.FillPath(&br, &p2);
            } else if (cmdId == ID_EDIT_PASTE) {
                // Clipboard icon: body rect + filled clip holder at top
                float s = (float)sz / 16.0f;
                Pen pen(iconClr, 1.0f * s);
                g.DrawRectangle(&pen, 2.0f*s, 4.0f*s, 11.0f*s, 10.0f*s);
                g.FillRectangle(&br, 5.0f*s, 2.0f*s, 6.0f*s, 3.5f*s);
                // Content lines
                Pen lp(iconClr, 0.8f * s);
                g.DrawLine(&lp, 3.5f*s, 7.0f*s,  12.5f*s, 7.0f*s);
                g.DrawLine(&lp, 3.5f*s, 9.5f*s,  12.5f*s, 9.5f*s);
                g.DrawLine(&lp, 3.5f*s, 12.0f*s, 10.0f*s, 12.0f*s);
            } else if (cmdId == ID_EDIT_SELECT_ALL) {
                // Dotted selection rectangle
                Pen dashPen(iconClr, 1.2f);
                REAL dashes[] = { 2.5f, 2.0f };
                dashPen.SetDashPattern(dashes, 2);
                float m = 2.5f, side = sz - 5.0f;
                g.DrawRectangle(&dashPen, m, m, side, side);
            } else {
                const char* d = nullptr;
                switch (cmdId) {
                case ID_EDIT_UNDO: d = kEditUndoD; break;
                case ID_EDIT_REDO: d = kEditRedoD; break;
                case ID_EDIT_COPY: d = kEditCopyD; break;
                }
                if (d) { GraphicsPath path(FillModeWinding); AddSvgPath(path, d, sc); g.FillPath(&br, &path); }
            }
        }

        HDC hdc    = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, sz, sz);
        ReleaseDC(NULL, hdc);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
        Graphics gDC(hdcMem);
        gDC.DrawImage(&bmp, 0, 0, sz, sz);
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        g_hbmEditMenuIcons[i] = hbm;

        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask      = MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;
        mii.fType      = MFT_STRING;
        mii.dwTypeData = const_cast<LPWSTR>(kEditMenuEntries[i].text);
        mii.hbmpItem   = hbm;
        SetMenuItemInfoW(hEdit, cmdId, FALSE, &mii);
    }
}

// Image menu entries — command ID, display text, and SVG path
struct ImageMenuEntry { UINT cmdId; const wchar_t* text; const char* svgD; float coordSz; };
static const ImageMenuEntry kImageMenuEntries[] = {
    { ID_IMAGE_ROTATE_RIGHT, L"Rotate &Right 90\xb0",              kImgRotateRightD, 24.0f },
    { ID_IMAGE_ROTATE_LEFT,  L"Rotate &Left 90\xb0",               kImgRotateLeftD,  24.0f },
    { ID_IMAGE_FLIP_H,       L"Flip &Horizontal",                   kImgFlipHD,       24.0f },
    { ID_IMAGE_FLIP_V,       L"Flip &Vertical",                     kImgFlipVD,       24.0f },
    { ID_IMAGE_CROP,         L"&Crop to Selection\tCtrl+Shift+X",   kStatusCropD,     20.0f },
    { ID_IMAGE_RESIZE,       L"&Resize Image...",                   kImgResizeD,      24.0f },
};

// Sets Image menu item icons using MIIM_BITMAP with opaque pre-composited bitmaps.
static void SetImageMenuIcons()
{
    if (!g_hMenuMain) return;
    HMENU hImg = GetSubMenu(g_hMenuMain, 2);
    if (!hImg) return;

    for (int i = 0; i < (int)ARRAYSIZE(g_hbmImageMenuIcons); i++) {
        if (g_hbmImageMenuIcons[i]) { DeleteObject(g_hbmImageMenuIcons[i]); g_hbmImageMenuIcons[i] = NULL; }
    }

    const int sz    = 16;
    COLORREF crBg   = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crIcon = g_darkMode ? RGB(200, 200, 200) : RGB(45, 45, 45);
    Color bgClr  (255, GetRValue(crBg),   GetGValue(crBg),   GetBValue(crBg));
    Color iconClr(255, GetRValue(crIcon), GetGValue(crIcon), GetBValue(crIcon));

    for (int i = 0; i < (int)ARRAYSIZE(kImageMenuEntries); i++) {
        Bitmap bmp(sz, sz, PixelFormat32bppRGB);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.Clear(bgClr);
            SolidBrush br(iconClr);
            float sc = (float)sz / kImageMenuEntries[i].coordSz;
            GraphicsPath path(FillModeWinding);
            AddSvgPath(path, kImageMenuEntries[i].svgD, sc);
            g.FillPath(&br, &path);
        }

        HDC hdc    = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, sz, sz);
        ReleaseDC(NULL, hdc);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
        Graphics gDC(hdcMem);
        gDC.DrawImage(&bmp, 0, 0, sz, sz);
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        g_hbmImageMenuIcons[i] = hbm;

        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask      = MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;
        mii.fType      = MFT_STRING;
        mii.dwTypeData = const_cast<LPWSTR>(kImageMenuEntries[i].text);
        mii.hbmpItem   = hbm;
        SetMenuItemInfoW(hImg, kImageMenuEntries[i].cmdId, FALSE, &mii);
    }
}

// View menu entries. Zoom, folder browser and dark mode reuse their status bar
// and toolbar artwork; svgD == nullptr items are drawn in SetViewMenuIcons.
static const ImageMenuEntry kViewMenuEntries[] = {
    { ID_VIEW_FULLSCREEN,    L"&Full Screen\tF11",        nullptr,             16.0f },
    { ID_VIEW_SLIDESHOW,     L"&Slideshow\tF5",           nullptr,             16.0f },
    { ID_ZOOM_IN,            L"Zoom &In\tCtrl+=",         kZoomInD,            20.0f },
    { ID_ZOOM_OUT,           L"Zoom &Out\tCtrl+-",        kZoomOutD,           20.0f },
    { ID_ZOOM_FIT_WINDOW,    L"Fi&t to Window\tCtrl+0",   kPageFitD,           20.0f },
    { ID_ZOOM_ACTUAL,        L"&Actual Size\tCtrl+1",     nullptr,             16.0f },
    { ID_FOLDER_BROWSER_BTN, L"Folder &Browser",          kFolderBrowserIconD, 20.0f },
    { ID_VIEW_DARKMODE,      L"&Dark Mode\tCtrl+Shift+D", kMoonD,              24.0f },
};

// Sets View menu item icons, in the same style as SetImageMenuIcons
static void SetViewMenuIcons()
{
    if (!g_hMenuMain) return;
    HMENU hView = GetSubMenu(g_hMenuMain, 3);
    if (!hView) return;

    for (auto& h : g_hbmViewMenuIcons) if (h) { DeleteObject(h); h = NULL; }

    const int sz    = 16;
    COLORREF crBg   = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crIcon = g_darkMode ? RGB(200, 200, 200) : RGB(45, 45, 45);
    Color bgClr  (255, GetRValue(crBg),   GetGValue(crBg),   GetBValue(crBg));
    Color iconClr(255, GetRValue(crIcon), GetGValue(crIcon), GetBValue(crIcon));

    for (int i = 0; i < (int)ARRAYSIZE(kViewMenuEntries); i++) {
        const ImageMenuEntry& e = kViewMenuEntries[i];
        Bitmap bmp(sz, sz, PixelFormat32bppRGB);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.Clear(bgClr);
            SolidBrush br(iconClr);
            Pen pen(iconClr, 1.2f);
            pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
            if (e.svgD) {
                GraphicsPath path(FillModeWinding);
                AddSvgPath(path, e.svgD, sz / e.coordSz);
                g.FillPath(&br, &path);
            } else if (e.cmdId == ID_VIEW_FULLSCREEN) {
                // Four corners pointing outwards
                const float a = 2.5f, b = 13.5f, k = 4.f;
                PointF tl[3] = { {a, a + k}, {a, a}, {a + k, a} }, tr[3] = { {b - k, a}, {b, a}, {b, a + k} };
                PointF bl[3] = { {a, b - k}, {a, b}, {a + k, b} }, brc[3] = { {b - k, b}, {b, b}, {b, b - k} };
                g.DrawLines(&pen, tl, 3); g.DrawLines(&pen, tr, 3); g.DrawLines(&pen, bl, 3); g.DrawLines(&pen, brc, 3);
            } else if (e.cmdId == ID_VIEW_SLIDESHOW) {
                // A screen with a play triangle
                GraphicsPath r;
                r.AddArc(1.5f, 3.f, 3.f, 3.f, 180, 90); r.AddArc(11.5f, 3.f, 3.f, 3.f, 270, 90);
                r.AddArc(11.5f, 10.f, 3.f, 3.f, 0, 90); r.AddArc(1.5f, 10.f, 3.f, 3.f, 90, 90);
                r.CloseFigure();
                g.DrawPath(&pen, &r);
                PointF tri[3] = { {6.5f, 5.5f}, {10.5f, 8.f}, {6.5f, 10.5f} };
                g.FillPolygon(&br, tri, 3);
            } else if (e.cmdId == ID_ZOOM_ACTUAL) {
                // "1:1"
                g.DrawLine(&pen, 4.f, 4.f, 4.f, 12.5f);   g.DrawLine(&pen, 2.5f, 5.5f, 4.f, 4.f);
                g.DrawLine(&pen, 12.f, 4.f, 12.f, 12.5f); g.DrawLine(&pen, 10.5f, 5.5f, 12.f, 4.f);
                g.FillEllipse(&br, 7.2f, 6.f, 1.6f, 1.6f);
                g.FillEllipse(&br, 7.2f, 9.6f, 1.6f, 1.6f);
            }
        }

        HDC hdc    = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, sz, sz);
        ReleaseDC(NULL, hdc);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
        Graphics gDC(hdcMem);
        gDC.DrawImage(&bmp, 0, 0, sz, sz);
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        g_hbmViewMenuIcons[i] = hbm;

        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask      = MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;
        mii.fType      = MFT_STRING;
        mii.dwTypeData = const_cast<LPWSTR>(e.text);
        mii.hbmpItem   = hbm;
        SetMenuItemInfoW(hView, e.cmdId, FALSE, &mii);
    }
}

static void ApplyDarkMode()
{
    BOOL dark = g_darkMode ? TRUE : FALSE;

    // uxtheme ordinals — must come BEFORE DwmSetWindowAttribute so the
    // per-window allow flag is set when DWM reads it.
    {
        typedef BOOL (WINAPI* FnAllowDark)(HWND, BOOL);
        typedef void (WINAPI* FnFlushMenuThemes)();
        typedef int  (WINAPI* FnAppMode)(int);
        typedef void (WINAPI* FnRefresh)();
        HMODULE hUx = GetModuleHandleW(L"uxtheme.dll");
        if (hUx) {
            auto fnMode    = (FnAppMode)  GetProcAddress(hUx, MAKEINTRESOURCEA(135));
            auto fnRefresh = (FnRefresh)  GetProcAddress(hUx, MAKEINTRESOURCEA(104));
            auto fnAllow   = (FnAllowDark)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
            auto fnFlush   = (FnFlushMenuThemes)GetProcAddress(hUx, MAKEINTRESOURCEA(136));
            if (fnMode)    fnMode(g_darkMode ? 2 : 3);   // 2=ForceDark, 3=ForceLight
            if (fnRefresh) fnRefresh();
            if (fnAllow)   fnAllow(g_hwndMain, dark);
            if (fnFlush)   fnFlush();
        }
        SetWindowTheme(g_hwndMain, g_darkMode ? L"DarkMode_Explorer" : NULL, NULL);
    }

    // Tell DWM to colour the title bar dark or light
    DwmSetWindowAttribute(g_hwndMain, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE
    DwmSetWindowAttribute(g_hwndMain, 19, &dark, sizeof(dark)); // pre-20H1 compat attribute

    // SWP_FRAMECHANGED recalculates NC metrics; the WM_NCACTIVATE pair forces
    // DWM to immediately re-read the attribute and repaint the title bar.
    SetWindowPos(g_hwndMain, NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    SendMessage(g_hwndMain, WM_NCACTIVATE, FALSE, 0);
    SendMessage(g_hwndMain, WM_NCACTIVATE, TRUE,  0);

    if (g_hwndToolbar)
        SetWindowTheme(g_hwndToolbar, g_darkMode ? L"DarkMode_Explorer" : L"Explorer", NULL);
    if (g_hwndStatus)
        SetWindowTheme(g_hwndStatus,  g_darkMode ? L"DarkMode_Explorer" : L"", NULL);
    if (g_hwndCanvas)
        SetWindowTheme(g_hwndCanvas,  g_darkMode ? L"DarkMode_Explorer" : L"Explorer", NULL);
    // Disable visual styles on the checkbox in dark mode so WM_CTLCOLORSTATIC can set text colour.
    // With styles active the theme engine paints the label black regardless of SetTextColor.
    if (g_hwndWandContig)
        InvalidateRect(g_hwndWandContig, NULL, TRUE);

    // Force WM_NCPAINT on edit controls so DarkEditSubclassProc redraws their borders.
    auto refreshEditBorder = [](HWND hw) {
        if (hw) SetWindowPos(hw, NULL, 0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    };
    refreshEditBorder(g_hwndBrushSz);
    refreshEditBorder(g_hwndZoomEdit);
    refreshEditBorder(g_hwndAlphaEdit);
    refreshEditBorder(g_hwndFontSize);

    // Recreate button icons BEFORE applying theme (SetWindowTheme triggers redraw)
    {
        Color ic = g_darkMode ? Color(255, 200, 200, 200) : Color(255, 45, 45, 45);
    }

    // BS_OWNERDRAW buttons render directly — just invalidate to repaint
    if (g_hwndBtnDark)    InvalidateRect(g_hwndBtnDark,    NULL, TRUE);
    if (g_hwndBtnHelp)    InvalidateRect(g_hwndBtnHelp,    NULL, TRUE);
    if (g_hwndZoomIn)     InvalidateRect(g_hwndZoomIn,     NULL, TRUE);
    if (g_hwndZoomOut)    InvalidateRect(g_hwndZoomOut,    NULL, TRUE);
    if (g_hwndZoomFit)       InvalidateRect(g_hwndZoomFit,       NULL, TRUE);
    if (g_hwndFolderBrowserBtn)  InvalidateRect(g_hwndFolderBrowserBtn,  NULL, TRUE);
    if (g_hwndFolderBrowser)    InvalidateRect(g_hwndFolderBrowser,    NULL, TRUE);
    if (g_hwndFolderSearch)     InvalidateRect(g_hwndFolderSearch,     NULL, TRUE);
    if (g_hwndFolderSearchEdit) InvalidateRect(g_hwndFolderSearchEdit, NULL, TRUE);
    if (g_hwndColorEdit)  InvalidateRect(g_hwndColorEdit,  NULL, TRUE);
    if (g_hwndReset)      InvalidateRect(g_hwndReset,      NULL, TRUE);
    if (g_hwndSzMinus)    InvalidateRect(g_hwndSzMinus,    NULL, TRUE);
    if (g_hwndSzPlus)     InvalidateRect(g_hwndSzPlus,     NULL, TRUE);
    if (g_hwndEraserSq)   InvalidateRect(g_hwndEraserSq,   NULL, TRUE);
    if (g_hwndEraserRd)   InvalidateRect(g_hwndEraserRd,   NULL, TRUE);

    // Apply theme to push buttons (triggers redraw — icons are already updated above)
    const wchar_t* btnTheme = g_darkMode ? L"DarkMode_Explorer" : L"Explorer";
    // Dark/Help overlay buttons use Toolbar theme for flat toolbar-like hover behaviour
    {
        const wchar_t* overlayTheme = g_darkMode ? L"DarkMode_Explorer" : L"Toolbar";
        if (g_hwndBtnDark) SetWindowTheme(g_hwndBtnDark, overlayTheme, NULL);
        if (g_hwndBtnHelp) SetWindowTheme(g_hwndBtnHelp, overlayTheme, NULL);
    }

    // Recreate dark bg brush
    if (g_hDarkBgBrush) { DeleteObject(g_hDarkBgBrush); g_hDarkBgBrush = NULL; }
    if (g_darkMode) g_hDarkBgBrush = CreateSolidBrush(RGB(45, 45, 48));

    // Rebuild toolbar image list with appropriate icon colour
    if (g_hToolImageList) { ImageList_Destroy(g_hToolImageList); g_hToolImageList = NULL; }
    g_hToolImageList = BuildToolImageList(g_darkMode);
    if (g_hToolImageList && g_hwndToolbar)
        SendMessage(g_hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)g_hToolImageList);

    // Recreate status bar icons with appropriate colour
    if (g_hIcoStatusBorder) { DestroyIcon(g_hIcoStatusBorder); g_hIcoStatusBorder = NULL; }
    if (g_hIcoStatusCursor) { DestroyIcon(g_hIcoStatusCursor); g_hIcoStatusCursor = NULL; }
    if (g_hIcoStatusCrop)   { DestroyIcon(g_hIcoStatusCrop);   g_hIcoStatusCrop   = NULL; }
    {
        Color iconClr = g_darkMode ? Color(255, 200, 200, 200) : Color(255, 45, 45, 45);
        g_hIcoStatusCrop   = MakeHIconFromSvg(kStatusCropD,   16, iconClr);
        g_hIcoStatusCursor = MakeHIconFromSvg(kStatusCursorD, 16, iconClr);
        g_hIcoStatusBorder = MakeHIconFromSvg(kStatusBorderD, 16, iconClr);
    }
    RebuildStatusIcons();

    // Install/uninstall the CBT hook that prevents WS_EX_DLGMODALFRAME on popup menus
    if (g_darkMode) {
        if (!g_hMenuCBTHook)
            g_hMenuCBTHook = SetWindowsHookExW(WH_CBT, MenuCBTProc, NULL, GetCurrentThreadId());
    } else {
        if (g_hMenuCBTHook) { UnhookWindowsHookEx(g_hMenuCBTHook); g_hMenuCBTHook = NULL; }
    }

    SetFileMenuIcons();
    SetEditMenuIcons();
    SetImageMenuIcons();
    SetViewMenuIcons();

    RedrawWindow(g_hwndMain, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);

    // Update dark-mode button tooltip to reflect the new state
    if (g_hwndColorTip && g_hwndBtnDark) {
        TOOLINFO ti  = {};
        ti.cbSize    = sizeof(ti);
        ti.uFlags    = TTF_IDISHWND;
        ti.hwnd      = g_hwndMain;
        ti.uId       = (UINT_PTR)g_hwndBtnDark;
        ti.lpszText  = g_darkMode ? (LPWSTR)L"Toggle light mode (Ctrl+Shift+D)"
                                  : (LPWSTR)L"Toggle dark mode (Ctrl+Shift+D)";
        SendMessage(g_hwndColorTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
    }

    // Sync help window title bar, control themes, and colours if it's open
    if (g_hwndHelp) {
        BOOL dark = g_darkMode ? TRUE : FALSE;
        // Same ordering as the main window: AllowDarkModeForWindow before DwmSetWindowAttribute
        HMODULE hUx2 = GetModuleHandleW(L"uxtheme.dll");
        if (hUx2) {
            typedef BOOL (WINAPI* FnAllowDark)(HWND, BOOL);
            auto fnAllow = (FnAllowDark)GetProcAddress(hUx2, MAKEINTRESOURCEA(133));
            if (fnAllow) fnAllow(g_hwndHelp, dark);
        }
        DwmSetWindowAttribute(g_hwndHelp, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(g_hwndHelp, 19, &dark, sizeof(dark));
        SetWindowPos(g_hwndHelp, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SendMessage(g_hwndHelp, WM_NCACTIVATE, FALSE, 0);
        SendMessage(g_hwndHelp, WM_NCACTIVATE, TRUE,  0);
        const wchar_t* ctrlTheme = g_darkMode ? L"DarkMode_Explorer" : L"Explorer";
        HWND hEdit = GetDlgItem(g_hwndHelp, IDC_HELP_TEXT);
        HWND hOK   = GetDlgItem(g_hwndHelp, IDOK);
        if (hEdit) SetWindowTheme(hEdit, ctrlTheme, NULL);
        if (hOK)   SetWindowTheme(hOK,   ctrlTheme, NULL);
        if (s_hwndSearchEdit) SetWindowTheme(s_hwndSearchEdit, ctrlTheme, NULL);
        if (s_hwndFindNext)   SetWindowTheme(s_hwndFindNext,   ctrlTheme, NULL);
        if (s_hwndFindClose)  SetWindowTheme(s_hwndFindClose,  ctrlTheme, NULL);
        RedrawWindow(g_hwndHelp, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_ERASE);
    }
}

// ============================================================
//  Drawing operations
// ============================================================
static Pen* MakePen()
{
    Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
    Pen* pen = new Pen(c, g_brushSize);
    pen->SetStartCap(LineCapRound);
    pen->SetEndCap(LineCapRound);
    pen->SetLineJoin(LineJoinRound);
    return pen;
}

// Fast xorshift RNG for brush texture effects; seeded at each stroke start.
static DWORD g_brushRandState = 1;
static inline DWORD BrushRand() {
    g_brushRandState ^= g_brushRandState << 13;
    g_brushRandState ^= g_brushRandState >> 17;
    g_brushRandState ^= g_brushRandState << 5;
    return g_brushRandState;
}

static void DrawFreehand(int x, int y, bool isEraser)
{
    if (!g_pGfxCanvas || !g_pCanvas) return;

    if (isEraser) {
        Graphics* gfx = Graphics::FromImage(g_pCanvas);
        gfx->SetSmoothingMode(SmoothingModeAntiAlias);
        gfx->SetCompositingMode(CompositingModeSourceCopy);
        Pen pen(Color(0, 0, 0, 0), (REAL)g_eraserSize);
        if (g_eraserRound) {
            pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
        } else {
            pen.SetStartCap(LineCapSquare); pen.SetEndCap(LineCapSquare); pen.SetLineJoin(LineJoinMiter);
        }
        gfx->DrawLine(&pen, (REAL)g_ptLast.x, (REAL)g_ptLast.y, (REAL)x, (REAL)y);
        delete gfx;
        g_ptLast = {x, y};
        return;
    }

    if (g_currentTool == TOOL_PENCIL) {
        Pen* pen = MakePen();   // LineCapRound / LineJoinRound
        g_pGfxCanvas->DrawLine(pen, (REAL)g_ptLast.x, (REAL)g_ptLast.y, (REAL)x, (REAL)y);
        delete pen;
        g_ptLast = {x, y};
        return;
    }

    // TOOL_BRUSH — dispatch on sub-type
    Color baseColor = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
    float dx   = (float)(x - g_ptLast.x);
    float dy   = (float)(y - g_ptLast.y);
    float dist = sqrtf(dx*dx + dy*dy);

    switch (g_brushType) {

    case BRUSH_ROUND: {
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        Pen* pen = MakePen();
        g_pGfxCanvas->DrawLine(pen, (REAL)g_ptLast.x, (REAL)g_ptLast.y, (REAL)x, (REAL)y);
        delete pen;
        break;
    }

    case BRUSH_CALLIGRAPHY: {
        // Stamp a flat ellipse tilted -45° at intervals along the segment.
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        float stepDist = max(1.0f, g_brushSize * 0.15f);
        int   steps    = max(1, (int)ceilf(dist / stepDist));
        float w        = g_brushSize;
        float h        = max(1.5f, g_brushSize * 0.2f);
        SolidBrush sb(baseColor);
        for (int i = 0; i <= steps; ++i) {
            float t  = (steps > 0) ? (float)i / steps : 0.f;
            float cx = g_ptLast.x + dx * t;
            float cy = g_ptLast.y + dy * t;
            GraphicsPath path;
            path.AddEllipse(-w * 0.5f, -h * 0.5f, w, h);
            Matrix mat;
            mat.Rotate(-45.0f);
            mat.Translate(cx, cy, MatrixOrderAppend);
            path.Transform(&mat);
            g_pGfxCanvas->FillPath(&sb, &path);
        }
        break;
    }

    case BRUSH_AIRBRUSH: {
        // Scatter random low-opacity dots in a circle around the cursor.
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        int   N      = max(6, (int)(g_brushSize * 1.5f));
        float radius = g_brushSize;
        for (int i = 0; i < N; ++i) {
            float rx = ((float)(BrushRand() & 0xFFFF) / 32767.5f) - 1.0f;
            float ry = ((float)(BrushRand() & 0xFFFF) / 32767.5f) - 1.0f;
            if (rx*rx + ry*ry > 1.0f) { --i; continue; }  // rejection sample
            float dotX = (float)x + rx * radius;
            float dotY = (float)y + ry * radius;
            Color dotColor(40, baseColor.GetR(), baseColor.GetG(), baseColor.GetB());
            SolidBrush sb(dotColor);
            g_pGfxCanvas->FillEllipse(&sb, dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
        }
        break;
    }

    case BRUSH_MARKER: {
        // Wide round-cap semi-transparent line — builds up like a felt-tip marker.
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        Color mc(100, baseColor.GetR(), baseColor.GetG(), baseColor.GetB());
        Pen pen(mc, g_brushSize * 1.5f);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinRound);
        g_pGfxCanvas->DrawLine(&pen, (REAL)g_ptLast.x, (REAL)g_ptLast.y, (REAL)x, (REAL)y);
        break;
    }

    case BRUSH_CRAYON: {
        // Stamp scattered 1×1px grains with random alpha, simulating wax texture.
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeNone);
        float stepDist   = max(1.0f, g_brushSize * 0.4f);
        int   steps      = max(1, (int)ceilf(dist / stepDist));
        float r          = g_brushSize * 0.6f;
        int   dotsPerStep = max(8, (int)(r * r * 0.8f));
        for (int i = 0; i <= steps; ++i) {
            float t  = (steps > 0) ? (float)i / steps : 0.f;
            float cx = g_ptLast.x + dx * t;
            float cy = g_ptLast.y + dy * t;
            for (int d = 0; d < dotsPerStep; ++d) {
                float rx = ((float)(BrushRand() & 0xFFFF) / 32767.5f) - 1.0f;
                float ry = ((float)(BrushRand() & 0xFFFF) / 32767.5f) - 1.0f;
                if (rx*rx + ry*ry > 1.0f) continue;
                int   px    = (int)(cx + rx * r);
                int   py    = (int)(cy + ry * r);
                BYTE  alpha = (BYTE)(60 + (BrushRand() % 121));
                Color dc(alpha, baseColor.GetR(), baseColor.GetG(), baseColor.GetB());
                SolidBrush sb(dc);
                g_pGfxCanvas->FillRectangle(&sb, (REAL)px, (REAL)py, 1.0f, 1.0f);
            }
        }
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        break;
    }

    case BRUSH_OIL: {
        // Five parallel bristle lines offset perpendicular to stroke direction.
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        const int NUM_BRISTLES = 5;
        float len   = (dist < 0.001f) ? 1.0f : dist;
        float perpX = -dy / len;
        float perpY =  dx / len;
        float maxOff = g_brushSize * 0.4f;
        auto clamp8 = [](int v) -> BYTE { return (BYTE)(v < 0 ? 0 : v > 255 ? 255 : v); };
        for (int b = 0; b < NUM_BRISTLES; ++b) {
            float offT   = (NUM_BRISTLES > 1) ? -1.0f + 2.0f * b / (NUM_BRISTLES - 1) : 0.0f;
            float off    = offT * maxOff;
            int   bv     = (int)(BrushRand() % 41) - 20;
            Color bc(baseColor.GetA(),
                clamp8((int)baseColor.GetR() + bv),
                clamp8((int)baseColor.GetG() + bv),
                clamp8((int)baseColor.GetB() + bv));
            Pen pen(bc, max(1.0f, g_brushSize * 0.25f));
            pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
            g_pGfxCanvas->DrawLine(&pen,
                (REAL)g_ptLast.x + perpX * off, (REAL)g_ptLast.y + perpY * off,
                (REAL)x          + perpX * off, (REAL)y          + perpY * off);
        }
        break;
    }

    } // end switch

    g_ptLast = {x, y};
}

// ============================================================
//  Smudge tool — drag canvas pixels in the direction of movement
// ============================================================
static void DoSmudge(int x, int y)
{
    if (!g_pCanvas) return;
    int ddx = x - g_ptLast.x, ddy = y - g_ptLast.y;
    if (ddx == 0 && ddy == 0) return;

    float len = sqrtf((float)(ddx * ddx + ddy * ddy));
    float ndx = ddx / len, ndy = ddy / len;

    int brushR  = max(2, g_smudgeSize / 2);
    int canW    = (int)g_pCanvas->GetWidth();
    int canH    = (int)g_pCanvas->GetHeight();
    float pull  = (len < (float)brushR * 0.9f) ? len : (float)brushR * 0.9f;

    int areaX = max(0, min(x, (int)g_ptLast.x) - brushR - 2);
    int areaY = max(0, min(y, (int)g_ptLast.y) - brushR - 2);
    int areaR = min(canW, max(x, (int)g_ptLast.x) + brushR + 2);
    int areaB = min(canH, max(y, (int)g_ptLast.y) + brushR + 2);
    int areaW = areaR - areaX, areaH = areaB - areaY;
    if (areaW <= 0 || areaH <= 0) return;

    Rect lockRect(areaX, areaY, areaW, areaH);
    BitmapData bd;
    if (g_pCanvas->LockBits(&lockRect,
            ImageLockModeRead | ImageLockModeWrite,
            PixelFormat32bppARGB, &bd) != Ok) return;

    BYTE* pix    = (BYTE*)bd.Scan0;
    int   stride = bd.Stride;

    for (int py = -brushR; py <= brushR; py++) {
        for (int px = -brushR; px <= brushR; px++) {
            float falloff;
            if (g_smudgeRound) {
                float d2 = (float)(px * px + py * py);
                if (d2 > (float)(brushR * brushR)) continue;
                falloff = 1.0f - sqrtf(d2) / (float)brushR;
            } else {
                float tx = fabsf((float)px) / (float)brushR;
                float ty = fabsf((float)py) / (float)brushR;
                falloff = 1.0f - max(tx, ty);
            }

            // Smooth falloff from centre to edge
            float blend   = 0.55f * falloff;

            int dx2 = (x + px) - areaX, dy2 = (y + py) - areaY;
            if (dx2 < 0 || dx2 >= areaW || dy2 < 0 || dy2 >= areaH) continue;

            // Source pixel: pulled back along the motion direction
            int sx = (int)((float)(x + px) - ndx * pull + 0.5f) - areaX;
            int sy = (int)((float)(y + py) - ndy * pull + 0.5f) - areaY;
            if (sx < 0 || sx >= areaW || sy < 0 || sy >= areaH) continue;

            BYTE* dst = pix + dy2 * stride + dx2 * 4;
            BYTE* src = pix + sy  * stride + sx  * 4;

            // Skip fully transparent sources — blending {0,0,0,0} darkens colours
            if (src[3] == 0) continue;

            // Scale blend by source alpha so semi-transparent pixels contribute less
            float eb = blend * (src[3] / 255.0f);
            dst[0] = (BYTE)(src[0] * eb + dst[0] * (1.0f - eb));
            dst[1] = (BYTE)(src[1] * eb + dst[1] * (1.0f - eb));
            dst[2] = (BYTE)(src[2] * eb + dst[2] * (1.0f - eb));
            dst[3] = (BYTE)(src[3] * blend + dst[3] * (1.0f - blend));
        }
    }

    g_pCanvas->UnlockBits(&bd);
    g_ptLast = {x, y};
}

// ============================================================
//  Stop marching ants timer and clear boundary data
// ============================================================
static void StopMarchingAnts()
{
    if (g_marchAntTimer && g_hwndCanvas) {
        KillTimer(g_hwndCanvas, 1002);
        g_marchAntTimer = 0;
    }
    g_selHEdges.clear();
    g_selVEdges.clear();
}

// ============================================================
//  Magic wand — build marching-ant boundary edges from mask
// ============================================================
static void LassoFillMask(int cw, int ch)
{
    // Scanline even-odd polygon fill from g_lassoPoints into g_selMask
    int n = (int)g_lassoPoints.size();
    if (n < 3) return;
    g_selMask.assign((size_t)cw * ch, 0);
    g_selMaskW = (UINT)cw;
    for (int y = 0; y < ch; ++y) {
        std::vector<int> xs;
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            float y0 = g_lassoPoints[i].Y, y1 = g_lassoPoints[j].Y;
            float x0 = g_lassoPoints[i].X, x1 = g_lassoPoints[j].X;
            if ((y0 <= (float)y && y1 > (float)y) ||
                (y1 <= (float)y && y0 > (float)y)) {
                int xi = (int)(x0 + ((float)y - y0) / (y1 - y0) * (x1 - x0));
                xs.push_back(xi);
            }
        }
        std::sort(xs.begin(), xs.end());
        for (int k = 0; k + 1 < (int)xs.size(); k += 2) {
            int x1c = max(0, xs[k]);
            int x2c = min(cw - 1, xs[k + 1]);
            for (int x = x1c; x <= x2c; ++x)
                g_selMask[(size_t)y * cw + x] = 1;
        }
    }
}

static void BuildWandBoundaryEdges(int cw, int ch)
{
    g_selHEdges.clear();
    g_selVEdges.clear();

    // Horizontal edges: between row y-1 and row y
    for (int y = 0; y <= ch; y++) {
        int x = 0;
        while (x < cw) {
            bool above = (y > 0  && g_selMask[(size_t)(y-1)*cw+x] != 0);
            bool below = (y < ch && g_selMask[(size_t) y   *cw+x] != 0);
            if (above == below) { x++; continue; }
            int x0 = x++;
            while (x < cw) {
                bool a2 = (y > 0  && g_selMask[(size_t)(y-1)*cw+x] != 0);
                bool b2 = (y < ch && g_selMask[(size_t) y   *cw+x] != 0);
                if (a2 != b2) x++; else break;
            }
            g_selHEdges.push_back({y, x0, x});
        }
    }

    // Vertical edges: between col x-1 and col x
    for (int x = 0; x <= cw; x++) {
        int y = 0;
        while (y < ch) {
            bool left  = (x > 0  && g_selMask[(size_t)y*cw+(x-1)] != 0);
            bool right = (x < cw && g_selMask[(size_t)y*cw+x] != 0);
            if (left == right) { y++; continue; }
            int y0 = y++;
            while (y < ch) {
                bool l2 = (x > 0  && g_selMask[(size_t)y*cw+(x-1)] != 0);
                bool r2 = (x < cw && g_selMask[(size_t)y*cw+x] != 0);
                if (l2 != r2) y++; else break;
            }
            g_selVEdges.push_back({x, y0, y});
        }
    }
}

// ============================================================
//  Magic wand — scanline flood fill selection
// ============================================================
static void WandFloodFill(int seedX, int seedY)
{
    if (!g_pCanvas) return;
    int cw = (int)g_pCanvas->GetWidth();
    int ch = (int)g_pCanvas->GetHeight();
    if (seedX < 0 || seedY < 0 || seedX >= cw || seedY >= ch) return;

    BitmapData bd;
    Rect lockRect(0, 0, cw, ch);
    if (g_pCanvas->LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok)
        return;

    BYTE* base   = (BYTE*)bd.Scan0;
    int   stride = bd.Stride;

    auto px = [&](int x, int y) -> DWORD {
        return *(DWORD*)(base + (ptrdiff_t)y * stride + x * 4);
    };

    DWORD seed = px(seedX, seedY);
    int sR = (seed >> 16) & 0xFF, sG = (seed >> 8) & 0xFF, sB = seed & 0xFF;
    int tol = g_wandTolerance;

    auto match = [&](int x, int y) -> bool {
        DWORD p = px(x, y);
        return abs(((int)((p >> 16) & 0xFF)) - sR) <= tol &&
               abs(((int)((p >>  8) & 0xFF)) - sG) <= tol &&
               abs(((int)( p        & 0xFF)) - sB) <= tol;
    };

    g_selMask.assign((size_t)cw * ch, 0);
    g_selMaskW = (UINT)cw;

    int bL = cw, bT = ch, bR = -1, bB = -1;

    if (g_wandContiguous) {
        // Flood fill from seed
        struct Span { int y, xl, xr, dy; };
        std::vector<Span> stk;
        stk.reserve(512);

        if (match(seedX, seedY)) {
            stk.push_back({seedY,     seedX, seedX,  1});
            stk.push_back({seedY - 1, seedX, seedX, -1});
        }

        while (!stk.empty()) {
            auto [y, xl, xr, dy] = stk.back(); stk.pop_back();
            if (y < 0 || y >= ch) continue;

            int L = xl;
            while (L > 0    && !g_selMask[y * cw + L - 1] && match(L - 1, y)) --L;
            int R = xr;
            while (R < cw-1 && !g_selMask[y * cw + R + 1] && match(R + 1, y)) ++R;

            for (int x = L; x <= R; ++x) g_selMask[y * cw + x] = 1;

            if (L < bL) bL = L;  if (R + 1 > bR) bR = R + 1;
            if (y < bT) bT = y;  if (y + 1 > bB) bB = y + 1;

            auto pushSpans = [&](int ny, int x1, int x2, int ndy) {
                if (ny < 0 || ny >= ch) return;
                int x = x1;
                while (x <= x2) {
                    while (x <= x2 && (g_selMask[ny * cw + x] || !match(x, ny))) ++x;
                    if (x > x2) break;
                    int sx = x;
                    while (x <= x2 && !g_selMask[ny * cw + x] && match(x, ny)) ++x;
                    stk.push_back({ny, sx, x - 1, ndy});
                }
            };

            pushSpans(y + dy,  L,    R,    dy);
            pushSpans(y - dy,  L,    xl-1, -dy);
            pushSpans(y - dy,  xr+1, R,    -dy);
        }
    } else {
        // Non-contiguous: select all matching pixels on the canvas
        for (int y = 0; y < ch; y++) {
            for (int x = 0; x < cw; x++) {
                if (match(x, y)) {
                    g_selMask[(size_t)y * cw + x] = 1;
                    if (x     < bL) bL = x;
                    if (x + 1 > bR) bR = x + 1;
                    if (y     < bT) bT = y;
                    if (y + 1 > bB) bB = y + 1;
                }
            }
        }
    }

    g_pCanvas->UnlockBits(&bd);

    if (bR < 0) { StopMarchingAnts(); g_hasSelMask = false; g_hasSelection = false; return; }

    g_hasSelMask   = true;
    g_hasSelection = true;
    g_selRect = { (LONG)bL, (LONG)bT, (LONG)bR, (LONG)bB };

    // Build boundary edges for marching ants animation
    BuildWandBoundaryEdges(cw, ch);

    // Start marching ants timer (ID 1002, 80 ms ≈ 12 fps)
    if (!g_marchAntTimer && g_hwndCanvas)
        g_marchAntTimer = SetTimer(g_hwndCanvas, 1002, 80, NULL);

    UpdateStatusBar(0, 0);
    InvalidateCanvas();
}

// ============================================================
//  Bucket fill
// ============================================================
static void FloodFill(int x, int y)
{
    if (!g_pCanvas) return;
    int W = (int)g_pCanvas->GetWidth();
    int H = (int)g_pCanvas->GetHeight();
    if (x < 0 || y < 0 || x >= W || y >= H) return;

    BitmapData bd;
    Rect lockRect(0, 0, W, H);
    if (g_pCanvas->LockBits(&lockRect, ImageLockModeRead | ImageLockModeWrite,
                             PixelFormat32bppARGB, &bd) != Ok) return;

    BYTE* pixels = (BYTE*)bd.Scan0;
    int   stride = bd.Stride;

    auto ref4 = [&](int gx, int gy) -> DWORD& {
        return *reinterpret_cast<DWORD*>(pixels + gy * stride + gx * 4);
    };

    DWORD seed  = ref4(x, y);
    int sR = (seed >> 16) & 0xFF, sG = (seed >> 8) & 0xFF, sB = seed & 0xFF;
    int tol = g_fillTolerance;

    Color fc        = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
    DWORD fillColor = fc.GetValue();

    // With tol==0 and identical RGB, nothing would change.
    // (With tol>0 we still proceed — nearby similar pixels should be filled.)
    if (tol == 0 && (fillColor & 0x00FFFFFFu) == (seed & 0x00FFFFFFu)) {
        g_pCanvas->UnlockBits(&bd);
        return;
    }

    auto matches = [&](int gx, int gy) -> bool {
        DWORD p = ref4(gx, gy);
        return abs((int)((p >> 16) & 0xFF) - sR) <= tol &&
               abs((int)((p >>  8) & 0xFF) - sG) <= tol &&
               abs((int)( p        & 0xFF) - sB) <= tol;
    };

    // Visited mask — needed because fill color can be within tolerance of the
    // seed color, which would make filled pixels re-match and loop forever.
    std::vector<bool> visited((size_t)(W * H), false);

    // Scanline span fill mirroring WandFloodFill.
    // Stack entries are hint spans; pushSpans finds unvisited matching sub-runs
    // within a hint range, keeping stack size proportional to boundary complexity.
    struct Span { int y, xl, xr, dy; };
    std::vector<Span> stk;
    stk.reserve(512);

    if (matches(x, y)) {
        stk.push_back({y,     x, x,  1});
        stk.push_back({y - 1, x, x, -1});
    }

    while (!stk.empty()) {
        auto [sy, xl, xr, sdy] = stk.back(); stk.pop_back();
        if (sy < 0 || sy >= H) continue;

        int L = xl;
        while (L > 0     && !visited[sy * W + L - 1] && matches(L - 1, sy)) --L;
        int R = xr;
        while (R < W - 1 && !visited[sy * W + R + 1] && matches(R + 1, sy)) ++R;

        for (int i = L; i <= R; ++i) { visited[sy * W + i] = true; ref4(i, sy) = fillColor; }

        auto pushSpans = [&](int ny, int x1, int x2, int ndy) {
            if (ny < 0 || ny >= H || x1 > x2) return;
            for (int xi = x1; xi <= x2; ) {
                while (xi <= x2 && (visited[ny * W + xi] || !matches(xi, ny))) ++xi;
                if (xi > x2) break;
                int sx = xi;
                while (xi <= x2 && !visited[ny * W + xi] && matches(xi, ny)) ++xi;
                stk.push_back({ny, sx, xi - 1, ndy});
            }
        };

        pushSpans(sy + sdy,  L,     R,     sdy);
        pushSpans(sy - sdy,  L,     xl-1, -sdy);
        pushSpans(sy - sdy,  xr+1,  R,    -sdy);
    }

    g_pCanvas->UnlockBits(&bd);
}

// ============================================================
//  Preview layer (in-progress shapes and rubber-band)
// ============================================================
static void ClearPreview()
{
    if (!g_pPreview) return;
    Graphics gfx(g_pPreview);
    gfx.Clear(Color(0, 0, 0, 0));
    g_previewUsed = false;
}

static void RenderPreview(int mx, int my)
{
    if (!g_pPreview) return;
    g_previewUsed = true;
    Graphics gfx(g_pPreview);
    gfx.Clear(Color(0, 0, 0, 0));
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);

    REAL x1 = (REAL)g_ptStart.x, y1 = (REAL)g_ptStart.y;
    REAL x2 = (REAL)mx,          y2 = (REAL)my;

    // When a shape is in the editing buffer with a rotation, spin the Graphics
    // context around the bounding-box centre before drawing.
    if (g_shapeEditing && (g_shapeEditAngle != 0.0f || g_shapeEditFlipH || g_shapeEditFlipV)) {
        REAL cx = (x1 + x2) / 2.0f, cy = (y1 + y2) / 2.0f;
        Matrix mat;
        mat.Translate(cx, cy);
        if (g_shapeEditAngle != 0.0f) mat.Rotate(g_shapeEditAngle);
        if (g_shapeEditFlipH)         mat.Scale(-1.0f,  1.0f);
        if (g_shapeEditFlipV)         mat.Scale( 1.0f, -1.0f);
        mat.Translate(-cx, -cy);
        gfx.SetTransform(&mat);
    }

    switch (g_currentTool) {
    case TOOL_LINE: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        gfx.DrawLine(&pen, x1, y1, x2, y2);
        break;
    }
    case TOOL_RECT: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillRectangle(&fill, rx, ry, rw, rh); }
        gfx.DrawRectangle(&pen, rx, ry, rw, rh);
        break;
    }
    case TOOL_ELLIPSE: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillEllipse(&fill, rx, ry, rw, rh); }
        gfx.DrawEllipse(&pen, rx, ry, rw, rh);
        break;
    }
    case TOOL_SELECT: {
        // Rubber-band dashed rectangle
        float dashes[] = {4.0f, 4.0f};
        Pen p1(Color(0,0,0), 1.0f);
        p1.SetDashPattern(dashes, 2);
        Pen p2(Color(255,255,255), 1.0f);
        p2.SetDashPattern(dashes, 2);
        p2.SetDashOffset(4.0f);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        gfx.DrawRectangle(&p1, rx, ry, rw, rh);
        gfx.DrawRectangle(&p2, rx, ry, rw, rh);
        break;
    }
    case TOOL_RRECT: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 4.f || rh < 4.f) break;
        REAL r = min(rw, rh) * 0.2f; if (r < 1.f) r = 1.f;
        GraphicsPath path;
        path.AddArc(rx,         ry,         r*2, r*2, 180.f,  90.f);
        path.AddArc(rx+rw-r*2,  ry,         r*2, r*2, 270.f,  90.f);
        path.AddArc(rx+rw-r*2,  ry+rh-r*2,  r*2, r*2,   0.f,  90.f);
        path.AddArc(rx,         ry+rh-r*2,  r*2, r*2,  90.f,  90.f);
        path.CloseFigure();
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_TRIANGLE: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        PointF tri[] = {PointF(rx+rw/2, ry), PointF(rx+rw, ry+rh), PointF(rx, ry+rh)};
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPolygon(&fill, tri, 3); }
        gfx.DrawPolygon(&pen, tri, 3);
        break;
    }
    case TOOL_ARROW_L:
    case TOOL_ARROW_R:
    case TOOL_ARROW_U:
    case TOOL_ARROW_D: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        PointF pts[7];
        if (g_currentTool == TOOL_ARROW_R) {
            pts[0]={rx,      ry+rh*0.35f}; pts[1]={rx+rw*0.55f,ry+rh*0.35f};
            pts[2]={rx+rw*0.55f,ry};       pts[3]={rx+rw,ry+rh*0.5f};
            pts[4]={rx+rw*0.55f,ry+rh};    pts[5]={rx+rw*0.55f,ry+rh*0.65f};
            pts[6]={rx,      ry+rh*0.65f};
        } else if (g_currentTool == TOOL_ARROW_L) {
            pts[0]={rx+rw,   ry+rh*0.35f}; pts[1]={rx+rw*0.45f,ry+rh*0.35f};
            pts[2]={rx+rw*0.45f,ry};       pts[3]={rx,ry+rh*0.5f};
            pts[4]={rx+rw*0.45f,ry+rh};    pts[5]={rx+rw*0.45f,ry+rh*0.65f};
            pts[6]={rx+rw,   ry+rh*0.65f};
        } else if (g_currentTool == TOOL_ARROW_U) {
            pts[0]={rx+rw*0.35f,ry+rh};   pts[1]={rx+rw*0.35f,ry+rh*0.45f};
            pts[2]={rx,ry+rh*0.45f};      pts[3]={rx+rw*0.5f,ry};
            pts[4]={rx+rw,ry+rh*0.45f};   pts[5]={rx+rw*0.65f,ry+rh*0.45f};
            pts[6]={rx+rw*0.65f,ry+rh};
        } else { // TOOL_ARROW_D
            pts[0]={rx+rw*0.35f,ry};      pts[1]={rx+rw*0.35f,ry+rh*0.55f};
            pts[2]={rx,ry+rh*0.55f};      pts[3]={rx+rw*0.5f,ry+rh};
            pts[4]={rx+rw,ry+rh*0.55f};   pts[5]={rx+rw*0.65f,ry+rh*0.55f};
            pts[6]={rx+rw*0.65f,ry};
        }
        GraphicsPath path; path.AddPolygon(pts, 7);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_ARROW: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        REAL dx = x2 - x1, dy = y2 - y1;
        REAL len = sqrtf(dx*dx + dy*dy);
        if (len < 2.f) break;
        REAL ux = dx / len, uy = dy / len;
        const REAL HEAD = max(16.f, g_brushSize * 5.f);     // scales with line width
        REAL bx = x2 - ux * HEAD, by = y2 - uy * HEAD;     // arrowhead base centre
        REAL px = -uy * HEAD * 0.4f, py = ux * HEAD * 0.4f; // perpendicular half-width
        Pen pen(c, g_brushSize); pen.SetStartCap(LineCapSquare); pen.SetEndCap(LineCapFlat);
        gfx.DrawLine(&pen, x1, y1, bx, by);
        PointF head[] = {PointF(x2, y2), PointF(bx+px, by+py), PointF(bx-px, by-py)};
        SolidBrush br(c); gfx.FillPolygon(&br, head, 3);
        break;
    }
    case TOOL_DIAMOND: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        PointF pts[] = {
            {rx + rw*0.5f, ry},
            {rx + rw,      ry + rh*0.5f},
            {rx + rw*0.5f, ry + rh},
            {rx,           ry + rh*0.5f},
        };
        GraphicsPath path; path.AddPolygon(pts, 4);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_PENTAGON: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        REAL cx2 = rx + rw*0.5f, cy2 = ry + rh*0.5f;
        PointF pts[5];
        for (int i = 0; i < 5; ++i) {
            float ang = -3.14159265f*0.5f + i * 2.f*3.14159265f / 5.f;
            pts[i] = { cx2 + rw*0.5f*cosf(ang), cy2 + rh*0.5f*sinf(ang) };
        }
        GraphicsPath path; path.AddPolygon(pts, 5);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_HEXAGON_H: {
        // flat top and bottom edges
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        PointF pts[] = {
            {rx + rw*0.25f, ry},
            {rx + rw*0.75f, ry},
            {rx + rw,       ry + rh*0.5f},
            {rx + rw*0.75f, ry + rh},
            {rx + rw*0.25f, ry + rh},
            {rx,            ry + rh*0.5f},
        };
        GraphicsPath path; path.AddPolygon(pts, 6);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_HEXAGON_V: {
        // flat left and right edges
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        PointF pts[] = {
            {rx + rw*0.5f, ry},
            {rx + rw,      ry + rh*0.25f},
            {rx + rw,      ry + rh*0.75f},
            {rx + rw*0.5f, ry + rh},
            {rx,           ry + rh*0.75f},
            {rx,           ry + rh*0.25f},
        };
        GraphicsPath path; path.AddPolygon(pts, 6);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_OCTAGON: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        const REAL k = 0.2929f;  // 1 - 1/sqrt(2)
        PointF pts[] = {
            {rx + rw*k,        ry},
            {rx + rw*(1.f-k),  ry},
            {rx + rw,          ry + rh*k},
            {rx + rw,          ry + rh*(1.f-k)},
            {rx + rw*(1.f-k),  ry + rh},
            {rx + rw*k,        ry + rh},
            {rx,               ry + rh*(1.f-k)},
            {rx,               ry + rh*k},
        };
        GraphicsPath path; path.AddPolygon(pts, 8);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_STICKMAN: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        REAL cx2 = rx + rw*0.5f;
        REAL headR = min(rw*0.22f, rh*0.18f);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillEllipse(&fill, cx2-headR, ry, headR*2, headR*2); }
        gfx.DrawEllipse(&pen, cx2-headR, ry, headR*2, headR*2);
        REAL bodyT = ry+headR*2, bodyB = ry+rh*0.62f;
        gfx.DrawLine(&pen, cx2, bodyT, cx2, bodyB);
        REAL armY = ry + rh*0.38f;
        gfx.DrawLine(&pen, rx+rw*0.05f, ry+rh*0.48f, cx2, armY);
        gfx.DrawLine(&pen, cx2, armY, rx+rw*0.95f, ry+rh*0.48f);
        gfx.DrawLine(&pen, cx2, bodyB, rx+rw*0.15f, ry+rh);
        gfx.DrawLine(&pen, cx2, bodyB, rx+rw*0.85f, ry+rh);
        break;
    }
    case TOOL_SPEECH: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 8.f || rh < 8.f) break;
        REAL bh = rh * 0.78f;
        REAL r = min(rw, bh) * 0.14f; if (r < 1.f) r = 1.f;
        // Tail direction: dragging top→down follows mouse x (x2), dragging bottom→up follows start x (x1)
        bool tailRight = ((y1 < y2) ? x2 : x1) > rx + rw * 0.5f;
        GraphicsPath path;
        path.AddArc(rx,         ry,         r*2, r*2, 180.f, 90.f);
        path.AddArc(rx+rw-r*2,  ry,         r*2, r*2, 270.f, 90.f);
        path.AddArc(rx+rw-r*2,  ry+bh-r*2,  r*2, r*2,   0.f, 90.f);
        if (tailRight) {
            // Tail exits bottom-right: base spans 0.54–0.80, tip at 0.92
            path.AddLine(PointF(rx+rw*0.80f, ry+bh), PointF(rx+rw*0.92f, ry+rh));
            path.AddLine(PointF(rx+rw*0.92f, ry+rh), PointF(rx+rw*0.54f, ry+bh));
        } else {
            // Tail exits bottom-left: base spans 0.20–0.46, tip at 0.08
            path.AddLine(PointF(rx+rw*0.46f, ry+bh), PointF(rx+rw*0.08f, ry+rh));
            path.AddLine(PointF(rx+rw*0.08f, ry+rh), PointF(rx+rw*0.20f, ry+bh));
        }
        path.AddArc(rx,         ry+bh-r*2,  r*2, r*2,  90.f, 90.f);
        path.CloseFigure();
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_COMPUTER: {
        // Layout derived from computer_shape.svg (viewBox 0 0 1024 1024)
        // Shape bbox in SVG: x 176-848 (w=672), y 232-792 (h=560)
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 8.f || rh < 8.f) break;
        SolidBrush bgFill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte()));
        // Monitor outer rounded rect (80% of height)
        REAL mH = rh * 0.800f;
        REAL cr = max(2.f, min(rw, mH) * 0.054f);
        {
            GraphicsPath mp;
            mp.AddArc(rx,          ry,          cr*2, cr*2, 180, 90);
            mp.AddArc(rx+rw-cr*2,  ry,          cr*2, cr*2, 270, 90);
            mp.AddArc(rx+rw-cr*2,  ry+mH-cr*2,  cr*2, cr*2,   0, 90);
            mp.AddArc(rx,          ry+mH-cr*2,  cr*2, cr*2,  90, 90);
            mp.CloseFigure();
            if (g_bgAlpha > 0) gfx.FillPath(&bgFill, &mp);
            gfx.DrawPath(&pen, &mp);
        }
        // Inner screen rect (stroke only — bezel is the gap between outer and screen)
        gfx.DrawRectangle(&pen, rx+rw*0.071f, ry+rh*0.086f, rw*0.857f, rh*0.629f);
        // Neck
        REAL nX = rx+rw*0.452f, nY = ry+rh*0.771f, nW = rw*0.071f, nH = rh*0.171f;
        if (g_bgAlpha > 0) gfx.FillRectangle(&bgFill, nX, nY, nW, nH);
        gfx.DrawRectangle(&pen, nX, nY, nW, nH);
        // Base
        REAL bX = rx+rw*0.381f, bY = ry+rh*0.914f, bW = rw*0.214f, bH = rh*0.086f;
        if (g_bgAlpha > 0) gfx.FillRectangle(&bgFill, bX, bY, bW, bH);
        gfx.DrawRectangle(&pen, bX, bY, bW, bH);
        break;
    }
    case TOOL_STAR: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        REAL cx2 = rx+rw*0.5f, cy2 = ry+rh*0.5f;
        PointF pts[10];
        for (int i = 0; i < 10; i++) {
            float ang = (i*36 - 90) * 3.14159265f / 180.f;
            float orx = rw*0.5f, ory = rh*0.5f;
            float irx = orx*0.4f, iry = ory*0.4f;
            pts[i] = PointF(cx2 + (i%2==0?orx:irx)*cosf(ang),
                            cy2 + (i%2==0?ory:iry)*sinf(ang));
        }
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPolygon(&fill, pts, 10); }
        gfx.DrawPolygon(&pen, pts, 10);
        break;
    }
    case TOOL_HEART: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 4.f || rh < 4.f) break;
        GraphicsPath path;
        AddSvgPath(path, kHeartD, 1.0f);
        // Map from viewBox space to drawing rect (non-uniform scale preserves shape)
        Matrix mat(rw / kHeartVW, 0, 0, rh / kHeartVH, rx, ry);
        path.Transform(&mat);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_LIGHTNING: {
        // 6-point polygon derived from lightning_shape.svg (viewBox 0 0 1024 1024)
        // Shape bbox in SVG: x 262-771 (w=508), y 152-865 (h=713)
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        PointF pts[] = {
            {rx+rw*0.685f, ry            },  // top
            {rx,           ry+rh*0.587f  },  // left outer
            {rx+rw*0.469f, ry+rh*0.587f  },  // inner notch left
            {rx+rw*0.374f, ry+rh         },  // bottom
            {rx+rw,        ry+rh*0.421f  },  // right outer
            {rx+rw*0.586f, ry+rh*0.421f  },  // inner notch right
        };
        GraphicsPath lpath; lpath.AddPolygon(pts, 6);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &lpath); }
        gfx.DrawPath(&pen, &lpath);
        break;
    }
    case TOOL_MUSTACHE: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f) rw = 2.f; if (rh < 2.f) rh = 2.f;
        // Glasses occupy the top 55% of the bounding box
        REAL gh = rh * 0.55f;
        REAL lensW = rw * 0.38f, lensH = gh;
        // Left lens
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillEllipse(&fill, rx, ry, lensW, lensH); }
        gfx.DrawEllipse(&pen, rx,             ry, lensW, lensH);
        // Right lens
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillEllipse(&fill, rx+rw-lensW, ry, lensW, lensH); }
        gfx.DrawEllipse(&pen, rx+rw-lensW,    ry, lensW, lensH);
        // Bridge between inner edges
        REAL bridgeY = ry + lensH * 0.5f;
        gfx.DrawLine(&pen, PointF(rx+lensW, bridgeY), PointF(rx+rw-lensW, bridgeY));
        // Short arm stubs on outer edges
        gfx.DrawLine(&pen, PointF(rx,      bridgeY), PointF(rx - rw*0.05f, bridgeY - gh*0.1f));
        gfx.DrawLine(&pen, PointF(rx+rw,   bridgeY), PointF(rx+rw + rw*0.05f, bridgeY - gh*0.1f));
        // Mustache occupies bottom 50% — two downward arcs + central dip
        REAL my = ry + rh * 0.52f;    // top of mustache arcs
        REAL mh = rh * 0.48f;         // height of mustache
        REAL mw = rw * 0.48f;
        // Left curl: arc from center sweeping down-left (0° to 180°)
        gfx.DrawArc(&pen, rx,              my, mw, mh, 0.f,  180.f);
        // Right curl: mirror
        gfx.DrawArc(&pen, rx+rw-mw,        my, mw, mh, 0.f,  180.f);
        // Central dip (small upward arc at join)
        REAL dipW = rw * 0.18f, dipH = mh * 0.4f;
        gfx.DrawArc(&pen, rx+rw*0.5f-dipW*0.5f, my, dipW, dipH, 0.f, -180.f);
        break;
    }
    case TOOL_SPIRAL: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 4.f || rh < 4.f) break;
        // Archimedean spiral: 2.5 turns, centre at bounding-box centre
        // r grows from 0 to half the smaller dimension, scaled to ellipse
        const int N = 200;
        const float turns = 2.5f;
        const float PI2 = 6.28318530f;
        REAL cx2 = rx + rw * 0.5f, cy2 = ry + rh * 0.5f;
        REAL radX = rw * 0.5f, radY = rh * 0.5f;
        std::vector<PointF> pts(N + 1);
        for (int i = 0; i <= N; ++i) {
            float t   = (float)i / (float)N;
            float ang = t * turns * PI2 - PI2 * 0.5f;  // start at top
            float r   = t;  // normalised radius 0..1
            pts[i].X = cx2 + r * radX * cosf(ang);
            pts[i].Y = cy2 + r * radY * sinf(ang);
        }
        gfx.DrawLines(&pen, pts.data(), (int)pts.size());
        break;
    }
    case TOOL_SPIRAL_R: {
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 4.f || rh < 4.f) break;
        const int N = 200;
        const float turns = 2.5f;
        const float PI2 = 6.28318530f;
        REAL cx2 = rx + rw * 0.5f, cy2 = ry + rh * 0.5f;
        REAL radX = rw * 0.5f, radY = rh * 0.5f;
        std::vector<PointF> pts(N + 1);
        for (int i = 0; i <= N; ++i) {
            float t   = (float)i / (float)N;
            float ang = -(t * turns * PI2) - PI2 * 0.5f;  // opposite winding
            float r   = t;
            pts[i].X = cx2 + r * radX * cosf(ang);
            pts[i].Y = cy2 + r * radY * sinf(ang);
        }
        gfx.DrawLines(&pen, pts.data(), (int)pts.size());
        break;
    }
    case TOOL_CROSS: {
        // Plus / cross shape — 12-point polygon on 24×24 viewBox
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        REAL sx = rw / 24.0f, sy = rh / 24.0f;
        static const float kCrossX[] = {10,14,14,22,22,14,14,10,10, 2, 2,10};
        static const float kCrossY[] = { 2, 2,10,10,14,14,22,22,14,14,10,10};
        PointF pts[12];
        for (int i = 0; i < 12; i++) pts[i] = PointF(rx + kCrossX[i]*sx, ry + kCrossY[i]*sy);
        GraphicsPath path; path.AddPolygon(pts, 12);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_XMARK: {
        // Diagonal X shape — 12-point polygon on 24×24 viewBox
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        REAL sx = rw / 24.0f, sy = rh / 24.0f;
        // Plus polygon rotated 45° around centre (12,12) — gives same arm width as the Plus
        static const float kXX[] = {17.7f,20.5f,14.8f,20.5f,17.7f,12.0f, 6.3f, 3.5f, 9.2f, 3.5f, 6.3f,12.0f};
        static const float kXY[] = { 3.5f, 6.3f,12.0f,17.7f,20.5f,14.8f,20.5f,17.7f,12.0f, 6.3f, 3.5f, 9.2f};
        PointF pts[12];
        for (int i = 0; i < 12; i++) pts[i] = PointF(rx + kXX[i]*sx, ry + kXY[i]*sy);
        GraphicsPath path; path.AddPolygon(pts, 12);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_CHECK: {
        // Checkmark / tick — 6-point polygon on 24×24 viewBox
        Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
        Pen pen(c, g_brushSize);
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        REAL sx = rw / 24.0f, sy = rh / 24.0f;
        static const float kCkX[] = { 2.f, 4.5f, 9.f,21.5f,24.f, 9.f};
        static const float kCkY[] = {12.f, 9.f,15.f, 3.5f, 6.f,21.f};
        PointF pts[6];
        for (int i = 0; i < 6; i++) pts[i] = PointF(rx + kCkX[i]*sx, ry + kCkY[i]*sy);
        GraphicsPath path; path.AddPolygon(pts, 6);
        if (g_bgAlpha > 0) { SolidBrush fill(GdipColorFromCOLORREF(g_bgColor, BgAlphaByte())); gfx.FillPath(&fill, &path); }
        gfx.DrawPath(&pen, &path);
        break;
    }
    case TOOL_TEXT: {
        if (g_textStr.empty()) break;
        REAL rx = min(x1,x2), ry = min(y1,y2);
        REAL rw = fabsf(x2-x1), rh = fabsf(y2-y1);
        if (rw < 2.f || rh < 2.f) break;
        int gdipStyle = FontStyleRegular;
        if (g_textFontStyle & 1) gdipStyle |= FontStyleBold;
        if (g_textFontStyle & 2) gdipStyle |= FontStyleItalic;
        if (g_textFontStyle & 4) gdipStyle |= FontStyleUnderline;
        FontFamily ff(g_textFontName);
        FontFamily* pFF = (ff.GetLastStatus() == Ok) ? &ff : nullptr;
        FontFamily ffFb(L"Arial"); if (!pFF) pFF = &ffFb;
        Font font(pFF, (REAL)g_textFontSize, gdipStyle, UnitPoint);
        SolidBrush brush(GdipColorFromCOLORREF(g_fgColor, FgAlphaByte()));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentNear);
        sf.SetLineAlignment(StringAlignmentNear);
        RectF layoutRect(rx, ry, rw, rh);
        gfx.SetTextRenderingHint(TextRenderingHintAntiAlias);
        gfx.DrawString(g_textStr.c_str(), -1, &font, layoutRect, &sf, &brush);
        gfx.SetTextRenderingHint(TextRenderingHintSystemDefault);
        break;
    }
    default:
        break;
    }
}

static void CommitPreview()
{
    if (!g_pGfxCanvas || !g_pPreview) return;
    g_pGfxCanvas->DrawImage(g_pPreview, 0, 0);
    ClearPreview();
}

// Re-render the shape/bezier editing preview using the current colour and
// brush-size globals.  Call this whenever FG colour or line width changes
// while a shape is still in the editing buffer.
static void RefreshEditingPreview()
{
    if (g_shapeEditing) {
        g_ptStart = g_shapeEditPt1;
        Tool savedTool = g_currentTool;
        g_currentTool  = g_shapeEditTool;
        RenderPreview(g_shapeEditPt2.x, g_shapeEditPt2.y);
        g_currentTool  = savedTool;
        InvalidateCanvas();
    } else if (g_bezEditing) {
        // DrawBezierArrow reads g_fgColor / g_brushSize live in WM_PAINT
        InvalidateCanvas();
    }
}

// ============================================================
//  Bezier arrow helpers
// ============================================================
// Draw the bezier curve + arrowhead into any Graphics (in canvas coordinates).
static void DrawBezierArrow(Graphics& gfx)
{
    Color c = GdipColorFromCOLORREF(g_fgColor, FgAlphaByte());
    PointF& P0 = g_bezPts[0]; PointF& P1 = g_bezPts[1];
    PointF& P2 = g_bezPts[2]; PointF& P3 = g_bezPts[3];

    // Arrowhead direction: tangent at P3 = P3−P2 (fallback P3−P1, P3−P0)
    REAL dx = P3.X - P2.X, dy = P3.Y - P2.Y;
    REAL len = sqrtf(dx*dx + dy*dy);
    if (len < 0.5f) { dx = P3.X - P1.X; dy = P3.Y - P1.Y; len = sqrtf(dx*dx + dy*dy); }
    if (len < 0.5f) { dx = P3.X - P0.X; dy = P3.Y - P0.Y; len = sqrtf(dx*dx + dy*dy); }

    if (len >= 0.5f) {
        REAL ux = dx/len, uy = dy/len;
        const REAL HEAD = max(16.f, g_brushSize * 5.f);
        REAL bx = P3.X - ux*HEAD, by = P3.Y - uy*HEAD;   // arrowhead base centre

        // Curve ends at arrowhead base so the line doesn't poke through the head
        GraphicsPath path;
        path.AddBezier(P0, P1, P2, PointF(bx, by));
        Pen pen(c, g_brushSize);
        pen.SetStartCap(LineCapSquare);
        pen.SetEndCap(LineCapFlat);
        gfx.DrawPath(&pen, &path);

        // Arrowhead triangle (tip = P3, base corners = bx±perp)
        REAL px = -uy*HEAD*0.4f, py = ux*HEAD*0.4f;
        PointF head[] = { PointF(P3.X, P3.Y), PointF(bx+px, by+py), PointF(bx-px, by-py) };
        SolidBrush br(c);
        gfx.FillPolygon(&br, head, 3);
    } else {
        // Degenerate (P0≈P3): just draw the curve stub
        GraphicsPath path;
        path.AddBezier(P0, P1, P2, P3);
        Pen pen(c, g_brushSize);
        pen.SetStartCap(LineCapSquare); pen.SetEndCap(LineCapRound);
        gfx.DrawPath(&pen, &path);
    }
}

static void CommitBezier()
{
    if (!g_bezEditing || !g_pGfxCanvas) return;
    PushUndo();
    g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    DrawBezierArrow(*g_pGfxCanvas);
    g_bezEditing  = false;
    g_bezDragMode = 0;
    g_modified = true;
    UpdateTitleBar();
    InvalidateCanvas();
}

// Updates the reference bbox from the current point positions without touching g_bezAngle.
// When rotated, inverse-rotates the points to get their local-frame bounding box.
static void UpdateBezRefBBox()
{
    if (g_bezAngle == 0.0f) {
        g_bezRefL = min({g_bezPts[0].X, g_bezPts[1].X, g_bezPts[2].X, g_bezPts[3].X});
        g_bezRefT = min({g_bezPts[0].Y, g_bezPts[1].Y, g_bezPts[2].Y, g_bezPts[3].Y});
        g_bezRefR = max({g_bezPts[0].X, g_bezPts[1].X, g_bezPts[2].X, g_bezPts[3].X});
        g_bezRefB = max({g_bezPts[0].Y, g_bezPts[1].Y, g_bezPts[2].Y, g_bezPts[3].Y});
    } else {
        REAL pivX = (g_bezRefL + g_bezRefR) / 2.f, pivY = (g_bezRefT + g_bezRefB) / 2.f;
        float rad = -g_bezAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad);
        REAL mnX = 1e9f, mnY = 1e9f, mxX = -1e9f, mxY = -1e9f;
        for (int i = 0; i < 4; i++) {
            float dx = g_bezPts[i].X - pivX, dy = g_bezPts[i].Y - pivY;
            REAL lx = pivX + dx * c - dy * s, ly = pivY + dx * s + dy * c;
            mnX = min(mnX, lx); mnY = min(mnY, ly);
            mxX = max(mxX, lx); mxY = max(mxY, ly);
        }
        // Force bbox to be SYMMETRIC around the pivot so the centre never drifts.
        // If the result were asymmetric (P1/P2 offset to one side), (g_bezRefL+g_bezRefR)/2
        // would shift away from pivX each frame, corrupting anchor calculations and
        // eventually collapsing W0 to near-zero → explosive scale on the next resize.
        REAL hw = max(mxX - pivX, pivX - mnX);
        REAL hh = max(mxY - pivY, pivY - mnY);
        if (hw < 1.f) hw = 1.f;
        if (hh < 1.f) hh = 1.f;
        g_bezRefL = pivX - hw; g_bezRefR = pivX + hw;
        g_bezRefT = pivY - hh; g_bezRefB = pivY + hh;
    }
}

// Resets angle to 0 and snaps the ref bbox. Use only when starting a fresh edit.
static void ResetBezRefBBox()
{
    g_bezAngle = 0.0f;
    UpdateBezRefBBox();
}

static void CancelBezier()
{
    if (!g_bezEditing) return;
    g_bezEditing  = false;
    g_bezDragMode = 0;
    InvalidateCanvas();
}

// ============================================================
//  Edit operations — Select / Cut / Copy / Paste
// ============================================================
static void DoSelectAll()
{
    if (!g_pCanvas) return;
    // Stamp any floating content onto the canvas first — select-all means
    // "select everything on the canvas", so the canvas must be complete.
    if (g_isPasting)   CommitPaste();
    if (g_selFloating) CommitSelectionFloat();
    StopMarchingAnts();
    g_hasSelMask = false;
    g_selRect = { 0, 0, (LONG)g_pCanvas->GetWidth(), (LONG)g_pCanvas->GetHeight() };
    g_hasSelection = true;
    SetTool(TOOL_SELECT);
    // Ensure the canvas has focus so Delete/arrow keys work even after a
    // file-open dialog (which leaves focus on the main window, not the canvas).
    if (g_hwndCanvas) SetFocus(g_hwndCanvas);
    InvalidateCanvas();
}

// Place a GDI+ Bitmap on the clipboard as CF_BITMAP + CF_DIB (for external apps) +
// private ARGB format (for lossless round-trip within MyPaint).
// Caller must have already called OpenClipboard + EmptyClipboard.
static void ClipboardPutBitmap(Bitmap* bmp)
{
    if (!bmp) return;
    int w = (int)bmp->GetWidth(), h = (int)bmp->GetHeight();

    // CF_BITMAP (white-composited, for legacy compat)
    HBITMAP hbm = NULL;
    bmp->GetHBITMAP(Color(255, 255, 255, 255), &hbm);
    if (hbm) {
        SetClipboardData(CF_BITMAP, hbm); // clipboard takes ownership

        // CF_DIB — 24-bit bottom-up for external apps (e.g. Outlook)
        BITMAPINFOHEADER bih = {};
        bih.biSize = sizeof(bih); bih.biWidth = w; bih.biHeight = h;
        bih.biPlanes = 1; bih.biBitCount = 24; bih.biCompression = BI_RGB;
        int stride = ((w * 3 + 3) & ~3);
        bih.biSizeImage = stride * h;
        HGLOBAL hDib = GlobalAlloc(GMEM_MOVEABLE, sizeof(bih) + bih.biSizeImage);
        if (hDib) {
            BYTE* p = (BYTE*)GlobalLock(hDib);
            if (p) {
                memcpy(p, &bih, sizeof(bih));
                HDC hdc = GetDC(NULL);
                BITMAPINFO bi = {}; bi.bmiHeader = bih;
                GetDIBits(hdc, hbm, 0, h, p + sizeof(bih), &bi, DIB_RGB_COLORS);
                ReleaseDC(NULL, hdc);
                GlobalUnlock(hDib);
                SetClipboardData(CF_DIB, hDib);
            } else { GlobalFree(hDib); }
        }
    }

    // Private ARGB format — preserves transparency for paste back into MyPaint
    if (g_cfARGB) {
        int stride = w * 4;
        HGLOBAL hArgb = GlobalAlloc(GMEM_MOVEABLE, 3 * sizeof(int) + (SIZE_T)stride * h);
        if (hArgb) {
            BYTE* p = (BYTE*)GlobalLock(hArgb);
            if (p) {
                ((int*)p)[0] = w; ((int*)p)[1] = h; ((int*)p)[2] = stride;
                BitmapData bd; Rect rc(0, 0, w, h);
                if (bmp->LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) == Ok) {
                    for (int y = 0; y < h; y++)
                        memcpy(p + 12 + y * stride, (BYTE*)bd.Scan0 + y * bd.Stride, stride);
                    bmp->UnlockBits(&bd);
                }
                GlobalUnlock(hArgb);
                SetClipboardData(g_cfARGB, hArgb);
            } else { GlobalFree(hArgb); }
        }
    }
}

// Build a new ARGB bitmap with selected pixels; non-selected pixels are transparent.
// Used by wand copy/cut when g_hasSelMask is active.
static Bitmap* ExtractMaskedRegion()
{
    int bw = g_selRect.right  - g_selRect.left;
    int bh = g_selRect.bottom - g_selRect.top;
    if (bw <= 0 || bh <= 0) return nullptr;

    Bitmap* result = new Bitmap(bw, bh, PixelFormat32bppARGB);
    BitmapData srcData, dstData;
    Rect srcRect(g_selRect.left, g_selRect.top, bw, bh);
    Rect dstRect(0, 0, bw, bh);
    if (g_pCanvas->LockBits(&srcRect, ImageLockModeRead,  PixelFormat32bppARGB, &srcData) != Ok)
        { delete result; return nullptr; }
    if (result->LockBits  (&dstRect, ImageLockModeWrite, PixelFormat32bppARGB, &dstData) != Ok)
        { g_pCanvas->UnlockBits(&srcData); delete result; return nullptr; }

    for (int y = 0; y < bh; ++y) {
        auto* src = (DWORD*)((BYTE*)srcData.Scan0 + (ptrdiff_t)y * srcData.Stride);
        auto* dst = (DWORD*)((BYTE*)dstData.Scan0 + (ptrdiff_t)y * dstData.Stride);
        int canY = g_selRect.top + y;
        for (int x = 0; x < bw; ++x) {
            int canX = g_selRect.left + x;
            bool sel = ((UINT)canX < g_selMaskW)
                       && g_selMask[(size_t)canY * g_selMaskW + canX];
            dst[x] = sel ? src[x] : 0u;
        }
    }
    result->UnlockBits(&dstData);
    g_pCanvas->UnlockBits(&srcData);
    return result;
}

static void DoCopy()
{
    // If a paste is active, copy the paste bitmap to the clipboard
    if (g_isPasting && g_pasteBitmap) {
        if (OpenClipboard(g_hwndMain)) {
            EmptyClipboard();
            ClipboardPutBitmap(g_pasteBitmap);
            CloseClipboard();
        }
        return;
    }

    if (!g_pCanvas) return;
    if (!g_hasSelection) {
        // Nothing selected: copy the whole image, as most editors do
        if (OpenClipboard(g_hwndMain)) {
            EmptyClipboard();
            ClipboardPutBitmap(g_pCanvas);
            CloseClipboard();
        }
        return;
    }

    // If a floating selection exists, use its original captured pixels directly —
    // the canvas region was already erased when the float was created, so cloning
    // from the canvas would yield blank/transparent pixels.
    Bitmap* srcBmp = nullptr;
    bool ownSrc = false;
    if (g_selFloating && g_selFloatBitmap) {
        srcBmp = g_selFloatBitmap;
    } else if (g_hasSelMask) {
        srcBmp = ExtractMaskedRegion();
        ownSrc = true;
    } else {
        int w = g_selRect.right  - g_selRect.left;
        int h = g_selRect.bottom - g_selRect.top;
        if (w <= 0 || h <= 0) return;
        srcBmp = g_pCanvas->Clone(g_selRect.left, g_selRect.top, w, h,
                                  PixelFormat32bppARGB);
        ownSrc = true;
    }
    if (!srcBmp) return;

    if (OpenClipboard(g_hwndMain)) {
        EmptyClipboard();
        ClipboardPutBitmap(srcBmp);
        CloseClipboard();
    }
    if (ownSrc) delete srcBmp;
}

static void DoCut()
{
    // If a paste is active, copy it to clipboard then discard without committing
    if (g_isPasting && g_pasteBitmap) {
        DoCopy();
        CancelPaste();
        return;
    }

    if (!g_hasSelection || !g_pCanvas || !g_pGfxCanvas) return;
    DoCopy();  // copy to clipboard first (uses float bitmap if floating)

    if (g_selFloating && g_selFloatBitmap) {
        // Canvas hole was already created when the float was extracted — just discard
        // the floating bitmap without committing it back.
        delete g_selFloatBitmap; g_selFloatBitmap = nullptr;
        g_selFloating  = false;
        g_selFloatAngle = 0.0f;
        g_selDragMode  = 0;
    } else if (g_hasSelMask) {
        PushUndo();
        // Erase only the masked pixels
        int bw = g_selRect.right - g_selRect.left;
        int bh = g_selRect.bottom - g_selRect.top;
        BitmapData bd;
        Rect lr(g_selRect.left, g_selRect.top, bw, bh);
        if (g_pCanvas->LockBits(&lr, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
            for (int y = 0; y < bh; ++y) {
                auto* row = (DWORD*)((BYTE*)bd.Scan0 + (ptrdiff_t)y * bd.Stride);
                int canY = g_selRect.top + y;
                for (int x = 0; x < bw; ++x) {
                    int canX = g_selRect.left + x;
                    if ((UINT)canX < g_selMaskW && g_selMask[(size_t)canY * g_selMaskW + canX])
                        row[x] = 0u;
                }
            }
            g_pCanvas->UnlockBits(&bd);
        }
    } else {
        PushUndo();
        // Erase selection area to transparent
        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceCopy);
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeNone);
        SolidBrush transparent(Color(0, 0, 0, 0));
        g_pGfxCanvas->FillRectangle(&transparent,
            (REAL)g_selRect.left, (REAL)g_selRect.top,
            (REAL)(g_selRect.right  - g_selRect.left),
            (REAL)(g_selRect.bottom - g_selRect.top));
        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceOver);
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    }
    g_hasSelection = false;
    StopMarchingAnts(); g_hasSelMask = false;
    g_modified = true;
    UpdateTitleBar();
    InvalidateCanvas();
}

static void CommitPaste()
{
    if (!g_isPasting || !g_pasteBitmap || !g_pGfxCanvas) return;
    if (!g_pasteDidResize) PushUndo();
    g_pasteDidResize = false;
    // Draw the floating bitmap onto the canvas at its current position/size/angle
    if (g_pasteAngle != 0.0f) {
        float cx = g_pastePos.x + g_pasteSizeW / 2.0f;
        float cy = g_pastePos.y + g_pasteSizeH / 2.0f;
        Matrix mat; mat.RotateAt(g_pasteAngle, PointF(cx, cy));
        g_pGfxCanvas->SetTransform(&mat);
    }
    g_pGfxCanvas->SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g_pGfxCanvas->DrawImage(g_pasteBitmap,
        g_pastePos.x, g_pastePos.y, g_pasteSizeW, g_pasteSizeH);
    g_pGfxCanvas->SetInterpolationMode(InterpolationModeDefault);
    if (g_pasteAngle != 0.0f) g_pGfxCanvas->ResetTransform();

    // Leave a selection around the pasted area (axis-aligned only when not rotated)
    int cw = g_pCanvas ? (int)g_pCanvas->GetWidth()  : 0;
    int ch = g_pCanvas ? (int)g_pCanvas->GetHeight() : 0;
    if (g_pasteAngle == 0.0f) {
        g_selRect.left   = max(g_pastePos.x,                       0L);
        g_selRect.top    = max(g_pastePos.y,                       0L);
        g_selRect.right  = min(g_pastePos.x + (LONG)g_pasteSizeW, (LONG)cw);
        g_selRect.bottom = min(g_pastePos.y + (LONG)g_pasteSizeH, (LONG)ch);
        g_hasSelection = (g_selRect.right > g_selRect.left && g_selRect.bottom > g_selRect.top);
    } else {
        g_hasSelection = false;
    }

    delete g_pasteBitmap; g_pasteBitmap = nullptr;
    g_isPasting      = false;
    g_pasteDragMode  = 0;
    g_pasteSizeW     = 0;
    g_pasteSizeH     = 0;
    g_pasteAngle     = 0.0f;
    g_modified = true;
    UpdateTitleBar();
    InvalidateCanvas();
}

static void CancelPaste()
{
    if (!g_isPasting) return;
    delete g_pasteBitmap; g_pasteBitmap = nullptr;
    g_isPasting      = false;
    g_pasteDragMode  = 0;
    g_pasteSizeW     = 0;
    g_pasteSizeH     = 0;
    g_pasteAngle     = 0.0f;
    g_pasteDidResize = false;
    InvalidateCanvas();
}

static void CommitSelectionFloat()
{
    if (!g_selFloating || !g_selFloatBitmap || !g_pGfxCanvas) return;
    int dw = g_selRect.right  - g_selRect.left;
    int dh = g_selRect.bottom - g_selRect.top;
    if (dw > 0 && dh > 0) {
        int origW = (int)g_selFloatBitmap->GetWidth();
        int origH = (int)g_selFloatBitmap->GetHeight();
        InterpolationMode im = (dw == origW && dh == origH)
            ? InterpolationModeNearestNeighbor
            : InterpolationModeHighQualityBicubic;
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
        if (g_selFloatAngle != 0.0f) {
            REAL canCx = (g_selRect.left + g_selRect.right)  / 2.0f;
            REAL canCy = (g_selRect.top  + g_selRect.bottom) / 2.0f;
            Matrix mat;
            mat.RotateAt(g_selFloatAngle, PointF(canCx, canCy));
            g_pGfxCanvas->SetTransform(&mat);
        }
        g_pGfxCanvas->SetInterpolationMode(im);
        g_pGfxCanvas->DrawImage(g_selFloatBitmap,
            Rect(g_selRect.left, g_selRect.top, dw, dh));
        g_pGfxCanvas->SetInterpolationMode(InterpolationModeDefault);
        if (g_selFloatAngle != 0.0f) g_pGfxCanvas->ResetTransform();
        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    }
    delete g_selFloatBitmap; g_selFloatBitmap = nullptr;
    g_selFloating    = false;
    g_selFloatAngle  = 0.0f;
    g_modified = true;
    UpdateTitleBar();
    InvalidateCanvas();
}

static void CancelSelectionFloat()
{
    if (!g_selFloating || !g_selFloatBitmap) return;
    // Draw pixels back at their original location (unrotated)
    if (g_pGfxCanvas) {
        int ow = (int)g_selFloatBitmap->GetWidth();
        int oh = (int)g_selFloatBitmap->GetHeight();
        g_pGfxCanvas->SetInterpolationMode(InterpolationModeNearestNeighbor);
        g_pGfxCanvas->DrawImage(g_selFloatBitmap,
            g_selFloatOrigRect.left, g_selFloatOrigRect.top, ow, oh);
        g_pGfxCanvas->SetInterpolationMode(InterpolationModeDefault);
    }
    delete g_selFloatBitmap; g_selFloatBitmap = nullptr;
    g_selFloating   = false;
    g_selFloatAngle = 0.0f;
    // Silently remove the undo entry pushed at extraction time
    if (!g_undoStack.empty()) {
        delete g_undoStack.back();
        g_undoStack.pop_back();
    }
    g_selRect      = g_selFloatOrigRect;
    g_hasSelection = true;
    InvalidateCanvas();
}

static void DoPaste()
{
    if (!g_pCanvas) return;
    bool hasARGB   = g_cfARGB && IsClipboardFormatAvailable(g_cfARGB);
    bool hasBitmap = IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB);
    if (!hasARGB && !hasBitmap) return;

    // Commit any existing paste first
    if (g_isPasting) CommitPaste();

    if (!OpenClipboard(g_hwndMain)) return;

    // Prefer private ARGB format — preserves transparency for round-trips within MyPaint
    if (hasARGB) {
        HANDLE hMem = GetClipboardData(g_cfARGB);
        if (hMem) {
            BYTE* p = (BYTE*)GlobalLock(hMem);
            if (p) {
                int w = ((int*)p)[0], h = ((int*)p)[1], stride = ((int*)p)[2];
                if (w > 0 && h > 0) {
                    g_pasteBitmap = new Bitmap(w, h, PixelFormat32bppARGB);
                    if (g_pasteBitmap) {
                        BitmapData bd; Rect rc(0, 0, w, h);
                        if (g_pasteBitmap->LockBits(&rc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                            for (int y = 0; y < h; y++)
                                memcpy((BYTE*)bd.Scan0 + y * bd.Stride, p + 12 + y * stride, stride);
                            g_pasteBitmap->UnlockBits(&bd);
                        }
                    }
                }
                GlobalUnlock(hMem);
            }
        }
    }

    // Fall back to CF_BITMAP (loses transparency — e.g. content copied from another app)
    if (!g_pasteBitmap) {
        HBITMAP hbm = (HBITMAP)GetClipboardData(CF_BITMAP);
        if (!hbm) { CloseClipboard(); return; }
        HBITMAP hbmCopy = (HBITMAP)CopyImage(hbm, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
        CloseClipboard();
        if (!hbmCopy) return;
        Bitmap* src = Bitmap::FromHBITMAP(hbmCopy, NULL);
        if (!src || src->GetLastStatus() != Ok) {
            delete src; DeleteObject(hbmCopy); return;
        }
        g_pasteBitmap = src->Clone(0, 0, src->GetWidth(), src->GetHeight(),
                                   PixelFormat32bppARGB);
        delete src;
        DeleteObject(hbmCopy);
    } else {
        CloseClipboard();
    }

    if (!g_pasteBitmap) return;

    // Auto-enlarge canvas if paste content is larger
    {
        int pw = (int)g_pasteBitmap->GetWidth();
        int ph = (int)g_pasteBitmap->GetHeight();
        int cw = (int)g_pCanvas->GetWidth();
        int ch = (int)g_pCanvas->GetHeight();
        int newW = max(cw, pw);
        int newH = max(ch, ph);
        if (newW > cw || newH > ch) {
            PushUndo();
            g_pasteDidResize = true;
            Bitmap*   newBmp = new Bitmap(newW, newH, PixelFormat32bppARGB);
            Graphics* newGfx = Graphics::FromImage(newBmp);
            newGfx->SetSmoothingMode(SmoothingModeAntiAlias);
            newGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
            newGfx->Clear(Color(0, 0, 0, 0));
            newGfx->DrawImage(g_pCanvas, 0, 0);
            delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
            delete g_pCanvas;
            delete g_pPreview;
            g_pCanvas    = newBmp;
            g_pGfxCanvas = newGfx;
            g_pPreview   = new Bitmap(newW, newH, PixelFormat32bppARGB);
            { Graphics gp(g_pPreview); gp.Clear(Color(0,0,0,0)); }
            g_modified = true;
            UpdateTitleBar();
            UpdateScrollBars();
        }
    }

    g_isPasting      = true;
    g_pasteDragMode  = 0;
    g_pastePos       = {0, 0};
    g_pasteSizeW     = (int)g_pasteBitmap->GetWidth();
    g_pasteSizeH     = (int)g_pasteBitmap->GetHeight();
    g_pasteAngle     = 0.0f;
    g_hasSelection   = false;
    SetTool(TOOL_SELECT);
    InvalidateCanvas();
}

// ============================================================
//  Image operations
// ============================================================
struct ResizeDlgData {
    int  width, height;   // result
    int  origW, origH;    // original (for computing percentages)
    bool lockAspect;
    bool canvasOnly;
};
static ResizeDlgData g_resizeDlgData = { 0, 0, 0, 0, true, false };

INT_PTR CALLBACK ResizeDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    // Guard against re-entrant EN_CHANGE updates
    static bool s_updating = false;

    auto getData = [&]() -> ResizeDlgData* {
        return reinterpret_cast<ResizeDlgData*>(GetWindowLongPtr(hDlg, DWLP_USER));
    };

    // Sync all four fields from newW/newH, skipping the field being typed into
    auto syncAll = [&](int newW, int newH, WORD skipId = 0) {
        ResizeDlgData* d = getData();
        if (!d) return;
        s_updating = true;
        if (skipId != IDC_RESIZE_W)
            SetDlgItemInt(hDlg, IDC_RESIZE_W,     newW, FALSE);
        if (skipId != IDC_RESIZE_H)
            SetDlgItemInt(hDlg, IDC_RESIZE_H,     newH, FALSE);
        if (skipId != IDC_RESIZE_W_PCT)
            SetDlgItemInt(hDlg, IDC_RESIZE_W_PCT,
                max(1, (int)round(newW * 100.0 / d->origW)), FALSE);
        if (skipId != IDC_RESIZE_H_PCT)
            SetDlgItemInt(hDlg, IDC_RESIZE_H_PCT,
                max(1, (int)round(newH * 100.0 / d->origH)), FALSE);
        s_updating = false;
    };

    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)&g_resizeDlgData);
        s_updating = false;
        syncAll(g_resizeDlgData.origW, g_resizeDlgData.origH);
        CheckDlgButton(hDlg, IDC_RESIZE_LOCK_ASPECT, g_resizeDlgData.lockAspect ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_RESIZE_CANVAS_ONLY, g_resizeDlgData.canvasOnly ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND: {
        WORD id = LOWORD(wParam), notif = HIWORD(wParam);
        ResizeDlgData* d = getData();

        if (id == IDOK) {
            BOOL ok;
            int w = GetDlgItemInt(hDlg, IDC_RESIZE_W, &ok, FALSE);
            if (!ok || w < 1 || w > 32000) {
                MessageBox(hDlg, L"Width must be 1–32000.", L"Error", MB_ICONERROR); return TRUE;
            }
            int h = GetDlgItemInt(hDlg, IDC_RESIZE_H, &ok, FALSE);
            if (!ok || h < 1 || h > 32000) {
                MessageBox(hDlg, L"Height must be 1–32000.", L"Error", MB_ICONERROR); return TRUE;
            }
            d->width       = w;
            d->height      = h;
            d->lockAspect  = IsDlgButtonChecked(hDlg, IDC_RESIZE_LOCK_ASPECT) == BST_CHECKED;
            d->canvasOnly  = IsDlgButtonChecked(hDlg, IDC_RESIZE_CANVAS_ONLY) == BST_CHECKED;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (id == IDCANCEL) { EndDialog(hDlg, IDCANCEL); return TRUE; }

        if (notif == EN_CHANGE && !s_updating && d) {
            BOOL ok;
            if (id == IDC_RESIZE_W) {
                int w = GetDlgItemInt(hDlg, IDC_RESIZE_W, &ok, FALSE);
                if (ok && w >= 1) {
                    int h = d->lockAspect
                        ? max(1, (int)round((double)w * d->origH / d->origW))
                        : max(1, (int)(GetDlgItemInt(hDlg, IDC_RESIZE_H, &ok, FALSE)));
                    syncAll(w, d->lockAspect ? h : (ok ? h : d->origH), IDC_RESIZE_W);
                }
            } else if (id == IDC_RESIZE_H) {
                int h = GetDlgItemInt(hDlg, IDC_RESIZE_H, &ok, FALSE);
                if (ok && h >= 1) {
                    int w = d->lockAspect
                        ? max(1, (int)round((double)h * d->origW / d->origH))
                        : max(1, (int)(GetDlgItemInt(hDlg, IDC_RESIZE_W, &ok, FALSE)));
                    syncAll(d->lockAspect ? w : (ok ? w : d->origW), h, IDC_RESIZE_H);
                }
            } else if (id == IDC_RESIZE_W_PCT) {
                int pct = GetDlgItemInt(hDlg, IDC_RESIZE_W_PCT, &ok, FALSE);
                if (ok && pct >= 1) {
                    int w = max(1, (int)round(pct * d->origW / 100.0));
                    int h = d->lockAspect
                        ? max(1, (int)round(pct * d->origH / 100.0))
                        : max(1, (int)(GetDlgItemInt(hDlg, IDC_RESIZE_H, &ok, FALSE)));
                    syncAll(w, d->lockAspect ? h : (ok ? h : d->origH), IDC_RESIZE_W_PCT);
                }
            } else if (id == IDC_RESIZE_H_PCT) {
                int pct = GetDlgItemInt(hDlg, IDC_RESIZE_H_PCT, &ok, FALSE);
                if (ok && pct >= 1) {
                    int h = max(1, (int)round(pct * d->origH / 100.0));
                    int w = d->lockAspect
                        ? max(1, (int)round(pct * d->origW / 100.0))
                        : max(1, (int)(GetDlgItemInt(hDlg, IDC_RESIZE_W, &ok, FALSE)));
                    syncAll(d->lockAspect ? w : (ok ? w : d->origW), h, IDC_RESIZE_H_PCT);
                }
            }
        }

        if (id == IDC_RESIZE_LOCK_ASPECT && notif == BN_CLICKED && d) {
            bool locked = IsDlgButtonChecked(hDlg, IDC_RESIZE_LOCK_ASPECT) == BST_CHECKED;
            d->lockAspect = locked;
            if (locked) {
                // Snap height to match current width
                BOOL ok;
                int w = GetDlgItemInt(hDlg, IDC_RESIZE_W, &ok, FALSE);
                if (ok && w >= 1) {
                    int h = max(1, (int)round((double)w * d->origH / d->origW));
                    syncAll(w, h);
                }
            }
        }
        break;
    }
    }
    return FALSE;
}

static void DoResizeCanvas()
{
    if (!g_pCanvas) return;
    // Commit any floating state before replacing the canvas
    if (g_isPasting)   CommitPaste();
    if (g_selFloating) CommitSelectionFloat();

    int curW = (int)g_pCanvas->GetWidth();
    int curH = (int)g_pCanvas->GetHeight();
    g_resizeDlgData.width      = curW;
    g_resizeDlgData.height     = curH;
    g_resizeDlgData.origW      = curW;
    g_resizeDlgData.origH      = curH;
    // Preserve checkbox states across invocations
    // (lockAspect and canvasOnly keep their last values from g_resizeDlgData)

    INT_PTR r = DialogBox(GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_RESIZE_CANVAS), g_hwndMain, ResizeDlgProc);
    if (r != IDOK) return;

    int newW = g_resizeDlgData.width;
    int newH = g_resizeDlgData.height;

    PushUndo();

    Bitmap*   newBmp = new Bitmap(newW, newH, PixelFormat32bppARGB);
    Graphics* newGfx = Graphics::FromImage(newBmp);
    newGfx->SetSmoothingMode(SmoothingModeAntiAlias);
    newGfx->SetInterpolationMode(InterpolationModeHighQualityBicubic);
    newGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    newGfx->Clear(Color(0, 0, 0, 0));

    if (g_resizeDlgData.canvasOnly) {
        // Canvas only: blit existing image at top-left without scaling
        newGfx->DrawImage(g_pCanvas, 0, 0);
    } else {
        // Resize image: scale to fill the new dimensions
        newGfx->DrawImage(g_pCanvas, 0, 0, newW, newH);
    }

    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;
    delete g_pPreview;
    g_pCanvas    = newBmp;
    g_pGfxCanvas = newGfx;
    g_pPreview   = new Bitmap(newW, newH, PixelFormat32bppARGB);
    { Graphics gp(g_pPreview); gp.Clear(Color(0,0,0,0)); }

    g_hasSelection = false;
    g_modified = true;
    InvalidateCanvas();
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    UpdateScrollBars();
}

static void DoRotateCanvas(bool clockwise)
{
    if (!g_pCanvas) return;

    // ── Shape / text being edited (not yet committed) ─────────────────────
    if (g_shapeEditing) {
        g_shapeEditAngle += clockwise ? 90.0f : -90.0f;
        RefreshEditingPreview();
        return;
    }

    // Helper: rotate a bitmap 90° CW or CCW into a new bitmap (W/H swapped)
    auto rotateBmp = [&](Bitmap* src) -> Bitmap* {
        int sw = (int)src->GetWidth(), sh = (int)src->GetHeight();
        Bitmap* dst = new Bitmap(sh, sw, PixelFormat32bppARGB);
        Graphics gf(dst);
        Matrix rm;
        if (clockwise) { rm.Translate((REAL)sh, 0.0f); rm.Rotate(90.0f); }
        else           { rm.Translate(0.0f, (REAL)sw); rm.Rotate(-90.0f); }
        gf.SetTransform(&rm);
        gf.DrawImage(src, 0, 0, sw, sh);
        return dst;
    };

    // ── Paste in flight: rotate the paste bitmap, swap its display size ──
    if (g_isPasting && g_pasteBitmap) {
        Bitmap* rotated = rotateBmp(g_pasteBitmap);
        delete g_pasteBitmap; g_pasteBitmap = rotated;
        std::swap(g_pasteSizeW, g_pasteSizeH);
        g_pasteAngle = 0.0f;
        InvalidateCanvas();
        return;
    }

    // ── Floating selection: rotate the float bitmap, update selRect ──────
    if (g_selFloating && g_selFloatBitmap) {
        Bitmap* rotated = rotateBmp(g_selFloatBitmap);
        delete g_selFloatBitmap; g_selFloatBitmap = rotated;
        // Swap W/H around the current centre
        int cx = (g_selRect.left + g_selRect.right)  / 2;
        int cy = (g_selRect.top  + g_selRect.bottom) / 2;
        int hw = (g_selRect.bottom - g_selRect.top)  / 2;  // new half-width  = old half-height
        int hh = (g_selRect.right  - g_selRect.left) / 2;  // new half-height = old half-width
        g_selRect = { cx - hw, cy - hh, cx + hw, cy + hh };
        g_selFloatAngle = 0.0f;
        g_modified = true;
        UpdateTitleBar();
        InvalidateCanvas();
        return;
    }

    // ── Rect / mask selection: lift into float (no canvas write until commit) ──
    if (g_hasSelection) {
        int sx = g_selRect.left,  sy = g_selRect.top;
        int sw = g_selRect.right - sx, sh = g_selRect.bottom - sy;
        if (sw > 0 && sh > 0) {
            Bitmap* region = g_pCanvas->Clone(sx, sy, sw, sh, PixelFormat32bppARGB);
            if (region) {
                PushUndo();
                Bitmap* rotated = rotateBmp(region);  // new size: sh × sw
                delete region;
                // Become floating WITHOUT erasing the canvas — original pixels stay
                // intact until the user explicitly commits (consistent with selection move)
                delete g_selFloatBitmap;
                g_selFloatBitmap  = rotated;
                g_selFloatOrigRect = g_selRect;
                g_selFloating     = true;
                g_selFloatAngle   = 0.0f;
                // Update selection rect: swap W/H, clamp to canvas
                int canW = (int)g_pCanvas->GetWidth(), canH = (int)g_pCanvas->GetHeight();
                g_selRect = { sx, sy, min(sx + sh, canW), min(sy + sw, canH) };
                g_modified = true;
                UpdateTitleBar();
                InvalidateCanvas();
                return;
            }
        }
    }

    // ── No selection: rotate entire canvas ───────────────────────────────
    StopMarchingAnts();
    PushUndo();
    INT srcW = (INT)g_pCanvas->GetWidth();
    INT srcH = (INT)g_pCanvas->GetHeight();
    INT dstW = srcH, dstH = srcW;
    Bitmap*   newBmp = new Bitmap(dstW, dstH, PixelFormat32bppARGB);
    Graphics* newGfx = Graphics::FromImage(newBmp);
    newGfx->SetSmoothingMode(SmoothingModeAntiAlias);
    newGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    newGfx->Clear(Color(0, 0, 0, 0));
    Matrix m;
    if (clockwise) { m.Translate((REAL)srcH, 0.0f); m.Rotate(90.0f); }
    else           { m.Translate(0.0f, (REAL)srcW); m.Rotate(-90.0f); }
    newGfx->SetTransform(&m);
    newGfx->DrawImage(g_pCanvas, 0, 0, srcW, srcH);
    newGfx->ResetTransform();
    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;
    delete g_pPreview;
    g_pCanvas    = newBmp;
    g_pGfxCanvas = newGfx;
    g_pPreview   = new Bitmap(dstW, dstH, PixelFormat32bppARGB);
    { Graphics gp(g_pPreview); gp.Clear(Color(0, 0, 0, 0)); }
    g_hasSelection = false;
    g_modified = true;
    InvalidateCanvas();
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    UpdateScrollBars();
}

// Crops the image to the selection's bounding rectangle. A moved selection is
// stamped down first; a lasso or wand selection keeps everything inside its
// bounds, as in Windows Paint.
static void DoCropToSelection()
{
    if (!g_pCanvas || !g_hasSelection) return;
    if (g_selFloating) CommitSelectionFloat();
    int canW = (int)g_pCanvas->GetWidth(), canH = (int)g_pCanvas->GetHeight();
    RECT r = { max(0L, g_selRect.left), max(0L, g_selRect.top),
               min((LONG)canW, g_selRect.right), min((LONG)canH, g_selRect.bottom) };
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;

    StopMarchingAnts();
    PushUndo();
    Bitmap* newBmp = g_pCanvas->Clone(r.left, r.top, w, h, PixelFormat32bppARGB);
    if (!newBmp || newBmp->GetLastStatus() != Ok) { delete newBmp; return; }
    Graphics* newGfx = Graphics::FromImage(newBmp);
    newGfx->SetSmoothingMode(SmoothingModeAntiAlias);
    newGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    delete g_pGfxCanvas;
    delete g_pCanvas;
    delete g_pPreview;
    g_pCanvas    = newBmp;
    g_pGfxCanvas = newGfx;
    g_pPreview   = new Bitmap(w, h, PixelFormat32bppARGB);
    { Graphics gp(g_pPreview); gp.Clear(Color(0, 0, 0, 0)); }
    g_previewUsed  = false;
    g_hasSelection = false;
    g_hasSelMask   = false;
    g_modified = true;
    if (g_zoomFitMode) ZoomToFit(); else CenterScrollOnCanvas();
    InvalidateCanvas();
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    UpdateScrollBars();
}

static void DoFlipCanvas(bool horizontal)
{
    if (!g_pCanvas) return;

    // ── Shape / text being edited (not yet committed) ─────────────────────
    if (g_shapeEditing) {
        if (horizontal) g_shapeEditFlipH = !g_shapeEditFlipH;
        else            g_shapeEditFlipV = !g_shapeEditFlipV;
        RefreshEditingPreview();
        return;
    }

    // Helper: flip a bitmap into a new same-size bitmap
    auto flipBmp = [&](Bitmap* src) -> Bitmap* {
        int fw = (int)src->GetWidth(), fh = (int)src->GetHeight();
        Bitmap* dst = new Bitmap(fw, fh, PixelFormat32bppARGB);
        Graphics gf(dst);
        Matrix fm;
        if (horizontal) { fm.Translate((REAL)fw, 0.0f); fm.Scale(-1.0f, 1.0f); }
        else            { fm.Translate(0.0f, (REAL)fh); fm.Scale(1.0f, -1.0f); }
        gf.SetTransform(&fm);
        gf.DrawImage(src, 0, 0, fw, fh);
        return dst;
    };

    // ── Paste in flight: flip the paste bitmap only ──────────────────
    if (g_isPasting && g_pasteBitmap) {
        Bitmap* flipped = flipBmp(g_pasteBitmap);
        delete g_pasteBitmap; g_pasteBitmap = flipped;
        InvalidateCanvas();
        return;
    }

    // ── Floating selection: flip the float bitmap only ───────────────
    if (g_selFloating && g_selFloatBitmap) {
        Bitmap* flipped = flipBmp(g_selFloatBitmap);
        delete g_selFloatBitmap; g_selFloatBitmap = flipped;
        g_modified = true;
        UpdateTitleBar();
        InvalidateCanvas();
        return;
    }

    // ── Rect / mask selection: lift into float (no canvas write until commit) ──
    if (g_hasSelection) {
        int sx = g_selRect.left,  sy = g_selRect.top;
        int sw = g_selRect.right - sx, sh = g_selRect.bottom - sy;
        if (sw > 0 && sh > 0) {
            Bitmap* region = g_pCanvas->Clone(sx, sy, sw, sh, PixelFormat32bppARGB);
            if (region) {
                PushUndo();
                Bitmap* flipped = flipBmp(region);
                delete region;
                // Become floating WITHOUT erasing the canvas — original pixels stay
                // intact until the user explicitly commits (consistent with selection move)
                delete g_selFloatBitmap;
                g_selFloatBitmap   = flipped;
                g_selFloatOrigRect = g_selRect;
                g_selFloating      = true;
                g_selFloatAngle    = 0.0f;
                g_modified = true;
                UpdateTitleBar();
                InvalidateCanvas();
                return;
            }
        }
    }

    // ── No selection: flip entire canvas ─────────────────────────────
    StopMarchingAnts();
    PushUndo();
    INT w = (INT)g_pCanvas->GetWidth();
    INT h = (INT)g_pCanvas->GetHeight();
    Bitmap*   newBmp = new Bitmap(w, h, PixelFormat32bppARGB);
    Graphics* newGfx = Graphics::FromImage(newBmp);
    newGfx->SetSmoothingMode(SmoothingModeAntiAlias);
    newGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    newGfx->Clear(Color(0, 0, 0, 0));
    Matrix m;
    if (horizontal) { m.Translate((REAL)w, 0.0f); m.Scale(-1.0f, 1.0f); }
    else            { m.Translate(0.0f, (REAL)h); m.Scale(1.0f, -1.0f); }
    newGfx->SetTransform(&m);
    newGfx->DrawImage(g_pCanvas, 0, 0, w, h);
    newGfx->ResetTransform();
    delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
    delete g_pCanvas;
    delete g_pPreview;
    g_pCanvas    = newBmp;
    g_pGfxCanvas = newGfx;
    g_pPreview   = new Bitmap(w, h, PixelFormat32bppARGB);
    { Graphics gp(g_pPreview); gp.Clear(Color(0, 0, 0, 0)); }
    g_hasSelection = false;
    g_modified = true;
    InvalidateCanvas();
    UpdateTitleBar();
    UpdateStatusBar(0, 0);
    UpdateScrollBars();
}

// ============================================================
//  Text tool dialog
// ============================================================

INT_PTR CALLBACK TextDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(hDlg, IDC_TEXT_EDIT, g_textDlgData.text);
        SetDlgItemInt(hDlg,  IDC_TEXT_SIZE, g_textDlgData.fontSize, FALSE);
        SendDlgItemMessage(hDlg, IDC_TEXT_EDIT, EM_LIMITTEXT, 255, 0);
        SetFocus(GetDlgItem(hDlg, IDC_TEXT_EDIT));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            GetDlgItemText(hDlg, IDC_TEXT_EDIT, g_textDlgData.text, 256);
            BOOL ok;
            int sz = GetDlgItemInt(hDlg, IDC_TEXT_SIZE, &ok, FALSE);
            if (ok && sz > 0 && sz < 500) g_textDlgData.fontSize = sz;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) { EndDialog(hDlg, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}

static void DoTextTool(int cx, int cy)
{
    g_textDlgData.text[0]  = L'\0';
    g_textDlgData.fontSize = 14;
    INT_PTR r = DialogBox(GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_TEXT_INPUT), g_hwndMain, TextDlgProc);
    if (r != IDOK || g_textDlgData.text[0] == L'\0') return;
    PushUndo();
    FontFamily ff(L"Arial");
    Font       font(&ff, (REAL)g_textDlgData.fontSize, FontStyleRegular, UnitPoint);
    SolidBrush brush(GdipColorFromCOLORREF(g_fgColor, FgAlphaByte()));
    PointF     pt((REAL)cx, (REAL)cy);
    g_pGfxCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
    g_pGfxCanvas->DrawString(g_textDlgData.text, -1, &font, pt, &brush);
    g_modified = true;
    UpdateTitleBar();
    InvalidateCanvas();
}

// ============================================================
//  About dialog
// ============================================================
static HFONT s_hHelpFont = NULL;

// Layout metrics captured at WM_INITDIALOG (pixels), used for WM_SIZE
struct HelpLayout {
    int editL, editT, editRMargin;   // edit: left, top, right margin from client edge
    int gapEditBtn;                   // gap between edit bottom and button top
    int btnW, btnH, btnBotMargin;    // button: width, height, bottom margin from client edge
    int minCx, minCy;                 // minimum window size
};
static HelpLayout s_lay = {};

static LRESULT CALLBACK SearchEditSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static void LayoutHelpDlg(HWND hDlg, int cx, int cy);
static void DoHelpFind(HWND hDlg);

static void LayoutHelpDlg(HWND hDlg, int cx, int cy)
{
    if (s_lay.minCx == 0) return;
    HWND hEdit = GetDlgItem(hDlg, IDC_HELP_TEXT);
    HWND hOK   = GetDlgItem(hDlg, IDOK);

    int editW = cx - s_lay.editL - s_lay.editRMargin;
    if (editW < 10) editW = 10;

    const int searchH = 24, searchGap = 4;
    const int findNextW = 90, closeW = 60, btnGap = 4;

    // Search bar sits at the TOP above the text area
    int editTop = s_searchVisible ? (s_lay.editT + searchH + searchGap) : s_lay.editT;
    int editH   = cy - editTop - s_lay.gapEditBtn - s_lay.btnH - s_lay.btnBotMargin;
    if (editH < 10) editH = 10;

    int btnX = (cx - s_lay.btnW) / 2;
    int btnY = cy - s_lay.btnBotMargin - s_lay.btnH;

    HDWP hdwp = BeginDeferWindowPos(s_searchVisible ? 5 : 2);
    if (hEdit) DeferWindowPos(hdwp, hEdit, NULL, s_lay.editL, editTop, editW, editH, SWP_NOZORDER|SWP_NOACTIVATE);
    if (hOK)   DeferWindowPos(hdwp, hOK,   NULL, btnX, btnY, s_lay.btnW, s_lay.btnH, SWP_NOZORDER|SWP_NOACTIVATE);
    if (s_searchVisible && s_hwndSearchEdit && s_hwndFindNext && s_hwndFindClose) {
        int top    = s_lay.editT;
        int seW    = editW - findNextW - closeW - btnGap * 2;
        if (seW < 20) seW = 20;
        int fnX    = s_lay.editL + seW + btnGap;
        int fcX    = fnX + findNextW + btnGap;
        DeferWindowPos(hdwp, s_hwndSearchEdit, NULL, s_lay.editL, top, seW,        searchH, SWP_NOZORDER|SWP_NOACTIVATE);
        DeferWindowPos(hdwp, s_hwndFindNext,   NULL, fnX,         top, findNextW,  searchH, SWP_NOZORDER|SWP_NOACTIVATE);
        DeferWindowPos(hdwp, s_hwndFindClose,  NULL, fcX,         top, closeW,     searchH, SWP_NOZORDER|SWP_NOACTIVATE);
    }
    EndDeferWindowPos(hdwp);
}

static void DoHelpFind(HWND hDlg)
{
    HWND hEdit = GetDlgItem(hDlg, IDC_HELP_TEXT);
    if (!hEdit || !s_hwndSearchEdit) return;

    wchar_t query[256] = {};
    GetWindowTextW(s_hwndSearchEdit, query, 256);
    if (!query[0]) return;

    int textLen = GetWindowTextLengthW(hEdit);
    if (textLen <= 0) return;
    std::wstring text(textLen, L'\0');
    GetWindowTextW(hEdit, &text[0], textLen + 1);

    // Case-insensitive: fold both to lower
    std::wstring textLow = text, queryLow = query;
    for (auto& c : textLow)  c = towlower(c);
    for (auto& c : queryLow) c = towlower(c);

    size_t qLen = queryLow.size();
    size_t pos  = textLow.find(queryLow, (size_t)s_searchLastPos);
    if (pos == std::wstring::npos && s_searchLastPos > 0)
        pos = textLow.find(queryLow, 0);   // wrap around

    if (pos == std::wstring::npos) {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    s_searchLastPos = (int)(pos + qLen);
    SendMessage(hEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + qLen));
    SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
    // Refocus Find Next so pressing Enter advances to the next result
    if (s_hwndFindNext) SetFocus(s_hwndFindNext);
    else SetFocus(hEdit);
}

static LRESULT CALLBACK SearchEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            DoHelpFind(GetParent(hwnd));
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            HWND hDlg = GetParent(hwnd);
            s_searchVisible = false;
            ShowWindow(s_hwndSearchEdit, SW_HIDE);
            ShowWindow(s_hwndFindNext,   SW_HIDE);
            ShowWindow(s_hwndFindClose,  SW_HIDE);
            RECT rc; GetClientRect(hDlg, &rc);
            LayoutHelpDlg(hDlg, rc.right, rc.bottom);
            SetFocus(GetDlgItem(hDlg, IDC_HELP_TEXT));
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_INITDIALOG: {
        // Window icon (LR_SHARED — no cleanup needed)
        HICON hIcoBig   = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPICON),
                                           IMAGE_ICON, 32, 32, LR_SHARED);
        HICON hIcoSmall = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPICON),
                                           IMAGE_ICON, 16, 16, LR_SHARED);
        SendMessage(hDlg, WM_SETICON, ICON_BIG,   (LPARAM)hIcoBig);
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcoSmall);

        // Dark title bar — AllowDarkModeForWindow must precede DwmSetWindowAttribute
        BOOL dark = g_darkMode ? TRUE : FALSE;
        HMODULE hUx = GetModuleHandleW(L"uxtheme.dll");
        if (hUx) {
            typedef BOOL (WINAPI* FnAllowDark)(HWND, BOOL);
            auto fnAllow = (FnAllowDark)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
            if (fnAllow) fnAllow(hDlg, dark);
        }
        DwmSetWindowAttribute(hDlg, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(hDlg, 19, &dark, sizeof(dark));

        // Fixed-pitch font so section dividers line up nicely
        if (s_hHelpFont) { DeleteObject(s_hHelpFont); s_hHelpFont = NULL; }
        s_hHelpFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        HWND hEdit = GetDlgItem(hDlg, IDC_HELP_TEXT);
        HWND hOK   = GetDlgItem(hDlg, IDOK);
        if (hEdit && s_hHelpFont) SendMessage(hEdit, WM_SETFONT, (WPARAM)s_hHelpFont, FALSE);

        // Apply themes for dark scrollbar and button
        const wchar_t* ctrlTheme = g_darkMode ? L"DarkMode_Explorer" : L"Explorer";
        if (hEdit) SetWindowTheme(hEdit, ctrlTheme, NULL);
        if (hOK)   SetWindowTheme(hOK,   ctrlTheme, NULL);

        const wchar_t* help =
L"MyPaint  \u2014  Quick Reference\r\n"
L"================================================\r\n"
L"\r\n"
L"KEYBOARD SHORTCUTS\r\n"
L"------------------\r\n"
L"Ctrl+N          New canvas\r\n"
L"Ctrl+O          Open image (or drag an image file onto the window)\r\n"
L"Ctrl+S          Save\r\n"
L"Ctrl+Z          Undo\r\n"
L"Ctrl+Y          Redo\r\n"
L"Ctrl+C/X/V      Copy / Cut / Paste\r\n"
L"                (Ctrl+C with nothing selected copies the whole image)\r\n"
L"Ctrl+A          Select All\r\n"
L"Ctrl+Shift+X    Crop to selection\r\n"
L"Ctrl+= / Ctrl+- Zoom in / out (numpad + / - too)\r\n"
L"Ctrl+0          Fit to window\r\n"
L"Ctrl+1          Actual size (100%)\r\n"
L"Ctrl+F          Search file names in the folder browser; in Help,\r\n"
L"                search this text\r\n"
L"F11             Full screen (View menu); Esc leaves it\r\n"
L"F5              Slideshow (View menu)\r\n"
L"Ctrl+Shift+D    Toggle dark mode\r\n"
L"\r\n"
L"SHAPES  (after drawing, shape enters Edit Mode)\r\n"
L"------------------------------------------------\r\n"
L"Shift+draw          Constrain to square (or 45\u00B0 snap for Line)\r\n"
L"Drag body           Move the shape\r\n"
L"Drag edge handle    Resize\r\n"
L"Shift+corner drag   Resize keeping aspect ratio\r\n"
L"Ctrl+corner drag    Rotate freely\r\n"
L"Arrow keys          Nudge 1 px\r\n"
L"Shift+Arrow         Nudge 10 px\r\n"
L"Enter / click outside   Commit to canvas\r\n"
L"Escape / Ctrl+Z     Cancel / discard\r\n"
L"Status bar (bottom) Shows x,y and w\u00D7h while drawing or editing\r\n"
L"\r\n"
L"TEXT TOOL\r\n"
L"---------\r\n"
L"Click canvas        Create a text box; start typing\r\n"
L"Click inside text   Reposition caret\r\n"
L"Drag edge handle    Resize text box\r\n"
L"Shift+corner drag   Resize keeping aspect ratio\r\n"
L"Ctrl+corner drag    Rotate text box\r\n"
L"Ctrl+Arrow          Move text box 1 px\r\n"
L"Ctrl+Shift+Arrow    Move text box 10 px\r\n"
L"Backspace/Delete    Edit text\r\n"
L"Left/Right/Home/End Navigate caret\r\n"
L"Enter               New line\r\n"
L"Click outside       Commit text to canvas\r\n"
L"Escape              Cancel / discard\r\n"
L"\r\n"
L"SELECTION — Select tool\r\n"
L"-----------------------\r\n"
L"Drag                Draw selection rectangle\r\n"
L"Arrow keys          Move selection 1 px\r\n"
L"Shift+Arrow         Move selection 10 px\r\n"
L"Ctrl+C / Ctrl+X     Copy / Cut to clipboard\r\n"
L"Ctrl+V              Paste (creates paste layer)\r\n"
L"Ctrl+Shift+X        Crop the image to the selection (Image menu)\r\n"
L"Delete              Clear selected area\r\n"
L"Escape              Deselect\r\n"
L"\r\n"
L"MAGIC WAND\r\n"
L"----------\r\n"
L"Click               Select region by colour similarity\r\n"
L"Threshold           How closely a pixel's colour must match the\r\n"
L"                    clicked pixel to be included (0 = exact match,\r\n"
L"                    255 = select everything).  Adjust with the\r\n"
L"                    Threshold edit box or +/- buttons in the\r\n"
L"                    colour strip while the wand is active.\r\n"
L"Contiguous          Checked: only pixels physically connected to\r\n"
L"                    the click point are selected (flood fill).\r\n"
L"                    Unchecked: every pixel on the canvas that\r\n"
L"                    matches the colour is selected, even if\r\n"
L"                    disconnected from the click point.\r\n"
L"Arrow keys          Move selection 1 px\r\n"
L"Shift+Arrow         Move selection 10 px\r\n"
L"Ctrl+C / Ctrl+X     Copy / Cut selected area\r\n"
L"Delete              Clear selected area\r\n"
L"Escape              Deselect\r\n"
L"\r\n"
L"LASSO SELECT  (in the Tools menu)\r\n"
L"----------------------------------\r\n"
L"Draw (hold & drag)  Freehand-draw a selection boundary;\r\n"
L"                    release to close the shape and fill it\r\n"
L"Arrow keys          Move selection 1 px\r\n"
L"Shift+Arrow         Move selection 10 px\r\n"
L"Ctrl+C / Ctrl+X     Copy / Cut selected area\r\n"
L"Delete              Clear selected area\r\n"
L"Escape              Deselect\r\n"
L"\r\n"
L"PASTE LAYER  (after Ctrl+V)\r\n"
L"----------------------------\r\n"
L"Drag body           Move\r\n"
L"Drag edge handle    Resize\r\n"
L"Shift+corner drag   Resize keeping aspect ratio\r\n"
L"Ctrl+corner drag    Rotate\r\n"
L"Arrow keys          Nudge 1 px\r\n"
L"Shift+Arrow         Nudge 10 px\r\n"
L"Enter               Commit to canvas\r\n"
L"Escape / Delete     Cancel\r\n"
L"\r\n"
L"BRUSH TOOL\r\n"
L"----------\r\n"
L"Click the Brush button to choose style:\r\n"
L"Round, Calligraphy, Airbrush, Marker, Crayon, Oil\r\n"
L"\r\n"
L"ZOOM\r\n"
L"----\r\n"
L"Ctrl+Scroll wheel   Zoom in / out\r\n"
L"Scroll wheel        Scroll the canvas (Shift+wheel: sideways)\r\n"
L"Zoom controls in the status bar (bottom-right)\r\n"
L"Fit to window       Shrinks large images to fit; small images stay\r\n"
L"                    at 100% (SVGs still scale up, as they stay sharp)\r\n"
L"\r\n"
L"FULL SCREEN AND SLIDESHOW  (View menu)\r\n"
L"--------------------------------------\r\n"
L"F11                 Full screen: the image on black, nothing else\r\n"
L"F5                  Slideshow: full screen, moving to the next image\r\n"
L"                    every few seconds\r\n"
L"Left / Right        Previous / next image in the folder (also Up /\r\n"
L"                    Down, Page Up / Down and the scroll wheel)\r\n"
L"Home / End          First / last image\r\n"
L"Space               Start / pause the slideshow\r\n"
L"Esc, F11            Back to MyPaint (or double-click the image)\r\n"
L"Mouse               Moving it shows a bar with previous, play /\r\n"
L"                    pause, next, the interval (3, 5 or 10 seconds;\r\n"
L"                    click to change) and close\r\n"
L"A folder browser search (Ctrl+F) limits both to the matching images.\r\n"
L"\r\n"
L"FILES\r\n"
L"-----\r\n"
L"Formats             Opens PNG, JPEG, WebP, GIF, TIFF, BMP and SVG.\r\n"
L"                    HEIC/HEIF (phone photos) and AVIF open when\r\n"
L"                    Windows has Microsoft's free Store extensions:\r\n"
L"                    HEIF Image Extensions (and HEVC Video Extensions\r\n"
L"                    for most phone photos), or AV1 Video Extension.\r\n"
L"                    Saves PNG, JPEG, WebP, GIF, TIFF and BMP.\r\n"
L"Photo orientation   Photos that store which way is up (as phones\r\n"
L"                    and cameras do) open upright, in the window, the\r\n"
L"                    thumbnails and when copied. Saving keeps them\r\n"
L"                    upright.\r\n"
L"Drag and drop      Drop an image file onto the window to open it\r\n"
L"File > Open Recent  The last 10 images opened or saved\r\n"
L"File > Delete       Send the open image to the Recycle Bin and\r\n"
L"                    open the next image in its folder\r\n"
L"File > Open in Inkscape\r\n"
L"                    Shown for SVG files when Inkscape is installed\r\n"
L"File > Save Options Settings for each format, on JPEG / PNG / WebP\r\n"
L"                    tabs, with the estimated size of the open image.\r\n"
L"                    Saving never asks; it uses these settings.\r\n"
L"  JPEG              Quality (default 100). JPEG has no transparency,\r\n"
L"                    so transparent areas are saved as white.\r\n"
L"  PNG               Full colour or 256 colours (much smaller; good\r\n"
L"                    for screenshots and graphics). Keep transparency\r\n"
L"                    (on by default). Images with no transparent\r\n"
L"                    areas are saved without a transparency layer.\r\n"
L"  WebP              Quality (default 100), Lossless, and Keep\r\n"
L"                    transparency (on by default).\r\n"
L"Saving an SVG       MyPaint saves PNG, JPEG, BMP, GIF, TIFF and WebP.\r\n"
L"                    Save on an SVG opens Save As with a .png name, so\r\n"
L"                    the original SVG is never overwritten.\r\n"
L"\r\n"
L"FOLDER BROWSER\r\n"
L"--------------\r\n"
L"A strip of thumbnails of every image in the open file's folder,\r\n"
L"along the bottom of the window. Handy for flicking through photos.\r\n"
L"It opens by default when you start MyPaint with an image (for\r\n"
L"example by double-clicking one), and stays closed when you start\r\n"
L"it on its own. Each way remembers your choice separately.\r\n"
L"\r\n"
L"Open / close        Rightmost button in the status bar. MyPaint\r\n"
L"                    also remembers the strip's height.\r\n"
L"Click thumbnail     Open that image (fitted to the window)\r\n"
L"Left / Right        Previous / next image; wraps around at the ends\r\n"
L"Home / End          First / last image\r\n"
L"Scroll wheel        Scroll the strip (when the mouse is over it)\r\n"
L"Delete              Send the open image to the Recycle Bin\r\n"
L"F2                  Rename the open image (Enter saves, Esc cancels)\r\n"
L"Ctrl+F              Search: a box at the left of the strip; only\r\n"
L"                    images whose names contain the text are shown.\r\n"
L"                    Enter opens the first match and moves to the\r\n"
L"                    strip; Esc (or the x) closes the search.\r\n"
L"Right-click, or the ... button on a thumbnail:\r\n"
L"  Rotate right / left   Turn the file and save it (JPEGs losslessly\r\n"
L"                        when their size allows; not SVG, GIF or\r\n"
L"                        animated WebP)\r\n"
L"  Copy image / path     Put the picture or its full path on the\r\n"
L"                        clipboard\r\n"
L"  Show in Explorer      Open the folder with the file selected\r\n"
L"  Open in Inkscape      SVG files, when Inkscape is installed\r\n"
L"  Rename / Delete       As F2 / Delete, for any thumbnail\r\n"
L"  Properties            The Windows file Properties dialog\r\n"
L"Menu key, Shift+F10 Menu for the open image\r\n"
L"Drag the top edge   Resize the strip, from 40 px up to half the\r\n"
L"                    screen; thumbnails grow with it\r\n"
L"The darker gap      Marks where the list wraps from last to first\r\n"
L"While the strip is open the Hand tool is selected, so dragging the\r\n"
L"canvas pans the image; the previous tool returns when it closes.\r\n"
L"\r\n"
L"OPACITY\r\n"
L"-------\r\n"
L"Opacity control in the colour strip sets brush / fill alpha\r\n"
L"\r\n"
L"CUSTOM COLOURS\r\n"
L"--------------\r\n"
L"The 16 colour swatches in the colour strip store your own\r\n"
L"custom palette. They are visible when the window is wide enough.\r\n"
L"\r\n"
L"Click swatch         Sets whichever colour is currently active\r\n"
L"                     (foreground or background) to that swatch.\r\n"
L"                     Select foreground/background first by clicking\r\n"
L"                     the FG or BG swatch on the left of the strip.\r\n"
L"Reset button (\u21BA)     Restore the 16 swatches to their defaults\r\n"
L"\r\n"
L"Custom colours are saved between sessions.\r\n"
;
        if (hEdit) SetWindowText(hEdit, help);

        // Capture pixel-exact layout metrics so WM_SIZE can resize proportionally
        {
            RECT dlgRc, editRc, btnRc;
            GetClientRect(hDlg, &dlgRc);
            GetWindowRect(hEdit, &editRc);
            MapWindowPoints(HWND_DESKTOP, hDlg, (POINT*)&editRc, 2);
            GetWindowRect(hOK, &btnRc);
            MapWindowPoints(HWND_DESKTOP, hDlg, (POINT*)&btnRc, 2);

            s_lay.editL       = editRc.left;
            s_lay.editT       = editRc.top;
            s_lay.editRMargin = dlgRc.right  - editRc.right;
            s_lay.gapEditBtn  = btnRc.top    - editRc.bottom;
            s_lay.btnW        = btnRc.right  - btnRc.left;
            s_lay.btnH        = btnRc.bottom - btnRc.top;
            s_lay.btnBotMargin = dlgRc.bottom - btnRc.bottom;

            RECT winRc; GetWindowRect(hDlg, &winRc);
            s_lay.minCx = winRc.right  - winRc.left;
            s_lay.minCy = winRc.bottom - winRc.top;
        }

        // Create the Ctrl+F search bar controls (hidden until user presses Ctrl+F)
        s_searchVisible = false;
        s_searchLastPos = 0;
        s_hwndSearchEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL,
            WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, hDlg,
            (HMENU)(UINT_PTR)IDC_HELP_SEARCH, GetModuleHandle(NULL), NULL);
        s_hwndFindNext = CreateWindowExW(0, L"BUTTON", L"Find Next",
            WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hDlg,
            (HMENU)(UINT_PTR)IDC_HELP_FINDNEXT, GetModuleHandle(NULL), NULL);
        s_hwndFindClose = CreateWindowExW(0, L"BUTTON", L"\u00D7 Close",
            WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hDlg,
            (HMENU)(UINT_PTR)IDC_HELP_FINDCLOSE, GetModuleHandle(NULL), NULL);
        if (s_hwndSearchEdit) {
            if (s_hHelpFont) SendMessage(s_hwndSearchEdit, WM_SETFONT, (WPARAM)s_hHelpFont, FALSE);
            SetWindowTheme(s_hwndSearchEdit, ctrlTheme, NULL);
            SendMessage(s_hwndSearchEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"Find\u2026");
            SetWindowSubclass(s_hwndSearchEdit, SearchEditSubclassProc, 0, 0);
            SetWindowSubclass(s_hwndSearchEdit, DarkEditSubclassProc, 1, 0);
        }
        if (s_hwndFindNext) {
            if (s_hHelpFont) SendMessage(s_hwndFindNext, WM_SETFONT, (WPARAM)s_hHelpFont, FALSE);
            SetWindowTheme(s_hwndFindNext, ctrlTheme, NULL);
        }
        if (s_hwndFindClose) {
            if (s_hHelpFont) SendMessage(s_hwndFindClose, WM_SETFONT, (WPARAM)s_hHelpFont, FALSE);
            SetWindowTheme(s_hwndFindClose, ctrlTheme, NULL);
        }
        return TRUE;
    }

    case WM_SIZE: {
        LayoutHelpDlg(hDlg, LOWORD(lParam), HIWORD(lParam));
        return TRUE;
    }

    case WM_GETMINMAXINFO: {
        if (s_lay.minCx > 0) {
            auto* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = s_lay.minCx;
            mmi->ptMinTrackSize.y = s_lay.minCy;
        }
        return TRUE;
    }

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hDlg, &rc);
        FillRect((HDC)wParam, &rc,
            g_darkMode ? g_hDarkBgBrush : GetSysColorBrush(COLOR_3DFACE));
        return TRUE;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        if (g_darkMode && g_hDarkBgBrush) {
            HDC hdcCtrl = (HDC)wParam;
            SetTextColor(hdcCtrl, RGB(220, 220, 220));
            SetBkColor(hdcCtrl, RGB(45, 45, 48));
            return (INT_PTR)g_hDarkBgBrush;
        }
        break;
    }

    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG: {
        if (g_darkMode && g_hDarkBgBrush) {
            HDC hdcCtrl = (HDC)wParam;
            SetTextColor(hdcCtrl, RGB(220, 220, 220));
            SetBkColor(hdcCtrl, RGB(45, 45, 48));
            return (INT_PTR)g_hDarkBgBrush;
        }
        break;
    }

    case WM_APP: {
        // Ctrl+F — show search bar and focus the search edit
        if (!s_searchVisible) {
            s_searchVisible = true;
            s_searchLastPos = 0;
            ShowWindow(s_hwndSearchEdit, SW_SHOW);
            ShowWindow(s_hwndFindNext,   SW_SHOW);
            ShowWindow(s_hwndFindClose,  SW_SHOW);
            RECT rc; GetClientRect(hDlg, &rc);
            LayoutHelpDlg(hDlg, rc.right, rc.bottom);
        }
        SetFocus(s_hwndSearchEdit);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hDlg);
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_HELP_FINDCLOSE) {
            s_searchVisible = false;
            ShowWindow(s_hwndSearchEdit, SW_HIDE);
            ShowWindow(s_hwndFindNext,   SW_HIDE);
            ShowWindow(s_hwndFindClose,  SW_HIDE);
            RECT rc; GetClientRect(hDlg, &rc);
            LayoutHelpDlg(hDlg, rc.right, rc.bottom);
            SetFocus(GetDlgItem(hDlg, IDC_HELP_TEXT));
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_HELP_FINDNEXT) {
            DoHelpFind(hDlg);   // refocuses Find Next internally
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_HELP_SEARCH && HIWORD(wParam) == EN_CHANGE) {
            s_searchLastPos = 0;  // reset position on new query
            return TRUE;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;

    case WM_DESTROY:
        if (s_hHelpFont) { DeleteObject(s_hHelpFont); s_hHelpFont = NULL; }
        s_hwndSearchEdit = NULL;
        s_hwndFindNext   = NULL;
        s_hwndFindClose  = NULL;
        s_searchVisible  = false;
        g_hwndHelp = NULL;
        return TRUE;
    }
    return FALSE;
}

// ============================================================
//  Rotate handle helpers
// ============================================================

// Compute screen position of the rotate handle.
// cenX,cenY = bounding-box centre in screen px; bT = screen Y of top edge; angleDeg = current rotation.
static void CalcRotHandlePos(REAL cenX, REAL cenY, REAL bT, float angleDeg, REAL& rhX, REAL& rhY)
{
    float dy = (bT - kRotHandOff) - cenY;
    float a  = angleDeg * (3.14159265f / 180.f);
    rhX = cenX              - dy * sinf(a);
    rhY = cenY + dy * cosf(a);
}

// True if screen point (mx,my) is within the rotate-handle hit area.
static bool HitRotHandle(int mx, int my, REAL rhX, REAL rhY)
{
    float dx = (float)mx - rhX, dy = (float)my - rhY;
    return dx*dx + dy*dy <= kRotHandRad * kRotHandRad;
}

// Draw rotate handle: connecting line (in current rotated GDI+ frame) + upright icon.
// Resets the GDI+ transform internally. Caller must re-apply rotation after if needed.
static void DrawRotHandle(Graphics& gfx, REAL cenX, REAL cenY, REAL bT, float angleDeg)
{
    // Connecting line (drawn in whatever transform is currently active)
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen lnPen(Color(160, 80, 80, 80), 1.0f);
    gfx.DrawLine(&lnPen, cenX, bT, cenX, bT - kRotHandOff);

    // Icon always upright — reset transform before drawing
    gfx.ResetTransform();
    {
        REAL rhX, rhY;
        CalcRotHandlePos(cenX, cenY, bT, angleDeg, rhX, rhY);
        gfx.SetSmoothingMode(SmoothingModeAntiAlias);
        const REAL R = kRotHandSz / 2.f;
        SolidBrush bgBrush(Color(255, 255, 255, 255));
        Pen        bgPen  (Color(255,   0,   0,   0), 1.5f);
        gfx.FillEllipse(&bgBrush, rhX - R, rhY - R, R * 2, R * 2);
        gfx.DrawEllipse(&bgPen,   rhX - R, rhY - R, R * 2, R * 2);
        if (g_pBmpRotHandle)
            gfx.DrawImage(g_pBmpRotHandle, rhX - R, rhY - R, R * 2, R * 2);
    }
    gfx.SetSmoothingMode(SmoothingModeNone);
}

// ============================================================
//  Canvas window procedure
// ============================================================
// Wheel and touchpad scrolls go to the focused window. Hand them to the canvas
// or folder strip under the cursor instead, so a scroll acts where it points.
static bool ForwardWheelToHovered(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg != WM_MOUSEWHEEL && msg != WM_MOUSEHWHEEL) return false;
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    HWND target = WindowFromPoint(pt);
    if (!target || target == hwnd) return false;
    if (target != g_hwndCanvas && target != g_hwndFolderBrowser) return false;
    SendMessage(target, msg, wParam, lParam);
    return true;
}

LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ForwardWheelToHovered(hwnd, msg, wParam, lParam)) return 0;

    // While an image loads the canvas is hidden behind the spinner; don't let
    // tools act on the old image underneath.
    if (g_imageLoading) {
        switch (msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_MOUSEMOVE:   case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:     case WM_CHAR:
            return 0;
        }
    }

    switch (msg) {

    case WM_LBUTTONDBLCLK: {
        // Double-click while inline text is active — commit and start fresh at click pos
        if (g_textInline) CommitTextInline();
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        // Background outside canvas (grey in light mode, darker in dark mode)
        {
            HBRUSH hBg = CreateSolidBrush(g_darkMode ? RGB(60, 60, 63) : RGB(128, 128, 128));
            FillRect(hdcMem, &rc, hBg);
            DeleteObject(hBg);
        }

        // Loading: show only a spinner on the background
        if (g_imageLoading) {
            {
                Graphics gfx(hdcMem);
                gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                const REAL r = 14.0f;
                REAL cx = rc.right / 2.0f, cy = rc.bottom / 2.0f;
                Pen track(Color(60, 255, 255, 255), 3.0f);
                gfx.DrawEllipse(&track, cx - r, cy - r, 2 * r, 2 * r);
                Pen arc(Color(230, 255, 255, 255), 3.0f);
                arc.SetStartCap(LineCapRound);
                arc.SetEndCap(LineCapRound);
                gfx.DrawArc(&arc, cx - r, cy - r, 2 * r, 2 * r, (REAL)g_spinnerAngle, 90.0f);
            }
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // Ensure checkerboard brush exists (lazy init, 16×16 tiling pattern)
        if (!g_pCheckBrush) {
            g_pCheckBmp = new Bitmap(16, 16, PixelFormat32bppARGB);
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    bool light = ((x / 8) ^ (y / 8)) == 0;
                    g_pCheckBmp->SetPixel(x, y, light ? Color(255,204,204,204) : Color(255,153,153,153));
                }
            g_pCheckBrush = new TextureBrush(g_pCheckBmp);
        }

        if (g_pCanvas) {
            int cw = (int)g_pCanvas->GetWidth();
            int ch = (int)g_pCanvas->GetHeight();
            int cliW = rc.right, cliH = rc.bottom;
            int scaledW = max(1, (int)(cw * g_zoom));
            int scaledH = max(1, (int)(ch * g_zoom));

            // Canvas position: center when much smaller than window, scroll otherwise
            if (cliW - scaledW >= 2 * SCROLL_PAD)
                g_canvasOffX = (cliW - scaledW) / 2;
            else
                g_canvasOffX = SCROLL_PAD - g_scrollX;
            if (cliH - scaledH >= 2 * SCROLL_PAD)
                g_canvasOffY = (cliH - scaledH) / 2;
            else
                g_canvasOffY = SCROLL_PAD - g_scrollY;

            Graphics gfx(hdcMem);
            gfx.SetSmoothingMode(SmoothingModeNone);

            // Border shadow (scaled)
            Pen borderPen(Color(80, 0, 0, 0), 1.0f);
            gfx.DrawRectangle(&borderPen, g_canvasOffX-1, g_canvasOffY-1, scaledW+1, scaledH+1);

            // Checkerboard under transparent canvas pixels
            if (g_pCheckBrush) {
                Matrix mat; mat.Translate((REAL)g_canvasOffX, (REAL)g_canvasOffY);
                g_pCheckBrush->SetTransform(&mat);
                gfx.FillRectangle(g_pCheckBrush, g_canvasOffX, g_canvasOffY, scaledW, scaledH);
                g_pCheckBrush->ResetTransform();
            }

            // Canvas + preview layer at zoom-scaled size
            // For SVGs, draw the pre-rasterized display bitmap 1:1 for crisp output
            Bitmap* drawBmp = (g_isSvgFile && g_svgDisplayBitmap) ? g_svgDisplayBitmap : g_pCanvas;
            gfx.SetPixelOffsetMode(PixelOffsetModeHalf);
            // While an edit is under way the canvas changes every frame
            bool editing = GetCapture() == hwnd && g_currentTool != TOOL_HAND;
            bool drawn = false;
            if (drawBmp == g_pCanvas && g_zoom < 1.0f && !editing &&
                (double)scaledW * scaledH <= 32.0 * 1024 * 1024) {
                // Zoomed out: shrinking a big canvas with a high-quality filter
                // takes far too long to redo every frame, so keep a shrunk copy
                // and reuse it while the view only scrolls.
                if (!g_zoomViewCache || g_zoomViewSrc != g_pCanvas || g_zoomViewVersion != g_canvasVersion ||
                    (int)g_zoomViewCache->GetWidth() != scaledW || (int)g_zoomViewCache->GetHeight() != scaledH) {
                    delete g_zoomViewCache;
                    g_zoomViewCache = new Bitmap(scaledW, scaledH, PixelFormat32bppPARGB);
                    if (g_zoomViewCache->GetLastStatus() == Ok) {
                        Graphics gc(g_zoomViewCache);
                        gc.SetCompositingMode(CompositingModeSourceCopy);
                        gc.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                        gc.SetPixelOffsetMode(PixelOffsetModeHalf);
                        gc.DrawImage(g_pCanvas, 0, 0, scaledW, scaledH);
                        g_zoomViewSrc     = g_pCanvas;
                        g_zoomViewVersion = g_canvasVersion;
                    } else {
                        delete g_zoomViewCache; g_zoomViewCache = nullptr;
                    }
                }
                if (g_zoomViewCache) {
                    gfx.SetInterpolationMode(InterpolationModeNearestNeighbor);   // 1:1 copy
                    gfx.DrawImage(g_zoomViewCache, g_canvasOffX, g_canvasOffY, scaledW, scaledH);
                    drawn = true;
                }
            }
            if (!drawn) {
                gfx.SetInterpolationMode(g_zoom >= 1.0f ? InterpolationModeNearestNeighbor
                                         : editing ? InterpolationModeBilinear   // fast enough per frame
                                                   : InterpolationModeHighQualityBicubic);
                gfx.DrawImage(drawBmp, g_canvasOffX, g_canvasOffY, scaledW, scaledH);
            }
            // The preview layer is canvas-sized and usually empty; skip it then
            if (g_pPreview && g_previewUsed) {
                gfx.SetInterpolationMode(g_zoom >= 1.0f ? InterpolationModeNearestNeighbor
                                         : editing ? InterpolationModeBilinear
                                                   : InterpolationModeHighQualityBicubic);
                gfx.DrawImage(g_pPreview, g_canvasOffX, g_canvasOffY, scaledW, scaledH);
            }
            gfx.SetInterpolationMode(InterpolationModeDefault);
            gfx.SetPixelOffsetMode(PixelOffsetModeDefault);

            // Eraser cursor overlay — shows eraser shape and size over canvas
            if (g_currentTool == TOOL_ERASER && g_eraserCurVis) {
                float halfSz = (float)g_eraserSize * g_zoom / 2.0f;
                float ex = (float)g_eraserCurPos.x;
                float ey = (float)g_eraserCurPos.y;
                gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen wPen(Color(160, 255, 255, 255), 2.5f);
                Pen bPen(Color(220, 0, 0, 0), 1.0f);
                if (g_eraserRound) {
                    gfx.DrawEllipse(&wPen, ex - halfSz, ey - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                    gfx.DrawEllipse(&bPen, ex - halfSz, ey - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                } else {
                    gfx.DrawRectangle(&wPen, ex - halfSz, ey - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                    gfx.DrawRectangle(&bPen, ex - halfSz, ey - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                }
                gfx.SetSmoothingMode(SmoothingModeNone);
            }

            // Smudge cursor overlay — shows smudge shape and size over canvas
            if (g_currentTool == TOOL_SMUDGE && g_smudgeCurVis) {
                float halfSz = (float)g_smudgeSize * g_zoom / 2.0f;
                float sx = (float)g_smudgeCurPos.x;
                float sy = (float)g_smudgeCurPos.y;
                gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen wPen(Color(160, 255, 255, 255), 2.5f);
                Pen bPen(Color(220, 0, 0, 0), 1.0f);
                if (g_smudgeRound) {
                    gfx.DrawEllipse(&wPen, sx - halfSz, sy - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                    gfx.DrawEllipse(&bPen, sx - halfSz, sy - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                } else {
                    gfx.DrawRectangle(&wPen, sx - halfSz, sy - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                    gfx.DrawRectangle(&bPen, sx - halfSz, sy - halfSz, halfSz * 2.0f, halfSz * 2.0f);
                }
                gfx.SetSmoothingMode(SmoothingModeNone);
            }

            // Floating paste bitmap — draw with rotation, handles, and dashed border
            if (g_isPasting && g_pasteBitmap) {
                int px  = g_canvasOffX + (int)(g_pastePos.x * g_zoom);
                int py  = g_canvasOffY + (int)(g_pastePos.y * g_zoom);
                int pwS = max(1, (int)(g_pasteSizeW * g_zoom));
                int phS = max(1, (int)(g_pasteSizeH * g_zoom));
                REAL bCx = px + pwS / 2.0f, bCy = py + phS / 2.0f;
                if (g_pasteAngle != 0.0f) {
                    Matrix rotMat;
                    rotMat.RotateAt(g_pasteAngle, PointF(bCx, bCy));
                    gfx.SetTransform(&rotMat);
                }
                gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                gfx.SetPixelOffsetMode(PixelOffsetModeHalf);
                gfx.DrawImage(g_pasteBitmap, px, py, pwS, phS);
                gfx.SetInterpolationMode(InterpolationModeDefault);
                gfx.SetPixelOffsetMode(PixelOffsetModeDefault);
                // Dashed border + 8 resize handles (same style as shape editing)
                gfx.SetSmoothingMode(SmoothingModeNone);
                const float PAD = 10.f;
                REAL bL = (REAL)px - PAD, bT = (REAL)py - PAD;
                REAL bR = (REAL)(px + pwS) + PAD, bB = (REAL)(py + phS) + PAD;
                float dashes[] = {4.f, 4.f};
                Pen dp(Color(180, 0, 90, 200), 1.f); dp.SetDashPattern(dashes, 2);
                gfx.DrawRectangle(&dp, bL, bT, bR - bL, bB - bT);
                REAL smx = (bL + bR) / 2.f, smy = (bT + bB) / 2.f;
                REAL shpts[][2] = {
                    {bL,bT},{smx,bT},{bR,bT},
                    {bR,smy},
                    {bR,bB},{smx,bB},{bL,bB},
                    {bL,smy},
                };
                const REAL SHS = 3.f;
                SolidBrush shFill(Color(255, 255, 255, 255));
                Pen shBorder(Color(220, 80, 80, 80), 1.0f);
                for (auto& hp : shpts) {
                    gfx.FillRectangle(&shFill,   hp[0]-SHS, hp[1]-SHS, SHS*2, SHS*2);
                    gfx.DrawRectangle(&shBorder, hp[0]-SHS, hp[1]-SHS, SHS*2, SHS*2);
                }
                DrawRotHandle(gfx, bCx, bCy, bT, g_pasteAngle);
                if (g_pasteAngle != 0.0f) gfx.ResetTransform();
                gfx.SetSmoothingMode(SmoothingModeAntiAlias);
            }

            // Dropper area-average rubber band
            if (g_drawing && g_currentTool == TOOL_DROPPER) {
                int rx1 = g_canvasOffX + (int)(min(g_ptStart.x, g_dropperCur.x) * g_zoom);
                int ry1 = g_canvasOffY + (int)(min(g_ptStart.y, g_dropperCur.y) * g_zoom);
                int rx2 = g_canvasOffX + (int)(max(g_ptStart.x, g_dropperCur.x) * g_zoom);
                int ry2 = g_canvasOffY + (int)(max(g_ptStart.y, g_dropperCur.y) * g_zoom);
                if (rx2 - rx1 > 2 || ry2 - ry1 > 2) {
                    float dashes[] = {4.0f, 4.0f};
                    Pen dp1(Color(0,0,0),      1.0f); dp1.SetDashPattern(dashes, 2);
                    Pen dp2(Color(255,255,255), 1.0f); dp2.SetDashPattern(dashes, 2); dp2.SetDashOffset(4.0f);
                    gfx.DrawRectangle(&dp1, rx1, ry1, rx2-rx1, ry2-ry1);
                    gfx.DrawRectangle(&dp2, rx1, ry1, rx2-rx1, ry2-ry1);
                }
            }

            // Floating selection bitmap (during move / resize / rotate)
            if (g_selFloating && g_selFloatBitmap) {
                int dw = g_selRect.right  - g_selRect.left;
                int dh = g_selRect.bottom - g_selRect.top;
                if (dw > 0 && dh > 0) {
                    int fpx = g_canvasOffX + (int)(g_selRect.left * g_zoom);
                    int fpy = g_canvasOffY + (int)(g_selRect.top  * g_zoom);
                    int fpw = max(1, (int)(dw * g_zoom));
                    int fph = max(1, (int)(dh * g_zoom));
                    if (g_selFloatAngle != 0.0f) {
                        REAL cenXScr = fpx + fpw / 2.0f;
                        REAL cenYScr = fpy + fph / 2.0f;
                        Matrix mat;
                        mat.RotateAt(g_selFloatAngle, PointF(cenXScr, cenYScr));
                        gfx.SetTransform(&mat);
                    }
                    gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                    gfx.SetPixelOffsetMode(PixelOffsetModeHalf);
                    gfx.DrawImage(g_selFloatBitmap, fpx, fpy, fpw, fph);
                    gfx.SetInterpolationMode(InterpolationModeDefault);
                    gfx.SetPixelOffsetMode(PixelOffsetModeDefault);
                    if (g_selFloatAngle != 0.0f) gfx.ResetTransform();
                }
            }

            // Committed selection rect — marching ants + 8 resize handles
            if (g_hasSelection && !g_drawing) {
                REAL sx = (REAL)(g_canvasOffX + (int)(g_selRect.left * g_zoom));
                REAL sy = (REAL)(g_canvasOffY + (int)(g_selRect.top  * g_zoom));
                REAL sw = (REAL)(int)((g_selRect.right  - g_selRect.left) * g_zoom);
                REAL sh = (REAL)(int)((g_selRect.bottom - g_selRect.top)  * g_zoom);

                if (!g_hasSelMask) {
                    // Rectangular selection: static dashed outline
                    if (g_selFloating && g_selFloatAngle != 0.0f) {
                        Matrix mat;
                        mat.RotateAt(g_selFloatAngle, PointF(sx + sw / 2.0f, sy + sh / 2.0f));
                        gfx.SetTransform(&mat);
                    }
                    float dashes[] = {4.0f, 4.0f};
                    Pen sp1(Color(0,0,0),       1.0f); sp1.SetDashPattern(dashes, 2);
                    Pen sp2(Color(255,255,255), 1.0f); sp2.SetDashPattern(dashes, 2); sp2.SetDashOffset(4.0f);
                    gfx.DrawRectangle(&sp1, sx, sy, sw, sh);
                    gfx.DrawRectangle(&sp2, sx, sy, sw, sh);
                    if (g_selFloating && g_selFloatAngle != 0.0f) gfx.ResetTransform();
                } else {
                    // Wand (mask) selection: animated marching ants along actual boundary
                    HDC rawDC = gfx.GetHDC();
                    HPEN hBlack = CreatePen(PS_SOLID, 1, RGB(  0,  0,  0));
                    HPEN hWhite = CreatePen(PS_SOLID, 1, RGB(255,255,255));
                    HPEN hOld   = (HPEN)SelectObject(rawDC, hBlack);
                    int phase   = g_marchAntPhase * 4; // advance 4 screen-px per tick

                    // Draw horizontal boundary edges
                    for (const auto& e : g_selHEdges) {
                        int scrY  = g_canvasOffY + (int)((float)e.a  * g_zoom);
                        int scrX1 = g_canvasOffX + (int)((float)e.b1 * g_zoom);
                        int scrX2 = g_canvasOffX + (int)((float)e.b2 * g_zoom);
                        if (scrX2 <= scrX1) continue;
                        int x = scrX1;
                        while (x < scrX2) {
                            int blk   = (x + phase) / 4;
                            int segEnd = min(scrX2, (blk + 1) * 4 - phase);
                            SelectObject(rawDC, (blk & 1) ? hWhite : hBlack);
                            MoveToEx(rawDC, x, scrY, NULL);
                            LineTo  (rawDC, segEnd, scrY);
                            x = segEnd;
                        }
                    }
                    // Draw vertical boundary edges
                    for (const auto& e : g_selVEdges) {
                        int scrX  = g_canvasOffX + (int)((float)e.a  * g_zoom);
                        int scrY1 = g_canvasOffY + (int)((float)e.b1 * g_zoom);
                        int scrY2 = g_canvasOffY + (int)((float)e.b2 * g_zoom);
                        if (scrY2 <= scrY1) continue;
                        int y = scrY1;
                        while (y < scrY2) {
                            int blk   = (y + phase) / 4;
                            int segEnd = min(scrY2, (blk + 1) * 4 - phase);
                            SelectObject(rawDC, (blk & 1) ? hWhite : hBlack);
                            MoveToEx(rawDC, scrX, y, NULL);
                            LineTo  (rawDC, scrX, segEnd);
                            y = segEnd;
                        }
                    }

                    SelectObject(rawDC, hOld);
                    DeleteObject(hBlack); DeleteObject(hWhite);
                    gfx.ReleaseHDC(rawDC);
                }

                // 8 resize handles — only for rectangular selections, not wand masks
                if (!g_hasSelMask) {
                    gfx.SetSmoothingMode(SmoothingModeNone);
                    REAL cenX = sx + sw / 2.f, cenY = sy + sh / 2.f;
                    float angle = g_selFloating ? g_selFloatAngle : 0.f;
                    if (angle != 0.f) {
                        Matrix rotH; rotH.RotateAt(angle, PointF(cenX, cenY));
                        gfx.SetTransform(&rotH);
                    }
                    REAL smx = cenX, smy = cenY;
                    REAL hpts[][2] = {
                        {sx,sy},    {smx,sy},    {sx+sw,sy},
                        {sx+sw,smy},
                        {sx+sw,sy+sh}, {smx,sy+sh}, {sx,sy+sh},
                        {sx,smy},
                    };
                    const REAL SHS = 3.f;
                    SolidBrush hFill(Color(255, 255, 255, 255));
                    Pen hBorder(Color(220, 80, 80, 80), 1.0f);
                    for (auto& hp : hpts) {
                        gfx.FillRectangle(&hFill,   hp[0]-SHS, hp[1]-SHS, SHS*2, SHS*2);
                        gfx.DrawRectangle(&hBorder, hp[0]-SHS, hp[1]-SHS, SHS*2, SHS*2);
                    }
                    // Rotate handle (shown for rectangular, non-mask selections only)
                    // DrawRotHandle resets transform internally
                    DrawRotHandle(gfx, cenX, cenY, sy, angle);
                    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                }
            }

            // Lasso tool in-progress: draw freehand path preview
            if (g_drawing && g_currentTool == TOOL_LASSO && g_lassoPoints.size() >= 2) {
                int np = (int)g_lassoPoints.size();
                std::vector<PointF> spts(np + 1);
                for (int i = 0; i < np; ++i)
                    spts[i] = PointF(g_canvasOffX + g_lassoPoints[i].X * g_zoom,
                                     g_canvasOffY + g_lassoPoints[i].Y * g_zoom);
                spts[np] = spts[0]; // closing line back to start
                float dashes[] = {4.0f, 4.0f};
                Pen lp1(Color(0,0,0),       1.0f); lp1.SetDashPattern(dashes, 2);
                Pen lp2(Color(255,255,255), 1.0f); lp2.SetDashPattern(dashes, 2); lp2.SetDashOffset(4.0f);
                gfx.DrawLines(&lp1, spts.data(), np + 1);
                gfx.DrawLines(&lp2, spts.data(), np + 1);
            }

            // Shape post-draw editing overlay — bounding box + 8 resize handles
            if (g_shapeEditing) {
                gfx.SetSmoothingMode(SmoothingModeNone);
                float sL = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x);
                float sT = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y);
                float sR = (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x);
                float sB = (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y);
                const float PAD = 10.f;
                REAL bL = g_canvasOffX + sL * g_zoom - PAD;
                REAL bT = g_canvasOffY + sT * g_zoom - PAD;
                REAL bR = g_canvasOffX + sR * g_zoom + PAD;
                REAL bB = g_canvasOffY + sB * g_zoom + PAD;
                REAL bCx = (bL + bR) / 2.f, bCy = (bT + bB) / 2.f;

                // Rotate the overlay to match the shape rotation
                if (g_shapeEditAngle != 0.0f) {
                    Matrix rotMat;
                    rotMat.RotateAt(g_shapeEditAngle, PointF(bCx, bCy));
                    gfx.SetTransform(&rotMat);
                }

                float dashes[] = {4.f, 4.f};
                Pen sdp(Color(180, 0, 90, 200), 1.f); sdp.SetDashPattern(dashes, 2);
                gfx.DrawRectangle(&sdp, bL, bT, bR - bL, bB - bT);
                // 8 resize handles
                REAL smx = (bL + bR) / 2.f, smy = (bT + bB) / 2.f;
                REAL shpts[][2] = {
                    {bL,bT},{smx,bT},{bR,bT},
                    {bR,smy},
                    {bR,bB},{smx,bB},{bL,bB},
                    {bL,smy},
                };
                const REAL SHS = 3.f;
                SolidBrush shFill(Color(255, 255, 255, 255));
                Pen shBorder(Color(220, 80, 80, 80), 1.0f);
                for (auto& hp : shpts) {
                    gfx.FillRectangle(&shFill,   hp[0]-SHS, hp[1]-SHS, SHS*2, SHS*2);
                    gfx.DrawRectangle(&shBorder, hp[0]-SHS, hp[1]-SHS, SHS*2, SHS*2);
                }

                DrawRotHandle(gfx, bCx, bCy, bT, g_shapeEditAngle);
                if (g_shapeEditAngle != 0.0f) gfx.ResetTransform();
            }

            // Canvas outline — semi-transparent border always visible above content
            {
                gfx.SetSmoothingMode(SmoothingModeNone);
                Pen pOut(Color(160, 0, 0, 0), 1.0f);
                gfx.DrawRectangle(&pOut, g_canvasOffX - 1, g_canvasOffY - 1, scaledW + 1, scaledH + 1);
                Pen pIn(Color(100, 255, 255, 255), 1.0f);
                gfx.DrawRectangle(&pIn,  g_canvasOffX,     g_canvasOffY,     scaledW - 1, scaledH - 1);

                // Resize handle squares on all 8 edges/corners
                {
                    const int HS = 3;  // half-size: each handle is (HS*2) x (HS*2) pixels
                    struct { int x, y; } handles[] = {
                        { g_canvasOffX,               g_canvasOffY               }, // NW
                        { g_canvasOffX + scaledW / 2, g_canvasOffY               }, // N
                        { g_canvasOffX + scaledW,     g_canvasOffY               }, // NE
                        { g_canvasOffX,               g_canvasOffY + scaledH / 2 }, // W
                        { g_canvasOffX + scaledW,     g_canvasOffY + scaledH / 2 }, // E
                        { g_canvasOffX,               g_canvasOffY + scaledH     }, // SW
                        { g_canvasOffX + scaledW / 2, g_canvasOffY + scaledH     }, // S
                        { g_canvasOffX + scaledW,     g_canvasOffY + scaledH     }, // SE
                    };
                    SolidBrush hFill(Color(255, 255, 255, 255));
                    Pen hBorder(Color(220, 80, 80, 80), 1.0f);
                    for (auto& h : handles) {
                        gfx.FillRectangle(&hFill,   h.x - HS, h.y - HS, HS * 2, HS * 2);
                        gfx.DrawRectangle(&hBorder, h.x - HS, h.y - HS, HS * 2, HS * 2);
                    }
                }

                // Resize preview: dashed outline of new canvas size during drag
                if (g_canvasResizeDir != 0) {
                    int pdx = g_canvasResizeCur.x - g_canvasResizeStart.x;
                    int pdy = g_canvasResizeCur.y - g_canvasResizeStart.y;
                    int pw = g_canvasResizeOrigW, ph = g_canvasResizeOrigH;
                    if (g_canvasResizeDir & 1) pw = max(1, g_canvasResizeOrigW + (int)(pdx / g_zoom));
                    if (g_canvasResizeDir & 4) pw = max(1, g_canvasResizeOrigW - (int)(pdx / g_zoom));
                    if (g_canvasResizeDir & 2) ph = max(1, g_canvasResizeOrigH + (int)(pdy / g_zoom));
                    if (g_canvasResizeDir & 8) ph = max(1, g_canvasResizeOrigH - (int)(pdy / g_zoom));
                    int pwS = max(1, (int)(pw * g_zoom));
                    int phS = max(1, (int)(ph * g_zoom));
                    // Origin shifts left/up when dragging W/N edge
                    int preX = g_canvasOffX + ((g_canvasResizeDir & 4) ? (int)((g_canvasResizeOrigW - pw) * g_zoom) : 0);
                    int preY = g_canvasOffY + ((g_canvasResizeDir & 8) ? (int)((g_canvasResizeOrigH - ph) * g_zoom) : 0);
                    float dashes[] = {5.f, 3.f};
                    Pen pd1(Color(220, 0, 0, 0),       1.0f); pd1.SetDashPattern(dashes, 2);
                    Pen pd2(Color(220, 255, 255, 255), 1.0f); pd2.SetDashPattern(dashes, 2); pd2.SetDashOffset(4.f);
                    gfx.DrawRectangle(&pd1, preX - 1, preY - 1, pwS + 1, phS + 1);
                    gfx.DrawRectangle(&pd2, preX - 1, preY - 1, pwS + 1, phS + 1);
                }
            }

            // Bezier arrow editing overlay (zoom-scaled)
            if (g_bezEditing) {
                gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                // Draw the curve (scale + translate into client coords)
                Matrix mat;
                mat.Scale(g_zoom, g_zoom);
                mat.Translate((REAL)g_canvasOffX, (REAL)g_canvasOffY, MatrixOrderAppend);
                gfx.SetTransform(&mat);
                DrawBezierArrow(gfx);
                gfx.ResetTransform();

                // Control-point handle lines
                auto bsX = [&](int i) { return (REAL)(g_canvasOffX + g_bezPts[i].X * g_zoom); };
                auto bsY = [&](int i) { return (REAL)(g_canvasOffY + g_bezPts[i].Y * g_zoom); };
                Pen hlPen(Color(160, 100, 100, 220), 1.f);
                gfx.DrawLine(&hlPen, bsX(0), bsY(0), bsX(1), bsY(1));
                gfx.DrawLine(&hlPen, bsX(3), bsY(3), bsX(2), bsY(2));

                // Handle circles at P1 and P2
                const REAL HR = 6.f;
                SolidBrush hBr(Color(255,  0, 120, 215));
                Pen        hPen(Color(255, 255, 255, 255), 1.f);
                for (int i = 1; i <= 2; i++) {
                    REAL hx = bsX(i) - HR, hy = bsY(i) - HR;
                    gfx.FillEllipse(&hBr, hx, hy, HR*2, HR*2);
                    gfx.DrawEllipse(&hPen, hx, hy, HR*2, HR*2);
                }

                // Dashed bounding box + resize handles drawn from ref bbox rotated by g_bezAngle.
                // UpdateBezRefBBox() keeps the ref bbox current after every point change.
                {
                    const REAL PAD = 10.f;
                    REAL bL = g_canvasOffX + g_bezRefL * g_zoom - PAD;
                    REAL bT = g_canvasOffY + g_bezRefT * g_zoom - PAD;
                    REAL bR = g_canvasOffX + g_bezRefR * g_zoom + PAD;
                    REAL bB = g_canvasOffY + g_bezRefB * g_zoom + PAD;
                    REAL cenX = (bL + bR) / 2.f, cenY = (bT + bB) / 2.f;
                    if (g_bezAngle != 0.0f) {
                        Matrix rotMat;
                        rotMat.RotateAt(g_bezAngle, PointF(cenX, cenY));
                        gfx.SetTransform(&rotMat);
                    }
                    float dashes[] = {4.f, 4.f};
                    Pen dp(Color(140, 0, 120, 215), 1.f); dp.SetDashPattern(dashes, 2);
                    gfx.DrawRectangle(&dp, bL, bT, bR - bL, bB - bT);
                    REAL mx = cenX, my = cenY;
                    REAL hpts[][2] = {
                        {bL, bT}, {mx, bT}, {bR, bT},
                        {bR, my},
                        {bR, bB}, {mx, bB}, {bL, bB},
                        {bL, my},
                    };
                    const REAL HS = 3.f;
                    SolidBrush hFill(Color(255, 255, 255, 255));
                    Pen hBorder(Color(220, 80, 80, 80), 1.0f);
                    for (auto& hp : hpts) {
                        gfx.FillRectangle(&hFill,   hp[0]-HS, hp[1]-HS, HS*2, HS*2);
                        gfx.DrawRectangle(&hBorder, hp[0]-HS, hp[1]-HS, HS*2, HS*2);
                    }
                    DrawRotHandle(gfx, cenX, cenY, bT, g_bezAngle);
                    if (g_bezAngle != 0.0f) gfx.ResetTransform();
                }
            }

            // ── Inline text caret overlay ────────────────────────────
            // Text is rendered into g_pPreview by RefreshEditingPreview();
            // we only need to draw the blinking caret on top.
            if (g_textInline && g_textCaretVis && g_shapeEditing) {
                float rx = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x);
                float ry = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y);

                REAL bL = g_canvasOffX + rx * g_zoom;
                REAL bT = g_canvasOffY + ry * g_zoom;

                // Apply same rotation as shape editing overlay
                REAL bR = g_canvasOffX + (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x) * g_zoom;
                REAL bB = g_canvasOffY + (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y) * g_zoom;
                REAL bCx = (bL + bR) / 2.f, bCy = (bT + bB) / 2.f;
                if (g_shapeEditAngle != 0.0f) {
                    Matrix rotMat;
                    rotMat.RotateAt(g_shapeEditAngle, PointF(bCx, bCy));
                    gfx.SetTransform(&rotMat);
                }

                // Build screen-scale font for caret measurement
                int gdipStyle2 = FontStyleRegular;
                if (g_textFontStyle & 1) gdipStyle2 |= FontStyleBold;
                if (g_textFontStyle & 2) gdipStyle2 |= FontStyleItalic;
                if (g_textFontStyle & 4) gdipStyle2 |= FontStyleUnderline;
                FontFamily ff2(g_textFontName);
                FontFamily* pFF2 = (ff2.GetLastStatus() == Ok) ? &ff2 : nullptr;
                FontFamily ffFb2(L"Arial"); if (!pFF2) pFF2 = &ffFb2;
                float scrFontSz2 = g_textFontSize * (96.0f / 72.0f) * g_zoom;
                if (scrFontSz2 < 2.f) scrFontSz2 = 2.f;
                Font fontScr2(pFF2, scrFontSz2, gdipStyle2, UnitPixel);
                StringFormat sf2;
                sf2.SetAlignment(StringAlignmentNear);
                sf2.SetLineAlignment(StringAlignmentNear);

                // Find caret line and column
                const std::wstring& ts2 = g_textStr;
                int lineNum2 = 0, lineStart2 = 0;
                for (int i = 0; i < g_textCursorIdx && i < (int)ts2.size(); ++i) {
                    if (ts2[i] == L'\n') { ++lineNum2; lineStart2 = i + 1; }
                }
                std::wstring linePrefix2 = ts2.substr(lineStart2,
                    (size_t)max(0, g_textCursorIdx - lineStart2));

                RectF lhB2;
                gfx.MeasureString(L"Ag", 2, &fontScr2, PointF(0.f,0.f), &sf2, &lhB2);
                float lineH2 = max(lhB2.Height, scrFontSz2 * 1.2f);

                float caretX2 = bL;
                if (!linePrefix2.empty()) {
                    RectF cb2;
                    gfx.MeasureString(linePrefix2.c_str(), (int)linePrefix2.size(),
                                      &fontScr2, PointF(0.f,0.f), &sf2, &cb2);
                    caretX2 = bL + cb2.Width;
                }
                float caretY2 = bT + lineNum2 * lineH2;

                gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen caretPen2(Color(220, 0, 0, 0), 1.5f);
                gfx.DrawLine(&caretPen2, caretX2, caretY2 + 1.f, caretX2, caretY2 + lineH2 - 1.f);
                gfx.SetSmoothingMode(SmoothingModeNone);

                if (g_shapeEditAngle != 0.0f) gfx.ResetTransform();
            }
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!g_pCanvas) break;
        SetFocus(hwnd);
        // ── Hand / pan tool ────────────────────────────────────
        if (g_currentTool == TOOL_HAND) {
            GetCursorPos(&g_handDragStart);
            g_handScrollX0 = g_scrollX;
            g_handScrollY0 = g_scrollY;
            SetCapture(hwnd);
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            return 0;
        }
        int cx = GET_X_LPARAM(lParam);
        int cy = GET_Y_LPARAM(lParam);
        POINT pt = ClientToCanvas(hwnd, cx, cy);

        // ── Click on gray border (outside canvas bitmap) ─────────
        // Recompute offset here so we don't rely on the WM_PAINT-updated globals
        {
            RECT rcC; GetClientRect(hwnd, &rcC);
            int cliW2 = rcC.right, cliH2 = rcC.bottom;
            int scaledW = max(1, (int)(g_pCanvas->GetWidth()  * g_zoom));
            int scaledH = max(1, (int)(g_pCanvas->GetHeight() * g_zoom));
            int offX = (cliW2 - scaledW >= 2*SCROLL_PAD) ? (cliW2 - scaledW)/2 : SCROLL_PAD - g_scrollX;
            int offY = (cliH2 - scaledH >= 2*SCROLL_PAD) ? (cliH2 - scaledH)/2 : SCROLL_PAD - g_scrollY;
            bool overBitmap = (cx >= offX && cx < offX + scaledW &&
                               cy >= offY && cy < offY + scaledH);
            bool onResizeEdge = (HitTestCanvasResize(cx, cy) != 0);
            // Don't commit/clear selection if clicking a handle that extends outside the canvas
            bool hitSelHandle = false;
            if (!overBitmap && g_hasSelection && !g_hasSelMask) {
                REAL sCx = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                REAL sCy = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                REAL sBT = g_canvasOffY + (REAL)(int)(g_selRect.top * g_zoom);
                REAL rhX, rhY;
                CalcRotHandlePos(sCx, sCy, sBT, g_selFloating ? g_selFloatAngle : 0.f, rhX, rhY);
                hitSelHandle = HitRotHandle(cx, cy, rhX, rhY) || (HitTestSelection(cx, cy) != 0);
            }
            if (!overBitmap && !onResizeEdge && !g_bezEditing && !g_shapeEditing && !g_isPasting && !hitSelHandle) {
                if (g_textInline)   CommitTextInline();
                if (g_selFloating)  { CommitSelectionFloat(); g_hasSelection = false; }
                else if (g_hasSelection) { g_hasSelection = false; InvalidateCanvas(); }
                break;
            }
        }

        // ── Paste mode ──────────────────────────────────────────
        if (g_isPasting && g_pasteBitmap) {
            // Check rotate handle before resize handles
            {
                int pwS = max(1, (int)(g_pasteSizeW * g_zoom));
                int phS = max(1, (int)(g_pasteSizeH * g_zoom));
                int px  = g_canvasOffX + (int)(g_pastePos.x * g_zoom);
                int py  = g_canvasOffY + (int)(g_pastePos.y * g_zoom);
                REAL bCx = px + pwS / 2.f, bCy = py + phS / 2.f;
                REAL bT  = (REAL)py - 10.f;  // matches PAD=10 in paint code
                REAL rhX, rhY;
                CalcRotHandlePos(bCx, bCy, bT, g_pasteAngle, rhX, rhY);
                if (HitRotHandle(cx, cy, rhX, rhY)) {
                    g_pasteRotAng0   = atan2f((float)cy - bCy, (float)cx - bCx);
                    g_pasteShapeAng0 = g_pasteAngle;
                    g_pasteDragMode  = 20;
                    g_pasteDragCorner = 0;
                    g_pasteDragStart = {cx, cy};
                    SetCapture(hwnd);
                    return 0;
                }
            }
            int hit = HitTestPaste(cx, cy);
            if (hit != 0) {
                bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isCorner = (hit==4||hit==6||hit==8||hit==10);
                g_pasteDragCorner = isCorner ? hit : 0;
                if (ctrlDown && isCorner) {
                    // Ctrl+corner → rotate drag
                    REAL sCxScr = g_canvasOffX + (g_pastePos.x + g_pasteSizeW / 2.0f) * g_zoom;
                    REAL sCyScr = g_canvasOffY + (g_pastePos.y + g_pasteSizeH / 2.0f) * g_zoom;
                    g_pasteRotAng0   = atan2f((float)cy - sCyScr, (float)cx - sCxScr);
                    g_pasteShapeAng0 = g_pasteAngle;
                    g_pasteDragMode  = 20;
                } else {
                    g_pasteOrigPos   = g_pastePos;
                    g_pasteOrigSizeW = g_pasteSizeW;
                    g_pasteOrigSizeH = g_pasteSizeH;
                    g_pasteDragMode  = hit;
                }
                g_pasteDragStart = {cx, cy};
                SetCapture(hwnd);
                return 0;
            }
            // Click outside paste — commit it, then handle as normal below
            CommitPaste();
        }

        // ── Shape post-draw editing ──────────────────────────────
        if (g_shapeEditing) {
            // Check rotate handle before resize handles
            {
                float sL = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x);
                float sT = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y);
                float sR = (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x);
                float sB = (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y);
                const float PAD = 10.f;
                REAL bL = g_canvasOffX + sL * g_zoom - PAD;
                REAL bT = g_canvasOffY + sT * g_zoom - PAD;
                REAL bR = g_canvasOffX + sR * g_zoom + PAD;
                REAL bB = g_canvasOffY + sB * g_zoom + PAD;
                REAL bCx = (bL + bR) / 2.f, bCy = (bT + bB) / 2.f;
                REAL rhX, rhY;
                CalcRotHandlePos(bCx, bCy, bT, g_shapeEditAngle, rhX, rhY);
                if (HitRotHandle(cx, cy, rhX, rhY)) {
                    REAL shapeCx = bCx, shapeCy = bCy;
                    g_shapeEditRotAng0   = atan2f((float)cy - shapeCy, (float)cx - shapeCx);
                    g_shapeEditShapeAng0 = g_shapeEditAngle;
                    g_shapeEditDrag      = 20;
                    g_shapeEditCorner    = 0;
                    g_shapeEditDragStart = {cx, cy};
                    SetCapture(hwnd);
                    return 0;
                }
            }
            int hit = HitTestShapeEdit(cx, cy);
            if (hit != 0) {
                // For the text tool: a click inside the body (hit==1) either
                // positions the caret (text interior) or moves the box (margin).
                if (hit == 1 && g_shapeEditTool == TOOL_TEXT) {
                    float canX = (cx - g_canvasOffX) / g_zoom;
                    float canY = (cy - g_canvasOffY) / g_zoom;
                    if (IsTextBoxInterior(canX, canY)) {
                        PlaceCaretAtCanvasPoint(canX, canY);
                        return 0;
                    }
                    // else: move-margin → fall through to move logic below
                }
                bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isCorner = (hit==4||hit==6||hit==8||hit==10);
                g_shapeEditCorner = isCorner ? hit : 0;
                if (ctrlDown && isCorner) {
                    // Ctrl+corner → rotate drag
                    REAL shapeCx = g_canvasOffX + (g_shapeEditPt1.x + g_shapeEditPt2.x) / 2.0f * g_zoom;
                    REAL shapeCy = g_canvasOffY + (g_shapeEditPt1.y + g_shapeEditPt2.y) / 2.0f * g_zoom;
                    g_shapeEditRotAng0   = atan2f((float)cy - shapeCy, (float)cx - shapeCx);
                    g_shapeEditShapeAng0 = g_shapeEditAngle;
                    g_shapeEditDrag      = 20;
                    g_shapeEditDragStart = {cx, cy};
                } else {
                    g_shapeEditDrag      = hit;
                    g_shapeEditDragStart = {cx, cy};
                    g_shapeEditOrigPt1   = g_shapeEditPt1;
                    g_shapeEditOrigPt2   = g_shapeEditPt2;
                }
                SetCapture(hwnd);
                return 0;
            }
            if (g_shapeEditTool == TOOL_TEXT) {
                CommitTextInline();  // also clears text state
                return 0;
            }
            CommitShape();  // click outside → commit
            if (g_currentTool == TOOL_TEXT) return 0;
            // fall through to start new drawing for shape tools
        }

        // ── Bezier arrow editing ─────────────────────────────────
        if (g_bezEditing) {
            REAL canX = (REAL)((cx - g_canvasOffX) / g_zoom);
            REAL canY = (REAL)((cy - g_canvasOffY) / g_zoom);
            const REAL HR = 10.f;
            auto hitH = [&](PointF& p) {
                REAL dx = canX - p.X, dy = canY - p.Y;
                return dx*dx + dy*dy <= HR*HR;
            };
            if (hitH(g_bezPts[1])) { SetCapture(hwnd); g_bezDragMode = 1; g_bezDragPivX = (g_bezRefL+g_bezRefR)*0.5f; g_bezDragPivY = (g_bezRefT+g_bezRefB)*0.5f; return 0; }
            if (hitH(g_bezPts[2])) { SetCapture(hwnd); g_bezDragMode = 2; g_bezDragPivX = (g_bezRefL+g_bezRefR)*0.5f; g_bezDragPivY = (g_bezRefT+g_bezRefB)*0.5f; return 0; }
            // Check rotate handle
            {
                const REAL PAD = 10.f;
                REAL bL = g_canvasOffX + g_bezRefL * g_zoom - PAD;
                REAL bT = g_canvasOffY + g_bezRefT * g_zoom - PAD;
                REAL bR = g_canvasOffX + g_bezRefR * g_zoom + PAD;
                REAL bB = g_canvasOffY + g_bezRefB * g_zoom + PAD;
                REAL bcx = (bL + bR) / 2.f, bcy = (bT + bB) / 2.f;
                REAL rhX, rhY;
                CalcRotHandlePos(bcx, bcy, bT, g_bezAngle, rhX, rhY);
                if (HitRotHandle(cx, cy, rhX, rhY)) {
                    memcpy(g_bezDragOrigPts, g_bezPts, sizeof(g_bezPts));
                    g_bezRotAng0  = atan2f((float)cy - bcy, (float)cx - bcx);
                    g_bezAngle0   = g_bezAngle;
                    g_bezDragMode = 20;
                    g_bezCorner   = 0;
                    SetCapture(hwnd);
                    return 0;
                }
            }
            {
                int rzHit = HitTestBezierResize(canX, canY);
                if (rzHit != 0) {
                    bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool isCorner = (rzHit==4||rzHit==6||rzHit==8||rzHit==10);
                    g_bezCorner = isCorner ? rzHit : 0;
                    memcpy(g_bezDragOrigPts, g_bezPts, sizeof(g_bezPts));
                    if (ctrlDown && isCorner) {
                        REAL bcx = g_canvasOffX + (g_bezRefL + g_bezRefR) / 2.0f * g_zoom;
                        REAL bcy = g_canvasOffY + (g_bezRefT + g_bezRefB) / 2.0f * g_zoom;
                        g_bezRotAng0  = atan2f((float)cy - bcy, (float)cx - bcx);
                        g_bezAngle0   = g_bezAngle;
                        g_bezDragMode = 20;
                    } else {
                        g_bezDragRefL = g_bezRefL; g_bezDragRefT = g_bezRefT;
                        g_bezDragRefR = g_bezRefR; g_bezDragRefB = g_bezRefB;
                        g_bezDragMode = rzHit;
                    }
                    SetCapture(hwnd);
                    return 0;
                }
            }
            // Inside-body check: inverse-rotate the click into local frame and test against
            // the ref bbox (same approach as HitTestBezierResize).  This correctly handles
            // rotated arrows — the old raw-AABB test would fire CommitBezier() for clicks
            // that were visually inside the rotated selection box but outside the AABB.
            {
                const REAL PAD = 10.f;
                REAL lx = canX, ly = canY;
                if (g_bezAngle != 0.0f) {
                    REAL pivX = (g_bezRefL + g_bezRefR) * 0.5f, pivY = (g_bezRefT + g_bezRefB) * 0.5f;
                    float rad = -g_bezAngle * (3.14159265f / 180.0f);
                    float c = cosf(rad), s = sinf(rad);
                    float dx = canX - pivX, dy = canY - pivY;
                    lx = pivX + dx * c - dy * s;
                    ly = pivY + dx * s + dy * c;
                }
                if (lx >= g_bezRefL - PAD && lx <= g_bezRefR + PAD &&
                    ly >= g_bezRefT - PAD && ly <= g_bezRefB + PAD) {
                    SetCapture(hwnd);
                    g_bezDragMode = 3;
                    g_bezDragOff  = PointF(canX - g_bezPts[0].X, canY - g_bezPts[0].Y);
                    return 0;
                }
            }
            // Click outside editing area — commit and consume the click
            CommitBezier();
            return 0;
        }

        // ── Canvas edge resize ───────────────────────────────────
        // Skip when TOOL_SELECT has an active selection — selection handles take priority
        {
            bool selectionTakesPriority = (g_currentTool == TOOL_SELECT && g_hasSelection &&
                                           HitTestSelection(cx, cy) != 0);
            if (!selectionTakesPriority) {
                int dir = HitTestCanvasResize(cx, cy);
                if (dir != 0) {
                    g_canvasResizeDir   = dir;
                    g_canvasResizeStart = {cx, cy};
                    g_canvasResizeCur   = {cx, cy};
                    g_canvasResizeOrigW = (int)g_pCanvas->GetWidth();
                    g_canvasResizeOrigH = (int)g_pCanvas->GetHeight();
                    SetCapture(hwnd);
                    return 0;
                }
            }
        }

        // ── Normal tools ─────────────────────────────────────────
        // Clear selection when starting any non-select drawing
        if (g_currentTool != TOOL_SELECT && g_currentTool != TOOL_WAND && g_currentTool != TOOL_LASSO)
            { g_hasSelection = false; StopMarchingAnts(); g_hasSelMask = false; }
        else if (g_currentTool == TOOL_LASSO)
            { StopMarchingAnts(); g_hasSelection = false; g_hasSelMask = false; }

        SetCapture(hwnd);
        g_drawing = true;
        g_ptStart = pt;
        g_ptLast  = pt;

        switch (g_currentTool) {
        case TOOL_SMUDGE:
            PushUndo();
            break;
        case TOOL_PENCIL:
        case TOOL_BRUSH:
        case TOOL_ERASER: {
            PushUndo();
            float r = (g_currentTool == TOOL_ERASER) ? g_eraserSize / 2.0f : g_brushSize / 2.0f;
            if (g_currentTool == TOOL_ERASER) {
                Graphics* gfx = Graphics::FromImage(g_pCanvas);
                gfx->SetSmoothingMode(SmoothingModeAntiAlias);
                gfx->SetCompositingMode(CompositingModeSourceCopy);
                SolidBrush b(Color(0, 0, 0, 0));
                if (g_eraserRound)
                    gfx->FillEllipse(&b, (REAL)pt.x-r, (REAL)pt.y-r, r*2, r*2);
                else
                    gfx->FillRectangle(&b, (REAL)pt.x-r, (REAL)pt.y-r, r*2, r*2);
                delete gfx;
            } else {
                // Seed the stroke RNG then stamp the initial dot via DrawFreehand
                // (ptLast == pt so dist==0; each brush type handles this safely).
                g_brushRandState = (DWORD)(GetTickCount() ^ (DWORD)pt.x ^ ((DWORD)pt.y << 16));
                if (g_currentTool == TOOL_PENCIL) {
                    g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
                    SolidBrush dot(GdipColorFromCOLORREF(g_fgColor, FgAlphaByte()));
                    g_pGfxCanvas->FillEllipse(&dot,
                        (REAL)pt.x - r, (REAL)pt.y - r, r * 2.0f, r * 2.0f);
                } else if (g_brushType == BRUSH_MARKER) {
                    // Marker flat caps produce nothing at zero dist; draw explicit dot.
                    Color mc(170, GetRValue(g_fgColor), GetGValue(g_fgColor), GetBValue(g_fgColor));
                    SolidBrush dot(mc);
                    float mr = g_brushSize * 1.5f / 2.0f;
                    g_pGfxCanvas->FillEllipse(&dot, (REAL)pt.x-mr, (REAL)pt.y-mr, mr*2, mr*2);
                } else {
                    // All other brush types handle dist==0 in DrawFreehand.
                    DrawFreehand(pt.x, pt.y, false);
                }
            }
            InvalidateCanvas();
            break;
        }
        case TOOL_FILL:
            PushUndo();
            FloodFill(pt.x, pt.y);
            g_modified = true; UpdateTitleBar();
            InvalidateCanvas();
            break;
        case TOOL_LINE:
        case TOOL_RECT:
        case TOOL_ELLIPSE:
        case TOOL_RRECT:
        case TOOL_TRIANGLE:
        case TOOL_ARROW_L:
        case TOOL_ARROW_R:
        case TOOL_ARROW_U:
        case TOOL_ARROW_D:
        case TOOL_STICKMAN:
        case TOOL_SPEECH:
        case TOOL_COMPUTER:
        case TOOL_STAR:
        case TOOL_HEART:
        case TOOL_LIGHTNING:
        case TOOL_MUSTACHE:
        case TOOL_DIAMOND:
        case TOOL_PENTAGON:
        case TOOL_HEXAGON_H:
        case TOOL_HEXAGON_V:
        case TOOL_OCTAGON:
        case TOOL_SPIRAL:
        case TOOL_SPIRAL_R:
        case TOOL_CROSS:
        case TOOL_XMARK:
        case TOOL_CHECK:
            PushUndo();
            break;
        case TOOL_ARROW:
            // Undo is pushed inside CommitBezier() when the curve is finalised
            break;
        case TOOL_SELECT:
            if (g_hasSelection && !g_hasSelMask) {
                // Check rotate handle first
                REAL selCenX = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                REAL selCenY = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                REAL selBT   = g_canvasOffY + (REAL)(int)(g_selRect.top  * g_zoom);
                float selAng = g_selFloating ? g_selFloatAngle : 0.f;
                REAL rhX, rhY;
                CalcRotHandlePos(selCenX, selCenY, selBT, selAng, rhX, rhY);
                if (HitRotHandle(cx, cy, rhX, rhY)) {
                    int sw = g_selRect.right - g_selRect.left;
                    int sh = g_selRect.bottom - g_selRect.top;
                    if (sw > 0 && sh > 0 && g_pCanvas && !g_selFloating) {
                        PushUndo();
                        g_selFloatBitmap = g_pCanvas->Clone(
                            g_selRect.left, g_selRect.top, sw, sh, PixelFormat32bppARGB);
                        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceCopy);
                        g_pGfxCanvas->SetSmoothingMode(SmoothingModeNone);
                        SolidBrush transp(Color(0,0,0,0));
                        g_pGfxCanvas->FillRectangle(&transp,
                            (REAL)g_selRect.left, (REAL)g_selRect.top, (REAL)sw, (REAL)sh);
                        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceOver);
                        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
                        g_selFloatOrigRect = g_selRect;
                        g_selFloating = true; g_selFloatAngle = 0.0f;
                    }
                    g_selRotMouseAng0 = atan2f((float)cy - selCenY, (float)cx - selCenX);
                    g_selRotFloatAng0 = g_selFloatAngle;
                    g_selDragMode   = 20;
                    g_selDragCorner = 0;  // prevent ctrl-toggle logic from firing
                    g_selDragStart = {cx, cy};
                    g_drawing = false;
                    SetCapture(hwnd);
                    break;
                }
            }
            if (g_hasSelection) {
                int hit = HitTestSelection(cx, cy);
                if (hit != 0) {
                    bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool isCorner = (hit==2||hit==4||hit==6||hit==8); // NW NE SE SW
                    // Extract pixels into floating bitmap (needed for move, resize, and rotate)
                    int sw = g_selRect.right  - g_selRect.left;
                    int sh = g_selRect.bottom - g_selRect.top;
                    if (sw > 0 && sh > 0 && g_pCanvas && !g_selFloating) {
                        PushUndo();
                        g_selFloatBitmap = g_pCanvas->Clone(
                            g_selRect.left, g_selRect.top, sw, sh, PixelFormat32bppARGB);
                        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceCopy);
                        g_pGfxCanvas->SetSmoothingMode(SmoothingModeNone);
                        SolidBrush transparent(Color(0, 0, 0, 0));
                        g_pGfxCanvas->FillRectangle(&transparent,
                            (REAL)g_selRect.left, (REAL)g_selRect.top, (REAL)sw, (REAL)sh);
                        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceOver);
                        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
                        g_selFloatOrigRect = g_selRect;
                        g_selFloating      = true;
                        g_selFloatAngle    = 0.0f;
                    }
                    g_selDragCorner = isCorner ? hit : 0;
                    if (ctrlDown && isCorner) {
                        // Rotate drag — pivot around selection centre
                        REAL cenXScr = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                        REAL cenYScr = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                        g_selRotMouseAng0 = atan2f((float)cy - cenYScr, (float)cx - cenXScr);
                        g_selRotFloatAng0 = g_selFloatAngle;
                        g_selDragMode     = 20;
                    } else {
                        g_selDragMode     = hit;
                        g_selDragOrigRect = g_selRect;
                    }
                    g_selDragStart = {cx, cy};
                    g_drawing = false;
                    break;
                }
                // Click outside existing selection: commit float, start new rubber band
                if (g_selFloating) CommitSelectionFloat();
            }
            StopMarchingAnts(); g_hasSelection = false; g_hasSelMask = false;  // new drag clears previous selection
            break;
        case TOOL_WAND:
            WandFloodFill(pt.x, pt.y);
            break;
        case TOOL_LASSO:
            g_lassoPoints.clear();
            g_lassoPoints.push_back(PointF((float)pt.x, (float)pt.y));
            break;
        case TOOL_DROPPER: {
            // Immediate single-pixel pick on click
            if (g_pCanvas) {
                Gdiplus::Color px;
                int sx = max(0, min((int)g_pCanvas->GetWidth()  - 1, (int)pt.x));
                int sy = max(0, min((int)g_pCanvas->GetHeight() - 1, (int)pt.y));
                g_pCanvas->GetPixel(sx, sy, &px);
                SetActiveColorFromPick(px.GetR(), px.GetG(), px.GetB(), px.GetA());
            }
            g_dropperCur = pt;
            // Keep capture so we can detect a drag for area-average
            break;
        }
        case TOOL_TEXT: {
            // Commit any previous inline text, then start new text box
            if (g_textInline) CommitTextInline();
            // Build initial bounding box sized to ~1 line of text
            {
                float fontPxH = g_textFontSize * (96.0f / 72.0f);
                int boxW = 150;
                int boxH = max(22, (int)(fontPxH * 1.2f));
                int cw = g_pCanvas ? (int)g_pCanvas->GetWidth()  : 800;
                int ch = g_pCanvas ? (int)g_pCanvas->GetHeight() : 600;
                POINT pt2;
                pt2.x = min((long)(pt.x + boxW), (long)(cw - 1));
                pt2.y = min((long)(pt.y + boxH), (long)(ch - 1));
                // PushUndo so CancelShape can pop it on Escape
                PushUndo();
                g_textInline      = true;
                g_textStr         = L"";
                g_textCursorIdx   = 0;
                g_textCaretVis    = true;
                g_shapeEditing    = true;
                g_shapeEditTool   = TOOL_TEXT;
                g_shapeEditPt1    = pt;
                g_shapeEditPt2    = pt2;
                g_shapeEditDrag   = 0;
                g_shapeEditAngle  = 0.0f;
                g_shapeEditFlipH  = false;
                g_shapeEditFlipV  = false;
                if (!g_textBlinkTimer)
                    g_textBlinkTimer = SetTimer(hwnd, 1001, 500, NULL);
                InvalidateCanvas();
            }
            g_drawing = false; ReleaseCapture();
            break;
        }
        default: break;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (!g_pCanvas) break;
        // ── Hand tool: pan on drag ───────────────────────────
        if (g_currentTool == TOOL_HAND && (wParam & MK_LBUTTON)) {
            POINT cur; GetCursorPos(&cur);
            int dx = cur.x - g_handDragStart.x;
            int dy = cur.y - g_handDragStart.y;
            g_scrollX = max(0, g_handScrollX0 - dx);
            g_scrollY = max(0, g_handScrollY0 - dy);
            UpdateScrollBars();
            InvalidateCanvasView();
            return 0;
        }
        int cx = GET_X_LPARAM(lParam);
        int cy = GET_Y_LPARAM(lParam);
        POINT pt = ClientToCanvas(hwnd, cx, cy);
        UpdateStatusBar(pt.x, pt.y);

        // Show live bounding box in SEL pane for shape-in-edit (non-drag hover)
        if (g_shapeEditing && g_shapeEditDrag == 0) {
            int sx = min(g_shapeEditPt1.x, g_shapeEditPt2.x);
            int sy = min(g_shapeEditPt1.y, g_shapeEditPt2.y);
            int sw = abs(g_shapeEditPt2.x - g_shapeEditPt1.x);
            int sh = abs(g_shapeEditPt2.y - g_shapeEditPt1.y);
            wchar_t shapeBuf[32];
            StringCchPrintf(shapeBuf, 32, L" %d,%d  %d\u00D7%d", sx, sy, sw, sh);
            SetStatusText(STATUS_PANE_SEL, shapeBuf);
        }

        // Track eraser cursor for overlay
        if (g_currentTool == TOOL_ERASER) {
            if (!g_eraserCurVis) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            g_eraserCurPos = { cx, cy };
            g_eraserCurVis = true;
            InvalidateCanvas();
        }

        // Track smudge cursor for overlay
        if (g_currentTool == TOOL_SMUDGE) {
            if (!g_smudgeCurVis) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            g_smudgeCurPos = { cx, cy };
            g_smudgeCurVis = true;
            InvalidateCanvas();
        }

        // Paste drag — move, resize, or rotate
        if (g_pasteDragMode != 0 && g_isPasting) {
            REAL canX = (REAL)((cx - g_canvasOffX) / g_zoom);
            REAL canY = (REAL)((cy - g_canvasOffY) / g_zoom);
            // Allow switching between rotate and resize when Ctrl is toggled mid-drag
            if (g_pasteDragCorner != 0) {
                bool ctrlNow    = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isRotating = (g_pasteDragMode == 20);
                if (ctrlNow && !isRotating) {
                    REAL sCxScr = g_canvasOffX + (g_pastePos.x + g_pasteSizeW / 2.0f) * g_zoom;
                    REAL sCyScr = g_canvasOffY + (g_pastePos.y + g_pasteSizeH / 2.0f) * g_zoom;
                    g_pasteRotAng0   = atan2f((float)cy - sCyScr, (float)cx - sCxScr);
                    g_pasteShapeAng0 = g_pasteAngle;
                    g_pasteDragMode  = 20;
                } else if (!ctrlNow && isRotating) {
                    g_pasteOrigPos   = g_pastePos;
                    g_pasteOrigSizeW = g_pasteSizeW;
                    g_pasteOrigSizeH = g_pasteSizeH;
                    g_pasteDragStart = {cx, cy};
                    g_pasteDragMode  = g_pasteDragCorner;
                }
            }
            if (g_pasteDragMode == 20) {
                // Rotate around bounding-box centre
                REAL sCxScr = g_canvasOffX + (g_pastePos.x + g_pasteSizeW / 2.0f) * g_zoom;
                REAL sCyScr = g_canvasOffY + (g_pastePos.y + g_pasteSizeH / 2.0f) * g_zoom;
                float curAng = atan2f((float)cy - sCyScr, (float)cx - sCxScr);
                g_pasteAngle = g_pasteShapeAng0 + (curAng - g_pasteRotAng0) * (180.0f / 3.14159265f);
            } else if (g_pasteDragMode == 1) {
                // Move
                int ddx = (int)((cx - g_pasteDragStart.x) / g_zoom);
                int ddy = (int)((cy - g_pasteDragStart.y) / g_zoom);
                g_pastePos.x = g_pasteOrigPos.x + ddx;
                g_pastePos.y = g_pasteOrigPos.y + ddy;
            } else {
                // Resize — same corner-anchor math as shape editing
                REAL L0 = (REAL)g_pasteOrigPos.x,  T0 = (REAL)g_pasteOrigPos.y;
                REAL R0 = L0 + g_pasteOrigSizeW,   B0 = T0 + g_pasteOrigSizeH;
                REAL cx0 = (L0 + R0) * 0.5f, cy0 = (T0 + B0) * 0.5f;
                REAL hw  = (R0 - L0) * 0.5f,  hh  = (B0 - T0) * 0.5f;
                float rad  = g_pasteAngle * (3.14159265f / 180.0f);
                float cosA = cosf(rad), sinA = sinf(rad);
                // Anchor signs for each handle: 4=NW 5=N 6=NE 7=E 8=SE 9=S 10=SW 11=W
                static const int xsign[] = {0,0,0,0, +1, 0,-1,-1,-1, 0,+1,+1};
                static const int ysign[] = {0,0,0,0, +1,+1,+1, 0,-1,-1,-1, 0};
                int m = g_pasteDragMode;
                REAL alx = xsign[m] * hw, aly = ysign[m] * hh;
                REAL Ax = cx0 + alx * cosA - aly * sinA;
                REAL Ay = cy0 + alx * sinA + aly * cosA;
                REAL dX = canX - Ax, dY = canY - Ay;
                REAL proj_x =  dX * cosA + dY * sinA;
                REAL proj_y = -dX * sinA + dY * cosA;
                REAL new_hw = hw, new_hh = hh;
                if (xsign[m] != 0) new_hw = max(4.f, ((xsign[m] < 0) ? 1.f : -1.f) * proj_x * 0.5f);
                if (ysign[m] != 0) new_hh = max(4.f, ((ysign[m] < 0) ? 1.f : -1.f) * proj_y * 0.5f);
                // Shift: lock aspect ratio on corner handles
                if ((GetKeyState(VK_SHIFT) & 0x8000) && xsign[m] != 0 && ysign[m] != 0
                    && hw > 0.001f && hh > 0.001f) {
                    float scale = max(new_hw / hw, new_hh / hh);
                    new_hw = hw * scale;
                    new_hh = hh * scale;
                }
                REAL new_alx = xsign[m] * new_hw, new_aly = ysign[m] * new_hh;
                REAL new_cx = Ax - (new_alx * cosA - new_aly * sinA);
                REAL new_cy = Ay - (new_alx * sinA + new_aly * cosA);
                g_pastePos.x  = (int)(new_cx - new_hw);
                g_pastePos.y  = (int)(new_cy - new_hh);
                g_pasteSizeW  = max(1, (int)(new_hw * 2));
                g_pasteSizeH  = max(1, (int)(new_hh * 2));
            }
            InvalidateCanvas();
            break;
        }

        // Bezier handle / whole-curve drag
        if (g_bezEditing && g_bezDragMode != 0) {
            REAL canX = (REAL)((cx - g_canvasOffX) / g_zoom);
            REAL canY = (REAL)((cy - g_canvasOffY) / g_zoom);
            // Clamp to window client-rect canvas coords for all modes.
            // Mouse capture can deliver coordinates far outside the window; without clamping
            // any drag (resize, rotate, move, CP) can permanently push the arrow and its
            // handles off-screen, making the selection box invisible.
            {
                RECT cli; GetClientRect(hwnd, &cli);
                REAL cxMin = (REAL)(0 - g_canvasOffX) / g_zoom;
                REAL cxMax = (REAL)(cli.right  - g_canvasOffX) / g_zoom;
                REAL cyMin = (REAL)(0 - g_canvasOffY) / g_zoom;
                REAL cyMax = (REAL)(cli.bottom - g_canvasOffY) / g_zoom;
                if (canX < cxMin) canX = cxMin;
                if (canX > cxMax) canX = cxMax;
                if (canY < cyMin) canY = cyMin;
                if (canY > cyMax) canY = cyMax;
            }
            // Switch between rotate and resize when Ctrl is toggled mid-drag
            if (g_bezCorner != 0) {
                bool ctrlNow    = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isRotating = (g_bezDragMode == 20);
                if (ctrlNow && !isRotating) {
                    memcpy(g_bezDragOrigPts, g_bezPts, sizeof(g_bezPts));
                    REAL bcxScr = g_canvasOffX + (g_bezRefL + g_bezRefR) / 2.0f * g_zoom;
                    REAL bcyScr = g_canvasOffY + (g_bezRefT + g_bezRefB) / 2.0f * g_zoom;
                    g_bezRotAng0  = atan2f((float)cy - bcyScr, (float)cx - bcxScr);
                    g_bezAngle0   = g_bezAngle;
                    g_bezDragMode = 20;
                } else if (!ctrlNow && isRotating) {
                    memcpy(g_bezDragOrigPts, g_bezPts, sizeof(g_bezPts));
                    g_bezDragRefL = g_bezRefL; g_bezDragRefT = g_bezRefT;
                    g_bezDragRefR = g_bezRefR; g_bezDragRefB = g_bezRefB;
                    g_bezDragMode = g_bezCorner;
                }
            }
            if (g_bezDragMode == 1) {
                g_bezPts[1] = PointF(canX, canY);
            } else if (g_bezDragMode == 2) {
                g_bezPts[2] = PointF(canX, canY);
            } else if (g_bezDragMode == 3) { // whole curve
                REAL newP0x = canX - g_bezDragOff.X;
                REAL newP0y = canY - g_bezDragOff.Y;
                REAL dx = newP0x - g_bezPts[0].X;
                REAL dy = newP0y - g_bezPts[0].Y;
                g_bezPts[0].X += dx; g_bezPts[0].Y += dy;
                g_bezPts[1].X += dx; g_bezPts[1].Y += dy;
                g_bezPts[2].X += dx; g_bezPts[2].Y += dy;
                g_bezPts[3].X += dx; g_bezPts[3].Y += dy;
                // Pre-shift ref bbox so UpdateBezRefBBox uses the correct new pivot
                g_bezRefL += dx; g_bezRefT += dy; g_bezRefR += dx; g_bezRefB += dy;
            } else if (g_bezDragMode == 20) { // rotate
                // Always use fixed ref bbox centre as pivot — keeps UpdateBezRefBBox consistent
                REAL ocx = (g_bezRefL + g_bezRefR) / 2.0f, ocy = (g_bezRefT + g_bezRefB) / 2.0f;
                REAL bcxScr = g_canvasOffX + ocx * g_zoom;
                REAL bcyScr = g_canvasOffY + ocy * g_zoom;
                float curAng = atan2f((float)cy - bcyScr, (float)cx - bcxScr);
                float delta  = curAng - g_bezRotAng0;
                g_bezAngle   = g_bezAngle0 + delta * (180.0f / 3.14159265f);
                float cos_a  = cosf(delta), sin_a = sinf(delta);
                // Rotate all 4 original points around the fixed ref bbox centre
                for (int i = 0; i < 4; i++) {
                    REAL rx = g_bezDragOrigPts[i].X - ocx;
                    REAL ry = g_bezDragOrigPts[i].Y - ocy;
                    g_bezPts[i].X = ocx + rx * cos_a - ry * sin_a;
                    g_bezPts[i].Y = ocy + rx * sin_a + ry * cos_a;
                }
            } else { // resize handle (modes 4–11)
                const REAL PAD = 10.f;
                int m = g_bezDragMode;
                // Drag-start ref bbox (fixed for the lifetime of this drag)
                REAL iL = g_bezDragRefL, iT = g_bezDragRefT, iR = g_bezDragRefR, iB = g_bezDragRefB;
                REAL origCx = (iL + iR) / 2.f, origCy = (iT + iB) / 2.f;
                // Inverse-rotation constants for mapping mouse/points into local frame
                float rad_i = -g_bezAngle * (3.14159265f / 180.0f);
                float ci = cosf(rad_i), si = sinf(rad_i);
                // Map mouse into local frame
                float mdx = canX - origCx, mdy = canY - origCy;
                REAL lMouseX = origCx + mdx * ci - mdy * si;
                REAL lMouseY = origCy + mdx * si + mdy * ci;
                if (g_bezAngle != 0.0f) {
                    // Anchor-pinning resize: fix the opposite corner/edge in canvas space.
                    // ci = cos(θ), -si = sin(θ)  (rad_i = -θ, so ci=cos(θ), si=-sin(θ))
                    static const int xsign[] = {0,0,0,0, +1, 0,-1,-1,-1, 0,+1,+1};
                    static const int ysign[] = {0,0,0,0, +1,+1,+1, 0,-1,-1,-1, 0};
                    float cosA = ci, sinA = -si;
                    REAL hw = (iR - iL) * 0.5f, hh = (iB - iT) * 0.5f;
                    int alxs = xsign[m], alys = ysign[m];
                    REAL alx = (REAL)alxs * hw, aly = (REAL)alys * hh;
                    // Anchor in canvas space (opposite corner/edge from the handle)
                    REAL Ax = origCx + alx * cosA - aly * sinA;
                    REAL Ay = origCy + alx * sinA + aly * cosA;
                    // Mouse delta from anchor projected onto local axes
                    REAL dX = canX - Ax, dY = canY - Ay;
                    REAL proj_x =  dX * cosA + dY * sinA;
                    REAL proj_y = -dX * sinA + dY * cosA;
                    // New half-dims (PAD-adjusted: handle is drawn PAD px outside the bbox edge)
                    REAL new_hw = hw, new_hh = hh;
                    if (alxs != 0) new_hw = max(1.f, ((alxs < 0 ? 1.f : -1.f) * proj_x - PAD) * 0.5f);
                    if (alys != 0) new_hh = max(1.f, ((alys < 0 ? 1.f : -1.f) * proj_y - PAD) * 0.5f);
                    // New center: local frame (for scaling) and canvas frame (for re-rotation)
                    REAL new_cx_local = origCx + (REAL)alxs * (hw - new_hw);
                    REAL new_cy_local = origCy + (REAL)alys * (hh - new_hh);
                    REAL new_alx = (REAL)alxs * new_hw, new_aly = (REAL)alys * new_hh;
                    REAL new_cx = Ax - (new_alx * cosA - new_aly * sinA);
                    REAL new_cy = Ay - (new_alx * sinA + new_aly * cosA);
                    // New bbox in local frame
                    REAL nL = new_cx_local - new_hw, nT = new_cy_local - new_hh;
                    REAL nR = new_cx_local + new_hw, nB = new_cy_local + new_hh;
                    REAL W0 = iR - iL, H0 = iB - iT, W1 = nR - nL, H1 = nB - nT;
                    for (int i = 0; i < 4; i++) {
                        float rx = g_bezDragOrigPts[i].X - origCx, ry = g_bezDragOrigPts[i].Y - origCy;
                        REAL lx = origCx + rx * cosA + ry * sinA;  // inverse rotation
                        REAL ly = origCy - rx * sinA + ry * cosA;
                        REAL sx = W0 > 1.f ? nL + (lx - iL) / W0 * W1 : (nL + nR) * 0.5f;
                        REAL sy = H0 > 1.f ? nT + (ly - iT) / H0 * H1 : (nT + nB) * 0.5f;
                        REAL dsx = sx - new_cx_local, dsy = sy - new_cy_local;
                        g_bezPts[i].X = new_cx + dsx * cosA - dsy * sinA;
                        g_bezPts[i].Y = new_cy + dsx * sinA + dsy * cosA;
                    }
                    // Set pivot to new_cx/new_cy for UpdateBezRefBBox.
                    // Use new_hw/new_hh so the initial extents are reasonable;
                    // UpdateBezRefBBox will overwrite them with symmetric max-extent values.
                    g_bezRefL = new_cx - new_hw;  g_bezRefR = new_cx + new_hw;
                    g_bezRefT = new_cy - new_hh;  g_bezRefB = new_cy + new_hh;
                } else {
                    // No rotation: original direct edge-assignment logic
                    REAL nL = iL, nT = iT, nR = iR, nB = iB;
                    if (m==4||m==10||m==11) nL = lMouseX + PAD;
                    if (m==4||m== 5||m== 6) nT = lMouseY + PAD;
                    if (m==6||m== 7||m== 8) nR = lMouseX - PAD;
                    if (m==8||m== 9||m==10) nB = lMouseY - PAD;
                    if (nR - nL < 2.f) { if (m==4||m==10||m==11) nL = nR - 2.f; else nR = nL + 2.f; }
                    if (nB - nT < 2.f) { if (m==4||m== 5||m== 6) nT = nB - 2.f; else nB = nT + 2.f; }
                    REAL W0 = iR - iL, H0 = iB - iT, W1 = nR - nL, H1 = nB - nT;
                    for (int i = 0; i < 4; i++) {
                        float rx = g_bezDragOrigPts[i].X - origCx, ry = g_bezDragOrigPts[i].Y - origCy;
                        REAL lx = origCx + rx * ci - ry * si;
                        REAL ly = origCy + rx * si + ry * ci;
                        g_bezPts[i].X = W0 > 1.f ? nL + (lx - iL) / W0 * W1 : (nL + nR) / 2.f;
                        g_bezPts[i].Y = H0 > 1.f ? nT + (ly - iT) / H0 * H1 : (nT + nB) / 2.f;
                    }
                }
            }
            // For CP drags (modes 1/2) with a rotated arrow the inverse-rotation pivot
            // drifts toward the off-screen CP each frame, permanently corrupting the bbox.
            // Fix: restore the pivot to its drag-start value before every UpdateBezRefBBox().
            if ((g_bezDragMode == 1 || g_bezDragMode == 2) && g_bezAngle != 0.f) {
                REAL shX = g_bezDragPivX - (g_bezRefL + g_bezRefR) * 0.5f;
                REAL shY = g_bezDragPivY - (g_bezRefT + g_bezRefB) * 0.5f;
                g_bezRefL += shX; g_bezRefR += shX;
                g_bezRefT += shY; g_bezRefB += shY;
            }
            UpdateBezRefBBox(); // keep ref bbox in sync after every point change
            InvalidateCanvas();
            break;
        }

        // Canvas edge resize — track position for live outline, committed on mouseup
        if (g_canvasResizeDir != 0) {
            g_canvasResizeCur = {cx, cy};
            // Show live canvas size in status bar during drag
            if (g_hwndStatus) {
                int pdx = cx - g_canvasResizeStart.x;
                int pdy = cy - g_canvasResizeStart.y;
                int pw = g_canvasResizeOrigW, ph = g_canvasResizeOrigH;
                if (g_canvasResizeDir & 1) pw = max(1, g_canvasResizeOrigW + (int)(pdx / g_zoom));
                if (g_canvasResizeDir & 4) pw = max(1, g_canvasResizeOrigW - (int)(pdx / g_zoom));
                if (g_canvasResizeDir & 2) ph = max(1, g_canvasResizeOrigH + (int)(pdy / g_zoom));
                if (g_canvasResizeDir & 8) ph = max(1, g_canvasResizeOrigH - (int)(pdy / g_zoom));
                wchar_t buf[64];
                StringCchPrintf(buf, 64, L" %d x %d", pw, ph);
                SetStatusText(STATUS_PANE_SIZE, buf);
            }
            InvalidateCanvas();
            break;
        }

        // Selection move/resize/rotate drag
        if (g_selDragMode != 0) {
            // Switch between rotate and resize when Ctrl is toggled mid-drag
            if (g_selDragCorner != 0) {
                bool ctrlNow    = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isRotating = (g_selDragMode == 20);
                if (ctrlNow && !isRotating) {
                    REAL cenXScr = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                    REAL cenYScr = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                    g_selRotMouseAng0 = atan2f((float)cy - cenYScr, (float)cx - cenXScr);
                    g_selRotFloatAng0 = g_selFloatAngle;
                    g_selDragMode     = 20;
                } else if (!ctrlNow && isRotating) {
                    g_selDragOrigRect = g_selRect;
                    g_selDragStart    = {cx, cy};
                    g_selDragMode     = g_selDragCorner;
                }
            }
            if (g_selDragMode == 20) {
                REAL cenXScr = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                REAL cenYScr = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                float curAng = atan2f((float)cy - cenYScr, (float)cx - cenXScr);
                g_selFloatAngle = g_selRotFloatAng0 + (curAng - g_selRotMouseAng0) * (180.0f / 3.14159265f);
                InvalidateCanvas();
                break;
            }
            int cw = (int)g_pCanvas->GetWidth();
            int ch = (int)g_pCanvas->GetHeight();
            RECT r = g_selDragOrigRect;
            if (g_selDragMode == 1) {
                int dx = (int)((cx - g_selDragStart.x) / g_zoom);
                int dy = (int)((cy - g_selDragStart.y) / g_zoom);
                LONG w = r.right - r.left, h = r.bottom - r.top;
                r.left   = g_selDragOrigRect.left + dx;
                r.top    = g_selDragOrigRect.top  + dy;
                r.right  = r.left + w;
                r.bottom = r.top  + h;
            } else {
                // Resize: pin the opposite side in canvas space (works for any rotation).
                REAL canX = (cx - g_canvasOffX) / g_zoom;
                REAL canY = (cy - g_canvasOffY) / g_zoom;
                REAL L0 = (REAL)g_selDragOrigRect.left,  R0 = (REAL)g_selDragOrigRect.right;
                REAL T0 = (REAL)g_selDragOrigRect.top,   B0 = (REAL)g_selDragOrigRect.bottom;
                REAL cx0 = (L0 + R0) * 0.5f, cy0 = (T0 + B0) * 0.5f;
                REAL hw  = (R0 - L0) * 0.5f,  hh  = (B0 - T0) * 0.5f;
                float rad  = g_selFloatAngle * (3.14159265f / 180.0f);
                float cosA = cosf(rad), sinA = sinf(rad);

                // Anchor local offsets per handle (opposite side)
                // selection handles: 2=NW 3=N 4=NE 5=E 6=SE 7=S 8=SW 9=W
                static const int xsign[] = {0,0, +1, 0,-1,-1,-1, 0,+1,+1};
                static const int ysign[] = {0,0, +1,+1,+1, 0,-1,-1,-1, 0};
                int m = g_selDragMode;
                int alxs = xsign[m], alys = ysign[m];
                REAL alx = alxs * hw, aly = alys * hh;

                // Anchor in canvas space
                REAL Ax = cx0 + alx * cosA - aly * sinA;
                REAL Ay = cy0 + alx * sinA + aly * cosA;

                // Project mouse delta onto selection's local axes
                REAL dX = canX - Ax, dY = canY - Ay;
                REAL proj_x =  dX * cosA + dY * sinA;
                REAL proj_y = -dX * sinA + dY * cosA;

                // New half-dims
                REAL new_hw = hw, new_hh = hh;
                if (alxs != 0) new_hw = max(1.f, ((alxs < 0) ? 1.f : -1.f) * proj_x * 0.5f);
                if (alys != 0) new_hh = max(1.f, ((alys < 0) ? 1.f : -1.f) * proj_y * 0.5f);

                // Shift: lock aspect ratio on corner handles
                if ((GetKeyState(VK_SHIFT) & 0x8000) && alxs != 0 && alys != 0
                    && hw > 0.001f && hh > 0.001f) {
                    float scale = max(new_hw / hw, new_hh / hh);
                    new_hw = hw * scale;
                    new_hh = hh * scale;
                }

                // New center from fixed anchor
                REAL new_alx = (alxs == 0) ? 0.f : alxs * new_hw;
                REAL new_aly = (alys == 0) ? 0.f : alys * new_hh;
                REAL new_cx = Ax - (new_alx * cosA - new_aly * sinA);
                REAL new_cy = Ay - (new_alx * sinA + new_aly * cosA);

                r.left   = (LONG)(new_cx - new_hw);
                r.top    = (LONG)(new_cy - new_hh);
                r.right  = (LONG)(new_cx + new_hw);
                r.bottom = (LONG)(new_cy + new_hh);
            }
            g_selRect = r;
            InvalidateCanvas();
            break;
        }

        // Shape editing drag (move, resize, or rotate)
        if (g_shapeEditing && g_shapeEditDrag != 0) {
            REAL canX = (cx - g_canvasOffX) / g_zoom;
            REAL canY = (cy - g_canvasOffY) / g_zoom;
            // Switch between rotate and resize when Ctrl is toggled mid-drag
            if (g_shapeEditCorner != 0) {
                bool ctrlNow   = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isRotating = (g_shapeEditDrag == 20);
                if (ctrlNow && !isRotating) {
                    REAL sCx = g_canvasOffX + (g_shapeEditPt1.x + g_shapeEditPt2.x) / 2.0f * g_zoom;
                    REAL sCy = g_canvasOffY + (g_shapeEditPt1.y + g_shapeEditPt2.y) / 2.0f * g_zoom;
                    g_shapeEditRotAng0   = atan2f((float)cy - sCy, (float)cx - sCx);
                    g_shapeEditShapeAng0 = g_shapeEditAngle;
                    g_shapeEditDrag      = 20;
                } else if (!ctrlNow && isRotating) {
                    g_shapeEditOrigPt1   = g_shapeEditPt1;
                    g_shapeEditOrigPt2   = g_shapeEditPt2;
                    g_shapeEditDragStart = {cx, cy};
                    g_shapeEditDrag      = g_shapeEditCorner;
                }
            }
            if (g_shapeEditDrag == 20) {
                // Rotate around bounding-box centre (client-space angle)
                REAL shapeCx = g_canvasOffX + (g_shapeEditPt1.x + g_shapeEditPt2.x) / 2.0f * g_zoom;
                REAL shapeCy = g_canvasOffY + (g_shapeEditPt1.y + g_shapeEditPt2.y) / 2.0f * g_zoom;
                float curAng = atan2f((float)cy - shapeCy, (float)cx - shapeCx);
                float delta  = curAng - g_shapeEditRotAng0;
                g_shapeEditAngle = g_shapeEditShapeAng0 + delta * (180.0f / 3.14159265f);
            } else if (g_shapeEditDrag == 1) {
                int ddx = (int)((cx - g_shapeEditDragStart.x) / g_zoom);
                int ddy = (int)((cy - g_shapeEditDragStart.y) / g_zoom);
                g_shapeEditPt1.x = g_shapeEditOrigPt1.x + ddx;
                g_shapeEditPt1.y = g_shapeEditOrigPt1.y + ddy;
                g_shapeEditPt2.x = g_shapeEditOrigPt2.x + ddx;
                g_shapeEditPt2.y = g_shapeEditOrigPt2.y + ddy;
            } else {
                REAL L0 = (REAL)min(g_shapeEditOrigPt1.x, g_shapeEditOrigPt2.x);
                REAL T0 = (REAL)min(g_shapeEditOrigPt1.y, g_shapeEditOrigPt2.y);
                REAL R0 = (REAL)max(g_shapeEditOrigPt1.x, g_shapeEditOrigPt2.x);
                REAL B0 = (REAL)max(g_shapeEditOrigPt1.y, g_shapeEditOrigPt2.y);
                REAL cx0 = (L0 + R0) * 0.5f, cy0 = (T0 + B0) * 0.5f;
                REAL hw  = (R0 - L0) * 0.5f,  hh  = (B0 - T0) * 0.5f;
                float rad  = g_shapeEditAngle * (3.14159265f / 180.0f);
                float cosA = cosf(rad), sinA = sinf(rad);

                // Anchor local offsets per handle (opposite side, in unrotated canvas frame)
                // handles: 4=NW 5=N 6=NE 7=E 8=SE 9=S 10=SW 11=W
                static const int xsign[] = {0,0,0,0, +1, 0,-1,-1,-1, 0,+1,+1};
                static const int ysign[] = {0,0,0,0, +1,+1,+1, 0,-1,-1,-1, 0};
                int m = g_shapeEditDrag;
                int alxs = xsign[m], alys = ysign[m];
                REAL alx = alxs * hw, aly = alys * hh;

                // Anchor in canvas space
                REAL Ax = cx0 + alx * cosA - aly * sinA;
                REAL Ay = cy0 + alx * sinA + aly * cosA;

                // Project mouse delta onto shape's local axes
                REAL dX = canX - Ax, dY = canY - Ay;
                REAL proj_x =  dX * cosA + dY * sinA;
                REAL proj_y = -dX * sinA + dY * cosA;

                // Compute new half-dims; sign ensures positive extent in the dragged direction
                REAL new_hw = hw, new_hh = hh;
                if (alxs != 0) new_hw = max(1.f, ((alxs < 0) ? 1.f : -1.f) * proj_x * 0.5f);
                if (alys != 0) new_hh = max(1.f, ((alys < 0) ? 1.f : -1.f) * proj_y * 0.5f);

                // Shift: lock aspect ratio (corner drags only; edge drags move one axis anyway)
                if ((GetKeyState(VK_SHIFT) & 0x8000) && alxs != 0 && alys != 0
                    && hw > 0.001f && hh > 0.001f) {
                    float scaleW = new_hw / hw;
                    float scaleH = new_hh / hh;
                    float scale  = max(scaleW, scaleH);
                    new_hw = hw * scale;
                    new_hh = hh * scale;
                }

                // New anchor local (using updated dims) → derive new center
                REAL new_alx = (alxs == 0) ? 0.f : alxs * new_hw;
                REAL new_aly = (alys == 0) ? 0.f : alys * new_hh;
                REAL new_cx = Ax - (new_alx * cosA - new_aly * sinA);
                REAL new_cy = Ay - (new_alx * sinA + new_aly * cosA);

                REAL L1 = new_cx - new_hw, T1 = new_cy - new_hh;
                REAL R1 = new_cx + new_hw, B1 = new_cy + new_hh;

                REAL W0=R0-L0, H0=B0-T0, W1=R1-L1, H1=B1-T1;
                auto scX = [&](REAL x) { return W0>0.001f ? L1+(x-L0)/W0*W1 : (L1+R1)/2.f; };
                auto scY = [&](REAL y) { return H0>0.001f ? T1+(y-T0)/H0*H1 : (T1+B1)/2.f; };
                g_shapeEditPt1.x = (int)scX((REAL)g_shapeEditOrigPt1.x);
                g_shapeEditPt1.y = (int)scY((REAL)g_shapeEditOrigPt1.y);
                g_shapeEditPt2.x = (int)scX((REAL)g_shapeEditOrigPt2.x);
                g_shapeEditPt2.y = (int)scY((REAL)g_shapeEditOrigPt2.y);
            }
            g_ptStart = g_shapeEditPt1;
            {
                int sx = min(g_shapeEditPt1.x, g_shapeEditPt2.x);
                int sy = min(g_shapeEditPt1.y, g_shapeEditPt2.y);
                int sw = abs(g_shapeEditPt2.x - g_shapeEditPt1.x);
                int sh = abs(g_shapeEditPt2.y - g_shapeEditPt1.y);
                wchar_t shapeBuf[32];
                StringCchPrintf(shapeBuf, 32, L" %d,%d  %d\u00D7%d", sx, sy, sw, sh);
                SetStatusText(STATUS_PANE_SEL, shapeBuf);
            }
            Tool savedTool = g_currentTool;
            g_currentTool  = g_shapeEditTool;
            RenderPreview(g_shapeEditPt2.x, g_shapeEditPt2.y);
            g_currentTool  = savedTool;
            InvalidateCanvas();
            break;
        }

        // Dropper: update drag rect for area-average preview
        if (g_drawing && g_currentTool == TOOL_DROPPER) {
            g_dropperCur = pt;
            InvalidateCanvas();
        }

        if (!g_drawing) break;

        switch (g_currentTool) {
        case TOOL_SMUDGE:
            DoSmudge(pt.x, pt.y);
            g_modified = true;
            InvalidateCanvas();
            break;
        case TOOL_PENCIL:
        case TOOL_BRUSH:
        case TOOL_ERASER:
            DrawFreehand(pt.x, pt.y, g_currentTool == TOOL_ERASER);
            g_modified = true;
            InvalidateCanvas();
            break;
        case TOOL_SELECT:
            RenderPreview(pt.x, pt.y);
            if (g_hwndStatus) {
                int sw = abs(pt.x - g_ptStart.x);
                int sh = abs(pt.y - g_ptStart.y);
                wchar_t selBuf[32];
                StringCchPrintf(selBuf, 32, L" %d\u00D7%d", sw, sh);
                SetStatusText(STATUS_PANE_SEL, selBuf);
            }
            InvalidateCanvas();
            break;
        case TOOL_LASSO:
            if (!g_lassoPoints.empty()) {
                float dx = (float)pt.x - g_lassoPoints.back().X;
                float dy = (float)pt.y - g_lassoPoints.back().Y;
                if (dx*dx + dy*dy >= 4.0f)
                    g_lassoPoints.push_back(PointF((float)pt.x, (float)pt.y));
                InvalidateCanvas();
            }
            break;
        case TOOL_LINE:
        case TOOL_RECT:
        case TOOL_ELLIPSE:
        case TOOL_RRECT:
        case TOOL_TRIANGLE:
        case TOOL_ARROW_L:
        case TOOL_ARROW_R:
        case TOOL_ARROW_U:
        case TOOL_ARROW_D:
        case TOOL_ARROW:
        case TOOL_STICKMAN:
        case TOOL_SPEECH:
        case TOOL_COMPUTER:
        case TOOL_STAR:
        case TOOL_HEART:
        case TOOL_LIGHTNING:
        case TOOL_MUSTACHE:
        case TOOL_DIAMOND:
        case TOOL_PENTAGON:
        case TOOL_HEXAGON_H:
        case TOOL_HEXAGON_V:
        case TOOL_OCTAGON:
        case TOOL_SPIRAL:
        case TOOL_SPIRAL_R:
        case TOOL_CROSS:
        case TOOL_XMARK:
        case TOOL_CHECK:
            {
                POINT ept = pt;
                if (GetKeyState(VK_SHIFT) & 0x8000) {
                    int dx = pt.x - g_ptStart.x;
                    int dy = pt.y - g_ptStart.y;
                    if (g_currentTool == TOOL_LINE) {
                        // Snap to nearest 45°
                        float angle    = atan2f((float)dy, (float)dx);
                        float snapped  = roundf(angle / (3.14159265f / 4.f)) * (3.14159265f / 4.f);
                        float dist     = sqrtf((float)(dx*dx + dy*dy));
                        ept.x = g_ptStart.x + (LONG)roundf(dist * cosf(snapped));
                        ept.y = g_ptStart.y + (LONG)roundf(dist * sinf(snapped));
                    } else {
                        // Constrain bounding box to square
                        int side = max(abs(dx), abs(dy));
                        ept.x = g_ptStart.x + (dx >= 0 ? side : -side);
                        ept.y = g_ptStart.y + (dy >= 0 ? side : -side);
                    }
                }
                int sx = min(g_ptStart.x, ept.x);
                int sy = min(g_ptStart.y, ept.y);
                int sw = abs(ept.x - g_ptStart.x);
                int sh = abs(ept.y - g_ptStart.y);
                wchar_t shapeBuf[32];
                StringCchPrintf(shapeBuf, 32, L" %d,%d  %d\u00D7%d", sx, sy, sw, sh);
                SetStatusText(STATUS_PANE_SEL, shapeBuf);
                RenderPreview(ept.x, ept.y);
            }
            InvalidateCanvas();
            break;
        default: break;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (g_currentTool == TOOL_HAND) { ReleaseCapture(); return 0; }
        if (g_pasteDragMode != 0 && g_isPasting) {
            g_pasteDragMode   = 0;
            g_pasteDragCorner = 0;
            ReleaseCapture();
            break;
        }
        if (g_bezEditing && g_bezDragMode != 0) {
            if ((g_bezDragMode == 1 || g_bezDragMode == 2) && g_bezAngle != 0.f) {
                REAL shX = g_bezDragPivX - (g_bezRefL + g_bezRefR) * 0.5f;
                REAL shY = g_bezDragPivY - (g_bezRefT + g_bezRefB) * 0.5f;
                g_bezRefL += shX; g_bezRefR += shX;
                g_bezRefT += shY; g_bezRefB += shY;
            }
            UpdateBezRefBBox();
            g_bezDragMode = 0;
            g_bezCorner   = 0;
            ReleaseCapture();
            break;
        }
        if (g_shapeEditing && g_shapeEditDrag != 0) {
            g_shapeEditDrag   = 0;
            g_shapeEditCorner = 0;
            ReleaseCapture();
            break;
        }

        // Canvas resize commit
        if (g_canvasResizeDir != 0) {
            int dx = GET_X_LPARAM(lParam) - g_canvasResizeStart.x;
            int dy = GET_Y_LPARAM(lParam) - g_canvasResizeStart.y;
            int newW = g_canvasResizeOrigW;
            int newH = g_canvasResizeOrigH;
            int drawOffX = 0, drawOffY = 0;
            if (g_canvasResizeDir & 1) newW = max(1, g_canvasResizeOrigW + (int)(dx / g_zoom));
            if (g_canvasResizeDir & 4) { newW = max(1, g_canvasResizeOrigW - (int)(dx / g_zoom)); drawOffX = newW - g_canvasResizeOrigW; }
            if (g_canvasResizeDir & 2) newH = max(1, g_canvasResizeOrigH + (int)(dy / g_zoom));
            if (g_canvasResizeDir & 8) { newH = max(1, g_canvasResizeOrigH - (int)(dy / g_zoom)); drawOffY = newH - g_canvasResizeOrigH; }
            if (newW != g_canvasResizeOrigW || newH != g_canvasResizeOrigH) {
                PushUndo();
                Bitmap*   newBmp = new Bitmap(newW, newH, PixelFormat32bppARGB);
                Graphics* newGfx = Graphics::FromImage(newBmp);
                newGfx->SetSmoothingMode(SmoothingModeAntiAlias);
                newGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
                newGfx->Clear(Color(0, 0, 0, 0));
                newGfx->DrawImage(g_pCanvas, drawOffX, drawOffY);
                // Adjust scroll so existing content stays visually fixed after left/top expand
                g_scrollX += (int)(drawOffX * g_zoom);
                g_scrollY += (int)(drawOffY * g_zoom);
                delete g_pGfxCanvas; g_pGfxCanvas = nullptr;
                delete g_pCanvas;
                delete g_pPreview;
                g_pCanvas    = newBmp;
                g_pGfxCanvas = newGfx;
                g_pPreview   = new Bitmap(newW, newH, PixelFormat32bppARGB);
                { Graphics gp(g_pPreview); gp.Clear(Color(0,0,0,0)); }
                g_hasSelection = false;
                g_modified = true;
                UpdateTitleBar();
                UpdateStatusBar(0, 0);
                UpdateScrollBars();
                InvalidateCanvas();
            }
            g_canvasResizeDir = 0;
            ReleaseCapture();
            break;
        }

        // Selection drag finish — keep float alive; commit happens when clicking outside
        if (g_selDragMode != 0) {
            if (g_selRect.left > g_selRect.right)  std::swap(g_selRect.left,  g_selRect.right);
            if (g_selRect.top  > g_selRect.bottom) std::swap(g_selRect.top,   g_selRect.bottom);
            g_hasSelection = (g_selRect.right > g_selRect.left && g_selRect.bottom > g_selRect.top);
            g_selDragMode   = 0;
            g_selDragCorner = 0;
            ReleaseCapture();
            InvalidateCanvas();
            break;
        }

        if (!g_drawing) break;
        g_drawing = false;
        ReleaseCapture();
        if (!g_pCanvas) break;
        POINT pt = ClientToCanvas(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

        switch (g_currentTool) {
        case TOOL_SMUDGE:
        case TOOL_PENCIL:
        case TOOL_BRUSH:
        case TOOL_ERASER:
            g_modified = true; UpdateTitleBar();
            break;
        case TOOL_DROPPER: {
            if (g_pCanvas) {
                int x1d = min(g_ptStart.x, g_dropperCur.x);
                int y1d = min(g_ptStart.y, g_dropperCur.y);
                int x2d = max(g_ptStart.x, g_dropperCur.x);
                int y2d = max(g_ptStart.y, g_dropperCur.y);
                if (x2d - x1d > 2 || y2d - y1d > 2) {
                    // Area average
                    x1d = max(0, x1d); y1d = max(0, y1d);
                    x2d = min((int)g_pCanvas->GetWidth()  - 1, (int)x2d);
                    y2d = min((int)g_pCanvas->GetHeight() - 1, (int)y2d);
                    UINT64 rS=0,gS=0,bS=0,aS=0,n=0;
                    for (int yy=y1d; yy<=y2d; ++yy)
                        for (int xx=x1d; xx<=x2d; ++xx) {
                            Gdiplus::Color px;
                            g_pCanvas->GetPixel(xx, yy, &px);
                            rS+=px.GetR(); gS+=px.GetG(); bS+=px.GetB(); aS+=px.GetA(); ++n;
                        }
                    if (n) SetActiveColorFromPick((BYTE)(rS/n),(BYTE)(gS/n),(BYTE)(bS/n),(BYTE)(aS/n));
                }
            }
            g_drawing = false;
            ReleaseCapture();
            InvalidateCanvas();
            break;
        }
        case TOOL_LINE:
        case TOOL_RECT:
        case TOOL_ELLIPSE:
        case TOOL_RRECT:
        case TOOL_TRIANGLE:
        case TOOL_ARROW_L:
        case TOOL_ARROW_R:
        case TOOL_ARROW_U:
        case TOOL_ARROW_D:
        case TOOL_STICKMAN:
        case TOOL_SPEECH:
        case TOOL_COMPUTER:
        case TOOL_STAR:
        case TOOL_HEART:
        case TOOL_LIGHTNING:
        case TOOL_MUSTACHE:
        case TOOL_DIAMOND:
        case TOOL_PENTAGON:
        case TOOL_HEXAGON_H:
        case TOOL_HEXAGON_V:
        case TOOL_OCTAGON:
        case TOOL_SPIRAL:
        case TOOL_SPIRAL_R:
        case TOOL_CROSS:
        case TOOL_XMARK:
        case TOOL_CHECK:
            // Discard degenerate shapes produced by a click with no drag
            if (abs(pt.x - g_ptStart.x) < 2 && abs(pt.y - g_ptStart.y) < 2) {
                ClearPreview();
                if (!g_undoStack.empty()) { delete g_undoStack.back(); g_undoStack.pop_back(); }
                SetStatusText(STATUS_PANE_SEL, L"");
                InvalidateCanvas();
                break;
            }
            {
                POINT ept = pt;
                if (GetKeyState(VK_SHIFT) & 0x8000) {
                    int dx = pt.x - g_ptStart.x;
                    int dy = pt.y - g_ptStart.y;
                    if (g_currentTool == TOOL_LINE) {
                        float angle   = atan2f((float)dy, (float)dx);
                        float snapped = roundf(angle / (3.14159265f / 4.f)) * (3.14159265f / 4.f);
                        float dist    = sqrtf((float)(dx*dx + dy*dy));
                        ept.x = g_ptStart.x + (LONG)roundf(dist * cosf(snapped));
                        ept.y = g_ptStart.y + (LONG)roundf(dist * sinf(snapped));
                    } else {
                        int side = max(abs(dx), abs(dy));
                        ept.x = g_ptStart.x + (dx >= 0 ? side : -side);
                        ept.y = g_ptStart.y + (dy >= 0 ? side : -side);
                    }
                }
                RenderPreview(ept.x, ept.y);
                // Enter shape editing mode — shape stays on preview until commit/cancel
                g_shapeEditing    = true;
                g_shapeEditTool   = g_currentTool;
                g_shapeEditPt1    = g_ptStart;
                g_shapeEditPt2    = ept;
            }
            g_shapeEditDrag   = 0;
            g_shapeEditAngle  = 0.0f;
            g_shapeEditFlipH  = false;
            g_shapeEditFlipV  = false;
            InvalidateCanvas();
            break;
        case TOOL_ARROW: {
            // Discard degenerate arrow produced by a click with no drag
            if (abs(pt.x - g_ptStart.x) < 2 && abs(pt.y - g_ptStart.y) < 2) {
                ClearPreview();
                if (!g_undoStack.empty()) { delete g_undoStack.back(); g_undoStack.pop_back(); }
                SetStatusText(STATUS_PANE_SEL, L"");
                InvalidateCanvas();
                break;
            }
            // Enter bezier editing mode — set default straight-line control points
            ClearPreview();
            g_bezPts[0] = PointF((REAL)g_ptStart.x, (REAL)g_ptStart.y);
            g_bezPts[3] = PointF((REAL)pt.x, (REAL)pt.y);
            REAL dx13 = (g_bezPts[3].X - g_bezPts[0].X) / 3.f;
            REAL dy13 = (g_bezPts[3].Y - g_bezPts[0].Y) / 3.f;
            g_bezPts[1] = PointF(g_bezPts[0].X + dx13,       g_bezPts[0].Y + dy13);
            g_bezPts[2] = PointF(g_bezPts[0].X + 2.f*dx13,   g_bezPts[0].Y + 2.f*dy13);
            g_bezEditing  = true;
            g_bezDragMode = 0;
            ResetBezRefBBox();
            InvalidateCanvas();
            break;
        }
        case TOOL_SELECT: {
            ClearPreview();
            // Normalise selection rect
            g_selRect.left   = min(g_ptStart.x, pt.x);
            g_selRect.top    = min(g_ptStart.y, pt.y);
            g_selRect.right  = max(g_ptStart.x, pt.x);
            g_selRect.bottom = max(g_ptStart.y, pt.y);
            g_hasSelection = (g_selRect.right > g_selRect.left &&
                              g_selRect.bottom > g_selRect.top);
            InvalidateCanvas();
            break;
        }
        case TOOL_LASSO: {
            if (g_lassoPoints.size() >= 3 && g_pCanvas) {
                int cw = (int)g_pCanvas->GetWidth();
                int ch = (int)g_pCanvas->GetHeight();
                LassoFillMask(cw, ch);
                // Bounding box for selection rect
                float minX = g_lassoPoints[0].X, maxX = minX;
                float minY = g_lassoPoints[0].Y, maxY = minY;
                for (auto& lp : g_lassoPoints) {
                    minX = min(minX, lp.X); maxX = max(maxX, lp.X);
                    minY = min(minY, lp.Y); maxY = max(maxY, lp.Y);
                }
                g_selRect = { max(0,(int)minX), max(0,(int)minY),
                              min(cw,(int)maxX+1), min(ch,(int)maxY+1) };
                g_hasSelMask  = true;
                g_hasSelection = true;
                BuildWandBoundaryEdges(cw, ch);
                if (g_marchAntTimer == 0)
                    g_marchAntTimer = SetTimer(g_hwndCanvas, 1002, 80, NULL);
            }
            g_lassoPoints.clear();
            g_drawing = false;
            ReleaseCapture();
            InvalidateCanvas();
            return 0;
        }
        default: break;
        }
        break;
    }

    case WM_MOUSELEAVE:
        if (g_eraserCurVis) {
            g_eraserCurVis = false;
            InvalidateCanvas();
        }
        if (g_smudgeCurVis) {
            g_smudgeCurVis = false;
            InvalidateCanvas();
        }
        return 0;

    case WM_KEYDOWN:
        // Folder browser navigation takes priority over all tool key handling
        if (g_showFolderBrowser && !g_textInline &&
            !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            if (wParam == VK_LEFT && g_folderCurrentIdx > 0) {
                NavigateToFile(g_folderCurrentIdx - 1);
                return 0;
            }
            if (wParam == VK_RIGHT &&
                g_folderCurrentIdx < (int)g_folderEntries.size() - 1) {
                NavigateToFile(g_folderCurrentIdx + 1);
                return 0;
            }
        }
        // Text inline mode intercepts navigation/control keys
        if (g_textInline) {
            // Ctrl+Arrow moves the text box itself; plain Arrow navigates the cursor.
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
                int dx = 0, dy = 0;
                if      (wParam == VK_LEFT)  dx = -step;
                else if (wParam == VK_RIGHT) dx =  step;
                else if (wParam == VK_UP)    dy = -step;
                else if (wParam == VK_DOWN)  dy =  step;
                if (dx || dy) {
                    g_shapeEditPt1.x += dx; g_shapeEditPt2.x += dx;
                    g_shapeEditPt1.y += dy; g_shapeEditPt2.y += dy;
                    RefreshEditingPreview();
                    return 0;
                }
            }
            switch (wParam) {
            case VK_ESCAPE:
                CancelTextInline(); return 0;
            case VK_LEFT:
                if (g_textCursorIdx > 0) { --g_textCursorIdx; g_textCaretVis = true; InvalidateCanvas(); }
                return 0;
            case VK_RIGHT:
                if (g_textCursorIdx < (int)g_textStr.size()) { ++g_textCursorIdx; g_textCaretVis = true; InvalidateCanvas(); }
                return 0;
            case VK_HOME:
                g_textCursorIdx = 0; g_textCaretVis = true; InvalidateCanvas(); return 0;
            case VK_END:
                g_textCursorIdx = (int)g_textStr.size(); g_textCaretVis = true; InvalidateCanvas(); return 0;
            case VK_DELETE:
                if (g_textCursorIdx < (int)g_textStr.size()) {
                    g_textStr.erase(g_textCursorIdx, 1);
                    g_textCaretVis = true; RefreshEditingPreview();
                }
                return 0;
            default:
                break;  // other keys pass through to WM_CHAR
            }
            break;
        }
        if (wParam == VK_ESCAPE) {
            if (g_shapeEditing)      CancelShape();
            else if (g_bezEditing)   CancelBezier();
            else if (g_isPasting)    CancelPaste();
            else if (g_selFloating)  CancelSelectionFloat();
            else if (g_hasSelection) { g_hasSelection = false; InvalidateCanvas(); }
        } else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (g_shapeEditing) CancelShape();
            else if (g_bezEditing) CancelBezier();
            else PopUndo();
        } else if (wParam == 'Y' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            DoRedo();
        } else if (wParam == VK_RETURN) {
            if (g_shapeEditing) CommitShape();
            else if (g_bezEditing) CommitBezier();
            else if (g_isPasting) CommitPaste();
        } else if (wParam == VK_DELETE) {
            if (g_isPasting)        { CancelPaste();   }
            else if (g_shapeEditing)  { CancelShape();   }
            else if (g_bezEditing)  { CancelBezier();  }
            else if (g_selFloating) {
                // Discard the floating content — canvas already has bg at the extraction site
                delete g_selFloatBitmap; g_selFloatBitmap = nullptr;
                g_selFloating  = false;
                g_selDragMode  = 0;
                g_hasSelection = false;
                g_modified = true; UpdateTitleBar();
                InvalidateCanvas();
            } else if (g_hasSelection && g_hasSelMask && g_pCanvas) {
                PushUndo();
                int bw = g_selRect.right - g_selRect.left;
                int bh = g_selRect.bottom - g_selRect.top;
                BitmapData bd;
                Rect lr(g_selRect.left, g_selRect.top, bw, bh);
                if (g_pCanvas->LockBits(&lr, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                    for (int y = 0; y < bh; ++y) {
                        auto* row = (DWORD*)((BYTE*)bd.Scan0 + (ptrdiff_t)y * bd.Stride);
                        int canY = g_selRect.top + y;
                        for (int x = 0; x < bw; ++x) {
                            int canX = g_selRect.left + x;
                            if ((UINT)canX < g_selMaskW && g_selMask[(size_t)canY * g_selMaskW + canX])
                                row[x] = 0u;
                        }
                    }
                    g_pCanvas->UnlockBits(&bd);
                }
                StopMarchingAnts(); g_hasSelection = false; g_hasSelMask = false;
                g_modified = true; UpdateTitleBar();
                InvalidateCanvas();
            } else if (g_hasSelection && g_pGfxCanvas) {
                PushUndo();
                g_pGfxCanvas->SetCompositingMode(CompositingModeSourceCopy);
                g_pGfxCanvas->SetSmoothingMode(SmoothingModeNone);
                SolidBrush transparent(Color(0, 0, 0, 0));
                g_pGfxCanvas->FillRectangle(&transparent,
                    (REAL)g_selRect.left,  (REAL)g_selRect.top,
                    (REAL)(g_selRect.right - g_selRect.left),
                    (REAL)(g_selRect.bottom - g_selRect.top));
                g_pGfxCanvas->SetCompositingMode(CompositingModeSourceOver);
                g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
                g_hasSelection = false;
                g_modified = true; UpdateTitleBar();
                InvalidateCanvas();
            }
        } else if (wParam == VK_LEFT  || wParam == VK_RIGHT ||
                   wParam == VK_UP    || wParam == VK_DOWN) {
            // Arrow keys nudge the active selection/shape by 1px (Shift = 10px).
            int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
            int dx = (wParam == VK_LEFT) ? -step : (wParam == VK_RIGHT) ? step : 0;
            int dy = (wParam == VK_UP)   ? -step : (wParam == VK_DOWN)  ? step : 0;
            if (g_shapeEditing) {
                g_shapeEditPt1.x += dx; g_shapeEditPt2.x += dx;
                g_shapeEditPt1.y += dy; g_shapeEditPt2.y += dy;
                RefreshEditingPreview();
            } else if (g_bezEditing) {
                for (auto& p : g_bezPts) { p.X += (float)dx; p.Y += (float)dy; }
                UpdateBezRefBBox();
                InvalidateCanvas();
            } else if (g_isPasting) {
                g_pastePos.x += dx; g_pastePos.y += dy;
                InvalidateCanvas();
            } else if (!g_showFolderBrowser && !g_selFloating && !g_hasSelection && g_pCanvas) {
                RECT crc; GetClientRect(hwnd, &crc);
                int scaledW = max(1, (int)(g_pCanvas->GetWidth()  * g_zoom));
                int scaledH = max(1, (int)(g_pCanvas->GetHeight() * g_zoom));
                bool canScrollH = (crc.right  - scaledW < 2 * SCROLL_PAD);
                bool canScrollV = (crc.bottom - scaledH < 2 * SCROLL_PAD);
                if ((dx && canScrollH) || (dy && canScrollV)) {
                    int scrollAmt = (GetKeyState(VK_SHIFT) & 0x8000) ? 50 : 16;
                    if (canScrollH) g_scrollX = max(0, g_scrollX + (dx > 0 ? scrollAmt : dx < 0 ? -scrollAmt : 0));
                    if (canScrollV) g_scrollY = max(0, g_scrollY + (dy > 0 ? scrollAmt : dy < 0 ? -scrollAmt : 0));
                    UpdateScrollBars();
                    InvalidateCanvas();
                }
            } else if (g_selFloating || g_hasSelection) {
                // Lift pixels into a floating bitmap on first arrow-key move,
                // exactly as mouse drag does — otherwise only the marquee moves.
                if (!g_selFloating && g_pCanvas) {
                    int sw = g_selRect.right  - g_selRect.left;
                    int sh = g_selRect.bottom - g_selRect.top;
                    if (sw > 0 && sh > 0) {
                        PushUndo();
                        g_selFloatBitmap = g_pCanvas->Clone(
                            g_selRect.left, g_selRect.top, sw, sh, PixelFormat32bppARGB);
                        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceCopy);
                        g_pGfxCanvas->SetSmoothingMode(SmoothingModeNone);
                        SolidBrush transparent(Color(0, 0, 0, 0));
                        g_pGfxCanvas->FillRectangle(&transparent,
                            (REAL)g_selRect.left, (REAL)g_selRect.top, (REAL)sw, (REAL)sh);
                        g_pGfxCanvas->SetCompositingMode(CompositingModeSourceOver);
                        g_pGfxCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
                        g_selFloatOrigRect = g_selRect;
                        g_selFloating      = true;
                        g_selFloatAngle    = 0.0f;
                    }
                }
                g_selRect.left   += dx; g_selRect.right  += dx;
                g_selRect.top    += dy; g_selRect.bottom += dy;
                InvalidateCanvas();
            }
        }
        break;

    case WM_CHAR: {
        if (!g_textInline) break;
        wchar_t ch = (wchar_t)wParam;
        if (ch == L'\b') {                         // Backspace
            if (g_textCursorIdx > 0) {
                g_textStr.erase(g_textCursorIdx - 1, 1);
                --g_textCursorIdx;
            }
        } else if (ch == L'\r') {                  // Enter → newline
            g_textStr.insert(g_textCursorIdx, 1, L'\n');
            ++g_textCursorIdx;
        } else if (ch == 27) {                     // Escape handled in WM_KEYDOWN
            break;
        } else if (ch >= 32 || ch == L'\t') {      // Printable / tab
            g_textStr.insert(g_textCursorIdx, 1, ch);
            ++g_textCursorIdx;
        }
        g_textCaretVis = true;
        RefreshEditingPreview();
        return 0;
    }

    case WM_TIMER:
        if (wParam == LOAD_SPINNER_TIMER) {
            g_spinnerAngle = (g_spinnerAngle + 12) % 360;
            InvalidateCanvasView();
            return 0;
        }
        if (wParam == 1001 && g_textInline) {
            g_textCaretVis = !g_textCaretVis;
            InvalidateCanvasView();
        }
        if (wParam == 1002) {
            // Marching ants animation
            g_marchAntPhase = (g_marchAntPhase + 1) & 7;
            InvalidateCanvasView();
        }
        break;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            if (g_currentTool == TOOL_HAND) {
                SetCursor(LoadCursor(NULL, (wParam == (WPARAM)hwnd && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                    ? IDC_SIZEALL : IDC_HAND));
                return TRUE;
            }
            POINT cp; GetCursorPos(&cp); ScreenToClient(hwnd, &cp);

            // Outside the canvas bitmap → plain arrow
            if (g_pCanvas) {
                int scaledW = max(1, (int)(g_pCanvas->GetWidth()  * g_zoom));
                int scaledH = max(1, (int)(g_pCanvas->GetHeight() * g_zoom));
                bool overBitmap = (cp.x >= g_canvasOffX && cp.x < g_canvasOffX + scaledW &&
                                   cp.y >= g_canvasOffY && cp.y < g_canvasOffY + scaledH);
                // Check selection handles before early-out (handles at canvas edge extend outside)
                if (!overBitmap && g_hasSelection && !g_hasSelMask && g_currentTool == TOOL_SELECT) {
                    REAL sCx = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                    REAL sCy = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                    REAL sBT = g_canvasOffY + (REAL)(int)(g_selRect.top * g_zoom);
                    REAL rhX, rhY;
                    CalcRotHandlePos(sCx, sCy, sBT, g_selFloating ? g_selFloatAngle : 0.f, rhX, rhY);
                    if (HitRotHandle(cp.x, cp.y, rhX, rhY)) {
                        SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND));
                        return TRUE;
                    }
                    int hit = HitTestSelection(cp.x, cp.y);
                    if (hit >= 2 && hit <= 9) {
                        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        bool isCorner = (hit==2||hit==4||hit==6||hit==8);
                        if (ctrlDown && isCorner) { SetCursor(g_hCurCornerRot ? g_hCurCornerRot : LoadCursor(NULL, IDC_HAND)); return TRUE; }
                        SetCursor(ResizeCursorForHandle(hit, g_selFloatAngle, false));
                        return TRUE;
                    }
                }
                if (!overBitmap && HitTestCanvasResize(cp.x, cp.y) == 0 && !g_isPasting
                        && !g_shapeEditing && !g_bezEditing && g_selDragMode != 20) {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                    return TRUE;
                }
            }

            if (g_isPasting && g_pasteBitmap) {
                if (g_pasteDragMode == 20) { SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND)); return TRUE; }
                // Check rotate handle cursor first
                {
                    int pwS = max(1, (int)(g_pasteSizeW * g_zoom));
                    int phS = max(1, (int)(g_pasteSizeH * g_zoom));
                    int px  = g_canvasOffX + (int)(g_pastePos.x * g_zoom);
                    int py  = g_canvasOffY + (int)(g_pastePos.y * g_zoom);
                    REAL bCx = px + pwS / 2.f, bCy = py + phS / 2.f;
                    REAL bT  = (REAL)py - 10.f;
                    REAL rhX, rhY;
                    CalcRotHandlePos(bCx, bCy, bT, g_pasteAngle, rhX, rhY);
                    if (HitRotHandle(cp.x, cp.y, rhX, rhY)) {
                        SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND));
                        return TRUE;
                    }
                }
                int hit = HitTestPaste(cp.x, cp.y);
                bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isCorner = (hit==4||hit==6||hit==8||hit==10);
                if      (ctrlDown && isCorner)  SetCursor(g_hCurCornerRot ? g_hCurCornerRot : LoadCursor(NULL, IDC_HAND));
                else if (hit == 1)              SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                else if (hit >= 4 && hit <= 11) SetCursor(ResizeCursorForHandle(hit, g_pasteAngle, true));
                else                            SetCursor(LoadCursor(NULL, IDC_ARROW));
                return TRUE;
            }

            if (g_shapeEditing) {
                // Keep rotate cursor for the duration of a rotation drag
                if (g_shapeEditDrag == 20) { SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND)); return TRUE; }
                // Check rotate handle
                {
                    float sL2 = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x);
                    float sT2 = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y);
                    float sR2 = (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x);
                    float sB2 = (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y);
                    const float PAD2 = 10.f;
                    REAL bL2 = g_canvasOffX + sL2 * g_zoom - PAD2;
                    REAL bT2 = g_canvasOffY + sT2 * g_zoom - PAD2;
                    REAL bR2 = g_canvasOffX + sR2 * g_zoom + PAD2;
                    REAL bCx2 = (bL2 + bR2) / 2.f;
                    REAL bCy2 = g_canvasOffY + (sT2 + (sB2-sT2)/2.f) * g_zoom;
                    REAL rhX2, rhY2;
                    CalcRotHandlePos(bCx2, bCy2, bT2, g_shapeEditAngle, rhX2, rhY2);
                    if (HitRotHandle(cp.x, cp.y, rhX2, rhY2)) {
                        SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND));
                        return TRUE;
                    }
                }
                int hit = HitTestShapeEdit(cp.x, cp.y);
                bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isCorner = (hit==4||hit==6||hit==8||hit==10);
                if (ctrlDown && isCorner) {
                    SetCursor(g_hCurCornerRot ? g_hCurCornerRot : LoadCursor(NULL, IDC_HAND));
                } else if (hit == 1 && g_shapeEditTool == TOOL_TEXT) {
                    float canX = (cp.x - g_canvasOffX) / g_zoom;
                    float canY = (cp.y - g_canvasOffY) / g_zoom;
                    if (IsTextBoxInterior(canX, canY))
                        SetCursor(LoadCursor(NULL, IDC_IBEAM));
                    else
                        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                } else if (hit == 1) {
                    SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                } else if (hit >= 4 && hit <= 11) {
                    SetCursor(ResizeCursorForHandle(hit, g_shapeEditAngle, true));
                } else {
                    SetCursor(LoadCursor(NULL, IDC_CROSS));
                }
                return TRUE;
            } else if (g_bezEditing) {
                REAL canX = (REAL)((cp.x - g_canvasOffX) / g_zoom);
                REAL canY = (REAL)((cp.y - g_canvasOffY) / g_zoom);
                const REAL HR = 10.f;
                auto hitH = [&](PointF& p) {
                    REAL ddx = canX - p.X, ddy = canY - p.Y;
                    return ddx*ddx + ddy*ddy <= HR*HR;
                };
                if (g_bezDragMode == 20) { SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND)); return TRUE; }
                if (hitH(g_bezPts[1]) || hitH(g_bezPts[2])) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                } else {
                    // Check rotate handle cursor
                    {
                        const REAL PAD3 = 10.f;
                        REAL bL3 = g_canvasOffX + g_bezRefL * g_zoom - PAD3;
                        REAL bT3 = g_canvasOffY + g_bezRefT * g_zoom - PAD3;
                        REAL bR3 = g_canvasOffX + g_bezRefR * g_zoom + PAD3;
                        REAL bB3 = g_canvasOffY + g_bezRefB * g_zoom + PAD3;
                        REAL bcx3 = (bL3 + bR3) / 2.f, bcy3 = (bT3 + bB3) / 2.f;
                        REAL rhX3, rhY3;
                        CalcRotHandlePos(bcx3, bcy3, bT3, g_bezAngle, rhX3, rhY3);
                        if (HitRotHandle(cp.x, cp.y, rhX3, rhY3)) {
                            SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND));
                            return TRUE;
                        }
                    }
                    int rz = HitTestBezierResize(canX, canY);
                    bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool isCorner = (rz==4||rz==6||rz==8||rz==10);
                    if      (ctrlDown && isCorner) SetCursor(g_hCurCornerRot ? g_hCurCornerRot : LoadCursor(NULL, IDC_HAND));
                    else if (rz == 1)              SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                    else if (rz >= 4 && rz <= 11)  SetCursor(ResizeCursorForHandle(rz, g_bezAngle, true));
                    else                           SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                }
            } else {
                int rdir = HitTestCanvasResize(cp.x, cp.y);
                if (rdir != 0) {
                    if      (rdir == 1 || rdir == 4)  SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                    else if (rdir == 2 || rdir == 8)  SetCursor(LoadCursor(NULL, IDC_SIZENS));
                    else if (rdir == 3 || rdir == 12) SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
                    else if (rdir == 6 || rdir == 9)  SetCursor(LoadCursor(NULL, IDC_SIZENESW));
                    return TRUE;
                }
                if (g_selDragMode == 20) { SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND)); return TRUE; }
                if (g_hasSelection && !g_hasSelMask && g_currentTool == TOOL_SELECT) {
                    REAL selCenX2 = g_canvasOffX + (g_selRect.left + g_selRect.right)  / 2.0f * g_zoom;
                    REAL selCenY2 = g_canvasOffY + (g_selRect.top  + g_selRect.bottom) / 2.0f * g_zoom;
                    REAL selBT2   = g_canvasOffY + (REAL)(int)(g_selRect.top  * g_zoom);
                    float selAng2 = g_selFloating ? g_selFloatAngle : 0.f;
                    REAL rhX2, rhY2;
                    CalcRotHandlePos(selCenX2, selCenY2, selBT2, selAng2, rhX2, rhY2);
                    if (HitRotHandle(cp.x, cp.y, rhX2, rhY2)) {
                        SetCursor(g_hCurRotate ? g_hCurRotate : LoadCursor(NULL, IDC_HAND));
                        return TRUE;
                    }
                }
                if (g_hasSelection && g_currentTool == TOOL_SELECT) {
                    int hit = HitTestSelection(cp.x, cp.y);
                    bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool isCorner = (hit==2||hit==4||hit==6||hit==8);
                    if (ctrlDown && isCorner)  { SetCursor(g_hCurCornerRot ? g_hCurCornerRot : LoadCursor(NULL, IDC_HAND)); return TRUE; }
                    if (hit == 1)              { SetCursor(LoadCursor(NULL, IDC_SIZEALL)); return TRUE; }
                    if (hit >= 2 && hit <= 9)  { SetCursor(ResizeCursorForHandle(hit, g_selFloatAngle, false)); return TRUE; }
                }
                // Tool-specific cursor over the canvas body
                switch (g_currentTool) {
                case TOOL_TEXT:
                    SetCursor(LoadCursor(NULL, IDC_IBEAM));
                    break;
                case TOOL_PENCIL:
                    SetCursor(g_hCurPencil ? g_hCurPencil : LoadCursor(NULL, IDC_CROSS));
                    break;
                case TOOL_BRUSH: {
                    if (g_fgColor != g_lastBrushCursorColor)
                        RecreateBrushCursors();  // color changed since last build
                    HCURSOR hc = g_hCurBrush[g_brushType];
                    SetCursor(hc ? hc : LoadCursor(NULL, IDC_CROSS));
                    break;
                }
                case TOOL_DROPPER:
                    if (g_drawing)
                        SetCursor(LoadCursor(NULL, IDC_CROSS));
                    else
                        SetCursor(g_hCurDropper ? g_hCurDropper : LoadCursor(NULL, IDC_CROSS));
                    break;
                case TOOL_FILL:
                    SetCursor(g_hCurFill ? g_hCurFill : LoadCursor(NULL, IDC_CROSS));
                    break;
                case TOOL_LASSO:
                    SetCursor(g_hCurLasso ? g_hCurLasso : LoadCursor(NULL, IDC_CROSS));
                    break;
                case TOOL_WAND:
                    SetCursor(g_hCurWand ? g_hCurWand : LoadCursor(NULL, IDC_CROSS));
                    break;
                case TOOL_SMUDGE:
                    SetCursor(g_hCurSmudge ? g_hCurSmudge : LoadCursor(NULL, IDC_CROSS));
                    break;
                default:
                    SetCursor(LoadCursor(NULL, IDC_CROSS));
                    break;
                }
            }
            return TRUE;
        }
        break;

    case WM_SIZE:
        if (g_pendingCenter) { g_pendingCenter = false; CenterScrollOnCanvas(); }
        else if (g_zoomFitMode) ZoomToFit();   // keep fit-to-window as the viewport changes
        else UpdateScrollBars();
        break;

    case WM_HSCROLL: {
        SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_HORZ, &si);
        switch (LOWORD(wParam)) {
        case SB_LINELEFT:      si.nPos -= 20;            break;
        case SB_LINERIGHT:     si.nPos += 20;            break;
        case SB_PAGELEFT:      si.nPos -= (int)si.nPage; break;
        case SB_PAGERIGHT:     si.nPos += (int)si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: si.nPos = si.nTrackPos;   break;
        }
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        GetScrollInfo(hwnd, SB_HORZ, &si);
        g_scrollX = si.nPos;
        InvalidateCanvasView();
        break;
    }

    case WM_VSCROLL: {
        SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        switch (LOWORD(wParam)) {
        case SB_LINEUP:        si.nPos -= 20;            break;
        case SB_LINEDOWN:      si.nPos += 20;            break;
        case SB_PAGEUP:        si.nPos -= (int)si.nPage; break;
        case SB_PAGEDOWN:      si.nPos += (int)si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: si.nPos = si.nTrackPos;   break;
        }
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        GetScrollInfo(hwnd, SB_VERT, &si);
        g_scrollY = si.nPos;
        InvalidateCanvasView();
        break;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT mp = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &mp);
        if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
            g_zoomFitMode = false;
            if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
            if (delta > 0) ZoomIn(mp.x, mp.y); else ZoomOut(mp.x, mp.y);
        } else if (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) {
            // Shift+wheel scrolls horizontally
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_HORZ, &si);
            si.nPos -= delta * 60 / WHEEL_DELTA;
            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            GetScrollInfo(hwnd, SB_HORZ, &si);
            g_scrollX = si.nPos;
            InvalidateCanvasView();
        } else {
            // Use delta*60/WHEEL_DELTA (not (delta/WHEEL_DELTA)*60) so touchpad
            // fractional deltas (< WHEEL_DELTA) still produce smooth movement
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            si.nPos -= delta * 60 / WHEEL_DELTA;
            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            GetScrollInfo(hwnd, SB_VERT, &si);
            g_scrollY = si.nPos;
            InvalidateCanvasView();
        }
        return 0;  // must NOT fall to DefWindowProc — it would re-propagate to parent
    }

    case WM_MOUSEHWHEEL: {
        // Horizontal two-finger scroll (positive delta = scroll right)
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_HORZ, &si);
        si.nPos += delta * 60 / WHEEL_DELTA;
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        GetScrollInfo(hwnd, SB_HORZ, &si);
        g_scrollX = si.nPos;
        InvalidateCanvasView();
        return 0;
    }

    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Convert an SVG elliptical arc segment to one or more GDI+ cubic bezier segments.
static void AddSvgArc(GraphicsPath& gp,
                       float x0, float y0,
                       float rx, float ry, float xRotDeg,
                       bool largeArc, bool sweep,
                       float x1, float y1, float scale)
{
    if (rx < 1e-6f || ry < 1e-6f) { gp.AddLine(x0*scale,y0*scale,x1*scale,y1*scale); return; }
    if (fabsf(x0-x1)<1e-6f && fabsf(y0-y1)<1e-6f) return;
    rx = fabsf(rx); ry = fabsf(ry);
    static const float PI = 3.14159265358979f;
    float phi = xRotDeg * PI / 180.f;
    float cp = cosf(phi), sp = sinf(phi);
    float dx = (x0-x1)*0.5f, dy = (y0-y1)*0.5f;
    float x1p =  cp*dx + sp*dy;
    float y1p = -sp*dx + cp*dy;
    float rx2=rx*rx, ry2=ry*ry, x1p2=x1p*x1p, y1p2=y1p*y1p;
    float lam = x1p2/rx2 + y1p2/ry2;
    if (lam > 1.f) { float s=sqrtf(lam); rx*=s; ry*=s; rx2=rx*rx; ry2=ry*ry; }
    float num = rx2*ry2 - rx2*y1p2 - ry2*x1p2;
    float den = rx2*y1p2 + ry2*x1p2;
    float sq  = (num>0.f && den>0.f) ? sqrtf(num/den) : 0.f;
    if (largeArc==sweep) sq = -sq;
    float cxp = sq * rx/ry * y1p;
    float cyp = -sq * ry/rx * x1p;
    float cx = cp*cxp - sp*cyp + (x0+x1)*0.5f;
    float cy = sp*cxp + cp*cyp + (y0+y1)*0.5f;
    auto vAngle = [](float ux,float uy,float vx,float vy)->float {
        float n = sqrtf((ux*ux+uy*uy)*(vx*vx+vy*vy));
        float a = (n>1e-10f) ? acosf(fmaxf(-1.f,fminf(1.f,(ux*vx+uy*vy)/n))) : 0.f;
        return (ux*vy-uy*vx < 0.f) ? -a : a;
    };
    float ux=(x1p-cxp)/rx, uy=(y1p-cyp)/ry;
    float theta = vAngle(1,0,ux,uy);
    float dth   = vAngle(ux,uy,(-x1p-cxp)/rx,(-y1p-cyp)/ry);
    if (!sweep && dth>0) dth -= 2.f*PI;
    if ( sweep && dth<0) dth += 2.f*PI;
    int nSeg = (int)ceilf(fabsf(dth)/(PI*0.5f));
    if (nSeg < 1) nSeg = 1;
    float dt = dth / nSeg;
    float ct = cosf(theta), st_ = sinf(theta);
    float ex  = cp*rx*ct  - sp*ry*st_  + cx;
    float ey  = sp*rx*ct  + cp*ry*st_  + cy;
    float edx = -rx*st_*cp - ry*ct*sp;
    float edy = -rx*st_*sp + ry*ct*cp;
    for (int i = 0; i < nSeg; i++) {
        float t2   = theta + (i+1)*dt;
        float ct2  = cosf(t2), st2 = sinf(t2);
        float ex2  = cp*rx*ct2  - sp*ry*st2  + cx;
        float ey2  = sp*rx*ct2  + cp*ry*st2  + cy;
        float edx2 = -rx*st2*cp - ry*ct2*sp;
        float edy2 = -rx*st2*sp + ry*ct2*cp;
        float ta   = tanf(dt*0.5f);
        float alp  = sinf(dt)*(sqrtf(4.f+3.f*ta*ta)-1.f)/3.f;
        gp.AddBezier(ex*scale, ey*scale,
                     (ex +alp*edx )*scale, (ey +alp*edy )*scale,
                     (ex2-alp*edx2)*scale, (ey2-alp*edy2)*scale,
                     ex2*scale, ey2*scale);
        ex=ex2; ey=ey2; edx=edx2; edy=edy2;
    }
}

// Minimal SVG 'd' path → GDI+ GraphicsPath.  Handles: M m L l H h V v C c Z z
static void AddSvgPath(GraphicsPath& gp, const char* d, float scale)
{
    float cx = 0.f, cy = 0.f, mx = 0.f, my = 0.f;
    const char* p = d;
    char cmd = 0;
    auto skip = [&]() { while (*p == ' ' || *p == ',' || *p == '\t') ++p; };
    auto num  = [&](float& v) -> bool {
        skip();
        if (!*p) return false;
        char* e; v = strtof(p, &e);
        if (e == p) return false;
        p = e; return true;
    };
    while (*p) {
        skip();
        if (!*p) break;
        if ((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')) cmd = *p++;
        if (!cmd) { ++p; continue; }
        bool r = (cmd >= 'a');
        float x=0,y=0,x1,y1,x2,y2;
        switch (cmd | 32) {
        case 'm': if (!num(x)||!num(y)){p+=strlen(p);break;} if(r){x+=cx;y+=cy;}
                  gp.StartFigure(); cx=mx=x; cy=my=y; cmd=r?'l':'L'; break;
        case 'l': if (!num(x)||!num(y)){cmd=0;break;} if(r){x+=cx;y+=cy;}
                  gp.AddLine(cx*scale,cy*scale,x*scale,y*scale); cx=x;cy=y; break;
        case 'h': if (!num(x)){cmd=0;break;} if(r)x+=cx;
                  gp.AddLine(cx*scale,cy*scale,x*scale,cy*scale); cx=x; break;
        case 'v': if (!num(y)){cmd=0;break;} if(r)y+=cy;
                  gp.AddLine(cx*scale,cy*scale,cx*scale,y*scale); cy=y; break;
        case 'c': if (!num(x1)||!num(y1)||!num(x2)||!num(y2)||!num(x)||!num(y)){cmd=0;break;}
                  if(r){x1+=cx;y1+=cy;x2+=cx;y2+=cy;x+=cx;y+=cy;}
                  gp.AddBezier(cx*scale,cy*scale,x1*scale,y1*scale,x2*scale,y2*scale,x*scale,y*scale);
                  cx=x;cy=y; break;
        case 'z': gp.CloseFigure(); cx=mx;cy=my; cmd=0; break;
        case 'a': { float rx_,ry_,rot_,la_,sw_,ex_,ey_;
                    if(!num(rx_)||!num(ry_)||!num(rot_)||!num(la_)||!num(sw_)||!num(ex_)||!num(ey_)){cmd=0;break;}
                    if(r){ex_+=cx;ey_+=cy;}
                    AddSvgArc(gp,cx,cy,rx_,ry_,rot_,la_>=0.5f,sw_>=0.5f,ex_,ey_,scale);
                    cx=ex_;cy=ey_; break; }
        default:  cmd=0; ++p; break;
        }
    }
}

// Build an HICON (sz×sz, premultiplied alpha) from an SVG 'd' path string.
static HICON MakeHIconFromSvg(const char* d, int sz, Color fillColor)
{
    Bitmap bmp(sz, sz, PixelFormat32bppARGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(Color(0, 0, 0, 0));
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, (float)sz / 20.0f);
        SolidBrush br(fillColor);
        g.FillPath(&br, &path);
    }
    BITMAPV5HEADER bmi = {};
    bmi.bV5Size = sizeof(bmi); bmi.bV5Width = sz; bmi.bV5Height = -sz;
    bmi.bV5Planes = 1; bmi.bV5BitCount = 32; bmi.bV5Compression = BI_BITFIELDS;
    bmi.bV5RedMask = 0x00FF0000; bmi.bV5GreenMask = 0x0000FF00;
    bmi.bV5BlueMask = 0x000000FF; bmi.bV5AlphaMask = 0xFF000000;
    void* pv = nullptr;
    HDC hdc = GetDC(NULL);
    HBITMAP hbmC = CreateDIBSection(hdc, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &pv, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (hbmC && pv) {
        BitmapData bd; Rect rc(0, 0, sz, sz);
        bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd);
        BYTE* src = (BYTE*)bd.Scan0; BYTE* dst = (BYTE*)pv;
        for (int i = 0; i < sz*sz*4; i += 4) {
            BYTE a = src[i+3];
            dst[i+0]=(BYTE)((int)src[i+0]*a/255);
            dst[i+1]=(BYTE)((int)src[i+1]*a/255);
            dst[i+2]=(BYTE)((int)src[i+2]*a/255);
            dst[i+3]=a;
        }
        bmp.UnlockBits(&bd);
    }
    // All-zero mask = let alpha channel handle transparency
    HBITMAP hbmM = CreateBitmap(sz, sz, 1, 1, NULL);
    HDC hdcM = CreateCompatibleDC(NULL);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcM, hbmM);
    RECT mr = {0,0,sz,sz}; FillRect(hdcM, &mr, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SelectObject(hdcM, hOld); DeleteDC(hdcM);
    ICONINFO ii = {TRUE, 0, 0, hbmM, hbmC};
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hbmM); if (hbmC) DeleteObject(hbmC);
    return hIcon;
}

// Creates an HICON with a stroked (outline-only) square or circle on a transparent background.
static HICON MakeStrokeIcon(bool round, int sz, Color strokeColor)
{
    Bitmap bmp(sz, sz, PixelFormat32bppARGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(Color(0, 0, 0, 0));
        Pen pen(strokeColor, 1.5f);
        float m = 2.0f;
        if (round)
            g.DrawEllipse(&pen, m, m, (REAL)sz - m * 2.0f, (REAL)sz - m * 2.0f);
        else
            g.DrawRectangle(&pen, m, m, (REAL)sz - m * 2.0f, (REAL)sz - m * 2.0f);
    }
    BITMAPV5HEADER bmi = {};
    bmi.bV5Size = sizeof(bmi); bmi.bV5Width = sz; bmi.bV5Height = -sz;
    bmi.bV5Planes = 1; bmi.bV5BitCount = 32; bmi.bV5Compression = BI_BITFIELDS;
    bmi.bV5RedMask = 0x00FF0000; bmi.bV5GreenMask = 0x0000FF00;
    bmi.bV5BlueMask = 0x000000FF; bmi.bV5AlphaMask = 0xFF000000;
    void* pv = nullptr;
    HDC hdc = GetDC(NULL);
    HBITMAP hbmC = CreateDIBSection(hdc, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &pv, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (hbmC && pv) {
        BitmapData bd; Rect rc(0, 0, sz, sz);
        bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd);
        BYTE* src = (BYTE*)bd.Scan0; BYTE* dst = (BYTE*)pv;
        for (int i = 0; i < sz * sz * 4; i += 4) {
            BYTE a = src[i + 3];
            dst[i + 0] = (BYTE)((int)src[i + 0] * a / 255);
            dst[i + 1] = (BYTE)((int)src[i + 1] * a / 255);
            dst[i + 2] = (BYTE)((int)src[i + 2] * a / 255);
            dst[i + 3] = a;
        }
        bmp.UnlockBits(&bd);
    }
    HBITMAP hbmM = CreateBitmap(sz, sz, 1, 1, NULL);
    HDC hdcM = CreateCompatibleDC(NULL);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcM, hbmM);
    RECT mr = { 0, 0, sz, sz }; FillRect(hdcM, &mr, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SelectObject(hdcM, hOld); DeleteDC(hdcM);
    ICONINFO ii = { TRUE, 0, 0, hbmM, hbmC };
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hbmM); if (hbmC) DeleteObject(hbmC);
    return hIcon;
}

// Same as MakeHIconFromSvg but produces an HCURSOR with an explicit hotspot.
// hotX/hotY are in pixels within the sz×sz bitmap.
static HCURSOR MakeHCursorFromSvg(const char* d, int sz, Color fillColor,
                                   int hotX, int hotY)
{
    Bitmap bmp(sz, sz, PixelFormat32bppARGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(Color(0, 0, 0, 0));
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, (float)sz / 20.0f);
        SolidBrush br(fillColor);
        g.FillPath(&br, &path);
    }
    BITMAPV5HEADER bmi = {};
    bmi.bV5Size = sizeof(bmi); bmi.bV5Width = sz; bmi.bV5Height = -sz;
    bmi.bV5Planes = 1; bmi.bV5BitCount = 32; bmi.bV5Compression = BI_BITFIELDS;
    bmi.bV5RedMask = 0x00FF0000; bmi.bV5GreenMask = 0x0000FF00;
    bmi.bV5BlueMask = 0x000000FF; bmi.bV5AlphaMask = 0xFF000000;
    void* pv = nullptr;
    HDC hdc = GetDC(NULL);
    HBITMAP hbmC = CreateDIBSection(hdc, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, &pv, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (hbmC && pv) {
        BitmapData bd; Rect rc(0, 0, sz, sz);
        bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd);
        BYTE* src = (BYTE*)bd.Scan0; BYTE* dst = (BYTE*)pv;
        for (int i = 0; i < sz*sz*4; i += 4) {
            BYTE a = src[i+3];
            dst[i+0]=(BYTE)((int)src[i+0]*a/255);
            dst[i+1]=(BYTE)((int)src[i+1]*a/255);
            dst[i+2]=(BYTE)((int)src[i+2]*a/255);
            dst[i+3]=a;
        }
        bmp.UnlockBits(&bd);
    }
    HBITMAP hbmM = CreateBitmap(sz, sz, 1, 1, NULL);
    HDC hdcM = CreateCompatibleDC(NULL);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcM, hbmM);
    RECT mr = {0,0,sz,sz}; FillRect(hdcM, &mr, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SelectObject(hdcM, hOld); DeleteDC(hdcM);
    ICONINFO ii = {};
    ii.fIcon    = FALSE;   // cursor, not icon
    ii.xHotspot = (DWORD)hotX;
    ii.yHotspot = (DWORD)hotY;
    ii.hbmMask  = hbmM;
    ii.hbmColor = hbmC;
    HCURSOR hCur = (HCURSOR)CreateIconIndirect(&ii);
    DeleteObject(hbmM); if (hbmC) DeleteObject(hbmC);
    return hCur;
}

// Creates an opaque HBITMAP for use as a menu item icon via MIIM_BITMAP.
// Renders the SVG path filled with fillColor on top of bgColor.
// MIIM_BITMAP uses BitBlt (not AlphaBlend), so the bitmap must be fully opaque —
// transparent background areas must carry the actual menu background colour.
// coordSz: SVG coordinate space (20 for 20px Fluent icons, 16 for 16px ones).
static HBITMAP MakeBitmapFromSvg(const char* d, int sz, Color fillColor,
                                  float coordSz = 20.0f,
                                  Color bgColor = Color(255, 255, 255, 255))
{
    Bitmap bmp(sz, sz, PixelFormat32bppRGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(bgColor);
        float scale = (float)sz / coordSz;
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, scale);
        SolidBrush br(fillColor);
        g.FillPath(&br, &path);
    }
    // Copy GDI+ bitmap into a compatible HBITMAP
    HDC hdc = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, sz, sz);
    ReleaseDC(NULL, hdc);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
    Graphics gDC(hdcMem);
    gDC.DrawImage(&bmp, 0, 0, sz, sz);
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    return hbm;
}

static const char* kDropperSvgD = "M17.2465 2.75409C16.2228 1.7304 14.563 1.73041 13.5393 2.75411L12.5 3.7935L12.1489 3.44243C11.5631 2.85663 10.6134 2.85663 10.0276 3.44242L9.44178 4.0282C8.856 4.61399 8.85599 5.56373 9.44178 6.14952L9.79287 6.50061L3.43946 12.854C3.15816 13.1353 3.00012 13.5169 3.00012 13.9147V14.4162L2.03559 16.6745C1.67987 17.5073 2.52597 18.3463 3.35577 17.9835L5.60452 17.0005H6.08591C6.48373 17.0005 6.86526 16.8424 7.14657 16.5611L13.5 10.2077L13.8533 10.5611C14.4391 11.1468 15.3888 11.1468 15.9746 10.5611L16.5604 9.97528C17.1462 9.38949 17.1462 8.43974 16.5604 7.85396L16.2071 7.50061L17.2465 6.46119C18.2701 5.4375 18.2701 3.77778 17.2465 2.75409ZM14.2464 3.46121C14.8796 2.82804 15.9062 2.82803 16.5394 3.4612C17.1725 4.09436 17.1725 5.12092 16.5394 5.75409L15.1464 7.14706C14.9511 7.34232 14.9511 7.6589 15.1464 7.85416L15.8533 8.56106C16.0486 8.75633 16.0486 9.07291 15.8533 9.26817L15.2675 9.85396C15.0723 10.0492 14.7557 10.0492 14.5604 9.85396L10.1489 5.44241C9.95363 5.24715 9.95363 4.93057 10.1489 4.73531L10.7347 4.14952C10.9299 3.95426 11.2465 3.95426 11.4418 4.14953L12.1464 4.85416C12.2402 4.94793 12.3674 5.00061 12.5 5.00061C12.6326 5.00061 12.7598 4.94793 12.8535 4.85416L14.2464 3.46121ZM12.7929 9.50062L6.43946 15.854C6.34569 15.9478 6.21852 16.0005 6.08591 16.0005H5.50001C5.43108 16.0005 5.36289 16.0147 5.29973 16.0423L2.95522 17.0673L3.95994 14.7149C3.98645 14.6528 4.00012 14.586 4.00012 14.5185V13.9147C4.00012 13.7821 4.0528 13.6549 4.14657 13.5611L10.5 7.20772L12.7929 9.50062Z";

static HIMAGELIST BuildToolImageList(bool dark)
{
    // Drawn with GDI+ for anti-aliased, smooth icons
    const int SZ = 22;
    HIMAGELIST himl = ImageList_Create(SZ, SZ, ILC_COLOR32, 24, 4);
    if (!himl) return NULL;

    Color cDark = dark ? Color(255, 200, 200, 200) : Color(255, 45, 45, 45);
    Color cYell (255, 255, 195,   0);
    Color cPink (255, 255, 105, 105);
    Color cGray (255, 175, 175, 175);
    Color cBrown(255, 130,  65,  10);
    Color cBlue (255,   0, 120, 215);
    Color cWhite(255, 255, 255, 255);
    // Opaque background for anti-aliasing: GDI+ blends stroke edges toward this colour,
    // so icons rendered against the actual toolbar background look correct when composited.
    Color cBg = dark ? Color(255, 45, 45, 48) : Color(255, 240, 240, 240);

    const float SW = 1.5f;  // standard stroke width

    // Render one icon via a draw-lambda, copy to premultiplied-alpha DIB for ILC_COLOR32
    auto addIcon = [&](auto drawFn) {
        Bitmap bmp(SZ, SZ, PixelFormat32bppARGB);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);
            g.Clear(cBg);
            drawFn(g);
        }
        BitmapData bd; Rect rc(0, 0, SZ, SZ);
        bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = SZ;  bi.bmiHeader.biHeight = -SZ;
        bi.bmiHeader.biPlanes = 1;  bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        BYTE* pv = nullptr;
        HBITMAP hbm = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&pv, NULL, 0);
        if (hbm && pv) {
            memcpy(pv, bd.Scan0, SZ * SZ * 4);
            // All pixels are fully opaque (rendered on opaque background) — set A=255
            // so ILC_COLOR32 composites them correctly without any premultiplication needed.
            for (int i = 3; i < SZ * SZ * 4; i += 4)
                pv[i] = 255;
            ImageList_Add(himl, hbm, NULL);
            DeleteObject(hbm);
        }
        bmp.UnlockBits(&bd);
    };

    // ── 0: Pencil (Fluent ic_fluent_edit_20_regular) ──
    addIcon([&](Graphics& g) {
        const char* d = kPencilSvgD;
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 1: Brush (Fluent ic_fluent_paint_brush_arrow_down_20_regular) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M8.99916 4V7.5C8.99916 7.77614 8.77531 8 8.49916 8H6.99916C6.44688 8 5.99916"
            " 8.44772 5.99916 9V10H13.9992V9C13.9992 8.44772 13.5514 8 12.9992 8H11.4992"
            "C11.223 8 10.9992 7.77614 10.9992 7.5V4C10.9992 3.44772 10.5515 3 9.99917 3"
            "C9.44688 3 8.99916 3.44772 8.99916 4Z"
            "M14.9992 9V10.4708C14.9997 10.4805 15 10.4902 15 10.5C15 10.5098 14.9997 10.5195"
            " 14.9992 10.5292V12.0851C14.843 12.03 14.675 12 14.5 12C14.3244 12 14.1558 12.0302"
            " 13.9992 12.0856V11H5.99916V11.5C5.99916 13.3184 5.60958 14.8756 5.22113 15.9773"
            "C5.07867 16.3813 4.93593 16.7254 4.81172 17H6.18483L6.20227 16.9623C6.29349 16.7647"
            " 6.41482 16.4917 6.53576 16.1893C6.65712 15.8859 6.77556 15.5593 6.86299 15.2533"
            "C6.95259 14.9396 7 14.6801 7 14.5C7 14.2239 7.22386 14 7.5 14C7.77614 14 8 14.2239"
            " 8 14.5C8 14.8199 7.92241 15.1854 7.82451 15.528C7.72444 15.8782 7.59288 16.2391"
            " 7.46424 16.5607C7.40184 16.7167 7.33954 16.8649 7.28094 17H8.51802C8.57184 16.8848"
            " 8.63621 16.7409 8.70033 16.5835C8.86788 16.1719 8.99674 15.7542 8.99674 15.5"
            "C8.99674 15.2239 9.2206 15 9.49674 15C9.77289 15 9.99675 15.2239 9.99675 15.5"
            "C9.99675 15.9712 9.79228 16.5534 9.6265 16.9606L9.61033 17H11.0854C11.1576 17.2047"
            " 11.2756 17.3969 11.4393 17.5607L11.8787 18H8.83974C8.83303 18.0001 8.82631 18.0001"
            " 8.81958 18H6.51009C6.50312 18.0001 6.49613 18.0001 6.48914 18H4C3.81995 18"
            " 3.6538 17.9032 3.56502 17.7466C3.47634 17.5901 3.47852 17.3981 3.57068 17.2437"
            "L3.57283 17.2401L3.58284 17.2227C3.59213 17.2065 3.60652 17.181 3.62528 17.1465"
            "C3.66281 17.0776 3.71781 16.973 3.78449 16.8357C3.91792 16.561 4.09772 16.1562"
            " 4.27803 15.6448C4.63917 14.6205 4.99916 13.1777 4.99916 11.5V9C4.99916 7.89543"
            " 5.89459 7 6.99916 7H7.99916V4C7.99916 2.89543 8.8946 2 9.99917 2C11.1037 2"
            " 11.9992 2.89543 11.9992 4V7H12.9992C14.1037 7 14.9992 7.89543 14.9992 9Z"
            "M15 13.5C15 13.2239 14.7761 13 14.5 13C14.2239 13 14 13.2239 14 13.5V17.2929"
            "L12.8536 16.1464C12.6583 15.9512 12.3417 15.9512 12.1464 16.1464C11.9512 16.3417"
            " 11.9512 16.6583 12.1464 16.8536L14.1464 18.8536C14.3417 19.0488 14.6583 19.0488"
            " 14.8536 18.8536L16.8536 16.8536C17.0488 16.6583 17.0488 16.3417 16.8536 16.1464"
            "C16.6583 15.9512 16.3417 15.9512 16.1464 16.1464L15 17.2929V13.5Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 2: Eraser (Fluent ic_fluent_eraser_20_regular) ──
    addIcon([&](Graphics& g) {
        static const char* d = "M11.197 2.43934C11.7828 1.85355 12.7326 1.85355 13.3184 2.43934L17.561 6.68198C18.1468 7.26777 18.1468 8.21751 17.561 8.8033L9.36408 17.0002H14.5001C14.7763 17.0002 15.0001 17.2241 15.0001 17.5002C15.0001 17.7764 14.7763 18.0002 14.5001 18.0002H7.81974C7.40996 18.0217 6.99303 17.876 6.68003 17.5629L2.43739 13.3203C1.8516 12.7345 1.8516 11.7848 2.43739 11.199L11.197 2.43934ZM12.6112 3.14645C12.416 2.95118 12.0994 2.95118 11.9041 3.14645L5.53846 9.51212L10.4882 14.4619L16.8539 8.09619C17.0491 7.90093 17.0491 7.58435 16.8539 7.38909L12.6112 3.14645ZM9.7811 15.169L4.83135 10.2192L3.14449 11.9061C2.94923 12.1014 2.94923 12.4179 3.14449 12.6132L7.38713 16.8558C7.5824 17.0511 7.89898 17.0511 8.09424 16.8558L9.7811 15.169Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 3: Fill (Fluent ic_fluent_paint_bucket_20_regular) ──
    addIcon([&](Graphics& g) {
        static const char* d = "M8.99997 2.5C8.99997 2.22386 8.77611 2 8.49997 2C8.22383 2 7.99997 2.22386 7.99997 2.5V3.9996C7.79524 4.0718 7.60306 4.18978 7.43931 4.35352L2.35352 9.43931C1.76774 10.0251 1.76774 10.9748 2.35352 11.5606L6.68931 15.8964C7.2751 16.4822 8.22484 16.4822 8.81063 15.8964L13.8964 10.8106C14.4822 10.2248 14.4822 9.2751 13.8964 8.68931L9.56063 4.35352C9.39688 4.18978 9.2047 4.0718 8.99997 3.9996V2.5ZM7.99997 5.20708V6.5C7.99997 6.77614 8.22383 7 8.49997 7C8.77611 7 8.99997 6.77614 8.99997 6.5V5.20708L13.1893 9.39642C13.3529 9.56002 13.3794 9.80878 13.2689 10H3.20705L7.99997 5.20708ZM7.39642 15.1893L3.20711 11H12.2928L8.10352 15.1893C7.90826 15.3846 7.59168 15.3846 7.39642 15.1893ZM16.016 11.3922C15.7855 10.997 15.2145 10.997 14.984 11.3922L13.3316 14.2248C12.3554 15.8983 13.5626 18 15.5 18C17.4374 18 18.6445 15.8983 17.6683 14.2248L16.016 11.3922ZM14.1954 14.7287L15.5 12.4923L16.8045 14.7287C17.3919 15.7355 16.6656 17 15.5 17C14.3343 17 13.6081 15.7355 14.1954 14.7287Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 4: Line ──
    addIcon([&](Graphics& g) {
        Pen lp(cDark, 2.f); lp.SetStartCap(LineCapRound); lp.SetEndCap(LineCapRound);
        g.DrawLine(&lp, PointF(2,20), PointF(20,2));
    });

    // ── 5: Rectangle ──
    addIcon([&](Graphics& g) {
        Pen rp(cDark, SW); rp.SetLineJoin(LineJoinRound);
        g.DrawRectangle(&rp, 2.f, 3.f, 17.f, 15.f);
    });

    // ── 6: Ellipse ──
    addIcon([&](Graphics& g) {
        Pen ep(cDark, 2.0f);
        g.DrawEllipse(&ep, 2.f, 3.f, 17.f, 15.f);
    });

    // ── 7: Text ──
    addIcon([&](Graphics& g) {
        FontFamily ff(L"Segoe UI");
        Font fnt(&ff, 15.f, FontStyleBold, UnitPixel);
        SolidBrush db(cDark);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF rc(0, 0, (REAL)SZ, (REAL)SZ);
        g.DrawString(L"A", -1, &fnt, rc, &sf, &db);
    });

    // ── 8: Select ──
    addIcon([&](Graphics& g) {
        Pen dp(cDark, 1.2f);
        REAL dashes[] = {3.f, 2.f}; dp.SetDashPattern(dashes, 2);
        g.DrawRectangle(&dp, 2.f, 3.f, 17.f, 15.f);
        SolidBrush hb(cDark);
        float hx[] = {2.f, 10.5f, 19.f}, hy[] = {3.f, 10.5f, 18.f};
        for (int xi = 0; xi < 3; xi++)
            for (int yi = 0; yi < 3; yi++)
                if (xi != 1 || yi != 1)
                    g.FillRectangle(&hb, hx[xi]-1.5f, hy[yi]-1.5f, 3.f, 3.f);
    });

    // ── 9: Rounded Rectangle ──
    addIcon([&](Graphics& g) {
        Pen rp(cDark, SW); rp.SetLineJoin(LineJoinRound);
        GraphicsPath path;
        path.AddArc(3.f, 4.f, 6.f, 6.f, 180.f, 90.f);
        path.AddArc(13.f, 4.f, 6.f, 6.f, 270.f, 90.f);
        path.AddArc(13.f, 12.f, 6.f, 6.f, 0.f, 90.f);
        path.AddArc(3.f, 12.f, 6.f, 6.f, 90.f, 90.f);
        path.CloseFigure();
        g.DrawPath(&rp, &path);
    });

    // ── 10: Triangle ──
    addIcon([&](Graphics& g) {
        PointF tri[] = {PointF(11,4), PointF(20,18), PointF(2,18)};
        Pen tp(cDark, SW); g.DrawPolygon(&tp, tri, 3);
    });

    // ── 11: Left Arrow ──
    addIcon([&](Graphics& g) {
        PointF pts[] = {PointF(20,8),PointF(10,8),PointF(10,4),PointF(2,11),PointF(10,18),PointF(10,14),PointF(20,14)};
        Pen p(cDark, SW); g.DrawPolygon(&p, pts, 7);
    });

    // ── 12: Right Arrow ──
    addIcon([&](Graphics& g) {
        PointF pts[] = {PointF(2,8),PointF(12,8),PointF(12,4),PointF(20,11),PointF(12,18),PointF(12,14),PointF(2,14)};
        Pen p(cDark, SW); g.DrawPolygon(&p, pts, 7);
    });

    // ── 13: Up Arrow ──
    addIcon([&](Graphics& g) {
        PointF pts[] = {PointF(8,18),PointF(8,10),PointF(4,10),PointF(11,2),PointF(18,10),PointF(14,10),PointF(14,18)};
        Pen p(cDark, SW); g.DrawPolygon(&p, pts, 7);
    });

    // ── 14: Down Arrow ──
    addIcon([&](Graphics& g) {
        PointF pts[] = {PointF(8,4),PointF(8,12),PointF(4,12),PointF(11,20),PointF(18,12),PointF(14,12),PointF(14,4)};
        Pen p(cDark, SW); g.DrawPolygon(&p, pts, 7);
    });

    // ── 15: Arrow/bezier (Fluent ic_fluent_arrow_curve_up_right_20_regular) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M14.1464 6.85355C14.3417 7.04882 14.6583 7.04882 14.8536 6.85355C15.0488 6.65829 15.0488 6.34171"
            " 14.8536 6.14645L10.8536 2.14645C10.6583 1.95118 10.3417 1.95118 10.1464 2.14645L6.14645 6.14645"
            "C5.95118 6.34171 5.95118 6.65829 6.14645 6.85355C6.34171 7.04882 6.65829 7.04882 6.85355 6.85355"
            "L10 3.70711L10 9.99999C10 11.9648 9.75319 13.3808 9.23563 14.4734C8.72437 15.5528 7.92808 16.36"
            " 6.74275 17.0712C6.50596 17.2133 6.42918 17.5204 6.57125 17.7572C6.71333 17.994 7.02046 18.0708"
            " 7.25725 17.9287C8.57192 17.1399 9.52563 16.1972 10.1394 14.9015C10.7468 13.6192 11 12.0351 11"
            " 9.99999L11 3.7071L14.1464 6.85355Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 16: Stickman ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
        g.DrawEllipse(&p, 8.f, 1.f, 6.f, 6.f);           // head
        g.DrawLine(&p, PointF(11,7),  PointF(11,15));     // body
        g.DrawLine(&p, PointF(4,11),  PointF(18,11));     // arms
        g.DrawLine(&p, PointF(11,15), PointF(5,21));      // left leg
        g.DrawLine(&p, PointF(11,15), PointF(17,21));     // right leg
    });

    // ── 17: Speech bubble ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW);
        GraphicsPath path;
        path.AddArc(2.f,  2.f, 5.f, 5.f, 180.f, 90.f);
        path.AddArc(15.f, 2.f, 5.f, 5.f, 270.f, 90.f);
        path.AddArc(15.f, 11.f, 5.f, 5.f, 0.f, 90.f);
        path.AddLine(PointF(14.f, 16.f), PointF(9.f,16.f));
        path.AddLine(PointF(9.f,  16.f), PointF(5.f,21.f));
        path.AddLine(PointF(5.f,  21.f), PointF(7.f,16.f));
        path.AddArc(2.f,  11.f, 5.f, 5.f, 90.f, 90.f);
        path.CloseFigure();
        g.DrawPath(&p, &path);
    });

    // ── 18: Computer monitor ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW);
        g.DrawRectangle(&p, 2.f, 2.f, 18.f, 13.f);       // monitor
        g.DrawRectangle(&p, 4.f, 4.f, 14.f, 9.f);        // screen
        g.DrawLine(&p, PointF(11,15), PointF(11,19));     // stand
        g.DrawLine(&p, PointF(7,19),  PointF(15,19));     // base
    });

    // ── 19: Star ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW);
        PointF pts[10];
        for (int i = 0; i < 10; i++) {
            float ang = (i * 36 - 90) * 3.14159265f / 180.f;
            float r = (i % 2 == 0) ? 10.f : 4.f;
            pts[i] = PointF(11 + r * cosf(ang), 11 + r * sinf(ang));
        }
        g.DrawPolygon(&p, pts, 10);
    });

    // ── 20: Heart ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW);
        GraphicsPath path;
        AddSvgPath(path, kHeartD, 1.0f);
        Matrix mat(18.f / kHeartVW, 0, 0, 18.f / kHeartVH, 2.f, 2.f);
        path.Transform(&mat);
        g.DrawPath(&p, &path);
    });

    // ── 21: Lightning bolt ──
    addIcon([&](Graphics& g) {
        PointF pts[] = {
            PointF(13,1), PointF(6,12), PointF(11,12),
            PointF(9,21), PointF(16,10), PointF(11,10)
        };
        Pen p(cDark, SW); g.DrawPolygon(&p, pts, 6);
    });

    // ── 22: Mustache & glasses ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, 1.5f);
        // Left lens
        g.DrawEllipse(&p, 2.f, 5.f, 7.f, 6.f);
        // Right lens
        g.DrawEllipse(&p, 13.f, 5.f, 7.f, 6.f);
        // Bridge
        g.DrawLine(&p, PointF(9.f, 8.f), PointF(13.f, 8.f));
        // Left side arm stub
        g.DrawLine(&p, PointF(2.f, 8.f), PointF(0.f, 7.f));
        // Right side arm stub
        g.DrawLine(&p, PointF(20.f, 8.f), PointF(22.f, 7.f));
        // Mustache: left curl (arc sweeping down-left from center)
        g.DrawArc(&p, 0.f, 13.f, 10.f, 7.f, 0.f, 180.f);
        // Mustache: right curl (mirror)
        g.DrawArc(&p, 12.f, 13.f, 10.f, 7.f, 0.f, 180.f);
        // Center dip between curls
        g.DrawArc(&p, 8.f, 14.f, 6.f, 4.f, 0.f, -180.f);
    });

    // ── 23: Dropper (Fluent ic_fluent_eyedropper_20_regular) ──
    addIcon([&](Graphics& g) {
        const char* d = kDropperSvgD;
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 24: File menu — ic_fluent_save_20_regular (floppy disk) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M3 5C3 3.89543 3.89543 3 5 3H13.3787C13.9091 3 14.4178 3.21071 14.7929 3.58579"
            "L16.4142 5.20711C16.7893 5.58218 17 6.09089 17 6.62132V15C17 16.1046 16.1046 17"
            " 15 17H5C3.89543 17 3 16.1046 3 15V5ZM5 4C4.44772 4 4 4.44772 4 5V15C4 15.5523"
            " 4.44772 16 5 16L5 11.5C5 10.6716 5.67157 10 6.5 10H13.5C14.3284 10 15 10.6716"
            " 15 11.5V16C15.5523 16 16 15.5523 16 15V6.62132C16 6.3561 15.8946 6.10175"
            " 15.7071 5.91421L14.0858 4.29289C13.8983 4.10536 13.6439 4 13.3787 4L13 4V6.5"
            "C13 7.32843 12.3284 8 11.5 8L7.5 8C6.67157 8 6 7.32843 6 6.5L6 4H5ZM7 4L7 6.5"
            "C7 6.77614 7.22386 7 7.5 7L11.5 7C11.7761 7 12 6.77614 12 6.5V4L7 4ZM14 16V11.5"
            "C14 11.2239 13.7761 11 13.5 11H6.5C6.22386 11 6 11.2239 6 11.5V16H14Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 25: Edit menu — ic_fluent_document_edit_20_regular ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M11.5 8H16V7.41421C16 7.29283 15.9853 7.17296 15.9568 7.057C15.9525 7.03943"
            " 15.9479 7.02195 15.943 7.00458L15.9417 7C15.8721 6.75786 15.742 6.53488"
            " 15.5607 6.35355L11.6458 2.43867C11.4646 2.25769 11.2418 2.12781 11 2.05832"
            "C10.8665 2.01996 10.7272 2 10.5858 2H6C4.89543 2 4 2.89543 4 4V16C4 17.1046"
            " 4.89543 18 6 18H8.22126L8.20776 17.9744C8.092 17.7508 8.01983 17.5018"
            " 8.00293 17.2392C7.99787 17.1606 7.99776 17.0807 8.00293 17H6C5.44772 17 5"
            " 16.5523 5 16V4C5 3.44772 5.44772 3 6 3H10V6.5C10 7.32843 10.6716 8 11.5 8Z"
            "M11.5 7C11.2239 7 11 6.77614 11 6.5V3.20711L14.7929 7H11.5Z"
            "M14.8092 9.54776C15.5395 8.81741 16.7237 8.81741 17.454 9.54776C18.1843 10.2781"
            " 18.1843 11.4622 17.454 12.1926L12.6246 17.022C12.343 17.3036 11.9902 17.5033"
            " 11.6039 17.5999L10.106 17.9744C9.45456 18.1372 8.86451 17.5472 9.02737"
            " 16.8958L9.40184 15.3979C9.49842 15.0116 9.69818 14.6588 9.97975 14.3772"
            "L14.8092 9.54776Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 26: Image menu — ic_fluent_image_20_regular ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M14 7.5C14 8.32843 13.3284 9 12.5 9C11.6716 9 11 8.32843 11 7.5C11 6.67157"
            " 11.6716 6 12.5 6C13.3284 6 14 6.67157 14 7.5ZM13 7.5C13 7.22386 12.7761 7"
            " 12.5 7C12.2239 7 12 7.22386 12 7.5C12 7.77614 12.2239 8 12.5 8C12.7761 8 13"
            " 7.77614 13 7.5ZM3 6C3 4.34315 4.34315 3 6 3H14C15.6569 3 17 4.34315 17 6V14"
            "C17 15.6569 15.6569 17 14 17H6C4.34315 17 3 15.6569 3 14V6ZM6 4C4.89543 4 4"
            " 4.89543 4 6V14C4 14.3726 4.10191 14.7215 4.27937 15.0201L8.94868 10.432"
            "C9.53227 9.85859 10.4677 9.85859 11.0513 10.432L15.7206 15.0201C15.8981"
            " 14.7215 16 14.3726 16 14V6C16 4.89543 15.1046 4 14 4H6ZM6 16H14C14.3692 16"
            " 14.7149 15.9 15.0118 15.7256L10.3504 11.1453C10.1559 10.9542 9.84409 10.9542"
            " 9.64956 11.1453L4.98824 15.7256C5.28505 15.9 5.63085 16 6 16Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 27: View menu — outline monitor, after ic_fluent_desktop_20_regular (_dev/desktop.svg) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M4.5 3C3.11929 3 2 4.11929 2 5.5V12.5C2 13.8807 3.11929 15 4.5 15H7V16H5.5"
            "C5.22386 16 5 16.2239 5 16.5C5 16.7761 5.22386 17 5.5 17H14.5C14.7761 17 15"
            " 16.7761 15 16.5C15 16.2239 14.7761 16 14.5 16H13V15H15.5C16.8807 15 18"
            " 13.8807 18 12.5V5.5C18 4.11929 16.8807 3 15.5 3H4.5ZM12 15V16H8V15H12Z"
            "M3 5.5C3 4.67157 3.67157 4 4.5 4H15.5C16.3284 4 17 4.67157 17 5.5V12.5"
            "C17 13.3284 16.3284 14 15.5 14H4.5C3.67157 14 3 13.3284 3 12.5V5.5Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 28: Help menu — ic_fluent_question_circle_20_regular ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2"
            " 14.4183 2 10C2 5.58172 5.58172 2 10 2ZM10 3C6.13401 3 3 6.13401 3 10C3"
            " 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3Z"
            "M10 13.5C10.4142 13.5 10.75 13.8358 10.75 14.25C10.75 14.6642 10.4142 15 10"
            " 15C9.58579 15 9.25 14.6642 9.25 14.25C9.25 13.8358 9.58579 13.5 10 13.5Z"
            "M10 5.5C11.3807 5.5 12.5 6.61929 12.5 8C12.5 8.72959 12.1848 9.40774 11.6513"
            " 9.8771L11.4967 10.0024L11.2782 10.1655L11.1906 10.2372C11.1348 10.2851"
            " 11.0835 10.3337 11.0346 10.3859C10.6963 10.7464 10.5 11.2422 10.5 12"
            "C10.5 12.2761 10.2761 12.5 10 12.5C9.72386 12.5 9.5 12.2761 9.5 12"
            "C9.5 10.988 9.79312 10.2475 10.3054 9.70162C10.4165 9.5832 10.532 9.47988"
            " 10.6609 9.37874L10.9076 9.19439L11.0256 9.09468C11.325 8.81435 11.5 8.42206"
            " 11.5 8C11.5 7.17157 10.8284 6.5 10 6.5C9.17157 6.5 8.5 7.17157 8.5 8"
            "C8.5 8.27614 8.27614 8.5 8 8.5C7.72386 8.5 7.5 8.27614 7.5 8"
            "C7.5 6.61929 8.61929 5.5 10 5.5Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 20.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 29: More (circle overlapping square + bottom ▼ indicator) ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW);
        SolidBrush br(cDark);
        g.DrawRectangle(&p, 1.f, 1.f, 10.f, 10.f);   // square, top-left
        g.DrawEllipse(  &p, 8.f, 5.f, 10.f, 10.f);   // circle, overlapping at bottom-right
        // Small ▼ at bottom centre
        PointF tri[] = { {11.f, 21.f}, {7.f, 17.f}, {15.f, 17.f} };
        g.FillPolygon(&br, tri, 3);
    });

    // ── 30: Tools (drawing_tools.svg, 24×24 viewBox, stroked + ▼) ──
    addIcon([&](Graphics& g) {
        Pen    pen(cDark, SW);
        SolidBrush br(cDark);
        // Scale 16/24 ≈ 0.667; centre horizontally (+3 px); leaves y=16-17 gap before ▼
        const float sc = 16.f / 24.f, ox = 3.f;
        static const char* segs[] = {
            // path2 — chisel/tool-head quadrilateral
            "M 11.986562,8.0963046 16.799,3.282 l 4.004,3.995 -4.819,4.817 z",
            // path3 — tool handle cap with curves
            "m 16.799,3.282 1.443417,-1.4462925 "
            "c 1.17476,-1.17709941 2.880536,-1.14047826 4.025889,-0.06361 "
            "1.046959,0.9843578 1.204056,2.8321734 0.02099,4.0167354 L 20.803,7.277 Z",
            // path4 — long tool body
            "m 15.987798,12.093579 5.080956,4.953506 "
            "c 0.643257,0.627122 0.607645,1.042238 0.582337,1.484296 "
            "-0.02871,0.501435 -0.04494,0.542252 0.355009,1.271138 "
            "l 0.956532,1.743218 "
            "c 0.264691,0.482382 0.224567,0.98268 -0.184705,1.335974 "
            "-0.482273,0.41631 -0.902839,0.230354 -1.467215,-0.07667 "
            "l -1.767537,-0.96155 "
            "c -0.497352,-0.270562 -0.495496,-0.229103 -1.105314,-0.216064 "
            "-0.602036,0.01287 -0.96065,-0.163063 -1.286992,-0.482575 "
            "L 11.99,16.092 6.197,10.306 10.2,6.307 l 5.784,5.787",
            // path5 — lower polygon element
            "m 11.99,16.092 -5.5295759,5.439223 "
            "-5.50217423,1.513651 1.53574753,-5.491126 5.505148,-5.47769 z",
            // path6 — left-side tool with curves
            "M 6.197,10.306 5.59375,10.90625 "
            "C 5.2883376,11.210144 4.6344707,11.243846 4.3203125,10.92969 "
            "L 3.640625,10.25 "
            "C 3.280482,9.889857 3.2488946,9.2589179 3.609375,8.8984375 "
            "L 4.1953125,8.3125 1.711818,5.8162913 "
            "C 0.56332695,4.6619207 0.6348432,2.9316655 1.6600413,1.840284 "
            "2.5562629,0.88620518 4.3402053,0.44753076 5.7130631,1.8235485 "
            "L 8.1947573,4.2998811 8.7850413,3.7168544 "
            "C 9.1030749,3.4027308 9.7502084,3.3546817 10.117743,3.722216 "
            "l 0.620883,0.6208835 "
            "c 0.320397,0.3203968 0.455187,0.9704965 0.08649,1.3390388 "
            "L 10.2,6.307 Z"
        };
        for (auto* d : segs) {
            GraphicsPath gp(FillModeWinding);
            AddSvgPath(gp, d, sc);
            Matrix mat(1.f, 0, 0, 1.f, ox, 0.f);
            gp.Transform(&mat);
            g.DrawPath(&pen, &gp);
        }
        // Small ▼ at bottom centre (matches More icon)
        PointF tri[] = { {11.f, 21.f}, {7.f, 17.f}, {15.f, 17.f} };
        g.FillPolygon(&br, tri, 3);
    });

    // ── 31: Magic Wand (from wand.svg, 24×24 viewBox, filled) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "m 9.7545104,7.565 0.136,0.126 10.4799996,10.488"
            " a 2.27,2.27 0 0 1 -3.211,3.208 L 6.6805104,10.9"
            " a 2.251,2.251 0 0 1 10e-4,-3.182 l 0.157,-0.146"
            " a 2.25,2.25 0 0 1 2.916,-0.007 z"
            " m 0.8479996,2.961 -1.0879996,1.088 8.7059996,8.713"
            " a 0.77,0.77 0 1 0 1.089,-1.088 z"
            " m -4.3859996,4.48 0.102,-0.006"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102 v 0.75 h 0.75"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102"
            " c 0,0.38 -0.282,0.694 -0.648,0.743 l -0.102,0.007 h -0.75 v 0.75"
            " c 0,0.38 -0.282,0.694 -0.648,0.743 l -0.102,0.007"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102 V 18 h -0.75"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102"
            " a 0.75,0.75 0 0 1 0.648,-0.743 l 0.102,-0.007 h 0.75 v -0.75"
            " a 0.75,0.75 0 0 1 0.648,-0.743 z"
            " m 1.553,-6.254 -0.027,0.027"
            " a 0.751,0.751 0 0 0 0,1.061 l 0.711,0.713 1.089,-1.089 -0.73,-0.73"
            " a 0.75,0.75 0 0 0 -1.043,0.018 z"
            " M 16.21651,5.007 16.31851,5"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102 V 6.5 h 0.75"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 L 17.81851,8 h -0.75 v 0.75"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 l -0.102,0.007"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 L 15.56851,8.75 V 8 h -0.75"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102"
            " a 0.75,0.75 0 0 1 0.648,-0.743 l 0.102,-0.007 h 0.75 V 5.75"
            " a 0.75,0.75 0 0 1 0.648,-0.743 z"
            " m -11.9999996,-2 0.102,-0.007"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102 V 4.5 h 0.75"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 L 5.8185104,6 h -0.75 v 0.75"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 l -0.102,0.007"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102 V 6 h -0.75"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102"
            " a 0.75,0.75 0 0 1 0.648,-0.743 l 0.102,-0.007 h 0.75 V 3.75"
            " a 0.75,0.75 0 0 1 0.648,-0.743 z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 24.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 32: Lasso (from lasso.svg, 24×24 viewBox, filled) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M9.703 2.265A10.026 10.026 0 0 1 12 2c.79 0 1.559.092 2.297.265a.75.75 0 1 1"
            "-.343 1.46A8.527 8.527 0 0 0 12 3.5c-.673 0-1.327.078-1.954.225a.75.75 0 1 1"
            "-.343-1.46ZM7.773 3.736a.75.75 0 0 1-.242 1.032 8.548 8.548 0 0 0-2.763 2.763"
            ".75.75 0 1 1-1.275-.79 10.048 10.048 0 0 1 3.248-3.248.75.75 0 0 1 1.032.243Z"
            "M16.227 3.736a.75.75 0 0 1 1.032-.243 10.048 10.048 0 0 1 3.248 3.248.75.75 0"
            " 1 1-1.275.79 8.55 8.55 0 0 0-2.763-2.763.75.75 0 0 1-.242-1.032Z"
            "M3.167 9.144a.75.75 0 0 1 .558.902A8.527 8.527 0 0 0 3.5 12c0 .673.078 1.327"
            ".225 1.954a.75.75 0 1 1-1.46.343A10.026 10.026 0 0 1 2 12c0-.79.092-1.559"
            ".265-2.297a.75.75 0 0 1 .902-.559Z"
            "M20.833 9.144a.75.75 0 0 1 .902.559C21.908 10.44 22 11.21 22 12c0 .79-.092"
            " 1.559-.265 2.297a.75.75 0 1 1-1.46-.343c.147-.627.225-1.28.225-1.954"
            " 0-.673-.078-1.327-.226-1.954a.75.75 0 0 1 .559-.902Z"
            "M3.736 16.227a.75.75 0 0 1 1.032.242 8.548 8.548 0 0 0 2.763 2.763.75.75 0 0"
            " 1-.79 1.275 10.048 10.048 0 0 1-3.248-3.248.75.75 0 0 1 .243-1.032Z"
            "M20.42 17.085a.75.75 0 1 0-1.34-.67l-.003.004-.015.029a8.004 8.004 0 0 1"
            "-.358.59 9.58 9.58 0 0 1-.965 1.218c-1.17-1.073-2.756-2.006-4.74-2.006"
            "-2.347 0-3.99 1.203-3.99 2.875S10.653 22 13 22c1.942 0 3.495-.75 4.658-1.645"
            "a11.73 11.73 0 0 1 1.315 2.01c.033.065.058.116.073.149l.017.035.004.009a.75"
            ".75 0 0 0 1.368-.615c-.087-.183 0-.001 0-.001v-.002l-.003-.004-.007-.015a11"
            ".874 11.874 0 0 0-.464-.87 13.199 13.199 0 0 0-1.189-1.703 11.057 11.057 0 0"
            " 0 1.525-2.032l.09-.162.024-.047.007-.014.002-.005.002-.003Z"
            "M13 17.75c1.433 0 2.644.652 3.616 1.512-.95.7-2.155 1.238-3.616 1.238"
            "-1.973 0-2.49-.922-2.49-1.375 0-.453.517-1.375 2.49-1.375Z";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, 22.0f / 24.0f);
        SolidBrush br(cDark); g.FillPath(&br, &path);
    });

    // ── 33: Spiral ──
    addIcon([&](Graphics& g) {
        Pen sp(cDark, SW); sp.SetStartCap(LineCapRound); sp.SetEndCap(LineCapRound);
        const int N = 120;
        const float turns = 2.5f;
        const float PI2 = 6.28318530f;
        float cx = SZ * 0.5f, cy = SZ * 0.5f;
        float radX = SZ * 0.44f, radY = SZ * 0.44f;
        std::vector<PointF> pts(N + 1);
        for (int i = 0; i <= N; ++i) {
            float t = (float)i / (float)N;
            float ang = t * turns * PI2 - PI2 * 0.5f;
            pts[i].X = cx + t * radX * cosf(ang);
            pts[i].Y = cy + t * radY * sinf(ang);
        }
        g.DrawLines(&sp, pts.data(), (int)pts.size());
    });

    // ── 34: Reverse Spiral ──
    addIcon([&](Graphics& g) {
        Pen sp(cDark, SW); sp.SetStartCap(LineCapRound); sp.SetEndCap(LineCapRound);
        const int N = 120;
        const float turns = 2.5f;
        const float PI2 = 6.28318530f;
        float cx = SZ * 0.5f, cy = SZ * 0.5f;
        float radX = SZ * 0.44f, radY = SZ * 0.44f;
        std::vector<PointF> pts(N + 1);
        for (int i = 0; i <= N; ++i) {
            float t = (float)i / (float)N;
            float ang = -(t * turns * PI2) - PI2 * 0.5f;
            pts[i].X = cx + t * radX * cosf(ang);
            pts[i].Y = cy + t * radY * sinf(ang);
        }
        g.DrawLines(&sp, pts.data(), (int)pts.size());
    });

    // ── 35: Diamond ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = { {11,2}, {20,11}, {11,20}, {2,11} };
        g.DrawPolygon(&p, pts, 4);
    });

    // ── 36: Pentagon ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[5];
        const float PI2 = 6.28318530f;
        for (int i = 0; i < 5; ++i) {
            float ang = -PI2*0.25f + i * PI2 / 5.f;
            pts[i] = { 11.f + 9.f*cosf(ang), 11.f + 9.f*sinf(ang) };
        }
        g.DrawPolygon(&p, pts, 5);
    });

    // ── 37: Hexagon (flat top/bottom) ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = { {6,2}, {16,2}, {20,11}, {16,20}, {6,20}, {2,11} };
        g.DrawPolygon(&p, pts, 6);
    });

    // ── 38: Hexagon (flat left/right) ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = { {11,2}, {20,6}, {20,16}, {11,20}, {2,16}, {2,6} };
        g.DrawPolygon(&p, pts, 6);
    });

    // ── 39: Octagon ──
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = {
            {7,2}, {15,2}, {20,7}, {20,15}, {15,20}, {7,20}, {2,15}, {2,7}
        };
        g.DrawPolygon(&p, pts, 8);
    });

    // ── 40: Plus / Cross ──
    // kCrossX/Y on 24×24 viewbox, scaled to icon's 2..20 range (×0.75+2)
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = {
            {9.5f,3.5f},{12.5f,3.5f},{12.5f,9.5f},{18.5f,9.5f},
            {18.5f,12.5f},{12.5f,12.5f},{12.5f,18.5f},{9.5f,18.5f},
            {9.5f,12.5f},{3.5f,12.5f},{3.5f,9.5f},{9.5f,9.5f}
        };
        g.DrawPolygon(&p, pts, 12);
    });

    // ── 41: X Mark ──
    // Rotated-Plus polygon on 24×24 viewbox, scaled to icon's 2..20 range (×0.75+2)
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = {
            {15.3f,4.6f},{17.4f,6.7f},{13.1f,11.0f},{17.4f,15.3f},
            {15.3f,17.4f},{11.0f,13.1f},{6.7f,17.4f},{4.6f,15.3f},
            {8.9f,11.0f},{4.6f,6.7f},{6.7f,4.6f},{11.0f,8.9f}
        };
        g.DrawPolygon(&p, pts, 12);
    });

    // ── 42: Checkmark ──
    // kCkX/Y on 24×24 viewbox, scaled to icon's 2..20 range (×0.75+2)
    addIcon([&](Graphics& g) {
        Pen p(cDark, SW); p.SetLineJoin(LineJoinRound); p.SetLineJoin(LineJoinMiter);
        PointF pts[] = {
            {3.5f,11.0f},{5.4f,8.8f},{8.8f,13.3f},{18.1f,4.6f},{20.0f,6.5f},{8.8f,17.8f}
        };
        g.DrawPolygon(&p, pts, 6);
    });

    // ── 43: Smudge finger (from finger_smudge.svg, 9×9 viewBox, stroked) ──
    addIcon([&](Graphics& g) {
        static const char* d =
            "M 5.4825026,0.52914232 3.880377,2.2929501 2.1900613,2.2635533"
            " C 1.9947559,2.2601567 1.8157329,2.4058668 1.6315222,2.6310132"
            " L 0.8378087,3.6011074"
            " C 0.64079119,3.8419066 0.60059631,4.1186135 0.70552312,4.3654241"
            " 0.77149417,4.5206023 0.94378984,4.6042128 1.1170783,4.6152969"
            " L 1.9842837,3.586409 0.98479267,4.8063761"
            " C 0.89290679,4.9456303 0.92519805,5.500615 1.219967,5.6588834"
            " c 0.1707579,0.091684 0.288581,0.098342 0.4313738,0.0242"
            " C 1.6673342,5.573567 1.679829,5.4631174 1.74618,5.3670295"
            " L 2.7486004,4.2772337 1.7491094,5.3649152"
            " C 1.6327905,5.567609 1.6360457,5.8277846 1.718337,5.9405732"
            " 1.8517687,6.123455 1.9822234,6.2775386 2.172841,6.3382702"
            " L 3.3365363,5.0562488 0.99949107,7.6284687"
            " c -0.18700073,0.2058184 -0.26630861,0.5036128 0,0.6614279"
            " C 1.3752601,8.5125783 1.8317808,8.5242847 2.2929501,8.231103"
            " 2.8310219,7.8890317 3.2621172,7.17582 3.7136509,6.5486588"
            " L 4.4389161,5.5412962"
            " C 4.585877,5.3371738 4.7817581,5.2523968 5.0121536,5.2767248"
            " c 0.1961795,0.020715 0.3992637,0.042519 0.5144442,0.1763808"
            " 0,0 -0.136036,0.2617502 -0.2204762,0.3821586"
            " -0.069876,0.099641 -0.2351744,0.2792695 -0.2351744,0.2792695"
            " -0.05817,0.069077 -0.1964423,0.1101586 -0.3821583,0.1175872"
            " L 3.9317832,6.2602185"
            " C 3.6780436,6.5863481 3.4451317,6.9333054 3.2037667,7.2718096"
            " l 1.9847677,0.047993"
            " C 5.3885549,7.2595792 5.5954784,7.1917858 5.7653386,7.0380109"
            " 6.0598482,6.7713907 6.2472309,6.5980374 6.5088686,6.2912204"
            " 6.7825749,5.970251 6.7334287,5.3901937 6.7465648,4.9680584"
            " V 3.7186946 L 8.6426581,1.9254902";
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, (float)SZ / 9.0f);
        Pen p(cDark, 1.2f); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
        p.SetLineJoin(LineJoinRound);
        g.DrawPath(&p, &path);
    });

    return himl;
}

// ============================================================
//  Rotate handle resource creation
// ============================================================
static void CreateRotateHandleResources()
{
    const int sz = kRotHandSz;
    Matrix flipMat(-1.f, 0.f, 0.f, 1.f, (REAL)sz, 0.f);
    static const char* kArrow = "M 5.2537514,20.250839 9.256,20.256 9.25311,16.250329";
    static const char* kArc   =
        "M 9.256,20.156 C 6.0660873,18.357732 3.7462661,15.690935 3.75,12 "
        "3.7549219,7.1347137 7.7522843,3.7434898 12,3.75 "
        "16.402956,3.75675 20.095705,7.1008655 20.25,12";
    static const char* kDot   =
        "m 12,14 c -1.200617,-0.0045 -2.0166946,-0.980084 -2,-2 "
        "0.01711,-1.045483 0.827048,-2.0092929 2,-2 "
        "1.182603,0.0094 1.994923,0.971052 2,2 "
        "0.0055,1.12495 -0.901694,2.004124 -2,2 z";

    // Build handle icon bitmap (GDI+ Bitmap, owned, used with DrawImage)
    g_pBmpRotHandle = new Bitmap(sz, sz, PixelFormat32bppARGB);
    {
        Graphics g(g_pBmpRotHandle);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.Clear(Color(0,0,0,0));
        GraphicsPath p2(FillModeWinding), p3(FillModeWinding), p4(FillModeWinding);
        AddSvgPath(p2, kArrow, 1.0f); AddSvgPath(p3, kArc, 1.0f); AddSvgPath(p4, kDot, 1.0f);
        p2.Transform(&flipMat); p3.Transform(&flipMat); p4.Transform(&flipMat);
        Pen wp(Color(255,255,255,255), 3.0f);
        wp.SetLineJoin(LineJoinRound); wp.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
        g.DrawPath(&wp, &p2); g.DrawPath(&wp, &p3); g.DrawPath(&wp, &p4);
        Pen bp(Color(255,40,40,40), 1.5f);
        bp.SetLineJoin(LineJoinRound); bp.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
        g.DrawPath(&bp, &p2); g.DrawPath(&bp, &p3); g.DrawPath(&bp, &p4);
    }

    // Build rotate cursor (HCURSOR, hotspot at icon center)
    {
        Bitmap cbmp(sz, sz, PixelFormat32bppARGB);
        {
            Graphics cg(&cbmp);
            cg.SetSmoothingMode(SmoothingModeAntiAlias);
            cg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            cg.Clear(Color(0,0,0,0));
            GraphicsPath cp2(FillModeWinding), cp3(FillModeWinding);
            AddSvgPath(cp2, kArrow, 1.0f); AddSvgPath(cp3, kArc, 1.0f);
            cp2.Transform(&flipMat); cp3.Transform(&flipMat);
            Pen cbp(Color(255,0,0,0), 2.0f);
            cbp.SetLineJoin(LineJoinRound); cbp.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
            cg.DrawPath(&cbp, &cp2); cg.DrawPath(&cbp, &cp3);
        }
        BITMAPV5HEADER bmi = {};
        bmi.bV5Size=sizeof(bmi); bmi.bV5Width=sz; bmi.bV5Height=-sz;
        bmi.bV5Planes=1; bmi.bV5BitCount=32; bmi.bV5Compression=BI_BITFIELDS;
        bmi.bV5RedMask=0x00FF0000; bmi.bV5GreenMask=0x0000FF00;
        bmi.bV5BlueMask=0x000000FF; bmi.bV5AlphaMask=0xFF000000;
        void* pv=nullptr;
        HDC hdc=GetDC(NULL);
        HBITMAP hbmC=CreateDIBSection(hdc,(BITMAPINFO*)&bmi,DIB_RGB_COLORS,&pv,NULL,0);
        ReleaseDC(NULL,hdc);
        if (hbmC && pv) {
            BitmapData bd; Rect rc(0,0,sz,sz);
            cbmp.LockBits(&rc,ImageLockModeRead,PixelFormat32bppARGB,&bd);
            BYTE* s=(BYTE*)bd.Scan0; BYTE* d=(BYTE*)pv;
            for (int i=0; i<sz*sz*4; i+=4) {
                BYTE a=s[i+3];
                d[i]=(BYTE)((int)s[i]*a/255); d[i+1]=(BYTE)((int)s[i+1]*a/255);
                d[i+2]=(BYTE)((int)s[i+2]*a/255); d[i+3]=a;
            }
            cbmp.UnlockBits(&bd);
        }
        HBITMAP hbmM=CreateBitmap(sz,sz,1,1,NULL);
        HDC hdcM=CreateCompatibleDC(NULL);
        HBITMAP hO=(HBITMAP)SelectObject(hdcM,hbmM);
        RECT mr={0,0,sz,sz}; FillRect(hdcM,&mr,(HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(hdcM,hO); DeleteDC(hdcM);
        ICONINFO ii={}; ii.fIcon=FALSE; ii.xHotspot=sz/2; ii.yHotspot=sz/2;
        ii.hbmMask=hbmM; ii.hbmColor=hbmC;
        g_hCurRotate=(HCURSOR)CreateIconIndirect(&ii);
        DeleteObject(hbmM); if (hbmC) DeleteObject(hbmC);
    }

    // Build corner-rotate cursor: white fill with black outline
    {
        Bitmap cbmp(sz, sz, PixelFormat32bppARGB);
        {
            Graphics cg(&cbmp);
            cg.SetSmoothingMode(SmoothingModeAntiAlias);
            cg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            cg.Clear(Color(0,0,0,0));
            GraphicsPath cp2(FillModeWinding), cp3(FillModeWinding);
            AddSvgPath(cp2, kArrow, 1.0f); AddSvgPath(cp3, kArc, 1.0f);
            cp2.Transform(&flipMat); cp3.Transform(&flipMat);
            Pen bp(Color(255,0,0,0), 3.5f);
            bp.SetLineJoin(LineJoinRound); bp.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
            cg.DrawPath(&bp, &cp2); cg.DrawPath(&bp, &cp3);
            Pen wp(Color(255,255,255,255), 2.0f);
            wp.SetLineJoin(LineJoinRound); wp.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
            cg.DrawPath(&wp, &cp2); cg.DrawPath(&wp, &cp3);
        }
        BITMAPV5HEADER bmi = {};
        bmi.bV5Size=sizeof(bmi); bmi.bV5Width=sz; bmi.bV5Height=-sz;
        bmi.bV5Planes=1; bmi.bV5BitCount=32; bmi.bV5Compression=BI_BITFIELDS;
        bmi.bV5RedMask=0x00FF0000; bmi.bV5GreenMask=0x0000FF00;
        bmi.bV5BlueMask=0x000000FF; bmi.bV5AlphaMask=0xFF000000;
        void* pv2=nullptr;
        HDC hdc2=GetDC(NULL);
        HBITMAP hbmC2=CreateDIBSection(hdc2,(BITMAPINFO*)&bmi,DIB_RGB_COLORS,&pv2,NULL,0);
        ReleaseDC(NULL,hdc2);
        if (hbmC2 && pv2) {
            BitmapData bd; Rect rc(0,0,sz,sz);
            cbmp.LockBits(&rc,ImageLockModeRead,PixelFormat32bppARGB,&bd);
            BYTE* s=(BYTE*)bd.Scan0; BYTE* d=(BYTE*)pv2;
            for (int i=0; i<sz*sz*4; i+=4) {
                BYTE a=s[i+3];
                d[i]=(BYTE)((int)s[i]*a/255); d[i+1]=(BYTE)((int)s[i+1]*a/255);
                d[i+2]=(BYTE)((int)s[i+2]*a/255); d[i+3]=a;
            }
            cbmp.UnlockBits(&bd);
        }
        HBITMAP hbmM2=CreateBitmap(sz,sz,1,1,NULL);
        HDC hdcM2=CreateCompatibleDC(NULL);
        HBITMAP hO2=(HBITMAP)SelectObject(hdcM2,hbmM2);
        RECT mr2={0,0,sz,sz}; FillRect(hdcM2,&mr2,(HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(hdcM2,hO2); DeleteDC(hdcM2);
        ICONINFO ii2={}; ii2.fIcon=FALSE; ii2.xHotspot=sz/2; ii2.yHotspot=sz/2;
        ii2.hbmMask=hbmM2; ii2.hbmColor=hbmC2;
        g_hCurCornerRot=(HCURSOR)CreateIconIndirect(&ii2);
        DeleteObject(hbmM2); if (hbmC2) DeleteObject(hbmC2);
    }
}

// ============================================================
//  Toolbar creation
// ============================================================
static void CreateToolbar(HWND hwndParent)
{
    g_hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS |
        CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN,
        0, 0, 0, TOOLBAR_HEIGHT,
        hwndParent, (HMENU)1, GetModuleHandle(NULL), NULL);

    SetWindowSubclass(g_hwndToolbar, ToolbarSubclassProc, 1, 0);

    SendMessage(g_hwndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    g_hToolImageList = BuildToolImageList(false);  // ApplyDarkMode() rebuilds after LoadSettings
    if (g_hToolImageList)
        SendMessage(g_hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)g_hToolImageList);

    // Menu bar replacement — image dropdown buttons at the left of the toolbar
    // Image indices 24-27 correspond to File/Edit/Image/View icons in BuildToolImageList;
    // Help and the dark mode toggle are right-aligned HWND overlay buttons (g_hwndBtnHelp / g_hwndBtnDark)
    {
        TBBUTTON mb[] = {
            {24, ID_MENU_FILE,  TBSTATE_ENABLED, BTNS_DROPDOWN|BTNS_AUTOSIZE, {0}, 0, -1},
            {25, ID_MENU_EDIT,  TBSTATE_ENABLED, BTNS_DROPDOWN|BTNS_AUTOSIZE, {0}, 0, -1},
            {26, ID_MENU_IMAGE, TBSTATE_ENABLED, BTNS_DROPDOWN|BTNS_AUTOSIZE, {0}, 0, -1},
            {27, ID_MENU_VIEW,  TBSTATE_ENABLED, BTNS_DROPDOWN|BTNS_AUTOSIZE, {0}, 0, -1},
            {0,  0,             TBSTATE_ENABLED, BTNS_SEP,                     {0}, 0, -1},
        };
        SendMessage(g_hwndToolbar, TB_ADDBUTTONS, ARRAYSIZE(mb), (LPARAM)mb);
    }

    // Tool cursors — Fluent UI ic_fluent_pen_20_regular, tip at lower-left
    {
        // The icon has two subpaths: the outer silhouette and an inner detail line.
        // Filling both (a "regular"/outlined icon) only fills the narrow ring strokes,
        // not the interior.  Fix: fill ONLY the outer subpath with white to get a solid
        // white silhouette, then draw BOTH subpaths in black for the outline + detail.
        static const char* penOuterD =          // first subpath only (outer silhouette)
            "M17.1813 2.92689C16.029 1.71505 14.1046 1.69077 12.9222 2.87317L3.54735 12.2475"
            "C3.21952 12.5754 2.99198 12.9899 2.89142 13.4424L2.0138 17.3923C1.97672 17.5592"
            " 2.02748 17.7335 2.14838 17.8544C2.26928 17.9753 2.44356 18.026 2.61044 17.9889"
            "L6.53683 17.1157C7.00426 17.0118 7.43237 16.7767 7.77096 16.4381L17.129 7.08003"
            "C18.27 5.939 18.2932 4.09631 17.1813 2.92689Z";
        static const char* penFullD =           // all subpaths (for black detail)
            "M17.1813 2.92689C16.029 1.71505 14.1046 1.69077 12.9222 2.87317L3.54735 12.2475"
            "C3.21952 12.5754 2.99198 12.9899 2.89142 13.4424L2.0138 17.3923C1.97672 17.5592"
            " 2.02748 17.7335 2.14838 17.8544C2.26928 17.9753 2.44356 18.026 2.61044 17.9889"
            "L6.53683 17.1157C7.00426 17.0118 7.43237 16.7767 7.77096 16.4381L17.129 7.08003"
            "C18.27 5.939 18.2932 4.09631 17.1813 2.92689Z"
            "M13.6293 3.58029C14.4142 2.79538 15.6917 2.8115 16.4566 3.61596C17.1947 4.39225"
            " 17.1793 5.61548 16.4218 6.37293L15.7506 7.04418L12.958 4.25155L13.6293 3.58029Z"
            "M12.2508 4.95864L15.0435 7.7513L7.06385 15.731C6.8597 15.9352 6.60158 16.0769"
            " 6.31975 16.1396L3.16044 16.8421L3.86762 13.6593C3.92692 13.3924 4.0611 13.148"
            " 4.25444 12.9547L12.2508 4.95864Z";
        const int csz = 24;
        const float cScale = (float)csz / 20.0f;
        Bitmap cbmp(csz, csz, PixelFormat32bppARGB);
        {
            Graphics cg(&cbmp);
            cg.SetSmoothingMode(SmoothingModeAntiAlias);
            cg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            cg.Clear(Color(0, 0, 0, 0));
            // White fill — outer silhouette only → solid white interior
            GraphicsPath cpOuter(FillModeWinding);
            AddSvgPath(cpOuter, penOuterD, cScale);
            SolidBrush whiteBr(Color(255, 255, 255, 255));
            cg.FillPath(&whiteBr, &cpOuter);
            // Black detail — both subpaths stroked → outline + inner division line
            GraphicsPath cpFull(FillModeWinding);
            AddSvgPath(cpFull, penFullD, cScale);
            Pen blackPen(Color(255, 0, 0, 0), 1.0f);
            cg.DrawPath(&blackPen, &cpFull);
        }
        // Convert GDI+ bitmap → premultiplied HBITMAP → HCURSOR
        BITMAPV5HEADER cbmi = {};
        cbmi.bV5Size = sizeof(cbmi); cbmi.bV5Width = csz; cbmi.bV5Height = -csz;
        cbmi.bV5Planes = 1; cbmi.bV5BitCount = 32; cbmi.bV5Compression = BI_BITFIELDS;
        cbmi.bV5RedMask = 0x00FF0000; cbmi.bV5GreenMask = 0x0000FF00;
        cbmi.bV5BlueMask = 0x000000FF; cbmi.bV5AlphaMask = 0xFF000000;
        void* cpv = nullptr;
        HDC chdc = GetDC(NULL);
        HBITMAP chbmC = CreateDIBSection(chdc, (BITMAPINFO*)&cbmi, DIB_RGB_COLORS, &cpv, NULL, 0);
        ReleaseDC(NULL, chdc);
        if (chbmC && cpv) {
            BitmapData cbd; Rect crc(0, 0, csz, csz);
            cbmp.LockBits(&crc, ImageLockModeRead, PixelFormat32bppARGB, &cbd);
            BYTE* src = (BYTE*)cbd.Scan0; BYTE* dst = (BYTE*)cpv;
            for (int i = 0; i < csz*csz*4; i += 4) {
                BYTE a = src[i+3];
                dst[i+0]=(BYTE)((int)src[i+0]*a/255);
                dst[i+1]=(BYTE)((int)src[i+1]*a/255);
                dst[i+2]=(BYTE)((int)src[i+2]*a/255);
                dst[i+3]=a;
            }
            cbmp.UnlockBits(&cbd);
        }
        HBITMAP chbmM = CreateBitmap(csz, csz, 1, 1, NULL);
        HDC chdcM = CreateCompatibleDC(NULL); HBITMAP chOld = (HBITMAP)SelectObject(chdcM, chbmM);
        RECT cmr = {0,0,csz,csz}; FillRect(chdcM, &cmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(chdcM, chOld); DeleteDC(chdcM);
        ICONINFO cii = {}; cii.fIcon = FALSE; cii.xHotspot = 3; cii.yHotspot = 22;
        cii.hbmMask = chbmM; cii.hbmColor = chbmC;
        g_hCurPencil = (HCURSOR)CreateIconIndirect(&cii);
        DeleteObject(chbmM); if (chbmC) DeleteObject(chbmC);
    }

    RecreateBrushCursors();

    // Dropper cursor — Fluent ic_fluent_eyedropper_20_regular, white fill + black outline
    {
        // Outer silhouette only (first subpath) — used for white fill so inner winding holes
        // don't punch through. Full path (all 3 subpaths) stroked for black detail.
        static const char* dropOuterD =
            "M17.2465 2.75409C16.2228 1.7304 14.563 1.73041 13.5393 2.75411L12.5 3.7935"
            "L12.1489 3.44243C11.5631 2.85663 10.6134 2.85663 10.0276 3.44242L9.44178 4.0282"
            "C8.856 4.61399 8.85599 5.56373 9.44178 6.14952L9.79287 6.50061L3.43946 12.854"
            "C3.15816 13.1353 3.00012 13.5169 3.00012 13.9147V14.4162L2.03559 16.6745"
            "C1.67987 17.5073 2.52597 18.3463 3.35577 17.9835L5.60452 17.0005H6.08591"
            "C6.48373 17.0005 6.86526 16.8424 7.14657 16.5611L13.5 10.2077L13.8533 10.5611"
            "C14.4391 11.1468 15.3888 11.1468 15.9746 10.5611L16.5604 9.97528"
            "C17.1462 9.38949 17.1462 8.43974 16.5604 7.85396L16.2071 7.50061L17.2465 6.46119"
            "C18.2701 5.4375 18.2701 3.77778 17.2465 2.75409Z";
        static const char* dropFullD =
            "M17.2465 2.75409C16.2228 1.7304 14.563 1.73041 13.5393 2.75411L12.5 3.7935"
            "L12.1489 3.44243C11.5631 2.85663 10.6134 2.85663 10.0276 3.44242L9.44178 4.0282"
            "C8.856 4.61399 8.85599 5.56373 9.44178 6.14952L9.79287 6.50061L3.43946 12.854"
            "C3.15816 13.1353 3.00012 13.5169 3.00012 13.9147V14.4162L2.03559 16.6745"
            "C1.67987 17.5073 2.52597 18.3463 3.35577 17.9835L5.60452 17.0005H6.08591"
            "C6.48373 17.0005 6.86526 16.8424 7.14657 16.5611L13.5 10.2077L13.8533 10.5611"
            "C14.4391 11.1468 15.3888 11.1468 15.9746 10.5611L16.5604 9.97528"
            "C17.1462 9.38949 17.1462 8.43974 16.5604 7.85396L16.2071 7.50061L17.2465 6.46119"
            "C18.2701 5.4375 18.2701 3.77778 17.2465 2.75409Z"
            "M14.2464 3.46121C14.8796 2.82804 15.9062 2.82803 16.5394 3.4612"
            "C17.1725 4.09436 17.1725 5.12092 16.5394 5.75409L15.1464 7.14706"
            "C14.9511 7.34232 14.9511 7.6589 15.1464 7.85416L15.8533 8.56106"
            "C16.0486 8.75633 16.0486 9.07291 15.8533 9.26817L15.2675 9.85396"
            "C15.0723 10.0492 14.7557 10.0492 14.5604 9.85396L10.1489 5.44241"
            "C9.95363 5.24715 9.95363 4.93057 10.1489 4.73531L10.7347 4.14952"
            "C10.9299 3.95426 11.2465 3.95426 11.4418 4.14953L12.1464 4.85416"
            "C12.2402 4.94793 12.3674 5.00061 12.5 5.00061C12.6326 5.00061"
            " 12.7598 4.94793 12.8535 4.85416L14.2464 3.46121Z"
            "M12.7929 9.50062L6.43946 15.854C6.34569 15.9478 6.21852 16.0005"
            " 6.08591 16.0005H5.50001C5.43108 16.0005 5.36289 16.0147 5.29973 16.0423"
            "L2.95522 17.0673L3.95994 14.7149C3.98645 14.6528 4.00012 14.586"
            " 4.00012 14.5185V13.9147C4.00012 13.7821 4.0528 13.6549 4.14657 13.5611"
            "L10.5 7.20772L12.7929 9.50062Z";
        const int dsz = 24;
        const float dScale = (float)dsz / 20.0f;
        Bitmap dbmp(dsz, dsz, PixelFormat32bppARGB);
        {
            Graphics dg(&dbmp);
            dg.SetSmoothingMode(SmoothingModeAntiAlias);
            dg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            dg.Clear(Color(0, 0, 0, 0));
            GraphicsPath dpOuter(FillModeWinding);
            AddSvgPath(dpOuter, dropOuterD, dScale);
            SolidBrush whiteBr(Color(255, 255, 255, 255));
            dg.FillPath(&whiteBr, &dpOuter);
            GraphicsPath dpFull(FillModeWinding);
            AddSvgPath(dpFull, dropFullD, dScale);
            Pen blackPen(Color(255, 0, 0, 0), 1.0f);
            dg.DrawPath(&blackPen, &dpFull);
        }
        BITMAPV5HEADER dbmi = {};
        dbmi.bV5Size = sizeof(dbmi); dbmi.bV5Width = dsz; dbmi.bV5Height = -dsz;
        dbmi.bV5Planes = 1; dbmi.bV5BitCount = 32; dbmi.bV5Compression = BI_BITFIELDS;
        dbmi.bV5RedMask = 0x00FF0000; dbmi.bV5GreenMask = 0x0000FF00;
        dbmi.bV5BlueMask = 0x000000FF; dbmi.bV5AlphaMask = 0xFF000000;
        void* dpv = nullptr;
        HDC dhdc = GetDC(NULL);
        HBITMAP dhbmC = CreateDIBSection(dhdc, (BITMAPINFO*)&dbmi, DIB_RGB_COLORS, &dpv, NULL, 0);
        ReleaseDC(NULL, dhdc);
        if (dhbmC && dpv) {
            BitmapData dbd; Rect drc(0, 0, dsz, dsz);
            dbmp.LockBits(&drc, ImageLockModeRead, PixelFormat32bppARGB, &dbd);
            BYTE* src = (BYTE*)dbd.Scan0; BYTE* dst = (BYTE*)dpv;
            for (int i = 0; i < dsz*dsz*4; i += 4) {
                BYTE a = src[i+3];
                dst[i+0]=(BYTE)((int)src[i+0]*a/255);
                dst[i+1]=(BYTE)((int)src[i+1]*a/255);
                dst[i+2]=(BYTE)((int)src[i+2]*a/255);
                dst[i+3]=a;
            }
            dbmp.UnlockBits(&dbd);
        }
        HBITMAP dhbmM = CreateBitmap(dsz, dsz, 1, 1, NULL);
        HDC dhdcM = CreateCompatibleDC(NULL);
        HBITMAP dhOld = (HBITMAP)SelectObject(dhdcM, dhbmM);
        RECT dmr = {0,0,dsz,dsz}; FillRect(dhdcM, &dmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(dhdcM, dhOld); DeleteDC(dhdcM);
        ICONINFO dii = {}; dii.fIcon = FALSE; dii.xHotspot = 2; dii.yHotspot = 20;
        dii.hbmMask = dhbmM; dii.hbmColor = dhbmC;
        g_hCurDropper = (HCURSOR)CreateIconIndirect(&dii);
        DeleteObject(dhbmM); if (dhbmC) DeleteObject(dhbmC);
    }

    // Create lasso cursor from lasso.svg path
    {
        static const char* lassoD =
            "M9.703 2.265A10.026 10.026 0 0 1 12 2c.79 0 1.559.092 2.297.265a.75.75 0 1 1"
            "-.343 1.46A8.527 8.527 0 0 0 12 3.5c-.673 0-1.327.078-1.954.225a.75.75 0 1 1"
            "-.343-1.46ZM7.773 3.736a.75.75 0 0 1-.242 1.032 8.548 8.548 0 0 0-2.763 2.763"
            ".75.75 0 1 1-1.275-.79 10.048 10.048 0 0 1 3.248-3.248.75.75 0 0 1 1.032.243Z"
            "M16.227 3.736a.75.75 0 0 1 1.032-.243 10.048 10.048 0 0 1 3.248 3.248.75.75 0"
            " 1 1-1.275.79 8.55 8.55 0 0 0-2.763-2.763.75.75 0 0 1-.242-1.032Z"
            "M3.167 9.144a.75.75 0 0 1 .558.902A8.527 8.527 0 0 0 3.5 12c0 .673.078 1.327"
            ".225 1.954a.75.75 0 1 1-1.46.343A10.026 10.026 0 0 1 2 12c0-.79.092-1.559"
            ".265-2.297a.75.75 0 0 1 .902-.559Z"
            "M20.833 9.144a.75.75 0 0 1 .902.559C21.908 10.44 22 11.21 22 12c0 .79-.092"
            " 1.559-.265 2.297a.75.75 0 1 1-1.46-.343c.147-.627.225-1.28.225-1.954"
            " 0-.673-.078-1.327-.226-1.954a.75.75 0 0 1 .559-.902Z"
            "M3.736 16.227a.75.75 0 0 1 1.032.242 8.548 8.548 0 0 0 2.763 2.763.75.75 0 0"
            " 1-.79 1.275 10.048 10.048 0 0 1-3.248-3.248.75.75 0 0 1 .243-1.032Z"
            "M20.42 17.085a.75.75 0 1 0-1.34-.67l-.003.004-.015.029a8.004 8.004 0 0 1"
            "-.358.59 9.58 9.58 0 0 1-.965 1.218c-1.17-1.073-2.756-2.006-4.74-2.006"
            "-2.347 0-3.99 1.203-3.99 2.875S10.653 22 13 22c1.942 0 3.495-.75 4.658-1.645"
            "a11.73 11.73 0 0 1 1.315 2.01c.033.065.058.116.073.149l.017.035.004.009a.75"
            ".75 0 0 0 1.368-.615c-.087-.183 0-.001 0-.001v-.002l-.003-.004-.007-.015a11"
            ".874 11.874 0 0 0-.464-.87 13.199 13.199 0 0 0-1.189-1.703 11.057 11.057 0 0"
            " 0 1.525-2.032l.09-.162.024-.047.007-.014.002-.005.002-.003Z"
            "M13 17.75c1.433 0 2.644.652 3.616 1.512-.95.7-2.155 1.238-3.616 1.238"
            "-1.973 0-2.49-.922-2.49-1.375 0-.453.517-1.375 2.49-1.375Z";
        const int lsz = 24;
        Bitmap lbmp(lsz, lsz, PixelFormat32bppARGB);
        {
            Graphics lg(&lbmp);
            lg.SetSmoothingMode(SmoothingModeAntiAlias);
            lg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            lg.Clear(Color(0, 0, 0, 0));
            GraphicsPath lpath(FillModeWinding);
            AddSvgPath(lpath, lassoD, 1.0f);
            SolidBrush wBr(Color(255,255,255,255)); lg.FillPath(&wBr, &lpath);
            Pen bPen(Color(255,0,0,0), 0.5f);       lg.DrawPath(&bPen, &lpath);
        }
        BITMAPV5HEADER lbmi = {};
        lbmi.bV5Size = sizeof(lbmi); lbmi.bV5Width = lsz; lbmi.bV5Height = -lsz;
        lbmi.bV5Planes = 1; lbmi.bV5BitCount = 32; lbmi.bV5Compression = BI_BITFIELDS;
        lbmi.bV5RedMask = 0x00FF0000; lbmi.bV5GreenMask = 0x0000FF00;
        lbmi.bV5BlueMask = 0x000000FF; lbmi.bV5AlphaMask = 0xFF000000;
        void* lpv = nullptr;
        HDC lhdc = GetDC(NULL);
        HBITMAP lhbmC = CreateDIBSection(lhdc, (BITMAPINFO*)&lbmi, DIB_RGB_COLORS, &lpv, NULL, 0);
        ReleaseDC(NULL, lhdc);
        if (lhbmC && lpv) {
            BitmapData lbd; Rect lrc(0,0,lsz,lsz);
            lbmp.LockBits(&lrc, ImageLockModeRead, PixelFormat32bppARGB, &lbd);
            BYTE* ls = (BYTE*)lbd.Scan0; BYTE* ld = (BYTE*)lpv;
            for (int i = 0; i < lsz*lsz*4; i += 4) {
                BYTE a = ls[i+3];
                ld[i+0]=(BYTE)((int)ls[i+0]*a/255); ld[i+1]=(BYTE)((int)ls[i+1]*a/255);
                ld[i+2]=(BYTE)((int)ls[i+2]*a/255); ld[i+3]=a;
            }
            lbmp.UnlockBits(&lbd);
        }
        HBITMAP lhbmM = CreateBitmap(lsz, lsz, 1, 1, NULL);
        HDC lhdcM = CreateCompatibleDC(NULL);
        HBITMAP lhOld = (HBITMAP)SelectObject(lhdcM, lhbmM);
        RECT lmr = {0,0,lsz,lsz}; FillRect(lhdcM, &lmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(lhdcM, lhOld); DeleteDC(lhdcM);
        ICONINFO lii = {}; lii.fIcon = FALSE; lii.xHotspot = 12; lii.yHotspot = 12;
        lii.hbmMask = lhbmM; lii.hbmColor = lhbmC;
        g_hCurLasso = (HCURSOR)CreateIconIndirect(&lii);
        DeleteObject(lhbmM); if (lhbmC) DeleteObject(lhbmC);
    }

    CreateRotateHandleResources();

    // Create magic wand cursor from wand SVG path
    {
        static const char* wandD =
            "m 9.7545104,7.565 0.136,0.126 10.4799996,10.488"
            " a 2.27,2.27 0 0 1 -3.211,3.208 L 6.6805104,10.9"
            " a 2.251,2.251 0 0 1 10e-4,-3.182 l 0.157,-0.146"
            " a 2.25,2.25 0 0 1 2.916,-0.007 z"
            " m 0.8479996,2.961 -1.0879996,1.088 8.7059996,8.713"
            " a 0.77,0.77 0 1 0 1.089,-1.088 z"
            " m -4.3859996,4.48 0.102,-0.006"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102 v 0.75 h 0.75"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102"
            " c 0,0.38 -0.282,0.694 -0.648,0.743 l -0.102,0.007 h -0.75 v 0.75"
            " c 0,0.38 -0.282,0.694 -0.648,0.743 l -0.102,0.007"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102 V 18 h -0.75"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102"
            " a 0.75,0.75 0 0 1 0.648,-0.743 l 0.102,-0.007 h 0.75 v -0.75"
            " a 0.75,0.75 0 0 1 0.648,-0.743 z"
            " m 1.553,-6.254 -0.027,0.027"
            " a 0.751,0.751 0 0 0 0,1.061 l 0.711,0.713 1.089,-1.089 -0.73,-0.73"
            " a 0.75,0.75 0 0 0 -1.043,0.018 z"
            " M 16.21651,5.007 16.31851,5"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102 V 6.5 h 0.75"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 L 17.81851,8 h -0.75 v 0.75"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 l -0.102,0.007"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 L 15.56851,8.75 V 8 h -0.75"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102"
            " a 0.75,0.75 0 0 1 0.648,-0.743 l 0.102,-0.007 h 0.75 V 5.75"
            " a 0.75,0.75 0 0 1 0.648,-0.743 z"
            " m -11.9999996,-2 0.102,-0.007"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102 V 4.5 h 0.75"
            " a 0.75,0.75 0 0 1 0.743,0.648 l 0.007,0.102"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 L 5.8185104,6 h -0.75 v 0.75"
            " c 0,0.38 -0.282,0.693 -0.648,0.743 l -0.102,0.007"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102 V 6 h -0.75"
            " a 0.75,0.75 0 0 1 -0.743,-0.648 l -0.007,-0.102"
            " a 0.75,0.75 0 0 1 0.648,-0.743 l 0.102,-0.007 h 0.75 V 3.75"
            " a 0.75,0.75 0 0 1 0.648,-0.743 z";
        const int wsz = 24;
        Bitmap wbmp(wsz, wsz, PixelFormat32bppARGB);
        {
            Graphics wg(&wbmp);
            wg.SetSmoothingMode(SmoothingModeAntiAlias);
            wg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            wg.Clear(Color(0, 0, 0, 0));
            GraphicsPath wpath(FillModeWinding);
            AddSvgPath(wpath, wandD, 22.0f / 24.0f);
            // Draw black outline first, then white fill on top — visible on any background
            Pen outlinePen(Color(255,0,0,0), 2.0f);       wg.DrawPath(&outlinePen, &wpath);
            SolidBrush wBr(Color(255,255,255,255));        wg.FillPath(&wBr, &wpath);
        }
        BITMAPV5HEADER wbmi = {};
        wbmi.bV5Size = sizeof(wbmi); wbmi.bV5Width = wsz; wbmi.bV5Height = -wsz;
        wbmi.bV5Planes = 1; wbmi.bV5BitCount = 32; wbmi.bV5Compression = BI_BITFIELDS;
        wbmi.bV5RedMask = 0x00FF0000; wbmi.bV5GreenMask = 0x0000FF00;
        wbmi.bV5BlueMask = 0x000000FF; wbmi.bV5AlphaMask = 0xFF000000;
        void* wpv = nullptr;
        HDC whdc = GetDC(NULL);
        HBITMAP whbmC = CreateDIBSection(whdc, (BITMAPINFO*)&wbmi, DIB_RGB_COLORS, &wpv, NULL, 0);
        ReleaseDC(NULL, whdc);
        if (whbmC && wpv) {
            BitmapData wbd; Rect wrc(0,0,wsz,wsz);
            wbmp.LockBits(&wrc, ImageLockModeRead, PixelFormat32bppARGB, &wbd);
            BYTE* ws = (BYTE*)wbd.Scan0; BYTE* wd = (BYTE*)wpv;
            for (int i = 0; i < wsz*wsz*4; i += 4) {
                BYTE a = ws[i+3];
                wd[i+0]=(BYTE)((int)ws[i+0]*a/255); wd[i+1]=(BYTE)((int)ws[i+1]*a/255);
                wd[i+2]=(BYTE)((int)ws[i+2]*a/255); wd[i+3]=a;
            }
            wbmp.UnlockBits(&wbd);
        }
        HBITMAP whbmM = CreateBitmap(wsz, wsz, 1, 1, NULL);
        HDC whdcM = CreateCompatibleDC(NULL);
        HBITMAP whOld = (HBITMAP)SelectObject(whdcM, whbmM);
        RECT wmr = {0,0,wsz,wsz}; FillRect(whdcM, &wmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(whdcM, whOld); DeleteDC(whdcM);
        // Hotspot at tip of wand (top-left area of the 24x24 icon, roughly 7,8)
        ICONINFO wii = {}; wii.fIcon = FALSE; wii.xHotspot = 7; wii.yHotspot = 8;
        wii.hbmMask = whbmM; wii.hbmColor = whbmC;
        g_hCurWand = (HCURSOR)CreateIconIndirect(&wii);
        DeleteObject(whbmM); if (whbmC) DeleteObject(whbmC);
    }

    // Create smudge cursor from finger_smudge.svg path (9×9 viewBox, stroked)
    {
        static const char* smudgeD =
            "M 5.4825026,0.52914232 3.880377,2.2929501 2.1900613,2.2635533"
            " C 1.9947559,2.2601567 1.8157329,2.4058668 1.6315222,2.6310132"
            " L 0.8378087,3.6011074"
            " C 0.64079119,3.8419066 0.60059631,4.1186135 0.70552312,4.3654241"
            " 0.77149417,4.5206023 0.94378984,4.6042128 1.1170783,4.6152969"
            " L 1.9842837,3.586409 0.98479267,4.8063761"
            " C 0.89290679,4.9456303 0.92519805,5.500615 1.219967,5.6588834"
            " c 0.1707579,0.091684 0.288581,0.098342 0.4313738,0.0242"
            " C 1.6673342,5.573567 1.679829,5.4631174 1.74618,5.3670295"
            " L 2.7486004,4.2772337 1.7491094,5.3649152"
            " C 1.6327905,5.567609 1.6360457,5.8277846 1.718337,5.9405732"
            " 1.8517687,6.123455 1.9822234,6.2775386 2.172841,6.3382702"
            " L 3.3365363,5.0562488 0.99949107,7.6284687"
            " c -0.18700073,0.2058184 -0.26630861,0.5036128 0,0.6614279"
            " C 1.3752601,8.5125783 1.8317808,8.5242847 2.2929501,8.231103"
            " 2.8310219,7.8890317 3.2621172,7.17582 3.7136509,6.5486588"
            " L 4.4389161,5.5412962"
            " C 4.585877,5.3371738 4.7817581,5.2523968 5.0121536,5.2767248"
            " c 0.1961795,0.020715 0.3992637,0.042519 0.5144442,0.1763808"
            " 0,0 -0.136036,0.2617502 -0.2204762,0.3821586"
            " -0.069876,0.099641 -0.2351744,0.2792695 -0.2351744,0.2792695"
            " -0.05817,0.069077 -0.1964423,0.1101586 -0.3821583,0.1175872"
            " L 3.9317832,6.2602185"
            " C 3.6780436,6.5863481 3.4451317,6.9333054 3.2037667,7.2718096"
            " l 1.9847677,0.047993"
            " C 5.3885549,7.2595792 5.5954784,7.1917858 5.7653386,7.0380109"
            " 6.0598482,6.7713907 6.2472309,6.5980374 6.5088686,6.2912204"
            " 6.7825749,5.970251 6.7334287,5.3901937 6.7465648,4.9680584"
            " V 3.7186946 L 8.6426581,1.9254902";
        const int csz = 24;
        Bitmap cbmp(csz, csz, PixelFormat32bppARGB);
        {
            Graphics cg(&cbmp);
            cg.SetSmoothingMode(SmoothingModeAntiAlias);
            cg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            cg.Clear(Color(0, 0, 0, 0));
            GraphicsPath cpath(FillModeWinding);
            AddSvgPath(cpath, smudgeD, (float)csz / 9.0f);
            Pen wPen(Color(255,255,255,255), 3.0f);
            wPen.SetStartCap(LineCapRound); wPen.SetEndCap(LineCapRound);
            wPen.SetLineJoin(LineJoinRound);
            cg.DrawPath(&wPen, &cpath);
            Pen bPen(Color(255,0,0,0), 1.5f);
            bPen.SetStartCap(LineCapRound); bPen.SetEndCap(LineCapRound);
            bPen.SetLineJoin(LineJoinRound);
            cg.DrawPath(&bPen, &cpath);
        }
        BITMAPV5HEADER cbmi = {};
        cbmi.bV5Size = sizeof(cbmi); cbmi.bV5Width = csz; cbmi.bV5Height = -csz;
        cbmi.bV5Planes = 1; cbmi.bV5BitCount = 32; cbmi.bV5Compression = BI_BITFIELDS;
        cbmi.bV5RedMask = 0x00FF0000; cbmi.bV5GreenMask = 0x0000FF00;
        cbmi.bV5BlueMask = 0x000000FF; cbmi.bV5AlphaMask = 0xFF000000;
        void* cpv = nullptr;
        HDC chdc = GetDC(NULL);
        HBITMAP chbmC = CreateDIBSection(chdc, (BITMAPINFO*)&cbmi, DIB_RGB_COLORS, &cpv, NULL, 0);
        ReleaseDC(NULL, chdc);
        if (chbmC && cpv) {
            BitmapData cbd; Rect crc(0,0,csz,csz);
            cbmp.LockBits(&crc, ImageLockModeRead, PixelFormat32bppARGB, &cbd);
            BYTE* cs2 = (BYTE*)cbd.Scan0; BYTE* cd = (BYTE*)cpv;
            for (int i = 0; i < csz*csz*4; i += 4) {
                BYTE a = cs2[i+3];
                cd[i+0]=(BYTE)((int)cs2[i+0]*a/255); cd[i+1]=(BYTE)((int)cs2[i+1]*a/255);
                cd[i+2]=(BYTE)((int)cs2[i+2]*a/255); cd[i+3]=a;
            }
            cbmp.UnlockBits(&cbd);
        }
        HBITMAP chbmM = CreateBitmap(csz, csz, 1, 1, NULL);
        HDC chdcM = CreateCompatibleDC(NULL);
        HBITMAP chOld = (HBITMAP)SelectObject(chdcM, chbmM);
        RECT cmr = {0,0,csz,csz}; FillRect(chdcM, &cmr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(chdcM, chOld); DeleteDC(chdcM);
        // Hotspot near the bottom-left smudge area (finger contact point)
        ICONINFO scii = {}; scii.fIcon = FALSE; scii.xHotspot = 3; scii.yHotspot = 21;
        scii.hbmMask = chbmM; scii.hbmColor = chbmC;
        g_hCurSmudge = (HCURSOR)CreateIconIndirect(&scii);
        DeleteObject(chbmM); if (chbmC) DeleteObject(chbmC);
    }

    RecreateFillCursor();   // initial build using default foreground colour

    SendMessage(g_hwndToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(22, 22));
    SendMessage(g_hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(34, TOOLBAR_HEIGHT - 4));

    TBBUTTON btns[] = {
        {0, ID_TOOL_PENCIL,  TBSTATE_ENABLED|TBSTATE_CHECKED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {1, ID_TOOL_BRUSH,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {2, ID_TOOL_ERASER,  TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {23, ID_TOOL_DROPPER, TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {3,  ID_TOOL_FILL,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {31, ID_TOOL_WAND,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {7,  ID_TOOL_TEXT,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {8,  ID_TOOL_SELECT, TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {30, ID_MENU_TOOLS, TBSTATE_ENABLED, BTNS_BUTTON,                   {0}, 0, -1},
        {0,  ID_SEP_EXTENDED, TBSTATE_ENABLED, BTNS_SEP,                     {0}, 0, -1},
        {4, ID_TOOL_LINE,    TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {5, ID_TOOL_RECT,    TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {6, ID_TOOL_ELLIPSE, TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {9,  ID_TOOL_RRECT,     TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {10, ID_TOOL_TRIANGLE,  TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {11, ID_TOOL_ARROW_L,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {12, ID_TOOL_ARROW_R,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {13, ID_TOOL_ARROW_U,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {14, ID_TOOL_ARROW_D,   TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {15, ID_TOOL_ARROW,     TBSTATE_ENABLED, BTNS_BUTTON|BTNS_CHECKGROUP, {0}, 0, -1},
        {29, ID_MENU_MORE, TBSTATE_ENABLED, BTNS_BUTTON,                            {0}, 0, -1},
    };
    SendMessage(g_hwndToolbar, TB_ADDBUTTONS, ARRAYSIZE(btns), (LPARAM)btns);
    SendMessage(g_hwndToolbar, TB_AUTOSIZE, 0, 0);
}

static void CreateStatusBar(HWND hwndParent)
{
    g_hwndStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        hwndParent, (HMENU)2, GetModuleHandle(NULL), NULL);
    int parts[6] = {160, 310, 450, 610, 700, -1};
    SendMessage(g_hwndStatus, SB_SETPARTS, 6, (LPARAM)parts);

    // Build pane icons from Fluent SVG paths using system text colour
    COLORREF sysTxt = GetSysColor(COLOR_BTNTEXT);
    Color iconClr(255, GetRValue(sysTxt), GetGValue(sysTxt), GetBValue(sysTxt));

    g_hIcoStatusCrop   = MakeHIconFromSvg(kStatusCropD,   16, iconClr);
    g_hIcoStatusCursor = MakeHIconFromSvg(kStatusCursorD, 16, iconClr);
    g_hIcoStatusBorder = MakeHIconFromSvg(kStatusBorderD, 16, iconClr);

    if (g_hIcoStatusCrop)   SendMessage(g_hwndStatus, SB_SETICON, STATUS_PANE_SIZE, (LPARAM)g_hIcoStatusCrop);
    if (g_hIcoStatusCursor) SendMessage(g_hwndStatus, SB_SETICON, STATUS_PANE_POS,  (LPARAM)g_hIcoStatusCursor);
    if (g_hIcoStatusBorder) SendMessage(g_hwndStatus, SB_SETICON, STATUS_PANE_SEL,  (LPARAM)g_hIcoStatusBorder);

    SetStatusText(STATUS_PANE_ZOOM, L" Zoom");
    UpdateStatusBar(0, 0);
    RebuildStatusIcons();

    // Suppress the status bar's own WM_ERASEBKGND so it never flashes the
    // system background colour before WM_DRAWITEM paints each pane.
    if (g_hwndStatus)
        SetWindowSubclass(g_hwndStatus, StatusBarSubclassProc, 0, 0);
}

// ============================================================
//  Scroll helpers
// ============================================================
static void UpdateScrollBars()
{
    if (!g_hwndCanvas || !g_pCanvas) return;
    RECT rc; GetClientRect(g_hwndCanvas, &rc);
    int cliW = rc.right, cliH = rc.bottom;
    int cw = (int)g_pCanvas->GetWidth();
    int ch = (int)g_pCanvas->GetHeight();

    // Scaled canvas size
    int scaledW = max(1, (int)(cw * g_zoom));
    int scaledH = max(1, (int)(ch * g_zoom));

    // Content extent: extend range to include paste bitmap if it goes beyond canvas
    int contentRight  = scaledW;
    int contentBottom = scaledH;
    if (g_isPasting && g_pasteBitmap) {
        contentRight  = max(contentRight,  (int)((g_pastePos.x + (int)g_pasteBitmap->GetWidth())  * g_zoom));
        contentBottom = max(contentBottom, (int)((g_pastePos.y + (int)g_pasteBitmap->GetHeight()) * g_zoom));
    }
    if (g_hasSelection) {
        contentRight  = max(contentRight,  (int)(g_selRect.right  * g_zoom));
        contentBottom = max(contentBottom, (int)(g_selRect.bottom * g_zoom));
    }

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;

    si.nMin  = 0; si.nMax  = contentRight  + 2*SCROLL_PAD - 1;
    si.nPage = (UINT)max(1, cliW);
    si.nPos  = g_scrollX;
    SetScrollInfo(g_hwndCanvas, SB_HORZ, &si, TRUE);

    si.nMin  = 0; si.nMax  = contentBottom + 2*SCROLL_PAD - 1;
    si.nPage = (UINT)max(1, cliH);
    si.nPos  = g_scrollY;
    SetScrollInfo(g_hwndCanvas, SB_VERT, &si, TRUE);

    SCROLLINFO sr = {}; sr.cbSize = sizeof(sr); sr.fMask = SIF_POS;
    GetScrollInfo(g_hwndCanvas, SB_HORZ, &sr); g_scrollX = sr.nPos;
    GetScrollInfo(g_hwndCanvas, SB_VERT, &sr); g_scrollY = sr.nPos;
}

static void CenterScrollOnCanvas()
{
    if (!g_hwndCanvas || !g_pCanvas) return;
    RECT rc; GetClientRect(g_hwndCanvas, &rc);
    int cliW = rc.right, cliH = rc.bottom;
    int cw = (int)g_pCanvas->GetWidth();
    int ch = (int)g_pCanvas->GetHeight();
    int scaledW = max(1, (int)(cw * g_zoom));
    int scaledH = max(1, (int)(ch * g_zoom));
    // max(0, ...) avoids overshooting when canvas >= window
    g_scrollX = max(0, SCROLL_PAD - max(0, cliW - scaledW) / 2);
    g_scrollY = max(0, SCROLL_PAD - max(0, cliH - scaledH) / 2);
    UpdateScrollBars();
    InvalidateCanvasView();
}

// Returns the resize cursor for a handle after accounting for shape/selection rotation.
// Shape edit handles: 4=NW 5=N 6=NE 7=E 8=SE 9=S 10=SW 11=W
// Selection handles:  2=NW 3=N 4=NE 5=E  6=SE 7=S  8=SW  9=W
// Base angle convention: 0=East, 90=South, clockwise (screen Y-down).
static HCURSOR ResizeCursorForHandle(int hit, float angleDeg, bool isShapeEdit)
{
    float base;
    if (isShapeEdit) {
        static const float kb[] = { 0,0,0,0, 225.f,270.f,315.f,0.f,45.f,90.f,135.f,180.f };
        if (hit < 4 || hit > 11) return LoadCursor(NULL, IDC_CROSS);
        base = kb[hit];
    } else {
        static const float kb[] = { 0,0, 225.f,270.f,315.f,0.f,45.f,90.f,135.f,180.f };
        if (hit < 2 || hit > 9)  return LoadCursor(NULL, IDC_CROSS);
        base = kb[hit];
    }
    float a = fmodf(base + angleDeg, 180.f);
    if (a < 0) a += 180.f;
    if      (a <  22.5f || a >= 157.5f) return LoadCursor(NULL, IDC_SIZEWE);
    else if (a <  67.5f)                return LoadCursor(NULL, IDC_SIZENWSE);
    else if (a < 112.5f)                return LoadCursor(NULL, IDC_SIZENS);
    else                                return LoadCursor(NULL, IDC_SIZENESW);
}

static int HitTestCanvasResize(int cx, int cy)
{
    if (!g_pCanvas) return 0;
    // Eraser/smudge must not accidentally grab a resize handle
    if (g_currentTool == TOOL_ERASER || g_currentTool == TOOL_SMUDGE) return 0;

    int cw  = (int)g_pCanvas->GetWidth();
    int ch  = (int)g_pCanvas->GetHeight();
    int stX = g_canvasOffX;
    int stY = g_canvasOffY;
    int edX = g_canvasOffX + (int)(cw * g_zoom);
    int edY = g_canvasOffY + (int)(ch * g_zoom);
    int midX = (stX + edX) / 2;
    int midY = (stY + edY) / 2;
    const int T = 6; // hit radius around handle centre

    // 8 handle points with their resize-direction bitmasks (1=E 2=S 4=W 8=N)
    struct { int x, y, dir; } handles[] = {
        { stX,  stY,  4|8 }, // NW
        { midX, stY,    8 }, // N
        { edX,  stY,  1|8 }, // NE
        { edX,  midY,   1 }, // E
        { edX,  edY,  1|2 }, // SE
        { midX, edY,    2 }, // S
        { stX,  edY,  4|2 }, // SW
        { stX,  midY,   4 }, // W
    };
    for (auto& h : handles) {
        if (abs(cx - h.x) <= T && abs(cy - h.y) <= T)
            return h.dir;
    }
    return 0;
}

// Returns 0=none 1=move 2=NW 3=N 4=NE 5=E 6=SE 7=S 8=SW 9=W
static int HitTestSelection(int cx, int cy)
{
    if (!g_hasSelection) return 0;
    float canX = (cx - g_canvasOffX) / g_zoom;
    float canY = (cy - g_canvasOffY) / g_zoom;
    // Inverse-rotate the mouse point when the floating selection is rotated
    if (g_selFloating && g_selFloatAngle != 0.0f) {
        float pivX = (g_selRect.left + g_selRect.right)  / 2.0f;
        float pivY = (g_selRect.top  + g_selRect.bottom) / 2.0f;
        float rad = -g_selFloatAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad), dx = canX - pivX, dy = canY - pivY;
        canX = pivX + dx * c - dy * s;
        canY = pivY + dx * s + dy * c;
    }
    const float TOL = 6.0f / g_zoom;  // tolerance in canvas pixels
    float SL = (float)g_selRect.left,  SR = (float)g_selRect.right;
    float ST = (float)g_selRect.top,   SB = (float)g_selRect.bottom;
    if (canX < SL - TOL || canX > SR + TOL || canY < ST - TOL || canY > SB + TOL) return 0;
    bool onL = fabsf(canX - SL) <= TOL;
    bool onR = fabsf(canX - SR) <= TOL;
    bool onT = fabsf(canY - ST) <= TOL;
    bool onB = fabsf(canY - SB) <= TOL;
    bool insX = canX >= SL && canX <= SR;
    bool insY = canY >= ST && canY <= SB;
    if (onL && onT) return 2;   // NW
    if (onR && onT) return 4;   // NE
    if (onR && onB) return 6;   // SE
    if (onL && onB) return 8;   // SW
    if (onT && insX) return 3;  // N
    if (onR && insY) return 5;  // E
    if (onB && insX) return 7;  // S
    if (onL && insY) return 9;  // W
    if (insX && insY) return 1; // move
    return 0;
}

// Returns 0=none, 4=NW 5=N 6=NE 7=E 8=SE 9=S 10=SW 11=W  (canvas coords in)
static int HitTestBezierResize(REAL canX, REAL canY)
{
    if (!g_bezEditing) return 0;
    const REAL PAD = 10.f, HT = 8.f;
    // Ref bbox is always current (UpdateBezRefBBox kept it in sync).
    // Inverse-rotate the mouse into local frame so handles match the rotated visual.
    REAL pivX = (g_bezRefL + g_bezRefR) / 2.f, pivY = (g_bezRefT + g_bezRefB) / 2.f;
    if (g_bezAngle != 0.0f) {
        float rad = -g_bezAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad), dx = canX - pivX, dy = canY - pivY;
        canX = pivX + dx * c - dy * s;
        canY = pivY + dx * s + dy * c;
    }
    REAL l = g_bezRefL - PAD, t = g_bezRefT - PAD, r = g_bezRefR + PAD, b = g_bezRefB + PAD;
    REAL mx = (l + r) / 2.f, my = (t + b) / 2.f;
    struct { REAL x, y; int m; } h[] = {
        {l, t,  4}, {mx, t,  5}, {r, t,  6},
        {r, my, 7},
        {r, b,  8}, {mx, b,  9}, {l, b, 10},
        {l, my, 11},
    };
    for (auto& e : h)
        if (fabsf(canX - e.x) <= HT && fabsf(canY - e.y) <= HT) return e.m;
    return 0;
}

// Hit-test the floating paste overlay.
// Returns: 0=none, 1=move, 4=NW, 5=N, 6=NE, 7=E, 8=SE, 9=S, 10=SW, 11=W
static int HitTestPaste(int cx, int cy)
{
    if (!g_isPasting || !g_pasteBitmap) return 0;
    const float PAD = 10.f, HT = 8.f;
    float canX = (cx - g_canvasOffX) / g_zoom;
    float canY = (cy - g_canvasOffY) / g_zoom;
    float L = (float)g_pastePos.x - PAD;
    float T = (float)g_pastePos.y - PAD;
    float R = (float)(g_pastePos.x + g_pasteSizeW) + PAD;
    float B = (float)(g_pastePos.y + g_pasteSizeH) + PAD;
    // Inverse-rotate the mouse point so hit-testing aligns with the rotated overlay
    if (g_pasteAngle != 0.0f) {
        float pivX = (L + R) / 2.f, pivY = (T + B) / 2.f;
        float rad = -g_pasteAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad), dx = canX - pivX, dy = canY - pivY;
        canX = pivX + dx * c - dy * s;
        canY = pivY + dx * s + dy * c;
    }
    float mx = (L + R) / 2.f, my = (T + B) / 2.f;
    struct { float x, y; int m; } h[] = {
        {L,T,4}, {mx,T,5}, {R,T,6},
        {R,my,7},
        {R,B,8}, {mx,B,9}, {L,B,10},
        {L,my,11},
    };
    for (auto& e : h)
        if (fabsf(canX - e.x) <= HT && fabsf(canY - e.y) <= HT) return e.m;
    if (canX >= L && canX <= R && canY >= T && canY <= B) return 1;
    return 0;
}

static int HitTestShapeEdit(int cx, int cy)
{
    if (!g_shapeEditing) return 0;
    const float PAD = 10.f, HT = 8.f;
    float canX = (cx - g_canvasOffX) / g_zoom;
    float canY = (cy - g_canvasOffY) / g_zoom;
    float L = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x) - PAD;
    float T = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y) - PAD;
    float R = (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x) + PAD;
    float B = (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y) + PAD;
    // Inverse-rotate the mouse point so hit-testing stays aligned with the rotated overlay
    if (g_shapeEditAngle != 0.0f) {
        float pivX = (L + R) / 2.f, pivY = (T + B) / 2.f;
        float rad = -g_shapeEditAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad), dx = canX - pivX, dy = canY - pivY;
        canX = pivX + dx * c - dy * s;
        canY = pivY + dx * s + dy * c;
    }
    float mx = (L + R) / 2.f, my = (T + B) / 2.f;
    struct { float x, y; int m; } h[] = {
        {L,T,4}, {mx,T,5}, {R,T,6},
        {R,my,7},
        {R,B,8}, {mx,B,9}, {L,B,10},
        {L,my,11},
    };
    for (auto& e : h)
        if (fabsf(canX - e.x) <= HT && fabsf(canY - e.y) <= HT) return e.m;
    if (canX >= L && canX <= R && canY >= T && canY <= B) return 1;
    return 0;
}

static void CommitShape()
{
    if (!g_shapeEditing) return;
    g_ptStart = g_shapeEditPt1;
    Tool savedTool = g_currentTool;
    g_currentTool  = g_shapeEditTool;
    RenderPreview(g_shapeEditPt2.x, g_shapeEditPt2.y);
    CommitPreview();
    g_currentTool    = savedTool;
    g_shapeEditing   = false;
    g_shapeEditAngle = 0.0f;
    g_shapeEditFlipH = false;
    g_shapeEditFlipV = false;
    SetStatusText(STATUS_PANE_SEL, L"");
    g_modified = true; UpdateTitleBar();
    InvalidateCanvas();
}

static void CancelShape()
{
    if (!g_shapeEditing) return;
    ClearPreview();
    g_shapeEditing   = false;
    g_shapeEditAngle = 0.0f;
    g_shapeEditFlipH = false;
    g_shapeEditFlipV = false;
    SetStatusText(STATUS_PANE_SEL, L"");
    // Silently remove the undo pushed at draw-start
    if (!g_undoStack.empty()) {
        delete g_undoStack.back();
        g_undoStack.pop_back();
    }
    InvalidateCanvas();
}

// ============================================================
//  Inline text tool helpers
// ============================================================

// Returns true when a canvas point (already in canvas coords) is inside the
// text-typing interior of the text box — i.e., not in the move-margin strip.
static bool IsTextBoxInterior(float canX, float canY)
{
    if (!g_shapeEditing || g_shapeEditTool != TOOL_TEXT) return false;
    float sL = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x);
    float sT = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y);
    float sR = (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x);
    float sB = (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y);
    // Inverse-rotate to box-local frame
    float lx = canX, ly = canY;
    if (g_shapeEditAngle != 0.0f) {
        float pivX = (sL + sR) / 2.f, pivY = (sT + sB) / 2.f;
        float rad = -g_shapeEditAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad), dx = lx - pivX, dy = ly - pivY;
        lx = pivX + dx * c - dy * s;
        ly = pivY + dx * s + dy * c;
    }
    const float M = 8.f; // move-margin width in canvas pixels
    float w = sR - sL, h = sB - sT;
    if (w < M * 3.f || h < M * 3.f) return false; // box too small: all margin
    return (lx > sL + M && lx < sR - M && ly > sT + M && ly < sB - M);
}

// Position the text caret at a canvas-coord click point inside the text box.
static void PlaceCaretAtCanvasPoint(float canX, float canY)
{
    float sL = (float)min(g_shapeEditPt1.x, g_shapeEditPt2.x);
    float sT = (float)min(g_shapeEditPt1.y, g_shapeEditPt2.y);
    float sR = (float)max(g_shapeEditPt1.x, g_shapeEditPt2.x);
    float sB = (float)max(g_shapeEditPt1.y, g_shapeEditPt2.y);
    // Inverse-rotate click to local frame
    float lx = canX, ly = canY;
    if (g_shapeEditAngle != 0.0f) {
        float pivX = (sL + sR) / 2.f, pivY = (sT + sB) / 2.f;
        float rad = -g_shapeEditAngle * (3.14159265f / 180.0f);
        float c = cosf(rad), s = sinf(rad), dx = lx - pivX, dy = ly - pivY;
        lx = pivX + dx * c - dy * s;
        ly = pivY + dx * s + dy * c;
    }
    float relX = lx - sL, relY = ly - sT;

    // Build a 1×1 bitmap Graphics for measuring at canvas resolution
    Bitmap tmpBmp(1, 1, PixelFormat32bppARGB);
    Graphics tmpGfx(&tmpBmp);
    tmpGfx.SetTextRenderingHint(TextRenderingHintAntiAlias);
    int gdipStyle = FontStyleRegular;
    if (g_textFontStyle & 1) gdipStyle |= FontStyleBold;
    if (g_textFontStyle & 2) gdipStyle |= FontStyleItalic;
    if (g_textFontStyle & 4) gdipStyle |= FontStyleUnderline;
    FontFamily ff(g_textFontName);
    FontFamily* pFF = (ff.GetLastStatus() == Ok) ? &ff : nullptr;
    FontFamily ffFb(L"Arial"); if (!pFF) pFF = &ffFb;
    Font font(pFF, (REAL)g_textFontSize, gdipStyle, UnitPoint);
    StringFormat sf; sf.SetAlignment(StringAlignmentNear); sf.SetLineAlignment(StringAlignmentNear);

    // Line height
    RectF lhB; tmpGfx.MeasureString(L"Ag", 2, &font, PointF(0.f,0.f), &sf, &lhB);
    float lineH = (lhB.Height > 0.f) ? lhB.Height : (float)g_textFontSize;

    // Which line was clicked?
    int targetLine = max(0, (int)(relY / lineH));

    // Build line start-index table
    std::vector<int> lineStarts;
    lineStarts.push_back(0);
    for (int i = 0; i < (int)g_textStr.size(); ++i)
        if (g_textStr[i] == L'\n') lineStarts.push_back(i + 1);

    targetLine = min(targetLine, (int)lineStarts.size() - 1);
    int lStart = lineStarts[targetLine];
    // Length of this line (up to next \n or end of string)
    int lEnd = (int)g_textStr.size();
    for (int i = lStart; i < (int)g_textStr.size(); ++i)
        if (g_textStr[i] == L'\n') { lEnd = i; break; }
    int lLen = lEnd - lStart;

    // Binary search for closest character boundary within line
    const wchar_t* lp = g_textStr.c_str() + lStart;
    int lo = 0, hi = lLen;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        RectF cb; tmpGfx.MeasureString(lp, mid, &font, PointF(0.f,0.f), &sf, &cb);
        if (cb.Width < relX) lo = mid; else hi = mid;
    }
    // Pick whichever boundary is closer
    RectF cbLo, cbHi;
    tmpGfx.MeasureString(lp, lo, &font, PointF(0.f,0.f), &sf, &cbLo);
    tmpGfx.MeasureString(lp, hi, &font, PointF(0.f,0.f), &sf, &cbHi);
    int bestInLine = (fabsf(relX - cbLo.Width) <= fabsf(relX - cbHi.Width)) ? lo : hi;

    g_textCursorIdx = min(lStart + bestInLine, (int)g_textStr.size());
    g_textCaretVis  = true;
    InvalidateCanvas();
}

// ============================================================
//  Inline text tool
// ============================================================
static void CommitTextInline()
{
    if (!g_textInline) return;
    g_textInline = false;
    if (g_shapeEditing) {
        if (!g_textStr.empty())
            CommitShape();   // renders text via RenderShape → CommitPreview
        else
            CancelShape();   // empty box → discard undo
    }
    g_textStr.clear();
    g_textCursorIdx = 0;
    if (g_textBlinkTimer && g_hwndCanvas) {
        KillTimer(g_hwndCanvas, g_textBlinkTimer);
        g_textBlinkTimer = 0;
    }
    // CommitShape / CancelShape already called InvalidateCanvas()
}

static void CancelTextInline()
{
    if (!g_textInline) return;
    g_textInline = false;
    if (g_shapeEditing) CancelShape();
    g_textStr.clear();
    g_textCursorIdx = 0;
    if (g_textBlinkTimer && g_hwndCanvas) {
        KillTimer(g_hwndCanvas, g_textBlinkTimer);
        g_textBlinkTimer = 0;
    }
    // CancelShape already called InvalidateCanvas()
}

static int CALLBACK FontFamilyEnumProc(const LOGFONT* lf, const TEXTMETRIC*,
                                        DWORD, LPARAM lp)
{
    HWND hCombo = (HWND)(DWORD_PTR)lp;
    // Deduplicate: some fonts enumerate multiple times for different charsets
    if (SendMessage(hCombo, CB_FINDSTRINGEXACT, (WPARAM)-1,
                    (LPARAM)lf->lfFaceName) == CB_ERR)
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
    return 1;
}

static void PopulateFontCombo(HWND hCombo)
{
    HDC hdc = GetDC(NULL);
    LOGFONT lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesEx(hdc, &lf, FontFamilyEnumProc, (LPARAM)(DWORD_PTR)hCombo, 0);
    ReleaseDC(NULL, hdc);
    // Pre-select the current font
    int idx = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT,
                               (WPARAM)-1, (LPARAM)g_textFontName);
    SendMessage(hCombo, CB_SETCURSEL, (idx != CB_ERR) ? idx : 0, 0);
}

// ============================================================
//  Slide panel window procedure
// ============================================================

// Thumbnails are built one per WM_TIMER tick rather than inside WM_PAINT.
// WM_TIMER is only delivered once no repaints are pending, so the canvas and
// the rest of the window draw first instead of staying blank while a strip
// full of large images is decoded.
static const UINT_PTR FOLDER_THUMB_TIMER = 1;

// Index of the next visible cell that still needs a thumbnail (the current
// file first), or -1 if every visible cell is done.
static int NextVisibleMissingThumb(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    auto missing = [](int i) {
        if (i < 0) return false;   // wrap gap
        auto& e = g_folderEntries[i];
        return (!e.thumb || e.stale) && !e.failed;
    };
    int found = -1;
    ForEachVisibleFolderCell(rc.right, [&](int i, int) {
        if (missing(i) && (found < 0 || i == g_folderCurrentIdx)) found = i;
    });
    return found;
}

// ─── Folder strip: ⋯ button, item menu and rename ───────────────────────────

static const char* kMoreHorizD =   // Fluent "More horizontal" 24 regular (_dev/more_horizontal.svg)
    "M7.75 12a1.75 1.75 0 1 1-3.5 0 1.75 1.75 0 0 1 3.5 0Z"
    "M13.75 12a1.75 1.75 0 1 1-3.5 0 1.75 1.75 0 0 1 3.5 0Z"
    "M18 13.75a1.75 1.75 0 1 0 0-3.5 1.75 1.75 0 0 0 0 3.5Z";

static const char* kCopyPathD =   // copy icon with a folder outline badge (_dev/copy_path.svg)
    "M20 11V4.25A2.25 2.25 0 0 0 17.75 2H8.75A2.25 2.25 0 0 0 6.5 4.25V17.25A2.25 2.25 0 0 0 8.75 19.5H10.5V18"
    "H8.75A0.75 0.75 0 0 1 8 17.25V4.25A0.75 0.75 0 0 1 8.75 3.5H17.75A0.75 0.75 0 0 1 18.5 4.25V11Z"
    "M5.5 4.63V17.25A3.25 3.25 0 0 0 8.75 20.5H10.5V22H8.75A4.75 4.75 0 0 1 4 17.25V6.75C4 5.77 4.627 4.935 5.5 4.63Z"
    "M13.25 12.5H15.4L16.6 14H21.75A1.25 1.25 0 0 1 23 15.25V20.75A1.25 1.25 0 0 1 21.75 22H13.25"
    "A1.25 1.25 0 0 1 12 20.75V13.75A1.25 1.25 0 0 1 13.25 12.5ZM13.5 15.5V20.5H21.5V15.5Z";

static const char* kPropertiesD =   // Fluent "Properties" style list icon (_dev/properties.svg)
    "M4.25 4C3.00736 4 2 5.00736 2 6.25V8.75C2 9.99264 3.00736 11 4.25 11H6.75C7.99264 11 9 9.99264 9 8.75"
    "V6.25C9 5.00736 7.99264 4 6.75 4H4.25Z"
    "M11.25 5C10.8358 5 10.5 5.33579 10.5 5.75C10.5 6.16421 10.8358 6.5 11.25 6.5H21.25"
    "C21.6642 6.5 22 6.16421 22 5.75C22 5.33579 21.6642 5 21.25 5H11.25Z"
    "M11.25 8C10.8358 8 10.5 8.33579 10.5 8.75C10.5 9.16421 10.8358 9.5 11.25 9.5H18.25"
    "C18.6642 9.5 19 9.16421 19 8.75C19 8.33579 18.6642 8 18.25 8H11.25Z"
    "M4.25 13C3.00736 13 2 14.0074 2 15.25V17.75C2 18.9926 3.00736 20 4.25 20H6.75"
    "C7.99264 20 9 18.9926 9 17.75V15.25C9 14.0074 7.99264 13 6.75 13H4.25Z"
    "M11.25 14C10.8358 14 10.5 14.3358 10.5 14.75C10.5 15.1642 10.8358 15.5 11.25 15.5H21.25"
    "C21.6642 15.5 22 15.1642 22 14.75C22 14.3358 21.6642 14 21.25 14H11.25Z"
    "M11.25 17C10.8358 17 10.5 17.3358 10.5 17.75C10.5 18.1642 10.8358 18.5 11.25 18.5H18.25"
    "C18.6642 18.5 19 18.1642 19 17.75C19 17.3358 18.6642 17 18.25 17H11.25Z";

static bool g_folderResizing    = false;   // dragging the strip's top edge
static int  g_folderResizeY0    = 0;       // screen y and height where the drag began
static int  g_folderResizeH0    = 0;

// Thumbnails are remade when the strip height calls for a clearly different size
static void UpdateFolderThumbSize(int panelH)
{
    int want = FolderThumbSizeFor(panelH);
    if (want <= g_folderThumbMax * 115 / 100 && want >= g_folderThumbMax * 60 / 100) return;
    g_folderThumbMax = want;
    for (auto& e : g_folderEntries) if (e.thumb) e.stale = true;
}

static int  g_folderMouseX  = -1;      // strip client x of the mouse, -1 when outside
static bool g_folderMoreHot = false;   // mouse is over a ⋯ button
static HWND g_hwndFolderRename = NULL; // in-place rename box
static std::wstring g_folderRenamePath;

// The ⋯ button in the top-right corner of the cell whose left edge is cx
static RECT FolderMoreBtnRect(int cx) { return { cx + g_folderCellW - 30, 5, cx + g_folderCellW - 6, 23 }; }

// The visible cell under client x: its file index (-1 if none) and left edge
static int FolderCellAt(int x, int panelW, int* pcx)
{
    int hit = -1;
    ForEachVisibleFolderCell(panelW, [&](int i, int cx) {
        if (i >= 0 && x >= cx && x < cx + g_folderCellW) { hit = i; *pcx = cx; }
    });
    return hit;
}

// Left edge of a visible cell showing file idx (the one nearest the centre), or INT_MIN
static int FolderCellLeft(int idx, int panelW)
{
    int best = INT_MIN;
    ForEachVisibleFolderCell(panelW, [&](int i, int cx) {
        if (i == idx && (best == INT_MIN || abs(cx - panelW / 2) < abs(best - panelW / 2))) best = cx;
    });
    return best;
}

// Full-size bitmap of an image file for the clipboard, or nullptr
static Bitmap* LoadBitmapForCopy(const wchar_t* path)
{
    if (IsSvgPath(path)) return LoadSvgToBitmap(path);
    const wchar_t* dot = wcsrchr(path, L'.');
    if (dot && _wcsicmp(dot + 1, L"webp") == 0) return LoadWebP(path);
    Bitmap* bmp = Bitmap::FromFile(path);
    if (bmp && bmp->GetLastStatus() != Ok) { delete bmp; bmp = nullptr; }
    ApplyExifOrientation(bmp, path);
    if (!bmp) bmp = LoadBitmapViaWic(path);   // HEIC, AVIF and others GDI+ can't read
    return bmp;
}

static void CopyImageFileToClipboard(const std::wstring& path)
{
    bool isCur = _wcsicmp(path.c_str(), g_currentFile) == 0;
    Bitmap* bmp = isCur ? g_pCanvas : LoadBitmapForCopy(path.c_str());
    if (!bmp) { MessageBox(g_hwndMain, L"Could not read this image.", APP_TITLE, MB_ICONERROR); return; }
    if (OpenClipboard(g_hwndMain)) {
        EmptyClipboard();
        ClipboardPutBitmap(bmp);
        CloseClipboard();
    }
    if (!isCur) delete bmp;
}

static void CopyTextToClipboard(const std::wstring& text)
{
    if (!OpenClipboard(g_hwndMain)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* p = GlobalLock(h)) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
        } else GlobalFree(h);
    }
    CloseClipboard();
}

static void EndFolderRename(bool apply)
{
    HWND edit = g_hwndFolderRename;
    if (!edit) return;
    g_hwndFolderRename = NULL;   // first: destroying the edit sends WM_KILLFOCUS
    wchar_t name[MAX_PATH] = {};
    GetWindowTextW(edit, name, MAX_PATH);
    DestroyWindow(edit);
    if (g_hwndFolderBrowser) SetFocus(g_hwndFolderBrowser);

    std::wstring oldPath = g_folderRenamePath;
    const wchar_t* oldName = PathFindFileNameW(oldPath.c_str());
    StrTrimW(name, L" ");
    if (!apply || !name[0] || wcscmp(name, oldName) == 0) return;
    if (wcspbrk(name, L"\\/:*?\"<>|")) {
        MessageBox(g_hwndMain, L"A file name can't contain any of these characters:\n\\ / : * ? \" < > |",
                   APP_TITLE, MB_ICONWARNING);
        return;
    }
    wchar_t newPath[MAX_PATH];
    StringCchCopy(newPath, MAX_PATH, oldPath.c_str());
    PathRemoveFileSpecW(newPath);
    if (!PathAppendW(newPath, name)) return;
    if (!MoveFileW(oldPath.c_str(), newPath)) {
        MessageBox(g_hwndMain, GetLastError() == ERROR_ALREADY_EXISTS
                   ? L"A file with that name already exists." : L"Could not rename this file.",
                   APP_TITLE, MB_ICONERROR);
        return;
    }
    if (_wcsicmp(oldPath.c_str(), g_currentFile) == 0) {
        StringCchCopy(g_currentFile, MAX_PATH, newPath);
        UpdateTitleBar();
    }
    BuildFolderList();
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
}

static LRESULT CALLBACK FolderRenameSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) { EndFolderRename(true);  return 0; }
        if (wParam == VK_ESCAPE) { EndFolderRename(false); return 0; }
        break;
    case WM_CHAR:
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) return 0;   // no beep
        break;
    case WM_KILLFOCUS:
        // Clicking elsewhere keeps the new name, as in Explorer
        if (hwnd == g_hwndFolderRename) PostMessage(GetParent(hwnd), WM_APP, 0, 0);
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, FolderRenameSubclassProc, 0);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Opens an edit box over the file name label of cell cx
static void BeginFolderRename(HWND strip, int idx, int cx)
{
    EndFolderRename(true);
    if (idx < 0 || idx >= (int)g_folderEntries.size() || cx == INT_MIN) return;
    g_folderRenamePath = g_folderEntries[idx].path;
    RECT rc; GetClientRect(strip, &rc);
    g_hwndFolderRename = CreateWindowExW(0, L"EDIT", PathFindFileNameW(g_folderRenamePath.c_str()),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        cx + 1, max(0, (int)rc.bottom - 21), g_folderCellW - 2, min(20, (int)rc.bottom), strip, NULL, GetModuleHandle(NULL), NULL);
    if (!g_hwndFolderRename) return;
    SendMessage(g_hwndFolderRename, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
    SetWindowSubclass(g_hwndFolderRename, FolderRenameSubclassProc, 0, 0);
    // Select the name without its extension, as Explorer does
    const wchar_t* name = PathFindFileNameW(g_folderRenamePath.c_str());
    const wchar_t* dot  = wcsrchr(name, L'.');
    SendMessage(g_hwndFolderRename, EM_SETSEL, 0, dot ? (LPARAM)(dot - name) : -1);
    SetFocus(g_hwndFolderRename);
}

enum { FCM_INKSCAPE = 100 };   // SVG only, added when Inkscape is installed
enum { FCM_ROTATE_RIGHT = 1, FCM_ROTATE_LEFT, FCM_COPY_IMAGE, FCM_COPY_PATH, FCM_SHOW, FCM_RENAME, FCM_DELETE, FCM_PROPERTIES };

// Menu for one strip thumbnail, opened from its ⋯ button or a right-click
static void ShowFolderItemMenu(HWND strip, int idx, int cx, POINT screenPt, UINT align)
{
    if (idx < 0 || idx >= (int)g_folderEntries.size()) return;
    std::wstring path = g_folderEntries[idx].path;   // the list can be rebuilt below

    const int ICO_SZ = 16;
    COLORREF crBg   = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crText = g_darkMode ? RGB(220, 220, 220) : GetSysColor(COLOR_MENUTEXT);
    Color menuBg (255, GetRValue(crBg),   GetGValue(crBg),   GetBValue(crBg));
    Color iconClr(255, GetRValue(crText), GetGValue(crText), GetBValue(crText));

    struct Item { UINT id; const wchar_t* text; const char* icon; float coordSz; };
    static const Item kItems[] = {
        { FCM_ROTATE_RIGHT, L"Rotate &right",     kImgRotateRightD, 24.0f },
        { FCM_ROTATE_LEFT,  L"Rotate &left",      kImgRotateLeftD,  24.0f },
        { 0 },
        { FCM_COPY_IMAGE,   L"&Copy image",       kEditCopyD,       24.0f },
        { FCM_COPY_PATH,    L"Copy &path",        kCopyPathD,       24.0f },
        { FCM_SHOW,         L"Show in &Explorer", kFileOpenD,       24.0f },
        { 0 },
        { FCM_RENAME,       L"Re&name\tF2",       kPencilSvgD,      20.0f },
        { FCM_DELETE,       L"&Delete\tDel",      kFileDeleteD,     24.0f },
        { 0 },
        { FCM_PROPERTIES,   L"Pr&operties",       kPropertiesD,     24.0f },
    };
    HMENU hm = CreatePopupMenu();
    std::vector<HBITMAP> icons;
    for (auto& it : kItems) {
        if (!it.id) { AppendMenuW(hm, MF_SEPARATOR, 0, NULL); continue; }
        AppendMenuW(hm, MF_STRING, it.id, it.text);
        if (it.icon) {
            HBITMAP hb = MakeBitmapFromSvg(it.icon, ICO_SZ, iconClr, it.coordSz, menuBg);
            icons.push_back(hb);
            MENUITEMINFOW mii = { sizeof(mii) };
            mii.fMask    = MIIM_BITMAP;
            mii.hbmpItem = hb;
            SetMenuItemInfoW(hm, it.id, FALSE, &mii);
        }
    }
    if (IsSvgPath(path.c_str())) {
        std::wstring inkscape = FindInkscape();
        if (!inkscape.empty()) {
            HBITMAP hb = MakeExeIconBitmap(inkscape.c_str(), ICO_SZ);
            icons.push_back(hb);
            MENUITEMINFOW mii = { sizeof(mii) };
            mii.fMask      = MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;
            mii.fType      = MFT_STRING;
            mii.wID        = FCM_INKSCAPE;
            mii.dwTypeData = const_cast<LPWSTR>(L"Open in &Inkscape");
            mii.hbmpItem   = hb;
            int pos = 0;   // just after Show in Explorer
            for (int n = GetMenuItemCount(hm); pos < n && GetMenuItemID(hm, pos) != FCM_SHOW; pos++) {}
            InsertMenuItemW(hm, pos + 1, TRUE, &mii);
        }
    }
    if (!CanRotateFileInPlace(path.c_str())) {
        EnableMenuItem(hm, FCM_ROTATE_RIGHT, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(hm, FCM_ROTATE_LEFT,  MF_BYCOMMAND | MF_GRAYED);
    }

    int cmd = (int)TrackPopupMenu(hm, align | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                  screenPt.x, screenPt.y, 0, g_hwndMain, NULL);
    DestroyMenu(hm);
    for (HBITMAP hb : icons) DeleteObject(hb);

    switch (cmd) {
    case FCM_ROTATE_RIGHT: RotateFolderFile(path, true);  break;
    case FCM_ROTATE_LEFT:  RotateFolderFile(path, false); break;
    case FCM_COPY_IMAGE: CopyImageFileToClipboard(path); break;
    case FCM_COPY_PATH:  CopyTextToClipboard(path); break;
    case FCM_SHOW: {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(g_hwndMain, NULL, L"explorer.exe", args.c_str(), NULL, SW_SHOWNORMAL);
        break;
    }
    case FCM_INKSCAPE:   OpenInInkscape(path); break;
    case FCM_RENAME:     BeginFolderRename(strip, idx, cx); break;
    case FCM_DELETE:     DeleteFolderFile(path); break;
    case FCM_PROPERTIES: SHObjectProperties(g_hwndMain, SHOP_FILEPATH, path.c_str(), NULL); break;
    }
}

// Draws the ⋯ button for the cell whose left edge is cx
static void DrawFolderMoreBtn(HDC hdc, int cx, bool hot)
{
    RECT r = FolderMoreBtnRect(cx);
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    // Dark translucent pill so the dots show on light and dark thumbnails alike
    GraphicsPath pill;
    float x = (float)r.left, y = (float)r.top, w = (float)(r.right - r.left), h = (float)(r.bottom - r.top), d = h;
    pill.AddArc(x, y, d, d, 90, 180);
    pill.AddArc(x + w - d, y, d, d, 270, 180);
    pill.CloseFigure();
    SolidBrush bg(hot ? Color(225, 30, 30, 30) : Color(150, 30, 30, 30));
    g.FillPath(&bg, &pill);
    GraphicsPath dots(FillModeWinding);
    AddSvgPath(dots, kMoreHorizD, 0.75f);   // 24-unit icon drawn at 18 px
    g.TranslateTransform(x + (w - 18.0f) / 2, y + (h - 18.0f) / 2);
    SolidBrush fg(Color(255, 255, 255, 255));
    g.FillPath(&fg, &dots);
}

static LRESULT CALLBACK FolderBrowserWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ForwardWheelToHovered(hwnd, msg, wParam, lParam)) return 0;
    switch (msg) {
    case WM_TIMER:
        if (wParam == FOLDER_THUMB_TIMER) {
            // Thumbnails decode on the UI thread, which would stall the canvas
            // spinner; pause them until the main image is in (its install
            // repaints the strip, which restarts this timer).
            int i = g_imageLoading ? -1 : NextVisibleMissingThumb(hwnd);
            if (i < 0) {
                KillTimer(hwnd, FOLDER_THUMB_TIMER);
            } else {
                auto& e = g_folderEntries[i];
                // A stale thumb stays on screen (scaled) until its replacement is ready
                if (Bitmap* t = MakeFolderThumb(e.path.c_str())) { delete e.thumb; e.thumb = t; }
                else if (!e.thumb) e.failed = true;
                e.stale = false;
                // A wrapped strip may show the file more than once
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int panelW = rc.right, panelH = rc.bottom;

        // Double-buffer to eliminate flicker
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, panelW, panelH);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
        HDC hdc0 = hdc; hdc = hdcMem;  // paint into back buffer

        COLORREF bgClr = g_darkMode ? RGB(28, 28, 28) : RGB(235, 235, 235);
        HBRUSH hbBg = CreateSolidBrush(bgClr);
        FillRect(hdc, &rc, hbBg);
        DeleteObject(hbBg);

        // Top separator line
        HPEN hSep = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(60,60,60) : RGB(180,180,180));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hSep);
        MoveToEx(hdc, 0, 0, nullptr); LineTo(hdc, panelW, 0);
        SelectObject(hdc, hOldPen); DeleteObject(hSep);

        int n = (int)g_folderEntries.size();
        if (n == 0) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_darkMode ? RGB(140,140,140) : RGB(110,110,110));
            RECT msg2 = rc;
            std::wstring empty = L"No images in this directory";
            if (!g_folderFilter.empty() && g_folderTotalCount > 0)
                empty = L"No file names contain \"" + g_folderFilter + L"\"";
            DrawText(hdc, empty.c_str(), -1, &msg2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            BitBlt(hdc0, 0, 0, panelW, panelH, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hbmOld); DeleteObject(hbmMem); DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // Visible cells without a thumbnail yet are filled in by FOLDER_THUMB_TIMER
        if (!g_imageLoading && NextVisibleMissingThumb(hwnd) >= 0)
            SetTimer(hwnd, FOLDER_THUMB_TIMER, USER_TIMER_MINIMUM, NULL);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        const int  textH    = FOLDER_LABEL_H;
        const int  imgAreaH = FolderImgAreaH(panelH);   // top+bottom pad inside image area
        const int  boxW = g_folderCellW - 12, boxH = imgAreaH - 4;

        ForEachVisibleFolderCell(panelW, [&](int i, int cx) {
            if (i < 0) {
                // Wrap gap between the last file and the first
                HBRUSH hbGap = CreateSolidBrush(g_darkMode ? RGB(12, 12, 12) : RGB(190, 190, 190));
                RECT gr = { cx, 1, cx + FOLDER_WRAP_GAP_W, panelH };
                FillRect(hdc, &gr, hbGap);
                DeleteObject(hbGap);
                return;
            }
            auto& e = g_folderEntries[i];
            bool isCur = (i == g_folderCurrentIdx);

            // Selection background
            if (isCur) {
                COLORREF selClr = g_darkMode ? RGB(0, 84, 153) : RGB(0, 120, 215);
                HBRUSH hbSel = CreateSolidBrush(selClr);
                RECT sr = { cx, 1, cx + g_folderCellW, panelH };
                FillRect(hdc, &sr, hbSel);
                DeleteObject(hbSel);
            }

            // Thumbnail image, fitted to the cell: a thumb made for another
            // strip height is drawn scaled until its replacement is ready
            if (e.thumb) {
                int tw = (int)e.thumb->GetWidth(), th2 = (int)e.thumb->GetHeight();
                float sc = min((float)boxW / tw, (float)boxH / th2);
                if (!e.stale && sc > 1.0f) sc = 1.0f;   // small images stay actual size
                int dw = max(1, (int)(tw * sc)), dh = max(1, (int)(th2 * sc));
                int ix = cx + (g_folderCellW - dw) / 2;
                int iy = 2 + (imgAreaH - dh) / 2;
                Graphics gfx(hdc);
                gfx.SetInterpolationMode(InterpolationModeBilinear);
                gfx.SetPixelOffsetMode(PixelOffsetModeHalf);
                gfx.DrawImage(e.thumb, ix, iy, dw, dh);
            } else if (e.failed) {
                // Draw placeholder X
                COLORREF xClr = g_darkMode ? RGB(80,80,80) : RGB(180,180,180);
                HPEN hxp = CreatePen(PS_SOLID, 1, xClr);
                HPEN hxOld = (HPEN)SelectObject(hdc, hxp);
                HBRUSH hxOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                int bx1=cx+4, by1=2, bx2=cx+g_folderCellW-4, by2=imgAreaH;
                Rectangle(hdc, bx1, by1, bx2, by2);
                MoveToEx(hdc, bx1, by1, nullptr); LineTo(hdc, bx2, by2);
                MoveToEx(hdc, bx2, by1, nullptr); LineTo(hdc, bx1, by2);
                SelectObject(hdc, hxOld); SelectObject(hdc, hxOldBr);
                DeleteObject(hxp);
            }

            // ⋯ menu button on the open file and the cell under the mouse
            bool hover = g_folderMouseX >= cx && g_folderMouseX < cx + g_folderCellW;
            if (isCur || hover) DrawFolderMoreBtn(hdc, cx, hover && g_folderMoreHot);

            // Filename label
            const wchar_t* fname = wcsrchr(e.path.c_str(), L'\\');
            fname = fname ? fname + 1 : e.path.c_str();
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isCur ? RGB(255,255,255)
                : (g_darkMode ? RGB(200,200,200) : RGB(50,50,50)));
            RECT tr = { cx+2, panelH-textH-2, cx+g_folderCellW-2, panelH-2 };
            DrawText(hdc, fname, -1, &tr,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // Right-edge separator
            if (!isCur) {
                HPEN hp = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(55,55,55) : RGB(200,200,200));
                HPEN hp0 = (HPEN)SelectObject(hdc, hp);
                MoveToEx(hdc, cx+g_folderCellW-1, 1, nullptr);
                LineTo  (hdc, cx+g_folderCellW-1, panelH);
                SelectObject(hdc, hp0); DeleteObject(hp);
            }
        });

        // Focus rectangle
        if (GetFocus() == hwnd) {
            RECT fr = { 1, 1, panelW-1, panelH-1 };
            DrawFocusRect(hdc, &fr);
        }

        SelectObject(hdc, hOldFont);
        BitBlt(hdc0, 0, 0, panelW, panelH, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hbmOld); DeleteObject(hbmMem); DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        EndFolderRename(true);
        SetFocus(hwnd);
        RECT rc; GetClientRect(hwnd, &rc);
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (pt.y < FOLDER_GRIP_H) {
            // Top edge: drag to resize the strip
            POINT sp = pt; ClientToScreen(hwnd, &sp);
            g_folderResizing = true;
            g_folderResizeY0 = sp.y;
            g_folderResizeH0 = rc.bottom;
            SetCapture(hwnd);
            return 0;
        }
        int cx = 0, idx = FolderCellAt(pt.x, rc.right, &cx);
        if (idx < 0) return 0;
        RECT br = FolderMoreBtnRect(cx);
        if (PtInRect(&br, pt)) {
            // Menu drops down from the button, right edges aligned
            POINT sp = { br.right, br.bottom + 2 };
            ClientToScreen(hwnd, &sp);
            ShowFolderItemMenu(hwnd, idx, cx, sp, TPM_RIGHTALIGN);
        } else {
            NavigateToFile(idx);
        }
        return 0;
    }

    case WM_CONTEXTMENU: {
        EndFolderRename(true);
        SetFocus(hwnd);
        RECT rc; GetClientRect(hwnd, &rc);
        int idx, cx = 0;
        POINT sp;
        if (lParam == (LPARAM)-1) {
            // Keyboard (menu key / Shift+F10): the open file, from its ⋯ button
            idx = g_folderCurrentIdx;
            cx  = FolderCellLeft(idx, rc.right);
            if (idx < 0 || cx == INT_MIN) return 0;
            RECT br = FolderMoreBtnRect(cx);
            sp = { br.right, br.bottom + 2 };
            ClientToScreen(hwnd, &sp);
            ShowFolderItemMenu(hwnd, idx, cx, sp, TPM_RIGHTALIGN);
        } else {
            sp = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            POINT cp = sp; ScreenToClient(hwnd, &cp);
            idx = FolderCellAt(cp.x, rc.right, &cx);
            if (idx >= 0) ShowFolderItemMenu(hwnd, idx, cx, sp, TPM_LEFTALIGN);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_folderResizing) {
            POINT sp; GetCursorPos(&sp);
            int h = ClampFolderBrowserH(g_folderResizeH0 + (g_folderResizeY0 - sp.y));
            if (h != g_folderBrowserH) {
                g_folderBrowserH = h;
                LayoutCanvasAndFolderBrowser(g_hwndMain);
                UpdateWindow(g_hwndMain);
            }
            return 0;
        }
        RECT rc; GetClientRect(hwnd, &rc);
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int cx = 0, idx = FolderCellAt(pt.x, rc.right, &cx);
        RECT br = FolderMoreBtnRect(cx);
        bool hot = idx >= 0 && PtInRect(&br, pt);
        int oldCx = 0, oldIdx = FolderCellAt(g_folderMouseX, rc.right, &oldCx);
        if (g_folderMouseX < 0 || idx != oldIdx || oldCx != cx || hot != g_folderMoreHot)
            InvalidateRect(hwnd, NULL, FALSE);
        if (g_folderMouseX < 0) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        g_folderMouseX  = pt.x;
        g_folderMoreHot = hot;
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_folderResizing) ReleaseCapture();   // finishes in WM_CAPTURECHANGED
        return 0;

    case WM_CAPTURECHANGED:
        if (g_folderResizing) {
            g_folderResizing = false;
            RECT rc; GetClientRect(hwnd, &rc);
            UpdateFolderThumbSize(rc.bottom);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            if (g_folderResizing || pt.y < FOLDER_GRIP_H) {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE;
            }
        }
        break;

    case WM_MOUSELEAVE:
        g_folderMouseX  = -1;
        g_folderMoreHot = false;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_APP:   // the rename box lost focus
        EndFolderRename(true);
        return 0;

    case WM_CTLCOLOREDIT:
        if (g_darkMode && (HWND)lParam == g_hwndFolderRename) {
            static HBRUSH hbr = CreateSolidBrush(RGB(45, 45, 45));
            SetTextColor((HDC)wParam, RGB(230, 230, 230));
            SetBkColor((HDC)wParam, RGB(45, 45, 45));
            return (LRESULT)hbr;
        }
        break;

    case WM_KEYDOWN: {
        int n = (int)g_folderEntries.size();
        // If the highlight was lost, find the open file again rather than
        // leaving the keys dead until a thumbnail is clicked
        int cur = g_folderCurrentIdx;
        for (int i = 0; cur < 0 && i < n; i++)
            if (_wcsicmp(g_folderEntries[i].path.c_str(), g_currentFile) == 0) cur = i;
        if (n > 0) {
            if (wParam == VK_HOME) NavigateToFile(0);
            if (wParam == VK_END)  NavigateToFile(n - 1);
            // Wraps from the last file to the first and back
            if (wParam == VK_LEFT)  NavigateToFile(cur < 0 ? n - 1 : (cur + n - 1) % n);
            if (wParam == VK_RIGHT) NavigateToFile(cur < 0 ? 0 : (cur + 1) % n);
        }
        // Delete acts on the highlighted file, which can differ from the open
        // one when a file wouldn't open
        if (wParam == VK_DELETE && cur >= 0 && cur < n) DeleteFolderFile(g_folderEntries[cur].path);
        if (wParam == VK_ESCAPE) CloseFolderSearch();
        if (wParam == VK_F2) {
            RECT rc; GetClientRect(hwnd, &rc);
            BeginFolderRename(hwnd, g_folderCurrentIdx, FolderCellLeft(g_folderCurrentIdx, rc.right));
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        EndFolderRename(true);   // the box would be left behind as cells scroll
        RECT rc; GetClientRect(hwnd, &rc);
        if (!FolderStripWraps(rc.right)) return 0;   // everything already visible
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        // Vertical scroll maps naturally to horizontal panning in the strip;
        // horizontal scroll (WM_MOUSEHWHEEL) keeps the same direction convention.
        int sign = (msg == WM_MOUSEHWHEEL) ? 1 : -1;
        g_folderScrollX = WrapFolderScroll(g_folderScrollX + sign * delta * g_folderCellW / WHEEL_DELTA);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_SIZE:
        EndFolderRename(true);
        g_folderCellW = FolderCellWFor(HIWORD(lParam));
        if (!g_folderResizing) UpdateFolderThumbSize(HIWORD(lParam));   // else on release
        CenterFolderBrowserOnCurrent();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ─── Folder browser search (Ctrl+F) ──────────────────────────
// A panel at the left end of the strip with a text box; the strip lists
// only the images whose file names contain the text.

// Box position in a search panel w × h; the ✕ that closes it sits to its right
static RECT FolderSearchEditRect(int w, int h)
{
    int y = h >= 64 ? 10 : max(2, (h - 22) / 2);
    return { 8, y, w - 34, y + 22 };
}
static RECT FolderSearchCloseRect(int w, int h)
{
    RECT e = FolderSearchEditRect(w, h);
    return { w - 30, e.top, w - 8, e.bottom };
}

static void OpenFolderSearch()
{
    if (!g_showFolderBrowser) ToggleFolderBrowser();
    if (!g_showFolderBrowser || !g_hwndFolderSearch) return;
    if (!g_folderSearchOpen) {
        g_folderSearchOpen = true;
        LayoutCanvasAndFolderBrowser(g_hwndMain);   // placed before it is shown
        ShowWindow(g_hwndFolderSearch, SW_SHOW);
    }
    SetFocus(g_hwndFolderSearchEdit);
    SendMessage(g_hwndFolderSearchEdit, EM_SETSEL, 0, -1);
}

static void CloseFolderSearch()
{
    if (!g_folderSearchOpen) return;
    g_folderSearchOpen = false;   // first: clearing the box below must not filter again
    g_folderFilter.clear();
    SetWindowTextW(g_hwndFolderSearchEdit, L"");
    if (g_hwndFolderBrowser) SetFocus(g_hwndFolderBrowser);   // before its box is hidden
    ShowWindow(g_hwndFolderSearch, SW_HIDE);
    LayoutCanvasAndFolderBrowser(g_hwndMain);
    BuildFolderList();
    if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
}

static LRESULT CALLBACK FolderSearchEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                     UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { CloseFolderSearch(); return 0; }
        // Enter opens the first match (unless a match is already open) and
        // moves to the strip, where the arrow keys step through the matches
        if (wParam == VK_RETURN || wParam == VK_DOWN || wParam == VK_TAB) {
            if (wParam == VK_RETURN && g_folderCurrentIdx < 0 && !g_folderEntries.empty()) NavigateToFile(0);
            if (g_hwndFolderBrowser) SetFocus(g_hwndFolderBrowser);
            return 0;
        }
        break;
    case WM_CHAR:
        if (wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_TAB) return 0;   // no beep
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, FolderSearchEditSubclassProc, 0);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK FolderSearchWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_hwndFolderSearchEdit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
        if (g_hwndFolderSearchEdit) {
            SendMessage(g_hwndFolderSearchEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
            SendMessage(g_hwndFolderSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search file names");
            SetWindowSubclass(g_hwndFolderSearchEdit, FolderSearchEditSubclassProc, 0, 0);
        }
        return 0;

    case WM_SIZE: {
        RECT e = FolderSearchEditRect(LOWORD(lParam), HIWORD(lParam));
        if (g_hwndFolderSearchEdit)
            SetWindowPos(g_hwndFolderSearchEdit, NULL, e.left, e.top, e.right - e.left, e.bottom - e.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_COMMAND:
        if ((HWND)lParam == g_hwndFolderSearchEdit && HIWORD(wParam) == EN_CHANGE && g_folderSearchOpen) {
            wchar_t text[MAX_PATH] = {};
            GetWindowTextW(g_hwndFolderSearchEdit, text, MAX_PATH);
            g_folderFilter = text;
            BuildFolderList();
            if (g_hwndFolderBrowser) InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
            InvalidateRect(hwnd, NULL, FALSE);   // the match count
        }
        return 0;

    case WM_CTLCOLOREDIT:
        if (g_darkMode) {
            static HBRUSH hbr = CreateSolidBrush(RGB(45, 45, 45));
            SetTextColor((HDC)wParam, RGB(230, 230, 230));
            SetBkColor((HDC)wParam, RGB(45, 45, 45));
            return (LRESULT)hbr;
        }
        break;

    case WM_LBUTTONDOWN: {
        RECT rc; GetClientRect(hwnd, &rc);
        RECT x = FolderSearchCloseRect(rc.right, rc.bottom);
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&x, pt)) CloseFolderSearch();
        else SetFocus(g_hwndFolderSearchEdit);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        // Same background and top line as the strip, and a line where the strip begins
        HBRUSH hbBg = CreateSolidBrush(g_darkMode ? RGB(28, 28, 28) : RGB(235, 235, 235));
        FillRect(hdc, &rc, hbBg);
        DeleteObject(hbBg);
        HPEN hSep = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(60,60,60) : RGB(180,180,180));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hSep);
        MoveToEx(hdc, 0, 0, nullptr); LineTo(hdc, rc.right, 0);
        MoveToEx(hdc, rc.right - 1, 0, nullptr); LineTo(hdc, rc.right - 1, rc.bottom);
        SelectObject(hdc, hOldPen); DeleteObject(hSep);

        COLORREF textClr = g_darkMode ? RGB(170,170,170) : RGB(90,90,90);
        // ✕ close button
        RECT x = FolderSearchCloseRect(rc.right, rc.bottom);
        int cx = (x.left + x.right) / 2, cy = (x.top + x.bottom) / 2;
        HPEN hX = CreatePen(PS_SOLID, 1, textClr);
        hOldPen = (HPEN)SelectObject(hdc, hX);
        MoveToEx(hdc, cx - 4, cy - 4, nullptr); LineTo(hdc, cx + 5, cy + 5);
        MoveToEx(hdc, cx + 4, cy - 4, nullptr); LineTo(hdc, cx - 5, cy + 5);
        SelectObject(hdc, hOldPen); DeleteObject(hX);

        // Match count under the box, when the strip is tall enough
        RECT e = FolderSearchEditRect(rc.right, rc.bottom);
        RECT t = { e.left, e.bottom + 6, rc.right - 8, e.bottom + 22 };
        if (t.bottom <= rc.bottom) {
            wchar_t count[64];
            int n = (int)g_folderEntries.size();
            if (g_folderFilter.empty())
                StringCchPrintfW(count, 64, L"%d image%s", g_folderTotalCount, g_folderTotalCount == 1 ? L"" : L"s");
            else
                StringCchPrintfW(count, 64, L"%d of %d image%s", n, g_folderTotalCount, g_folderTotalCount == 1 ? L"" : L"s");
            HFONT hOldFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textClr);
            DrawTextW(hdc, count, -1, &t, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, hOldFont);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ─── Full screen and slideshow (View menu) ───────────────────
// A black window over the whole monitor showing the open image, fitted
// (small images are not enlarged, SVGs are re-rendered to fill it). The arrow
// keys step through the folder, honouring a folder browser search. Moving the
// mouse shows a control bar; the slideshow advances every g_slideSeconds.
static const UINT_PTR FS_SLIDE_TIMER = 1;
static const UINT_PTR FS_IDLE_TIMER  = 2;   // hides the bar and pointer when the mouse rests
static const DWORD    FS_IDLE_MS     = 2500;

static bool    g_fsPlaying     = false;
static bool    g_fsBarVisible  = false;
static bool    g_fsOwnsList    = false;   // the folder list was built for full screen (strip closed)
static DWORD   g_fsLastMove    = 0;
static POINT   g_fsLastPt      = { -1, -1 };
static int     g_fsHot         = -1;      // control bar button under the mouse
static Bitmap* g_fsCache       = nullptr; // the image at its full screen size
static Bitmap* g_fsCacheSrc    = nullptr;
static unsigned g_fsCacheVersion = 0;
static SIZE    g_fsCacheFor    = {};

enum { FSB_PREV, FSB_PLAY, FSB_NEXT, FSB_INTERVAL, FSB_EXIT, FSB_COUNT };

// Control bar, centred near the bottom; the file name fills the middle
static RECT FsBarRect(int w, int h) { int bw = min(560, w - 32); return { (w - bw) / 2, h - 80, (w + bw) / 2, h - 32 }; }
static RECT FsButtonRect(int w, int h, int b)
{
    RECT r = FsBarRect(w, h);
    switch (b) {
    case FSB_PREV:     return { r.left + 8,   r.top + 4, r.left + 48,  r.bottom - 4 };
    case FSB_PLAY:     return { r.left + 48,  r.top + 4, r.left + 88,  r.bottom - 4 };
    case FSB_NEXT:     return { r.left + 88,  r.top + 4, r.left + 128, r.bottom - 4 };
    case FSB_INTERVAL: return { r.right - 104, r.top + 4, r.right - 48, r.bottom - 4 };
    case FSB_EXIT:     return { r.right - 48, r.top + 4, r.right - 8,  r.bottom - 4 };
    }
    return {};
}
static int FsButtonAt(HWND hwnd, POINT pt)
{
    if (!g_fsBarVisible) return -1;
    RECT rc; GetClientRect(hwnd, &rc);
    for (int b = 0; b < FSB_COUNT; b++) { RECT r = FsButtonRect(rc.right, rc.bottom, b); if (PtInRect(&r, pt)) return b; }
    return -1;
}

static void FsFreeCache() { delete g_fsCache; g_fsCache = nullptr; g_fsCacheSrc = nullptr; }

// Makes g_fsCache: the open image scaled to fit w × h
static void FsBuildCache(int w, int h)
{
    if (g_fsCache && g_fsCacheSrc == g_pCanvas && g_fsCacheVersion == g_canvasVersion &&
        g_fsCacheFor.cx == w && g_fsCacheFor.cy == h) return;
    FsFreeCache();
    if (!g_pCanvas || w <= 0 || h <= 0) return;
    g_fsCacheSrc = g_pCanvas; g_fsCacheVersion = g_canvasVersion; g_fsCacheFor = { w, h };
    int cw = (int)g_pCanvas->GetWidth(), ch = (int)g_pCanvas->GetHeight();
    // An unedited SVG is drawn again at the size it is shown, so it stays sharp
    if (g_isSvgFile && !g_modified && g_currentFile[0] && g_svgCanvasScale > 0) {
        double ow = cw / g_svgCanvasScale, oh = ch / g_svgCanvasScale;
        if (Bitmap* b = LoadSvgToBitmap(g_currentFile, (float)min(w / ow, h / oh))) { g_fsCache = b; return; }
    }
    double s = min((double)w / cw, (double)h / ch);
    if (s > 1.0 && !g_isSvgFile) s = 1.0;
    int dw = max(1, (int)(cw * s + 0.5)), dh = max(1, (int)(ch * s + 0.5));
    g_fsCache = new Bitmap(dw, dh, PixelFormat32bppPARGB);
    if (g_fsCache->GetLastStatus() != Ok) { FsFreeCache(); return; }
    Graphics g(g_fsCache);
    g.SetInterpolationMode(s < 1.0 ? InterpolationModeHighQualityBicubic : InterpolationModeHighQualityBilinear);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.DrawImage(g_pCanvas, 0, 0, dw, dh);
}

// Steps through the folder list: dir -1 / +1 wraps around; 0 with toEnd
// goes to the first (toEnd < 0) or last (toEnd > 0) image
static void FsStep(int dir, int toEnd = 0)
{
    int n = (int)g_folderEntries.size();
    if (n == 0) return;
    int cur = g_folderCurrentIdx;
    for (int i = 0; cur < 0 && i < n; i++)
        if (_wcsicmp(g_folderEntries[i].path.c_str(), g_currentFile) == 0) cur = i;
    int idx = toEnd < 0 ? 0 : toEnd > 0 ? n - 1
            : cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    NavigateToFile(idx);
}

static void FsRestartSlideTimer()
{
    if (!g_hwndFullscreen) return;
    KillTimer(g_hwndFullscreen, FS_SLIDE_TIMER);
    if (g_fsPlaying) SetTimer(g_hwndFullscreen, FS_SLIDE_TIMER, g_slideSeconds * 1000, NULL);
}

static void FsSetPlaying(bool play)
{
    g_fsPlaying = play && g_folderEntries.size() > 1;
    FsRestartSlideTimer();
    if (g_hwndFullscreen) InvalidateRect(g_hwndFullscreen, NULL, FALSE);
}

static void FsShowBar(HWND hwnd)
{
    g_fsLastMove = GetTickCount();
    if (!g_fsBarVisible) { g_fsBarVisible = true; InvalidateRect(hwnd, NULL, FALSE); }
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

static void FsPress(HWND hwnd, int b)
{
    switch (b) {
    case FSB_PREV: FsStep(-1); FsRestartSlideTimer(); break;
    case FSB_NEXT: FsStep(+1); FsRestartSlideTimer(); break;
    case FSB_PLAY: FsSetPlaying(!g_fsPlaying); break;
    case FSB_INTERVAL:
        g_slideSeconds = g_slideSeconds == 3 ? 5 : g_slideSeconds == 5 ? 10 : 3;
        FsRestartSlideTimer();
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case FSB_EXIT: ExitFullscreen(); break;
    }
}

static void FsPaintBar(Graphics& g, int w, int h)
{
    RECT r = FsBarRect(w, h);
    GraphicsPath bg;
    const float rad = 10.f;
    float L = (float)r.left, T = (float)r.top, R = (float)r.right, B = (float)r.bottom;
    bg.AddArc(L, T, 2*rad, 2*rad, 180, 90); bg.AddArc(R - 2*rad, T, 2*rad, 2*rad, 270, 90);
    bg.AddArc(R - 2*rad, B - 2*rad, 2*rad, 2*rad, 0, 90); bg.AddArc(L, B - 2*rad, 2*rad, 2*rad, 90, 90);
    bg.CloseFigure();
    SolidBrush bgBr(Color(215, 32, 32, 32));
    g.FillPath(&bgBr, &bg);

    SolidBrush hotBr(Color(255, 70, 70, 70));
    SolidBrush fg(Color(255, 235, 235, 235));
    Pen fgPen(Color(255, 235, 235, 235), 2.f);
    for (int b = 0; b < FSB_COUNT; b++) {
        RECT br = FsButtonRect(w, h, b);
        float cx = (br.left + br.right) / 2.f, cy = (br.top + br.bottom) / 2.f;
        if (b == g_fsHot) g.FillEllipse(&hotBr, cx - 17.f, cy - 17.f, 34.f, 34.f);
        switch (b) {
        case FSB_PREV: case FSB_NEXT: {
            float s = b == FSB_NEXT ? 1.f : -1.f;
            PointF tri[3] = { {cx - 5*s, cy - 7}, {cx + 5*s, cy}, {cx - 5*s, cy + 7} };
            g.FillPolygon(&fg, tri, 3);
            g.FillRectangle(&fg, s > 0 ? cx + 5.f : cx - 7.f, cy - 7.f, 2.f, 14.f);
            break;
        }
        case FSB_PLAY:
            if (g_fsPlaying) { g.FillRectangle(&fg, cx - 6.f, cy - 8.f, 4.f, 16.f); g.FillRectangle(&fg, cx + 2.f, cy - 8.f, 4.f, 16.f); }
            else { PointF tri[3] = { {cx - 5, cy - 9}, {cx + 9, cy}, {cx - 5, cy + 9} }; g.FillPolygon(&fg, tri, 3); }
            break;
        case FSB_EXIT:
            g.DrawLine(&fgPen, cx - 6.f, cy - 6.f, cx + 6.f, cy + 6.f);
            g.DrawLine(&fgPen, cx + 6.f, cy - 6.f, cx - 6.f, cy + 6.f);
            break;
        }
    }
    FontFamily ff(L"Segoe UI");
    Font font(&ff, 10.f, FontStyleRegular, UnitPoint);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(StringTrimmingEllipsisCharacter); sf.SetFormatFlags(StringFormatFlagsNoWrap);
    // Interval: click to cycle 3 / 5 / 10 seconds
    RECT ir = FsButtonRect(w, h, FSB_INTERVAL);
    if (g_fsHot == FSB_INTERVAL) {
        RectF hr((REAL)ir.left, (REAL)ir.top + 6, (REAL)(ir.right - ir.left), (REAL)(ir.bottom - ir.top) - 12);
        g.FillRectangle(&hotBr, hr);
    }
    wchar_t secs[16]; StringCchPrintfW(secs, 16, L"%d s", g_slideSeconds);
    g.DrawString(secs, -1, &font, RectF((REAL)ir.left, (REAL)ir.top, (REAL)(ir.right - ir.left), (REAL)(ir.bottom - ir.top)), &sf, &fg);
    // File name, and its place in the folder
    RECT nb = FsButtonRect(w, h, FSB_NEXT);
    std::wstring name = g_currentFile[0] ? PathFindFileNameW(g_currentFile) : L"Untitled";
    int n = (int)g_folderEntries.size();
    if (n > 0 && g_folderCurrentIdx >= 0)
        name += L"   " + std::to_wstring(g_folderCurrentIdx + 1) + L" / " + std::to_wstring(n);
    RectF nr((REAL)nb.right + 8, (REAL)r.top, (REAL)(ir.left - nb.right - 16), (REAL)(r.bottom - r.top));
    g.DrawString(name.c_str(), -1, &font, nr, &sf, &fg);
}

static LRESULT CALLBACK FullscreenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        Bitmap back(rc.right, rc.bottom, PixelFormat32bppPARGB);
        {
            Graphics g(&back);
            g.Clear(Color(255, 0, 0, 0));
            FsBuildCache(rc.right, rc.bottom);
            if (g_fsCache) {
                int dw = (int)g_fsCache->GetWidth(), dh = (int)g_fsCache->GetHeight();
                g.DrawImage(g_fsCache, (rc.right - dw) / 2, (rc.bottom - dh) / 2, dw, dh);
            }
            if (g_fsBarVisible) {
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                FsPaintBar(g, rc.right, rc.bottom);
            }
        }
        Graphics screen(hdc);
        screen.DrawImage(&back, 0, 0, rc.right, rc.bottom);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) { SetCursor(g_fsBarVisible ? LoadCursor(NULL, IDC_ARROW) : NULL); return TRUE; }
        break;

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        // Windows repeats WM_MOUSEMOVE without movement (e.g. after repaints)
        if (pt.x == g_fsLastPt.x && pt.y == g_fsLastPt.y) return 0;
        g_fsLastPt = pt;
        FsShowBar(hwnd);
        int hot = FsButtonAt(hwnd, pt);
        if (hot != g_fsHot) { g_fsHot = hot; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int b = FsButtonAt(hwnd, pt);
        if (b >= 0) FsPress(hwnd, b);
        else FsShowBar(hwnd);
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int b = FsButtonAt(hwnd, pt);
        if (b >= 0) FsPress(hwnd, b);   // quick clicks on a button are two presses
        else ExitFullscreen();
        return 0;
    }

    case WM_MOUSEWHEEL:
        FsStep(GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? +1 : -1);
        FsRestartSlideTimer();
        return 0;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE: case VK_F11: ExitFullscreen(); break;
        case VK_SPACE:  case VK_F5:  FsSetPlaying(!g_fsPlaying); break;
        case VK_RIGHT: case VK_DOWN: case VK_NEXT:  FsStep(+1); FsRestartSlideTimer(); break;
        case VK_LEFT:  case VK_UP:   case VK_PRIOR: FsStep(-1); FsRestartSlideTimer(); break;
        case VK_HOME: FsStep(0, -1); FsRestartSlideTimer(); break;
        case VK_END:  FsStep(0, +1); FsRestartSlideTimer(); break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == FS_SLIDE_TIMER) {
            // Wait while a file is still loading or a message (e.g. save
            // changes?) is up and has disabled the main window, rather than
            // piling up loads behind it
            if (g_imageLoading || !IsWindowEnabled(g_hwndMain)) return 0;
            FsStep(+1);
            return 0;
        }
        if (wParam == FS_IDLE_TIMER) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            if (g_fsBarVisible && GetTickCount() - g_fsLastMove > FS_IDLE_MS && FsButtonAt(hwnd, pt) < 0) {
                g_fsBarVisible = false;
                g_fsHot = -1;
                InvalidateRect(hwnd, NULL, FALSE);
                if (GetForegroundWindow() == hwnd) SetCursor(NULL);
            }
            return 0;
        }
        break;

    case WM_CLOSE:   // Alt+F4 leaves full screen rather than closing MyPaint
        ExitFullscreen();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, FS_SLIDE_TIMER);
        KillTimer(hwnd, FS_IDLE_TIMER);
        FsFreeCache();
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void EnterFullscreen(bool play)
{
    if (!g_hwndFullscreen) {
        static bool registered = false;
        if (!registered) {
            WNDCLASSEX wc = {};
            wc.cbSize        = sizeof(wc);
            wc.style         = CS_DBLCLKS;
            wc.lpfnWndProc   = FullscreenWndProc;
            wc.hInstance     = GetModuleHandle(NULL);
            wc.lpszClassName = L"MyPaintFullscreen";
            wc.hIcon         = (HICON)GetClassLongPtr(g_hwndMain, GCLP_HICON);
            registered = RegisterClassEx(&wc) != 0;
        }
        EndFolderRename(true);
        // The arrow keys need the folder list, which only exists while the strip is open
        g_fsOwnsList = !g_showFolderBrowser;
        if (g_fsOwnsList) BuildFolderList();
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(g_hwndMain, MONITOR_DEFAULTTONEAREST), &mi);
        RECT m = mi.rcMonitor;
        g_fsBarVisible = false; g_fsHot = -1; g_fsLastPt = { -1, -1 };
        g_hwndFullscreen = CreateWindowEx(0, L"MyPaintFullscreen", APP_TITLE, WS_POPUP,
            m.left, m.top, m.right - m.left, m.bottom - m.top, g_hwndMain, NULL, GetModuleHandle(NULL), NULL);
        if (!g_hwndFullscreen) { if (g_fsOwnsList) FreeFolderThumbs(); return; }
        ShowWindow(g_hwndFullscreen, SW_SHOW);
        SetForegroundWindow(g_hwndFullscreen);
        SetFocus(g_hwndFullscreen);
        SetTimer(g_hwndFullscreen, FS_IDLE_TIMER, 500, NULL);
        FsShowBar(g_hwndFullscreen);   // show the controls briefly, so they can be found
    }
    FsSetPlaying(play);
}

static void ExitFullscreen()
{
    HWND fs = g_hwndFullscreen;
    if (!fs) return;
    g_hwndFullscreen = NULL;
    g_fsPlaying = false;
    DestroyWindow(fs);
    if (g_fsOwnsList && !g_showFolderBrowser) FreeFolderThumbs();
    g_fsOwnsList = false;
    SetForegroundWindow(g_hwndMain);
    if (g_showFolderBrowser && g_hwndFolderBrowser) {
        CenterFolderBrowserOnCurrent();
        InvalidateRect(g_hwndFolderBrowser, NULL, TRUE);
        SetFocus(g_hwndFolderBrowser);
    } else if (g_hwndCanvas) SetFocus(g_hwndCanvas);
}

// ============================================================
//  Layout
// ============================================================
// Places the canvas and, below it, the folder browser above the status bar.
// Dragging the strip's edge calls only this, so the toolbar and status bar
// aren't re-laid out and repainted on every mouse move.
static void LayoutCanvasAndFolderBrowser(HWND hwndMain)
{
    RECT rc;
    GetClientRect(hwndMain, &rc);
    int W = rc.right, H = rc.bottom;
    int statusH = 0;
    if (g_hwndStatus) { RECT src; GetWindowRect(g_hwndStatus, &src); statusH = src.bottom - src.top; }

    bool showStrip = g_showFolderBrowser && g_hwndFolderBrowser;
    int stripH  = showStrip ? ClampFolderBrowserH(g_folderBrowserH) : 0;
    int canvasY = TOOLBAR_HEIGHT + COLORSTRIP_H;
    int canvasH = max(0, H - canvasY - statusH - stripH);

    // The search panel, when open, takes the left end of the strip's row
    int searchW = (showStrip && g_folderSearchOpen && g_hwndFolderSearch) ? min(FOLDER_SEARCH_W, W / 2) : 0;

    // Move them in one step so the gaps between them never show
    HDWP dwp = BeginDeferWindowPos(3);
    if (dwp && searchW)
        dwp = DeferWindowPos(dwp, g_hwndFolderSearch, NULL, 0, H - statusH - stripH, searchW, stripH,
                             SWP_NOZORDER | SWP_NOACTIVATE);
    if (dwp && showStrip)
        dwp = DeferWindowPos(dwp, g_hwndFolderBrowser, NULL, searchW, H - statusH - stripH, W - searchW, stripH,
                             SWP_NOZORDER | SWP_NOACTIVATE);
    if (dwp && g_hwndCanvas)
        dwp = DeferWindowPos(dwp, g_hwndCanvas, NULL, 0, canvasY, W, canvasH,
                             SWP_NOZORDER | SWP_NOACTIVATE);
    if (dwp) EndDeferWindowPos(dwp);
}

static void LayoutChildren(HWND hwndMain)
{
    RECT rc;
    GetClientRect(hwndMain, &rc);
    int W = rc.right, H = rc.bottom;

    if (g_hwndToolbar)
        SetWindowPos(g_hwndToolbar, NULL, 0, 0, W, TOOLBAR_HEIGHT, SWP_NOZORDER|SWP_NOACTIVATE);

    // Right-aligned toolbar overlay buttons: Dark Mode toggle and Help
    {
        static const int RB_W = 32, RB_H = TOOLBAR_HEIGHT - 6;
        static const int RB_Y = (TOOLBAR_HEIGHT - RB_H) / 2;
        const int helpX = W - RB_W - 2;
        const int darkX = helpX - RB_W - 2;
        g_tbSepX = darkX - 6;   // separator 6px to the left of the dark-mode button
        if (g_hwndBtnHelp) SetWindowPos(g_hwndBtnHelp, HWND_TOP, helpX, RB_Y, RB_W, RB_H, SWP_NOACTIVATE);
        if (g_hwndBtnDark) SetWindowPos(g_hwndBtnDark, HWND_TOP, darkX, RB_Y, RB_W, RB_H, SWP_NOACTIVATE);
        if (g_hwndToolbar) InvalidateRect(g_hwndToolbar, NULL, FALSE);  // repaint separator
    }

    // Overflow: hide buttons that don't fit into More / Tools palettes
    if (g_hwndToolbar && g_tbSepX > 0) {
        // Restore everything first
        for (int i = 0; i < kToolsOverflowCount; i++)
            SendMessage(g_hwndToolbar, TB_HIDEBUTTON, kToolsOverflowCmds[i], MAKELONG(FALSE, 0));
        for (int i = 0; i < kOverflowCount; i++)
            SendMessage(g_hwndToolbar, TB_HIDEBUTTON, kOverflowCmds[i], MAKELONG(FALSE, 0));
        SendMessage(g_hwndToolbar, TB_HIDEBUTTON, ID_SEP_EXTENDED, MAKELONG(FALSE, 0));
        g_numOverflowed = 0;
        g_numToolsOverflowed = 0;

        const int avail = g_tbSepX - 4;
        SIZE ideal = {};
        SendMessage(g_hwndToolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);

        // Phase 1: overflow extended shapes into More (rightmost first)
        while (g_numOverflowed < kOverflowCount && ideal.cx > avail) {
            SendMessage(g_hwndToolbar, TB_HIDEBUTTON, kOverflowCmds[g_numOverflowed], MAKELONG(TRUE, 0));
            g_numOverflowed++;
            if (g_numOverflowed == kOverflowCount)
                SendMessage(g_hwndToolbar, TB_HIDEBUTTON, ID_SEP_EXTENDED, MAKELONG(TRUE, 0));
            SendMessage(g_hwndToolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);
        }

        // Phase 2: overflow basic tools into Tools (rightmost first)
        while (g_numToolsOverflowed < kToolsOverflowCount && ideal.cx > avail) {
            SendMessage(g_hwndToolbar, TB_HIDEBUTTON, kToolsOverflowCmds[g_numToolsOverflowed], MAKELONG(TRUE, 0));
            g_numToolsOverflowed++;
            SendMessage(g_hwndToolbar, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);
        }

        // Update check states for both palette buttons
        bool inMore = false;
        for (int i = 0; i < kMorePermanentCount && !inMore; i++)
            if (TOOL_COMMANDS[(int)g_currentTool] == kMorePermanentCmds[i]) inMore = true;
        for (int i = 0; i < g_numOverflowed && !inMore; i++)
            if (TOOL_COMMANDS[(int)g_currentTool] == kOverflowCmds[i]) inMore = true;
        SendMessage(g_hwndToolbar, TB_CHECKBUTTON, ID_MENU_MORE, MAKELONG(inMore, 0));

        bool inTools = false;
        for (int i = 0; i < g_numToolsOverflowed && !inTools; i++)
            if (TOOL_COMMANDS[(int)g_currentTool] == kToolsOverflowCmds[i]) inTools = true;
        for (int i = 0; i < kToolsPermanentCount && !inTools; i++)
            if (TOOL_COMMANDS[(int)g_currentTool] == kToolsPermanentCmds[i]) inTools = true;
        SendMessage(g_hwndToolbar, TB_CHECKBUTTON, ID_MENU_TOOLS, MAKELONG(inTools, 0));
    }

    int statusH = 0;
    if (g_hwndStatus) {
        SendMessage(g_hwndStatus, WM_SIZE, 0, 0);
        RECT src; GetWindowRect(g_hwndStatus, &src);
        statusH = src.bottom - src.top;
    }

    // Position zoom controls over the right section of the status bar
    if (g_hwndStatus && g_hwndZoomEdit) {
        int sbY   = H - statusH;
        int ctrlH = ZOOM_BTN_H;
        int ctrlY = sbY + (statusH - ctrlH) / 2;
        const int grip  = 16;           // sizing grip at far right
        int zFolderBrowserL = W - grip - ZOOM_BTN_W;          // folder browser: rightmost button
        int zInL   = zFolderBrowserL - 4 - ZOOM_BTN_W;        // zoom-in: 4px sep gap to its right
        int zOutL  = zInL - ZOOM_BTN_W;
        int zFitL  = zOutL - ZOOM_BTN_W - 2;
        int zEdL   = zFitL - 2 - ZOOM_EDIT_W;
        int zoneL  = max(600, W - ZOOM_ZONE_W);
        g_sbFolderSepX = zFolderBrowserL - 4;  // vertical separator between zoom-in and folder browser
        SetWindowPos(g_hwndZoomIn,       HWND_TOP, zInL,   ctrlY, ZOOM_BTN_W,  ctrlH, SWP_NOACTIVATE);
        SetWindowPos(g_hwndZoomOut,      HWND_TOP, zOutL,  ctrlY, ZOOM_BTN_W,  ctrlH, SWP_NOACTIVATE);
        SetWindowPos(g_hwndZoomFit,      HWND_TOP, zFitL,  ctrlY, ZOOM_BTN_W,  ctrlH, SWP_NOACTIVATE);
        SetWindowPos(g_hwndZoomEdit,     HWND_TOP, zEdL,   ctrlY, ZOOM_EDIT_W, ctrlH, SWP_NOACTIVATE);
        SetWindowPos(g_hwndFolderBrowserBtn, HWND_TOP, zFolderBrowserL, ctrlY, ZOOM_BTN_W,  ctrlH, SWP_NOACTIVATE);
        if (g_hwndStatus) InvalidateRect(g_hwndStatus, NULL, TRUE); // repaint separator
        // Pane 5 starts at zoneL and holds the "Zoom" label text + controls area
        int parts[6] = {180, 320, 500, min(600, zoneL - 20), zoneL, -1};
        SendMessage(g_hwndStatus, SB_SETPARTS, 6, (LPARAM)parts);
    }

    LayoutCanvasAndFolderBrowser(hwndMain);

    // Hide custom swatches + colour-edit + reset buttons when window is too narrow
    {
        RECT winRc; GetWindowRect(hwndMain, &winRc);
        int winW = winRc.right - winRc.left;
        bool show = (winW > 600);
        if (show != g_showCustomColors) {
            g_showCustomColors = show;
            if (g_hwndColorEdit) ShowWindow(g_hwndColorEdit, show ? SW_SHOW : SW_HIDE);
            if (g_hwndReset)     ShowWindow(g_hwndReset,     show ? SW_SHOW : SW_HIDE);
        }
    }

    // Reposition colour-strip controls to fill the space when custom colours are hidden
    {
        int xShift = g_showCustomColors ? 0 : (CS_CUSTOM_X - CS_ALPHA_SEP_X);
        auto cy = [](int h){ return TOOLBAR_HEIGHT + (COLORSTRIP_H - h) / 2; };

        if (g_hwndAlphaEdit)
            SetWindowPos(g_hwndAlphaEdit, NULL, CS_ALPHA_EDIT_X + xShift, cy(CS_ALPHA_EDIT_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndBrushSz)
            SetWindowPos(g_hwndBrushSz, NULL, CS_LW_EDIT_X + xShift, cy(CS_LW_EDIT_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndSzMinus)
            SetWindowPos(g_hwndSzMinus, NULL, CS_LW_MINUS_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndSzPlus)
            SetWindowPos(g_hwndSzPlus, NULL, CS_LW_PLUS_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndEraserSq)
            SetWindowPos(g_hwndEraserSq, NULL, CS_LW_ER_SQ_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndEraserRd)
            SetWindowPos(g_hwndEraserRd, NULL, CS_LW_ER_RD_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndSmudgeSq)
            SetWindowPos(g_hwndSmudgeSq, NULL, CS_LW_ER_SQ_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndSmudgeRd)
            SetWindowPos(g_hwndSmudgeRd, NULL, CS_LW_ER_RD_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndWandContig)
            SetWindowPos(g_hwndWandContig, NULL, CS_WD_CONTIG_X + xShift, cy(CS_LW_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndFontFace)
            SetWindowPos(g_hwndFontFace, NULL, CS_TF_FACE_X + xShift, cy(CS_TF_FACE_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndFontSize)
            SetWindowPos(g_hwndFontSize, NULL, CS_TF_SIZE_X + xShift, cy(CS_TF_FACE_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndFontBold)
            SetWindowPos(g_hwndFontBold, NULL, CS_TF_B_X + xShift, cy(CS_TF_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndFontItal)
            SetWindowPos(g_hwndFontItal, NULL, CS_TF_I_X + xShift, cy(CS_TF_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
        if (g_hwndFontUnder)
            SetWindowPos(g_hwndFontUnder, NULL, CS_TF_U_X + xShift, cy(CS_TF_BTN_H),
                         0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);

        // Redraw the entire colour strip
        RECT stripRc = {0, TOOLBAR_HEIGHT, W, TOOLBAR_HEIGHT + COLORSTRIP_H};
        InvalidateRect(hwndMain, &stripRc, TRUE);
    }
}

// ============================================================
//  Colour strip — paint & hit-test
// Draw a colour swatch with checkerboard background when alpha < 255.
static void DrawAlphaSwatch(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (alpha < 255) {
        // Checkerboard background: 4×4 alternating white / light-gray squares
        const int csz = 4;
        for (int cy = 0; cy < h; cy += csz) {
            for (int cx = 0; cx < w; cx += csz) {
                bool light = (((cx / csz) + (cy / csz)) % 2 == 0);
                COLORREF chk = light ? RGB(255,255,255) : RGB(192,192,192);
                RECT sr = { rc.left + cx, rc.top + cy,
                            min(rc.left + cx + csz, rc.right),
                            min(rc.top  + cy + csz, rc.bottom) };
                HBRUSH hb = CreateSolidBrush(chk);
                FillRect(hdc, &sr, hb); DeleteObject(hb);
            }
        }
        // Alpha-blend the solid colour on top
        HDC memDC  = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP hOld = (HBITMAP)SelectObject(memDC, hbm);
        HBRUSH hFill = CreateSolidBrush(color);
        RECT fill = { 0, 0, w, h };
        FillRect(memDC, &fill, hFill); DeleteObject(hFill);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, 0 };
        AlphaBlend(hdc, rc.left, rc.top, w, h, memDC, 0, 0, w, h, bf);
        SelectObject(memDC, hOld); DeleteObject(hbm); DeleteDC(memDC);
    } else {
        HBRUSH hb = CreateSolidBrush(color);
        FillRect(hdc, &rc, hb); DeleteObject(hb);
    }
    FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
}

// ============================================================
static void PaintColorStrip(HDC hdc, int stripX, int stripY, int stripW)
{
    RECT rc = {stripX, stripY, stripX + stripW, stripY + COLORSTRIP_H};
    if (g_darkMode) {
        HBRUSH hb = CreateSolidBrush(RGB(45,45,48)); FillRect(hdc, &rc, hb); DeleteObject(hb);
    } else {
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DFACE));
    }

    int sy  = stripY + (COLORSTRIP_H - CS_FG_H) / 2;    // y for FG/BG swatches

    // Fill swatch (drawn first so stroke overlaps it for stacked-card look)
    RECT bgRc = {stripX + CS_FG_X + CS_BG_OX, sy + 4,
                 stripX + CS_FG_X + CS_BG_OX + CS_FG_W, sy + 4 + CS_FG_H};
    DrawAlphaSwatch(hdc, bgRc, g_bgColor, BgAlphaByte());

    // Stroke swatch
    RECT fgRc = {stripX + CS_FG_X, sy, stripX + CS_FG_X + CS_FG_W, sy + CS_FG_H};
    DrawAlphaSwatch(hdc, fgRc, g_fgColor, FgAlphaByte());

    // Active selection indicator: white+black outer ring on the active swatch
    {
        const RECT& activeRc = g_strokeActive ? fgRc : bgRc;
        RECT outer = { activeRc.left - 2, activeRc.top - 2, activeRc.right + 2, activeRc.bottom + 2 };
        FrameRect(hdc, &outer, (HBRUSH)GetStockObject(WHITE_BRUSH));
        RECT inner = { outer.left - 1, outer.top - 1, outer.right + 1, outer.bottom + 1 };
        FrameRect(hdc, &inner, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    // 16 custom colour swatches (hidden when window is too narrow)
    if (g_showCustomColors) {
        int csy  = stripY + (COLORSTRIP_H - CS_SWATCH_H) / 2;
        int step = CS_SWATCH_W + CS_SWATCH_GAP;
        for (int i = 0; i < CS_CUSTOM_COUNT; ++i) {
            int sx = stripX + CS_CUSTOM_X + i * step;
            RECT sr = {sx, csy, sx + CS_SWATCH_W, csy + CS_SWATCH_H};
            HBRUSH hb = CreateSolidBrush(g_customColors[i]);
            FillRect(hdc, &sr, hb); DeleteObject(hb);
            FrameRect(hdc, &sr, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
    }

    bool wandActive  = (g_currentTool == TOOL_WAND);  // hides alpha section
    bool threshActive = (g_currentTool == TOOL_WAND || g_currentTool == TOOL_FILL);  // shows threshold label, hides line-width preview

    // Horizontal shift applied to everything right of the FG/BG area when
    // custom colours are hidden (closes the gap left by the hidden swatches).
    int xShift = g_showCustomColors ? 0 : (CS_CUSTOM_X - CS_ALPHA_SEP_X);

    // Alpha section: separator + "Opacity" label — hidden when wand tool is active
    if (!wandActive)
    {
        int sy1 = stripY + 3, sy2 = stripY + COLORSTRIP_H - 3;
        COLORREF sepDark  = g_darkMode ? RGB(80,80,80)  : GetSysColor(COLOR_BTNSHADOW);
        COLORREF sepLight = g_darkMode ? RGB(90,90,90)  : GetSysColor(COLOR_BTNHIGHLIGHT);
        HPEN hDark  = CreatePen(PS_SOLID, 1, sepDark);
        HPEN hLight = CreatePen(PS_SOLID, 1, sepLight);
        HPEN hOld   = (HPEN)SelectObject(hdc, hDark);
        int sepX = stripX + CS_ALPHA_SEP_X + xShift;
        MoveToEx(hdc, sepX,     sy1, NULL); LineTo(hdc, sepX,     sy2);
        SelectObject(hdc, hLight);
        MoveToEx(hdc, sepX + 1, sy1, NULL); LineTo(hdc, sepX + 1, sy2);
        SelectObject(hdc, hOld);
        DeleteObject(hDark); DeleteObject(hLight);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT hOldF = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g_darkMode ? RGB(220,220,220) : GetSysColor(COLOR_BTNTEXT));
        int lblX = stripX + CS_ALPHA_LBL_X + xShift;
        RECT alrc = { lblX, stripY, lblX + CS_ALPHA_LBL_W, stripY + COLORSTRIP_H };
        DrawText(hdc, L"Opacity", -1, &alrc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, hOldF);
    }

    // Left etched vertical separator before the Line Width / Font / Wand section
    {
        int sy1 = stripY + 3, sy2 = stripY + COLORSTRIP_H - 3;
        COLORREF sepDark  = g_darkMode ? RGB(80,80,80)  : GetSysColor(COLOR_BTNSHADOW);
        COLORREF sepLight = g_darkMode ? RGB(90,90,90)  : GetSysColor(COLOR_BTNHIGHLIGHT);
        HPEN hDark  = CreatePen(PS_SOLID, 1, sepDark);
        HPEN hLight = CreatePen(PS_SOLID, 1, sepLight);
        HPEN hOld   = (HPEN)SelectObject(hdc, hDark);
        int lSepX = stripX + CS_LW_SEP_L_X + xShift;
        MoveToEx(hdc, lSepX,     sy1, NULL); LineTo(hdc, lSepX,     sy2);
        SelectObject(hdc, hLight);
        MoveToEx(hdc, lSepX + 1, sy1, NULL); LineTo(hdc, lSepX + 1, sy2);
        SelectObject(hdc, hOld);
        DeleteObject(hDark); DeleteObject(hLight);
    }

    // Section label (hidden when text tool is active — font controls replace this section)
    if (g_currentTool != TOOL_TEXT)
    {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT hOld  = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g_darkMode ? RGB(220,220,220) : GetSysColor(COLOR_BTNTEXT));
        int lblX = stripX + CS_LW_LABEL_X + xShift;
        RECT txtRc = { lblX, stripY, lblX + CS_LW_LABEL_W, stripY + COLORSTRIP_H };
        const wchar_t* lbl = threshActive                     ? L"Threshold" :
                             (g_currentTool == TOOL_ERASER)   ? L"Eraser Size" :
                             (g_currentTool == TOOL_SMUDGE)   ? L"Smudge Size" :
                                                                 L"Line Width";
        DrawText(hdc, lbl, -1, &txtRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, hOld);
    }

    // Line-width preview — hidden when eraser, smudge, wand, or text tool is active
    if (g_currentTool != TOOL_ERASER && g_currentTool != TOOL_SMUDGE && g_currentTool != TOOL_TEXT && !threshActive)
    {
        // Match the exact vertical extent of the HWND controls in the strip
        int ctrlTop = stripY + (COLORSTRIP_H - CS_LW_EDIT_H) / 2;
        int ctrlBot = ctrlTop + CS_LW_EDIT_H;
        int pvCX    = stripX + CS_LW_PREV_X + xShift + CS_LW_PREV_W / 2;
        // Round brush size for pixel rendering; cap to the box height so it stays inside
        int pvW     = min((int)(g_brushSize + 0.5f), CS_LW_EDIT_H - 2);
        if (pvW < 1) pvW = 1;

        HPEN hpen;
        if (pvW <= 1) {
            // 1-px cosmetic pen — no caps needed
            hpen = CreatePen(PS_SOLID, 1, g_fgColor);
        } else {
            // Geometric pen with flat (square) caps that don't extend past endpoints
            LOGBRUSH lb = { BS_SOLID, g_fgColor, 0 };
            hpen = ExtCreatePen(
                PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | PS_JOIN_MITER,
                (DWORD)pvW, &lb, 0, NULL);
        }
        HPEN hOldPen = (HPEN)SelectObject(hdc, hpen);
        MoveToEx(hdc, pvCX, ctrlTop, NULL);
        LineTo(hdc,   pvCX, ctrlBot);
        SelectObject(hdc, hOldPen);
        DeleteObject(hpen);
    }
}

// Returns: 0=nothing, 1=FG swatch, 2=BG swatch, 3+i=custom colour i
static int HitTestColorStrip(int x, int y, int stripY)
{
    if (y < stripY || y >= stripY + COLORSTRIP_H) return 0;

    int sy = stripY + (COLORSTRIP_H - CS_FG_H) / 2;

    // FG swatch (test first — it overlaps BG)
    if (x >= CS_FG_X && x < CS_FG_X + CS_FG_W &&
        y >= sy && y < sy + CS_FG_H)
        return 1;

    // BG swatch
    int bgx = CS_FG_X + CS_BG_OX;
    if (x >= bgx && x < bgx + CS_FG_W &&
        y >= sy + 4 && y < sy + 4 + CS_FG_H)
        return 2;

    // Custom colour swatches
    int csy  = stripY + (COLORSTRIP_H - CS_SWATCH_H) / 2;
    int step = CS_SWATCH_W + CS_SWATCH_GAP;
    for (int i = 0; i < CS_CUSTOM_COUNT; ++i) {
        int sx = CS_CUSTOM_X + i * step;
        if (x >= sx && x < sx + CS_SWATCH_W &&
            y >= csy && y < csy + CS_SWATCH_H)
            return 3 + i;
    }

    // Reset button
    int rsy = stripY + (COLORSTRIP_H - CS_RESET_H) / 2;
    if (x >= CS_RESET_X && x < CS_RESET_X + CS_RESET_W &&
        y >= rsy && y < rsy + CS_RESET_H)
        return CS_HIT_RESET;

    return 0;
}

// ============================================================
//  Main window procedure
// ============================================================
static void ShowBrushTypePopup(HWND hwnd); // forward declaration

// Toolbar subclass — intercepts WM_LBUTTONDOWN so we can show the brush popup
// on every click, even when the brush button is already the active (checked) tool.
// BTNS_CHECKGROUP silently discards clicks on already-pressed buttons, so neither
// WM_COMMAND nor NM_CLICK would fire in that case without this subclass.
static LRESULT CALLBACK ToolbarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam, UINT_PTR uId, DWORD_PTR dwRef)
{
    if (msg == WM_PAINT) {
        // Let the toolbar paint normally, then draw the right-side vertical separator on top.
        DefSubclassProc(hwnd, msg, wParam, lParam);
        if (g_tbSepX > 0) {
            HDC hdc = GetDC(hwnd);
            if (hdc) {
                COLORREF c1 = g_darkMode ? RGB(80, 80, 80)   : GetSysColor(COLOR_BTNSHADOW);
                COLORREF c2 = g_darkMode ? RGB(100, 100, 100) : GetSysColor(COLOR_BTNHIGHLIGHT);
                HPEN p1 = CreatePen(PS_SOLID, 1, c1);
                HPEN p2 = CreatePen(PS_SOLID, 1, c2);
                RECT rc; GetClientRect(hwnd, &rc);
                HPEN hOld = (HPEN)SelectObject(hdc, p1);
                MoveToEx(hdc, g_tbSepX,     5, NULL); LineTo(hdc, g_tbSepX,     rc.bottom - 5);
                SelectObject(hdc, p2);
                MoveToEx(hdc, g_tbSepX + 1, 5, NULL); LineTo(hdc, g_tbSepX + 1, rc.bottom - 5);
                SelectObject(hdc, hOld);
                DeleteObject(p1); DeleteObject(p2);
                ReleaseDC(hwnd, hdc);
            }
        }
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int idx = (int)SendMessage(hwnd, TB_HITTEST, 0, (LPARAM)&pt);
        if (idx >= 0) {
            TBBUTTON tb = {};
            SendMessage(hwnd, TB_GETBUTTON, idx, (LPARAM)&tb);
            if (tb.idCommand == ID_TOOL_BRUSH) {
                // Give brief visual press feedback, then show the popup
                SendMessage(hwnd, TB_PRESSBUTTON, ID_TOOL_BRUSH, TRUE);
                ShowBrushTypePopup(GetParent(hwnd));
                SendMessage(hwnd, TB_PRESSBUTTON, ID_TOOL_BRUSH, FALSE);
                return 0; // swallow — no WM_COMMAND, no checkgroup toggle
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Subclass proc for the overlay buttons (dark-mode toggle + help).
// Manually tracks mouse enter/leave so WM_DRAWITEM re-fires with the correct hot state.
static LRESULT CALLBACK OverlayBtnSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                LPARAM lParam, UINT_PTR /*uId*/, DWORD_PTR dwRef)
{
    bool& hot = *reinterpret_cast<bool*>(dwRef);
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!hot) {
            hot = true;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        hot = false;
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Builds 16x16 HBITMAPs for each status bar icon, rendered on the exact same
// background colour used by WM_DRAWITEM.  BitBlt from these in WM_DRAWITEM is a
// single atomic GDI operation so icon panes never show a partial-render flash.
static void RebuildStatusIcons()
{
    if (g_hbmStatusCursor) { DeleteObject(g_hbmStatusCursor); g_hbmStatusCursor = NULL; }
    if (g_hbmStatusBorder) { DeleteObject(g_hbmStatusBorder); g_hbmStatusBorder = NULL; }
    if (g_hbmStatusCrop)   { DeleteObject(g_hbmStatusCrop);   g_hbmStatusCrop   = NULL; }

    constexpr int SZ = 16;
    COLORREF bgCr = g_darkMode ? RGB(45, 45, 48) : GetSysColor(COLOR_3DFACE);
    Color bgClr(255, GetRValue(bgCr), GetGValue(bgCr), GetBValue(bgCr));
    Color fgClr = g_darkMode ? Color(255, 200, 200, 200) : Color(255, 45, 45, 45);

    auto make = [&](const char* d) -> HBITMAP {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = SZ;
        bi.bmiHeader.biHeight      = -SZ;
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 24;
        bi.bmiHeader.biCompression = BI_RGB;
        void* pBits = nullptr;
        HDC hdcScr = GetDC(NULL);
        HBITMAP hbm = CreateDIBSection(hdcScr, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
        ReleaseDC(NULL, hdcScr);
        if (!hbm) return NULL;

        HDC hdcMem = CreateCompatibleDC(NULL);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);

        // Fill opaque background matching the pane background
        HBRUSH hbr = CreateSolidBrush(bgCr);
        RECT rc = { 0, 0, SZ, SZ };
        FillRect(hdcMem, &rc, hbr);
        DeleteObject(hbr);

        // Render the icon path with GDI+ — done once per theme change, not per frame
        Graphics g(hdcMem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        GraphicsPath path(FillModeWinding);
        AddSvgPath(path, d, SZ / 20.0f);
        SolidBrush br(fgClr);
        g.FillPath(&br, &path);

        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        return hbm;
    };

    g_hbmStatusCursor = make(kStatusCursorD);
    g_hbmStatusBorder = make(kStatusBorderD);
    g_hbmStatusCrop   = make(kStatusCropD);
}

// Overrides WM_NCPAINT on edit controls in dark mode so the WS_EX_CLIENTEDGE border
// is drawn in a muted dark colour instead of the system's light-grey sunken border.
static LRESULT CALLBACK DarkEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                              LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_NCPAINT && g_darkMode) {
        HDC hdc = GetWindowDC(hwnd);
        if (hdc) {
            RECT rc; GetWindowRect(hwnd, &rc);
            OffsetRect(&rc, -rc.left, -rc.top);
            // Fill the entire non-client frame area (covers the system's light border)
            HBRUSH hBr = CreateSolidBrush(RGB(60, 60, 63));
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);
            // Draw a single-pixel dark border
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(85, 85, 90));
            HPEN hOld = (HPEN)SelectObject(hdc, hPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            SelectObject(hdc, hOld);
            DeleteObject(hPen);
            ReleaseDC(hwnd, hdc);
        }
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Dark-mode owner-draw for the "Contiguous" checkbox on the wand colour strip.
// Bypasses uxtheme entirely by handling WM_PAINT ourselves.
static LRESULT CALLBACK WandContigSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                LPARAM lParam, UINT_PTR uid, DWORD_PTR data)
{
    if (msg == WM_PAINT && g_darkMode) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        // Background — match the colour strip
        HBRUSH bgBr = CreateSolidBrush(RGB(45,45,48));
        FillRect(hdc, &rc, bgBr);
        DeleteObject(bgBr);

        // Checkbox box (13×13, vertically centred)
        const int cbSz = 13;
        int bx = rc.left + 1;
        int by = rc.top + (rc.bottom - rc.top - cbSz) / 2;
        RECT cbRc = {bx, by, bx + cbSz, by + cbSz};
        HPEN bPen = CreatePen(PS_SOLID, 1, RGB(160,160,165));
        HPEN oldPen = (HPEN)SelectObject(hdc, bPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, cbRc.left, cbRc.top, cbRc.right, cbRc.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(bPen);

        bool checked = (SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (checked) {
            HPEN ckPen = CreatePen(PS_SOLID, 2, RGB(220,220,220));
            oldPen = (HPEN)SelectObject(hdc, ckPen);
            MoveToEx(hdc, bx + 2, by + cbSz/2,     NULL);
            LineTo  (hdc, bx + cbSz/3 + 1, by + cbSz - 3);
            LineTo  (hdc, bx + cbSz - 2,   by + 2);
            SelectObject(hdc, oldPen);
            DeleteObject(ckPen);
        }

        // Label text
        wchar_t text[64] = {};
        GetWindowTextW(hwnd, text, 64);
        SetTextColor(hdc, RGB(210,210,210));
        SetBkMode(hdc, TRANSPARENT);
        HFONT hf = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        HFONT oldFont = hf ? (HFONT)SelectObject(hdc, hf) : NULL;
        RECT textRc = {cbRc.right + 4, rc.top, rc.right, rc.bottom};
        DrawTextW(hdc, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (oldFont) SelectObject(hdc, oldFont);

        EndPaint(hwnd, &ps);
        return 0;
    }
    // In light mode fall through; also pass all other messages through
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Suppresses the status bar's default WM_ERASEBKGND so the system doesn't flash
// a light background before WM_DRAWITEM fires in dark mode.
static LRESULT CALLBACK StatusBarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                               LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_ERASEBKGND) {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_darkMode ? g_hDarkBgBrush : GetSysColorBrush(COLOR_3DFACE));
        // Vertical separator before folder browser button
        if (g_sbFolderSepX > 0) {
            int top = 3, bot = rc.bottom - 3;
            HPEN hLight = CreatePen(PS_SOLID, 1,
                g_darkMode ? RGB(80,80,80) : GetSysColor(COLOR_3DHIGHLIGHT));
            HPEN hShade = CreatePen(PS_SOLID, 1,
                g_darkMode ? RGB(55,55,55) : GetSysColor(COLOR_3DSHADOW));
            HPEN hOld = (HPEN)SelectObject(hdc, hShade);
            MoveToEx(hdc, g_sbFolderSepX,     top, nullptr);
            LineTo  (hdc, g_sbFolderSepX,     bot);
            SelectObject(hdc, hLight);
            MoveToEx(hdc, g_sbFolderSepX + 1, top, nullptr);
            LineTo  (hdc, g_sbFolderSepX + 1, bot);
            SelectObject(hdc, hOld);
            DeleteObject(hLight); DeleteObject(hShade);
        }
        return TRUE;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Brush popup type/icon data — file-scope so WM_DRAWITEM can reach it
static const struct {
    UINT id; const wchar_t* name; BrushStyle type;
} kBrushTypes[BRUSH_COUNT] = {
    { ID_BRUSH_ROUND,       L"Round",       BRUSH_ROUND       },
    { ID_BRUSH_CALLIGRAPHY, L"Calligraphy", BRUSH_CALLIGRAPHY },
    { ID_BRUSH_AIRBRUSH,    L"Airbrush",    BRUSH_AIRBRUSH    },
    { ID_BRUSH_MARKER,      L"Marker",      BRUSH_MARKER      },
    { ID_BRUSH_CRAYON,      L"Crayon",      BRUSH_CRAYON      },
    { ID_BRUSH_OIL,         L"Oil",         BRUSH_OIL         },
};
static const struct { const char* d; float coordSz; float strokeW; } kBrushIconDefs[BRUSH_COUNT] = {
        { "M10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17"
          "C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3Z"
          "M2 10C2 5.58172 5.58172 2 10 2C14.4183 2 18 5.58172 18 10"
          "C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10Z", 20.0f, 0.f },
        { "M348.596,604.309 830.20829,64.215005"
          "c31.51663,-35.343667 93.81847,-23.060479 124.98992,4.586787"
          "27.60598,24.484898 46.87249,89.358608 11.46696,120.403138"
          "L417.398,670.817c-10.62816,-25.74673 -35.2774,-51.83393 -68.802,-66.508z"
          "M220.54763,617.85771"
          "c38.07589,-31.9548 87.03891,-34.00561 128.04837,-13.54871"
          "30.25341,15.09142 59.75112,36.58885 68.802,66.508"
          "20.36842,67.33112 7.57313,131.35468 -33.46673,177.7238"
          "C295.14639,948.8549 162.40004,968.67841 29.190128,987.59933"
          "101.16928,958.49339 169.37111,894.6818 171.08658,777.59258"
          "c0.9762,-66.62991 7.90095,-124.85598 49.46105,-159.73487z", 1024.0f, 1.2f },
        { "M9.5 4C9.776 4 10 3.776 10 3.5C10 3.224 9.776 3 9.5 3C9.224 3 9 3.224 9 3.5C9 3.776 9.224 4 9.5 4Z"
          "M11 2.5C11 2.224 11.224 2 11.5 2C11.776 2 12 2.224 12 2.5C12 2.776 11.776 3 11.5 3C11.224 3 11 2.776 11 2.5Z"
          "M11 4.5C11 4.224 11.224 4 11.5 4C11.776 4 12 4.224 12 4.5C12 4.776 11.776 5 11.5 5C11.224 5 11 4.776 11 4.5Z"
          "M13 1.5C13 1.224 13.224 1 13.5 1C13.776 1 14 1.224 14 1.5C14 1.776 13.776 2 13.5 2C13.224 2 13 1.776 13 1.5Z"
          "M13 3.5C13 3.224 13.224 3 13.5 3C13.776 3 14 3.224 14 3.5C14 3.776 13.776 4 13.5 4C13.224 4 13 3.776 13 3.5Z"
          "M13 5.5C13 5.224 13.224 5 13.5 5C13.776 5 14 5.224 14 5.5C14 5.776 13.776 6 13.5 6C13.224 6 13 5.776 13 5.5Z"
          "M9.975 6.372C9.981 6.394 9.985 6.413 9.988 6.436C9.98924 6.4451 9.99068 6.45368 9.99311 6.46225"
          "C9.99656 6.4744 10 6.48651 10 6.5V13.25C10 14.216 9.216 15 8.25 15H4.75C3.784 15 3 14.216 3 13.25"
          "V6.5C3 6.48651 3.00344 6.4744 3.00689 6.46225C3.00932 6.45368 3.01176 6.4451 3.013 6.436"
          "C3.016 6.413 3.02 6.394 3.026 6.372C3.05 6.286 3.086 6.207 3.147 6.146L5.001 4.293V2.5"
          "C5.001 2.224 5.225 2 5.501 2H7.5C7.776 2 8 2.224 8 2.5V4.292L9.854 6.146"
          "C9.915 6.207 9.952 6.287 9.975 6.372Z"
          "M7.001 3H6.001V4H7.001V3Z"
          "M7.294 5H5.708L4.708 6H8.294L7.294 5Z"
          "M8.251 14C8.665 14 9.001 13.664 9.001 13.25V7H4.001V13.25C4.001 13.664 4.337 14 4.751 14H8.251Z", 16.0f, 0.f },
        { "M8.081973,18.76509l19.08188,-16.75355"
          "c0.73506,-0.64537 1.52003,-0.48784 1.91238,-0.0831"
          "l3.94946,4.07419"
          "c0.54909,0.56644 0.42553,1.26969 -0.16629,1.95394"
          "L16.354973,27.03909Z"
          "M8.081973,18.76509l8.273,8.274 -3.11819,3.24232"
          "c-2.54969,-1.01438 -4.558723,-0.82884 -6.11081,0.37368"
          "l-2.786,-2.66"
          "c1.099875,-1.5035 1.81694,-3.592 0.498993,-6.19464z"
          "M0.764776,31.44547 4.339973,27.99509"
          "l2.786,2.66 -1.912848,1.87129z", 34.0f, 1.2f },
        { "M566.1 733.5L339.3 543.2 668.4 151c14.2-16.9 39.5-19.1 56.4-4.9L890.3 285"
          "c8.2 6.9 13.2 16.5 14.1 27.2 0.9 10.6-2.3 21-9.2 29.2L566.1 733.5z"
          "M395.6 538.3l165.5 138.9 303.4-361.6L699 176.7 395.6 538.3z"
          "M479.5 814.4c-9.4 0-18.4-3.3-25.6-9.4L288.3 666.2c-8.2-6.9-13.2-16.5-14.1-27.2"
          "-0.9-10.6 2.3-21 9.2-29.2l82.2-97.9 226.8 190.3-82.2 97.9"
          "c-6.9 8.2-16.5 13.2-27.2 14.1-1.2 0.1-2.4 0.2-3.5 0.2z"
          "m-109-246.2L314 635.5l165.5 138.9 56.4-67.3-165.4-138.9z"
          "M285.5 886.9l-58.2-48.8 65.9-167.9-1.7-1.5 4-4.5 2.7-6.8 2.6 1 17.5-19.3"
          " 22.2 20L275 825.9l15.3 12.9L451 751.9l30.5 23.5-24.5 31.7-9.9-7.7z", 1024.0f, 0.f },
        { "m 8.701,10.054 c 1.55576,-1.9748837 4.517049,-5.9148881 8.231555,-7.9374307"
          " 0.79921,-0.4351685 1.841804,0.0048 1.293459,1.1758718"
          " C 16.578856,6.8102046 14.039646,9.4716048 10.965,12.111"
          " 11.05214,11.097396 9.8474097,9.9906905 8.701,10.054 Z"
          "M 6.0073487,14.010218 C 6.7970108,12.649905 7.6342376,11.414313 8.701,10.054"
          " c 1.2770287,0.02284 2.162993,0.871655 2.264,2.057"
          " C 9.7129891,13.200043 8.4497388,14.153127 7.0882557,15.132699"
          " 6.9416934,14.63823 6.583428,14.234492 6.0073487,14.010218 Z"
          "M 2.2033874,18.105193 C 2.7586121,17.517989 3.1868836,17.075397 3.4713746,16.379899"
          " 3.7657063,15.660342 3.716033,15.048017 4.074188,14.613032"
          " 4.7429538,13.800805 5.3719059,13.89656 6.0489221,14.051791"
          " c 0.4919039,0.112788 0.9052619,0.61516 1.0393337,1.101694"
          " 0.21699,0.787436 -0.1024805,1.509112 -0.6443868,2.120241"
          " -0.5241445,0.591097 -1.3395783,0.772625 -2.1202409,0.893827"
          " -0.693192,0.107623 -2.1202407,-0.06236 -2.1202407,-0.06236 z", 20.0f, 1.0f },
};

// Shows the brush sub-type popup below the Brush toolbar button.
// Called on left-click (WM_COMMAND) so the user always picks the type first.
static void ShowBrushTypePopup(HWND hwnd)
{
    const int ICO_SZ = 16;
    COLORREF crBg   = g_darkMode ? RGB(43, 43, 43)    : GetSysColor(COLOR_MENU);
    COLORREF crText = g_darkMode ? RGB(220, 220, 220) : GetSysColor(COLOR_MENUTEXT);
    Color menuBg (255, GetRValue(crBg),   GetGValue(crBg),   GetBValue(crBg));
    Color iconClr(255, GetRValue(crText), GetGValue(crText), GetBValue(crText));

    HBITMAP hIcons[BRUSH_COUNT] = {};
    for (int i = 0; i < BRUSH_COUNT; ++i) {
        const auto& def = kBrushIconDefs[i];
        if (def.strokeW > 0.f) {
            Bitmap bmp(ICO_SZ, ICO_SZ, PixelFormat32bppRGB);
            {
                Graphics g(&bmp);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                g.Clear(menuBg);
                float scale = (float)ICO_SZ / def.coordSz;
                GraphicsPath path(FillModeWinding);
                AddSvgPath(path, def.d, scale);
                Pen pen(iconClr, def.strokeW);
                g.DrawPath(&pen, &path);
            }
            HDC hdc = GetDC(NULL);
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbm = CreateCompatibleBitmap(hdc, ICO_SZ, ICO_SZ);
            ReleaseDC(NULL, hdc);
            HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);
            Graphics gDC(hdcMem);
            gDC.DrawImage(&bmp, 0, 0, ICO_SZ, ICO_SZ);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
            hIcons[i] = hbm;
        } else {
            hIcons[i] = MakeBitmapFromSvg(def.d, ICO_SZ, iconClr, def.coordSz, menuBg);
        }
    }

    HMENU hm = CreatePopupMenu();
    for (int i = 0; i < BRUSH_COUNT; ++i) {
        auto& t = kBrushTypes[i];
        AppendMenuW(hm, MF_STRING | (g_brushType == t.type ? MF_CHECKED : 0), t.id, t.name);
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask    = MIIM_BITMAP;
        mii.hbmpItem = hIcons[i];
        SetMenuItemInfoW(hm, t.id, FALSE, &mii);
    }
    RECT rc = {};
    int btnIdx = (int)SendMessage(g_hwndToolbar, TB_COMMANDTOINDEX, ID_TOOL_BRUSH, 0);
    SendMessage(g_hwndToolbar, TB_GETITEMRECT, (WPARAM)btnIdx, (LPARAM)&rc);
    POINT pt = { rc.left, rc.bottom };
    ClientToScreen(g_hwndToolbar, &pt);
    int sel = (int)TrackPopupMenu(hm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
                                  pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hm);
    for (int i = 0; i < BRUSH_COUNT; ++i) if (hIcons[i]) DeleteObject(hIcons[i]);
    for (auto& t : kBrushTypes)
        if (sel == (int)t.id) { g_brushType = t.type; SetTool(TOOL_BRUSH); break; }
}

// ============================================================
//  More-button palette popup
// ============================================================
static void ClosePalette() {
    if (g_hwndPalette) {
        HWND h = g_hwndPalette;
        g_hwndPalette = NULL;   // null first to prevent re-entry from WM_CAPTURECHANGED
        ReleaseCapture();
        DestroyWindow(h);
    }
}

static void ShowPalette() {
    if (g_hwndPalette) { ClosePalette(); return; }   // toggle off

    // Build flat item array: overflow first (L→R order), then permanent shapes
    g_paletteCount = 0;
    for (int i = g_numOverflowed - 1; i >= 0; i--) {
        g_paletteCmds[g_paletteCount] = kOverflowCmds[i];
        g_paletteImgs[g_paletteCount] = kOverflowImgIdx[i];
        g_paletteCount++;
    }
    for (int i = 0; i < kMorePermanentCount; i++) {
        g_paletteCmds[g_paletteCount] = kMorePermanentCmds[i];
        g_paletteImgs[g_paletteCount] = kMorePermanentImgIdx[i];
        g_paletteCount++;
    }

    int rows = (g_paletteCount + PALETTE_COLS - 1) / PALETTE_COLS;
    int w    = PALETTE_COLS * PALETTE_CELL + PALETTE_PAD * 2;
    int h    = rows           * PALETTE_CELL + PALETTE_PAD * 2;

    // Position below the More toolbar button
    RECT rc = {};
    int btnIdx = (int)SendMessage(g_hwndToolbar, TB_COMMANDTOINDEX, ID_MENU_MORE, 0);
    SendMessage(g_hwndToolbar, TB_GETITEMRECT, (WPARAM)btnIdx, (LPARAM)&rc);
    MapWindowPoints(g_hwndToolbar, NULL, (POINT*)&rc, 2);
    int x = rc.left, y = rc.bottom;

    // Keep on-screen
    HMONITOR hMon = MonitorFromPoint({x, y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(hMon, &mi)) {
        if (x + w > mi.rcWork.right)  x = mi.rcWork.right  - w;
        if (y + h > mi.rcWork.bottom) y = rc.top - h;        // flip above button
        if (x < mi.rcWork.left)       x = mi.rcWork.left;
    }

    g_paletteHot    = -1;
    g_hwndPalette   = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"MyPaintPalette", NULL,
        WS_POPUP | WS_BORDER,
        x, y, w, h,
        g_hwndMain, NULL, GetModuleHandle(NULL), NULL);

    if (g_hwndPalette) {
        ShowWindow(g_hwndPalette, SW_SHOWNOACTIVATE);
        SetCapture(g_hwndPalette);
    }
}

static void ShowToolsPalette() {
    if (g_hwndPalette) { ClosePalette(); return; }   // toggle off

    // Build item array: overflowed tools in L→R toolbar order (reverse kToolsOverflowCmds)
    g_paletteCount = 0;
    for (int i = g_numToolsOverflowed - 1; i >= 0; i--) {
        g_paletteCmds[g_paletteCount] = kToolsOverflowCmds[i];
        g_paletteImgs[g_paletteCount] = kToolsOverflowImgIdx[i];
        g_paletteCount++;
    }
    // Append permanently-Tools tools (always present regardless of overflow)
    for (int i = 0; i < kToolsPermanentCount; i++) {
        g_paletteCmds[g_paletteCount] = kToolsPermanentCmds[i];
        g_paletteImgs[g_paletteCount] = kToolsPermanentImgIdx[i];
        g_paletteCount++;
    }
    if (g_paletteCount == 0) return;

    int rows = (g_paletteCount + PALETTE_COLS - 1) / PALETTE_COLS;
    int w    = PALETTE_COLS * PALETTE_CELL + PALETTE_PAD * 2;
    int h    = rows           * PALETTE_CELL + PALETTE_PAD * 2;

    // Position below the Tools toolbar button
    RECT rc = {};
    int btnIdx = (int)SendMessage(g_hwndToolbar, TB_COMMANDTOINDEX, ID_MENU_TOOLS, 0);
    SendMessage(g_hwndToolbar, TB_GETITEMRECT, (WPARAM)btnIdx, (LPARAM)&rc);
    MapWindowPoints(g_hwndToolbar, NULL, (POINT*)&rc, 2);
    int x = rc.left, y = rc.bottom;

    // Keep on-screen
    HMONITOR hMon = MonitorFromPoint({x, y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(hMon, &mi)) {
        if (x + w > mi.rcWork.right)  x = mi.rcWork.right  - w;
        if (y + h > mi.rcWork.bottom) y = rc.top - h;
        if (x < mi.rcWork.left)       x = mi.rcWork.left;
    }

    g_paletteHot  = -1;
    g_hwndPalette = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"MyPaintPalette", NULL,
        WS_POPUP | WS_BORDER,
        x, y, w, h,
        g_hwndMain, NULL, GetModuleHandle(NULL), NULL);

    if (g_hwndPalette) {
        ShowWindow(g_hwndPalette, SW_SHOWNOACTIVATE);
        SetCapture(g_hwndPalette);
    }
}

static LRESULT CALLBACK PaletteWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Colours that match the toolbar icon background (so icons blend seamlessly)
    auto bgColor  = []() { return g_darkMode ? RGB(45,45,48)   : RGB(240,240,240); };
    auto hotColor = []() { return g_darkMode ? RGB(65,65,80)   : RGB(204,232,255); };
    auto actColor = []() { return g_darkMode ? RGB(40,80,130)  : RGB(150,200,240); };
    auto brdColor = []() { return g_darkMode ? RGB(90,90,110)  : RGB(120,170,220); };

    auto cellRect = [](int i, RECT* r) {
        int col = i % PALETTE_COLS, row = i / PALETTE_COLS;
        r->left   = PALETTE_PAD + col * PALETTE_CELL;
        r->top    = PALETTE_PAD + row * PALETTE_CELL;
        r->right  = r->left + PALETTE_CELL;
        r->bottom = r->top  + PALETTE_CELL;
    };

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRc; GetClientRect(hwnd, &clientRc);

        // Fill background
        HBRUSH hbg = CreateSolidBrush(bgColor());
        FillRect(hdc, &clientRc, hbg);
        DeleteObject(hbg);

        for (int i = 0; i < g_paletteCount; i++) {
            RECT cr; cellRect(i, &cr);
            bool isHot    = (i == g_paletteHot);
            bool isActive = (g_paletteCmds[i] == TOOL_COMMANDS[(int)g_currentTool]);

            // Cell fill for hot/active states
            if (isHot || isActive) {
                HBRUSH hfill = CreateSolidBrush(isActive ? actColor() : hotColor());
                FillRect(hdc, &cr, hfill);
                DeleteObject(hfill);
            }

            // 1px border for hot/active
            if (isHot || isActive) {
                HPEN hp   = CreatePen(PS_SOLID, 1, brdColor());
                HPEN oldP = (HPEN)SelectObject(hdc, hp);
                HBRUSH oldB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, cr.left, cr.top, cr.right, cr.bottom);
                SelectObject(hdc, oldP); SelectObject(hdc, oldB);
                DeleteObject(hp);
            }

            // Icon centred in cell
            if (g_hToolImageList) {
                int ix = cr.left + (PALETTE_CELL - 22) / 2;
                int iy = cr.top  + (PALETTE_CELL - 22) / 2;
                ImageList_DrawEx(g_hToolImageList, g_paletteImgs[i],
                    hdc, ix, iy, 22, 22, CLR_NONE, CLR_NONE, ILD_NORMAL);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int newHot = -1;
        for (int i = 0; i < g_paletteCount; i++) {
            RECT cr; cellRect(i, &cr);
            if (PtInRect(&cr, pt)) { newHot = i; break; }
        }
        if (newHot != g_paletteHot) {
            g_paletteHot = newHot;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        // Click outside palette → dismiss
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT cr; GetClientRect(hwnd, &cr);
        if (!PtInRect(&cr, pt))
            ClosePalette();
        return 0;
    }

    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (int i = 0; i < g_paletteCount; i++) {
            RECT cr; cellRect(i, &cr);
            if (PtInRect(&cr, pt)) {
                UINT cmd = g_paletteCmds[i];
                ClosePalette();
                SendMessage(g_hwndMain, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
                return 0;
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) ClosePalette();
        return 0;

    case WM_CAPTURECHANGED:
        // Another window grabbed capture (e.g. Alt-Tab) — close cleanly
        if (g_hwndPalette) {
            HWND h = g_hwndPalette;
            g_hwndPalette = NULL;
            DestroyWindow(h);
        }
        return 0;

    case WM_DESTROY:
        // Restore correct More-button check state in case user dismissed without selecting
        if (g_hwndToolbar) {
            bool inMore = false;
            for (int i = 0; i < kMorePermanentCount && !inMore; i++)
                if (TOOL_COMMANDS[(int)g_currentTool] == kMorePermanentCmds[i]) inMore = true;
            for (int i = 0; i < g_numOverflowed && !inMore; i++)
                if (TOOL_COMMANDS[(int)g_currentTool] == kOverflowCmds[i]) inMore = true;
            SendMessage(g_hwndToolbar, TB_CHECKBUTTON, ID_MENU_MORE, MAKELONG(inMore, 0));
            // Restore Tools check state
            bool inTools = false;
            for (int i = 0; i < g_numToolsOverflowed && !inTools; i++)
                if (TOOL_COMMANDS[(int)g_currentTool] == kToolsOverflowCmds[i]) inTools = true;
            for (int i = 0; i < kToolsPermanentCount && !inTools; i++)
                if (TOOL_COMMANDS[(int)g_currentTool] == kToolsPermanentCmds[i]) inTools = true;
            SendMessage(g_hwndToolbar, TB_CHECKBUTTON, ID_MENU_TOOLS, MAKELONG(inTools, 0));
            // Re-check the current tool button (BTNS_CHECKGROUP unchecked it when More was clicked)
            SendMessage(g_hwndToolbar, TB_CHECKBUTTON,
                TOOL_COMMANDS[(int)g_currentTool], MAKELONG(TRUE, 0));
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Create a 20×20 HBITMAP from the tool image list — for use as MENUITEMINFO.hbmpItem.
// Caller is responsible for DeleteObject on the returned handle.
static HBITMAP MakeMenuBitmap(HIMAGELIST himl, int idx) {
    if (!himl) return NULL;
    HICON hIcon = ImageList_GetIcon(himl, idx, ILD_NORMAL);
    if (!hIcon) return NULL;
    const int SZ = 20;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = SZ;
    bi.bmiHeader.biHeight      = -SZ;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* pv = nullptr;
    HBITMAP hbm = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pv, NULL, 0);
    if (!hbm) { DestroyIcon(hIcon); return NULL; }
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hOld = (HBITMAP)SelectObject(hdc, hbm);
    DrawIconEx(hdc, 0, 0, hIcon, SZ, SZ, 0, NULL, DI_NORMAL);
    SelectObject(hdc, hOld);
    DeleteDC(hdc);
    DestroyIcon(hIcon);
    return hbm;
}

// CBT hook: fires at HCBT_CREATEWND for every window on this thread.
// WS_EX_DLGMODALFRAME is passed directly in the CreateWindowEx dwExStyle for
// MFT_OWNERDRAW popup menus — it is never added via SetWindowLong, so
// WM_STYLECHANGING never fires for it.  The only reliable intercept point is here,
// where lParam points to a CBT_CREATEWND whose lpcs->dwExStyle we can modify before
// user32 commits it.  We also subclass the window for dark background/border painting.
static LRESULT CALLBACK MenuCBTProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HCBT_CREATEWND && g_darkMode) {
        CBT_CREATEWND* pcw = reinterpret_cast<CBT_CREATEWND*>(lp);
        if (pcw && pcw->lpcs) {
            // lpszClass is either a string pointer or an ATOM (stored as low-word pointer).
            // The popup-menu class "#32768" has atom value 32768 = 0x8000.
            const wchar_t* cls = reinterpret_cast<const wchar_t*>(pcw->lpcs->lpszClass);
            bool isPopupMenu = IS_INTRESOURCE(cls)
                               ? (reinterpret_cast<ULONG_PTR>(cls) == 32768)
                               : (wcscmp(cls, L"#32768") == 0);
            if (isPopupMenu) {
                // Strip before the window is fully created — this is the only moment
                // that reliably prevents the thick light 3-D dialog border in dark mode.
                pcw->lpcs->dwExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_STATICEDGE);
                SetWindowSubclass((HWND)wp, DarkMenuSubclassProc, 0, 0);
            }
        }
    }
    return CallNextHookEx(g_hMenuCBTHook, code, wp, lp);
}

// Subclass proc for the #32768 popup-menu window — fills background and border dark.
// SetWindowTheme("","") is applied to the popup before this subclass intercepts
// WM_NCPAINT, switching the NC renderer to classic GDI (no DWM compositing), so
// our FrameRect there is authoritative and won't be overwritten by DWM.
static LRESULT CALLBACK DarkMenuSubclassProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp,
                                              UINT_PTR uid, DWORD_PTR)
{
    // user32 adds WS_EX_DLGMODALFRAME to MFT_OWNERDRAW popup windows internally
    // during WM_NCCREATE via SetWindowLong, which triggers WM_STYLECHANGING.
    // Return 0 without forwarding to DefSubclassProc: the original menu window proc
    // would restore WS_EX_DLGMODALFRAME into styleNew if we let it run.
    if (msg == WM_STYLECHANGING && wp == (WPARAM)(LONG_PTR)GWL_EXSTYLE && g_darkMode) {
        reinterpret_cast<STYLESTRUCT*>(lp)->styleNew &=
            ~(DWORD)(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_STATICEDGE);
        return 0;
    }
    // Belt-and-suspenders: strip after WM_NCCREATE's default processing but before
    // WM_NCCALCSIZE computes the NC metrics — the window is not yet visible at this point.
    if (msg == WM_NCCREATE && g_darkMode) {
        LRESULT r = DefSubclassProc(hw, msg, wp, lp);
        LONG_PTR ex = GetWindowLongPtrW(hw, GWL_EXSTYLE);
        SetWindowLongPtrW(hw, GWL_EXSTYLE,
            ex & ~(LONG_PTR)(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_STATICEDGE));
        return r;
    }
    if (msg == WM_ERASEBKGND && g_darkMode) {
        RECT rc; GetClientRect(hw, &rc);
        FillRect((HDC)wp, &rc, g_hDarkBgBrush);
        return 1;
    }
    if (msg == WM_NCPAINT && g_darkMode) {
        // Classic renderer — GDI-only, no DWM override.
        // Call DefSubclassProc first (handles scroll arrows), then flood every NC
        // strip dark.  A plain FrameRect only covers 1 px; when WS_EX_DLGMODALFRAME
        // is present (added by Windows for MFT_OWNERDRAW menus) the NC strips are
        // 3-4 px wide and the inner pixels would show light system border colours.
        // Computing the strips from GetClientRect + ClientToScreen covers the exact
        // NC area regardless of how many pixels wide it happens to be.
        DefSubclassProc(hw, msg, wp, lp);
        HDC hdc = GetWindowDC(hw);
        if (hdc) {
            RECT wr; GetWindowRect(hw, &wr);
            int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
            RECT cr; GetClientRect(hw, &cr);
            POINT pt = {0, 0}; ClientToScreen(hw, &pt);
            int cx = pt.x - wr.left, cy = pt.y - wr.top;
            HBRUSH hDark = CreateSolidBrush(RGB(43, 43, 43));
            RECT t = {0, 0,              ww, cy},
                 b = {0, cy + cr.bottom, ww, wh},
                 l = {0, cy,             cx, cy + cr.bottom},
                 r = {cx + cr.right, cy, ww, cy + cr.bottom};
            FillRect(hdc, &t, hDark); FillRect(hdc, &b, hDark);
            FillRect(hdc, &l, hDark); FillRect(hdc, &r, hDark);
            DeleteObject(hDark);
            RECT outer = {0, 0, ww, wh};
            HBRUSH hFr = CreateSolidBrush(RGB(43, 43, 43));
            FrameRect(hdc, &outer, hFr);
            DeleteObject(hFr);
            ReleaseDC(hw, hdc);
        }
        return 0;
    }
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hw, DarkMenuSubclassProc, uid);
    return DefSubclassProc(hw, msg, wp, lp);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (mis->CtlType != ODT_MENU) break;
        // Separator
        if (mis->itemData == 0) { mis->itemHeight = 5; mis->itemWidth = 0; return TRUE; }
        mis->itemHeight = (UINT)max(GetSystemMetrics(SM_CYMENU), 20);
        // Measure label + shortcut text using the menu font
        UINT cmdId = (UINT)mis->itemData;
        const wchar_t* text = L"";
        for (auto& e : kFileMenuEntries) if (e.cmdId == cmdId) { text = e.text; break; }
        NONCLIENTMETRICSW ncm = { sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        HFONT hf  = CreateFontIndirectW(&ncm.lfMenuFont);
        HDC   hdc = GetDC(hwnd);
        HGDIOBJ old = SelectObject(hdc, hf);
        const wchar_t* tab = wcschr(text, L'\t');
        int labelLen = tab ? (int)(tab - text) : (int)wcslen(text);
        SIZE szL = {}, szS = {};
        GetTextExtentPoint32W(hdc, text, labelLen, &szL);
        if (tab) GetTextExtentPoint32W(hdc, tab + 1, (int)wcslen(tab + 1), &szS);
        SelectObject(hdc, old); DeleteObject(hf); ReleaseDC(hwnd, hdc);
        // 20px icon col + 6 gap + label + 24 col gap + shortcut + 8 right margin
        mis->itemWidth = 20 + 6 + szL.cx + (szS.cx ? 24 + szS.cx : 0) + 8;
        return TRUE;
    }

    case WM_INITMENUPOPUP: {
        // Dark-mode only: fix the popup border and fill the background dark.
        // MFT_OWNERDRAW causes Windows to add WS_EX_DLGMODALFRAME *after* creation
        // (not during), so we strip it here in WM_INITMENUPOPUP where it is present.
        // SetWindowTheme("","") switches to the classic (non-DWM) renderer, giving a
        // thin system-color border instead of the thick themed one.
        // Exception: the file menu uses MIIM_BITMAP icons, which need the themed renderer
        // so that Windows uses AlphaBlend (not BitBlt) to composite them.  Classic mode
        // ignores alpha and renders transparent pixels as solid black.
        if (g_darkMode) {
            EnumThreadWindows(GetCurrentThreadId(), [](HWND hw, LPARAM) -> BOOL {
                wchar_t cls[16] = {};
                GetClassNameW(hw, cls, 16);
                if (wcscmp(cls, L"#32768") != 0) return TRUE;

                // Subclass FIRST so that any redraws triggered below go through
                // our dark WM_ERASEBKGND and WM_NCPAINT handlers.
                SetWindowSubclass(hw, DarkMenuSubclassProc, 0, 0);

                // Classic (non-DWM) renderer: WM_NCPAINT is now GDI-only so our
                // subclass's NC fill won't be composited over by DWM.
                SetWindowTheme(hw, L"", L"");

                // Strip the extra extended styles added for ownerdraw menus.
                LONG_PTR ex = GetWindowLongPtrW(hw, GWL_EXSTYLE);
                LONG_PTR newEx = ex & ~WS_EX_DLGMODALFRAME & ~WS_EX_WINDOWEDGE & ~WS_EX_STATICEDGE;
                if (newEx != ex) {
                    SetWindowLongPtrW(hw, GWL_EXSTYLE, newEx);
                    SetWindowPos(hw, NULL, 0,0,0,0,
                        SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOACTIVATE);
                }
                return TRUE;
            }, 0);
        }
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

        // File-menu owner-draw items
        if (dis->CtlType == ODT_MENU) {
            // Separator
            if (dis->itemData == 0) {
                COLORREF bg  = g_darkMode ? RGB(43, 43, 43) : GetSysColor(COLOR_MENU);
                HBRUSH hBg = CreateSolidBrush(bg); FillRect(dis->hDC, &dis->rcItem, hBg); DeleteObject(hBg);
                int midY = (dis->rcItem.top + dis->rcItem.bottom) / 2;
                COLORREF ln = g_darkMode ? RGB(80, 80, 80) : RGB(200, 200, 200);
                HPEN hPen = CreatePen(PS_SOLID, 1, ln);
                HGDIOBJ old = SelectObject(dis->hDC, hPen);
                MoveToEx(dis->hDC, dis->rcItem.left + 2, midY, NULL);
                LineTo(dis->hDC, dis->rcItem.right - 2, midY);
                SelectObject(dis->hDC, old); DeleteObject(hPen);
                return TRUE;
            }
            bool sel    = (dis->itemState & ODS_SELECTED) != 0;
            bool grayed = (dis->itemState & ODS_GRAYED)   != 0;
            COLORREF bgClr, fgClr;
            if (g_darkMode) {
                bgClr = sel ? RGB(51, 82, 122) : RGB(43, 43, 43);
                fgClr = grayed ? RGB(100,100,100) : (sel ? RGB(255,255,255) : RGB(220,220,220));
            } else {
                bgClr = sel ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_MENU);
                fgClr = grayed ? GetSysColor(COLOR_GRAYTEXT)
                               : (sel ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT));
            }
            HBRUSH hBg = CreateSolidBrush(bgClr);
            FillRect(dis->hDC, &dis->rcItem, hBg);
            DeleteObject(hBg);

            UINT cmdId = (UINT)dis->itemData;
            int  itemH = dis->rcItem.bottom - dis->rcItem.top;
            int  iconSz = 16;
            int  iconX  = dis->rcItem.left + 2;
            int  iconY  = dis->rcItem.top  + (itemH - iconSz) / 2;
            Color iconFill(255, GetRValue(fgClr), GetGValue(fgClr), GetBValue(fgClr));
            DrawFileMenuIcon(dis->hDC, cmdId, iconX, iconY, iconSz, iconFill);

            const wchar_t* text = L"";
            for (auto& e : kFileMenuEntries) if (e.cmdId == cmdId) { text = e.text; break; }
            NONCLIENTMETRICSW ncm = { sizeof(ncm) };
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            HFONT   hf  = CreateFontIndirectW(&ncm.lfMenuFont);
            HGDIOBJ old = SelectObject(dis->hDC, hf);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, fgClr);

            const wchar_t* tab = wcschr(text, L'\t');
            int labelLen = tab ? (int)(tab - text) : (int)wcslen(text);
            UINT dtf = DT_SINGLELINE | DT_VCENTER | DT_LEFT;
            if (dis->itemState & ODS_NOACCEL) dtf |= DT_HIDEPREFIX;
            RECT rcLabel = dis->rcItem;
            rcLabel.left = iconX + iconSz + 6;
            DrawTextW(dis->hDC, text, labelLen, &rcLabel, dtf);
            if (tab) {
                RECT rcS = dis->rcItem; rcS.right -= 8;
                DrawTextW(dis->hDC, tab + 1, -1, &rcS,
                          DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
            }
            SelectObject(dis->hDC, old); DeleteObject(hf);
            return TRUE;
        }

        // All BS_OWNERDRAW buttons (toolbar overlays, zoom, colour-strip)
        if (dis->hwndItem == g_hwndBtnDark    || dis->hwndItem == g_hwndBtnHelp  ||
            dis->hwndItem == g_hwndZoomIn     || dis->hwndItem == g_hwndZoomOut  ||
            dis->hwndItem == g_hwndZoomFit    ||
            dis->hwndItem == g_hwndFolderBrowserBtn ||
            dis->hwndItem == g_hwndColorEdit  || dis->hwndItem == g_hwndReset    ||
            dis->hwndItem == g_hwndSzMinus    || dis->hwndItem == g_hwndSzPlus   ||
            dis->hwndItem == g_hwndEraserSq   || dis->hwndItem == g_hwndEraserRd ||
            dis->hwndItem == g_hwndSmudgeSq   || dis->hwndItem == g_hwndSmudgeRd) {
            HDC  hdc   = dis->hDC;
            RECT rc    = dis->rcItem;
            UINT state = dis->itemState;

            // Background matched to toolbar strip
            COLORREF bgClr = g_darkMode ? RGB(45, 45, 48) : RGB(240, 240, 240);
            HBRUSH hbBg = CreateSolidBrush(bgClr);
            FillRect(hdc, &rc, hbBg);
            DeleteObject(hbBg);

            // Hover / pressed — driven by our own mouse-tracking subclass, not ODS_HOTLIGHT
            bool isHot;
            if      (dis->hwndItem == g_hwndBtnDark)   isHot = g_btnDarkHot;
            else if (dis->hwndItem == g_hwndBtnHelp)   isHot = g_btnHelpHot;
            else if (dis->hwndItem == g_hwndZoomIn)    isHot = g_btnZoomInHot;
            else if (dis->hwndItem == g_hwndZoomOut)   isHot = g_btnZoomOutHot;
            else if (dis->hwndItem == g_hwndZoomFit)      isHot = g_btnZoomFitHot;
            else if (dis->hwndItem == g_hwndFolderBrowserBtn) isHot = g_btnFolderBrowserHot;
            else if (dis->hwndItem == g_hwndColorEdit) isHot = g_btnColorEditHot;
            else if (dis->hwndItem == g_hwndReset)     isHot = g_btnResetHot;
            else if (dis->hwndItem == g_hwndSzMinus)   isHot = g_btnSzMinusHot;
            else if (dis->hwndItem == g_hwndSzPlus)    isHot = g_btnSzPlusHot;
            else if (dis->hwndItem == g_hwndEraserSq)  isHot = g_btnEraserSqHot;
            else if (dis->hwndItem == g_hwndEraserRd)  isHot = g_btnEraserRdHot;
            else if (dis->hwndItem == g_hwndSmudgeSq)  isHot = g_btnSmudgeSqHot;
            else                                        isHot = g_btnSmudgeRdHot;
            // Eraser/smudge buttons show active (pressed) highlight when their shape is selected
            bool isActive = (dis->hwndItem == g_hwndEraserSq    && !g_eraserRound)  ||
                            (dis->hwndItem == g_hwndEraserRd    &&  g_eraserRound)   ||
                            (dis->hwndItem == g_hwndSmudgeSq    && !g_smudgeRound)   ||
                            (dis->hwndItem == g_hwndSmudgeRd    &&  g_smudgeRound)   ||
                            (dis->hwndItem == g_hwndFolderBrowserBtn &&  g_showFolderBrowser);
            bool isPressed = (state & ODS_SELECTED) != 0 || isActive;
            if (isHot || isPressed) {
                HTHEME ht = OpenThemeData(g_hwndToolbar, L"Toolbar");
                if (ht) {
                    int ts = isPressed ? 3 /*TS_PRESSED*/ : 2 /*TS_HOT*/;
                    DrawThemeBackground(ht, hdc, 1 /*TP_BUTTON*/, ts, &rc, NULL);
                    CloseThemeData(ht);
                }
            }

            // Render SVG icon directly with GDI+ for crisp anti-aliased output
            {
                const char* svgD;
                float vb, vbOff = 0.f;
                int pad;
                if (dis->hwndItem == g_hwndBtnDark) {
                    svgD  = g_darkMode ? kSunD : kMoonD;
                    vb    = g_darkMode ? kSunVB : kMoonVB;
                    vbOff = (!g_darkMode) ? kMoonOff : 0.f;
                    pad   = 4;
                } else if (dis->hwndItem == g_hwndBtnHelp) {
                    svgD = kHelpBtnD;   vb = 20.f; pad = 8;
                } else if (dis->hwndItem == g_hwndZoomIn) {
                    svgD = kZoomInD;    vb = 20.f; pad = 2;
                } else if (dis->hwndItem == g_hwndZoomOut) {
                    svgD = kZoomOutD;   vb = 20.f; pad = 2;
                } else if (dis->hwndItem == g_hwndZoomFit) {
                    // Show SlideSize (1:1) when fitted; PageFit when not fitted
                    svgD = g_zoomFitMode ? kFolderSizeD : kPageFitD; vb = 20.f; pad = 2;
                } else if (dis->hwndItem == g_hwndFolderBrowserBtn) {
                    svgD = kFolderBrowserIconD; vb = 20.f; pad = 2;
                } else if (dis->hwndItem == g_hwndColorEdit) {
                    svgD = kColorEditD; vb = 20.f; pad = 2;
                } else {
                    svgD = nullptr; vb = 20.f; pad = 0; // text buttons handled below
                }
                int bw = rc.right - rc.left, bh = rc.bottom - rc.top;
                int sz = min(bw, bh) - pad;
                if (svgD && sz > 0) {
                    float scale = (float)sz / vb;
                    float ox = rc.left + (bw - sz) / 2.0f;
                    float oy = rc.top  + (bh - sz) / 2.0f;
                    Color iconClr = g_darkMode
                        ? Color(255, 200, 200, 200)
                        : Color(255,  45,  45,  45);
                    Graphics gfx(hdc);
                    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                    gfx.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    Matrix mat;
                    mat.Translate(ox + vbOff * scale, oy + vbOff * scale);
                    gfx.SetTransform(&mat);
                    GraphicsPath path(FillModeWinding);
                    AddSvgPath(path, svgD, scale);
                    SolidBrush br(iconClr);
                    gfx.FillPath(&br, &path);
                }

                // Text-only buttons (Reset ↺, +, −) — draw centred text in theme colour
                if (!svgD &&
                    dis->hwndItem != g_hwndEraserSq &&
                    dis->hwndItem != g_hwndEraserRd &&
                    dis->hwndItem != g_hwndSmudgeSq &&
                    dis->hwndItem != g_hwndSmudgeRd) {
                    const wchar_t* label =
                        (dis->hwndItem == g_hwndReset)  ? L"\u21BA" :
                        (dis->hwndItem == g_hwndSzMinus) ? L"\u2212" : L"+";
                    COLORREF textClr = g_darkMode ? RGB(200, 200, 200) : RGB(45, 45, 45);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, textClr);
                    HFONT hFont = (dis->hwndItem == g_hwndReset) ? g_hSymbolFont
                                : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                    HFONT hOld = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;
                    DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    if (hOld) SelectObject(hdc, hOld);
                }

                // Eraser/smudge shape buttons — draw square or circle stroke inline
                if (dis->hwndItem == g_hwndEraserSq || dis->hwndItem == g_hwndEraserRd ||
                    dis->hwndItem == g_hwndSmudgeSq  || dis->hwndItem == g_hwndSmudgeRd) {
                    bool isRound = (dis->hwndItem == g_hwndEraserRd || dis->hwndItem == g_hwndSmudgeRd);
                    int bw = rc.right - rc.left, bh = rc.bottom - rc.top;
                    float pad = 4.0f;
                    float x = rc.left + pad, y = rc.top + pad;
                    float w = bw - pad * 2.0f, h = bh - pad * 2.0f;
                    Color strokeClr = g_darkMode
                        ? Color(255, 200, 200, 200)
                        : Color(255,  45,  45,  45);
                    Graphics gfx(hdc);
                    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
                    gfx.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    Pen pen(strokeClr, 1.5f);
                    if (isRound)
                        gfx.DrawEllipse(&pen, x, y, w, h);
                    else
                        gfx.DrawRectangle(&pen, x, y, w, h);
                }
            }
            return TRUE;
        }

        // Status bar owner-draw panes
        if (!g_hwndStatus || dis->hwndItem != g_hwndStatus) break;
        int pane = (int)dis->itemID;
        if (pane < 0 || pane >= 6) break;

        // Background
        FillRect(dis->hDC, &dis->rcItem,
            g_darkMode ? g_hDarkBgBrush : GetSysColorBrush(COLOR_3DFACE));

        // Icon for the three labelled panes — BitBlt from a pre-rendered opaque bitmap
        // so the draw is a single atomic GDI operation with no partial-render flash.
        HBITMAP hbmIcon = NULL;
        if      (pane == STATUS_PANE_POS)  hbmIcon = g_hbmStatusCursor;
        else if (pane == STATUS_PANE_SEL)  hbmIcon = g_hbmStatusBorder;
        else if (pane == STATUS_PANE_SIZE) hbmIcon = g_hbmStatusCrop;

        RECT textRc = dis->rcItem;
        textRc.left += 2;
        if (hbmIcon) {
            constexpr int ICO = 16;
            int iconY = dis->rcItem.top + (dis->rcItem.bottom - dis->rcItem.top - ICO) / 2;
            HDC hdcMem = CreateCompatibleDC(dis->hDC);
            HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbmIcon);
            BitBlt(dis->hDC, textRc.left, iconY, ICO, ICO, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
            textRc.left += ICO + 4;
        }

        // Text
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, g_darkMode ? RGB(210, 210, 210) : GetSysColor(COLOR_WINDOWTEXT));
        HFONT hFont = (HFONT)SendMessage(g_hwndStatus, WM_GETFONT, 0, 0);
        HFONT hOld  = hFont ? (HFONT)SelectObject(dis->hDC, hFont) : NULL;
        DrawTextW(dis->hDC, g_statusPaneText[pane], -1, &textRc,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (hOld) SelectObject(dis->hDC, hOld);
        return TRUE;
    }

    case WM_CREATE: {
        g_hwndMain = hwnd;
        g_cfARGB = RegisterClipboardFormat(L"MyPaint-ARGB32");
        DragAcceptFiles(hwnd, TRUE);   // drops anywhere on the window arrive as WM_DROPFILES
        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(icc);
        icc.dwICC  = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        g_hMenuMain = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU_MAIN));
        CreateToolbar(hwnd);
        CreateStatusBar(hwnd);

        // Right-aligned toolbar overlay: Dark Mode toggle and Help buttons
        {
            static const int RB_W = 32, RB_H = TOOLBAR_HEIGHT - 6;
            static const int RB_Y = (TOOLBAR_HEIGHT - RB_H) / 2;
            g_hwndBtnDark = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                0, RB_Y, RB_W, RB_H,
                hwnd, (HMENU)ID_VIEW_DARKMODE, GetModuleHandle(NULL), NULL);
            g_hwndBtnHelp = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                0, RB_Y, RB_W, RB_H,
                hwnd, (HMENU)ID_HELP_ABOUT, GetModuleHandle(NULL), NULL);
            if (g_hwndBtnDark)
                SetWindowSubclass(g_hwndBtnDark, OverlayBtnSubclassProc, 0, (DWORD_PTR)&g_btnDarkHot);
            if (g_hwndBtnHelp)
                SetWindowSubclass(g_hwndBtnHelp, OverlayBtnSubclassProc, 1, (DWORD_PTR)&g_btnHelpHot);
        }

        // Register folder browser class
        {
            WNDCLASSEX wcs = {};
            wcs.cbSize        = sizeof(wcs);
            wcs.lpfnWndProc   = FolderBrowserWndProc;
            wcs.hInstance     = GetModuleHandle(NULL);
            wcs.hCursor       = LoadCursor(NULL, IDC_ARROW);
            wcs.lpszClassName = L"MyPaintFolderBrowser";
            RegisterClassEx(&wcs);

            g_hwndFolderBrowser = CreateWindowEx(0, L"MyPaintFolderBrowser", NULL,
                WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,   // rename box
                0, 0, 0, FOLDER_BROWSER_H,
                hwnd, nullptr, GetModuleHandle(NULL), NULL);
            // hidden by default — shown when toggled

            wcs.lpfnWndProc   = FolderSearchWndProc;
            wcs.lpszClassName = L"MyPaintFolderSearch";
            RegisterClassEx(&wcs);
            g_hwndFolderSearch = CreateWindowEx(0, L"MyPaintFolderSearch", NULL,
                WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                0, 0, FOLDER_SEARCH_W, FOLDER_BROWSER_H, hwnd, nullptr, GetModuleHandle(NULL), NULL);
            // shown by Ctrl+F
        }

        // Register canvas class
        WNDCLASSEX wcc = {};
        wcc.cbSize      = sizeof(wcc);
        wcc.style       = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
        wcc.lpfnWndProc = CanvasWndProc;
        wcc.hInstance   = GetModuleHandle(NULL);
        wcc.hCursor     = LoadCursor(NULL, IDC_CROSS);
        wcc.lpszClassName = L"MyPaintCanvas";
        RegisterClassEx(&wcc);

        // Register palette popup class
        WNDCLASSEX wcp = {};
        wcp.cbSize        = sizeof(wcp);
        wcp.lpfnWndProc   = PaletteWndProc;
        wcp.hInstance     = GetModuleHandle(NULL);
        wcp.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wcp.lpszClassName = L"MyPaintPalette";
        RegisterClassEx(&wcp);

        g_hwndCanvas = CreateWindowEx(WS_EX_CLIENTEDGE,
            L"MyPaintCanvas", NULL,
            WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL,
            0, TOOLBAR_HEIGHT + COLORSTRIP_H, 800, 600,
            hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);

        CreateCanvas(DEFAULT_CANVAS_W, DEFAULT_CANVAS_H, false);
        UpdateStatusBar(0, 0);   // show initial canvas size immediately
        g_pendingCenter = true;  // defer centering until canvas is properly laid out

        // "Edit Colors" button — Fluent color-palette icon
        {
            const int btnY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_COLEDIT_H) / 2;
            g_hwndColorEdit = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_COLEDIT_X, btnY, CS_COLEDIT_W, CS_COLEDIT_H,
                hwnd, (HMENU)ID_COLOR_EDIT, GetModuleHandle(NULL), NULL);
            if (g_hwndColorEdit)
                SetWindowSubclass(g_hwndColorEdit, OverlayBtnSubclassProc, 4, (DWORD_PTR)&g_btnColorEditHot);
        }

        // Reset button — real themed BUTTON with ↺ glyph
        {
            const int resetY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_RESET_H) / 2;
            g_hwndReset = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_RESET_X, resetY, CS_RESET_W, CS_RESET_H,
                hwnd, (HMENU)ID_COLOR_RESET, GetModuleHandle(NULL), NULL);

            // Keep the symbol font for DrawText in WM_DRAWITEM
            if (!g_hSymbolFont)
                g_hSymbolFont = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
            if (g_hwndReset)
                SetWindowSubclass(g_hwndReset, OverlayBtnSubclassProc, 5, (DWORD_PTR)&g_btnResetHot);
        }

        // Line-width controls — all live in the colour strip, fixed positions
        {
            const int ctrlY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_LW_EDIT_H) / 2;

            g_hwndBrushSz = CreateWindowEx(WS_EX_CLIENTEDGE,
                L"EDIT", L"8",
                WS_CHILD | WS_VISIBLE | ES_CENTER,   // no ES_NUMBER — allow decimal point
                CS_LW_EDIT_X, ctrlY, CS_LW_EDIT_W, CS_LW_EDIT_H,
                hwnd, (HMENU)ID_BRUSHSIZE_EDIT, GetModuleHandle(NULL), NULL);
            SendMessage(g_hwndBrushSz, EM_LIMITTEXT, 6, 0);
            // Smaller font so text fits comfortably in the compact edit box
            g_hEditFont = CreateFont(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            if (g_hEditFont)
                SendMessage(g_hwndBrushSz, WM_SETFONT, (WPARAM)g_hEditFont, FALSE);
            if (g_hwndBrushSz)
                SetWindowSubclass(g_hwndBrushSz, DarkEditSubclassProc, 0, 0);

            g_hwndSzMinus = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_LW_MINUS_X, ctrlY, CS_LW_BTN_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_BRUSHSIZE_DOWN, GetModuleHandle(NULL), NULL);

            g_hwndSzPlus = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_LW_PLUS_X, ctrlY, CS_LW_BTN_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_BRUSHSIZE_UP, GetModuleHandle(NULL), NULL);
            if (g_hwndSzMinus)
                SetWindowSubclass(g_hwndSzMinus, OverlayBtnSubclassProc, 6, (DWORD_PTR)&g_btnSzMinusHot);
            if (g_hwndSzPlus)
                SetWindowSubclass(g_hwndSzPlus,  OverlayBtnSubclassProc, 7, (DWORD_PTR)&g_btnSzPlusHot);
        }

        // Eraser shape buttons — hidden initially; shown when TOOL_ERASER is active
        {
            const int ctrlY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_LW_EDIT_H) / 2;
            g_hwndEraserSq = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_LW_ER_SQ_X, ctrlY, CS_LW_BTN_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_ERASER_SQUARE, GetModuleHandle(NULL), NULL);
            g_hwndEraserRd = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_LW_ER_RD_X, ctrlY, CS_LW_BTN_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_ERASER_ROUND, GetModuleHandle(NULL), NULL);
            if (g_hwndEraserSq)
                SetWindowSubclass(g_hwndEraserSq, OverlayBtnSubclassProc, 8, (DWORD_PTR)&g_btnEraserSqHot);
            if (g_hwndEraserRd)
                SetWindowSubclass(g_hwndEraserRd, OverlayBtnSubclassProc, 9, (DWORD_PTR)&g_btnEraserRdHot);
        }

        // Smudge shape buttons — hidden initially; shown when TOOL_SMUDGE is active
        {
            const int ctrlY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_LW_EDIT_H) / 2;
            g_hwndSmudgeSq = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_LW_ER_SQ_X, ctrlY, CS_LW_BTN_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_SMUDGE_SQUARE, GetModuleHandle(NULL), NULL);
            g_hwndSmudgeRd = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                CS_LW_ER_RD_X, ctrlY, CS_LW_BTN_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_SMUDGE_ROUND, GetModuleHandle(NULL), NULL);
            if (g_hwndSmudgeSq)
                SetWindowSubclass(g_hwndSmudgeSq, OverlayBtnSubclassProc, 10, (DWORD_PTR)&g_btnSmudgeSqHot);
            if (g_hwndSmudgeRd)
                SetWindowSubclass(g_hwndSmudgeRd, OverlayBtnSubclassProc, 11, (DWORD_PTR)&g_btnSmudgeRdHot);
        }

        // Wand "Contiguous" checkbox — hidden initially; shown when TOOL_WAND is active
        {
            const int ctrlY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_LW_BTN_H) / 2;
            g_hwndWandContig = CreateWindowEx(0, L"BUTTON", L"Contiguous",
                WS_CHILD | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX,
                CS_WD_CONTIG_X, ctrlY, CS_WD_CONTIG_W, CS_LW_BTN_H,
                hwnd, (HMENU)ID_WAND_CONTIG, GetModuleHandle(NULL), NULL);
            if (g_hwndWandContig) {
                if (g_hEditFont) SendMessage(g_hwndWandContig, WM_SETFONT, (WPARAM)g_hEditFont, FALSE);
                SendMessage(g_hwndWandContig, BM_SETCHECK, BST_CHECKED, 0); // default: contiguous
                SetWindowSubclass(g_hwndWandContig, WandContigSubclassProc, 0, 0);
            }
        }

        // Font controls — hidden initially; shown when TOOL_TEXT is active
        {
            const int ctrlY     = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_TF_BTN_H) / 2;
            HINSTANCE hInst     = GetModuleHandle(NULL);

            // Font face combobox — total height 200 includes dropdown list
            g_hwndFontFace = CreateWindowEx(0, L"COMBOBOX", NULL,
                WS_CHILD | CBS_DROPDOWN | CBS_HASSTRINGS | CBS_AUTOHSCROLL |
                CBS_SORT | WS_VSCROLL | CBS_NOINTEGRALHEIGHT,
                CS_TF_FACE_X, ctrlY, CS_TF_FACE_W, 200,
                hwnd, (HMENU)ID_TEXT_FONT_FACE, hInst, NULL);
            if (g_hwndFontFace) {
                if (g_hEditFont) SendMessage(g_hwndFontFace, WM_SETFONT, (WPARAM)g_hEditFont, FALSE);
                // CB_GETITEMHEIGHT(-1) returns the content height (excludes borders/
                // padding), which is smaller than the total visible height.
                // Subtracting 2 from the content height reduces the total visible
                // height by 2px to match the 18px sibling edit controls.
                // Y is set to the same ctrlY formula used by all other 18px controls
                // so alignment is consistent regardless of theme or DPI.
                // SWP_NOSIZE: never pass 200 as cy or the HWND becomes 200px tall.
                LRESULT nih = SendMessage(g_hwndFontFace, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
                if (nih > 2)
                    SendMessage(g_hwndFontFace, CB_SETITEMHEIGHT, (WPARAM)-1, nih - 2);
                {
                    int faceY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_TF_BTN_H) / 2;
                    SetWindowPos(g_hwndFontFace, NULL, CS_TF_FACE_X, faceY, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                }
                PopulateFontCombo(g_hwndFontFace);
            }

            // Font size edit
            g_hwndFontSize = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"24",
                WS_CHILD | ES_CENTER | ES_NUMBER,
                CS_TF_SIZE_X, ctrlY, CS_TF_SIZE_W, CS_TF_BTN_H,
                hwnd, (HMENU)ID_TEXT_FONT_SIZE, hInst, NULL);
            if (g_hwndFontSize) {
                SendMessage(g_hwndFontSize, EM_LIMITTEXT, 3, 0);
                if (g_hEditFont) SendMessage(g_hwndFontSize, WM_SETFONT, (WPARAM)g_hEditFont, FALSE);
                SetWindowSubclass(g_hwndFontSize, DarkEditSubclassProc, 0, 0);
            }

            // Bold / Italic / Underline toggle buttons
            g_hFontBold = CreateFont(-11, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
            g_hFontItal = CreateFont(-11, 0, 0, 0, FW_NORMAL, TRUE,  FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
            g_hFontUndr = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, TRUE,  FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");

            g_hwndFontBold = CreateWindowEx(0, L"BUTTON", L"B",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                CS_TF_B_X, ctrlY, CS_TF_BTN_W, CS_TF_BTN_H,
                hwnd, (HMENU)ID_TEXT_BOLD, hInst, NULL);
            if (g_hwndFontBold && g_hFontBold)
                SendMessage(g_hwndFontBold, WM_SETFONT, (WPARAM)g_hFontBold, FALSE);

            g_hwndFontItal = CreateWindowEx(0, L"BUTTON", L"I",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                CS_TF_I_X, ctrlY, CS_TF_BTN_W, CS_TF_BTN_H,
                hwnd, (HMENU)ID_TEXT_ITALIC, hInst, NULL);
            if (g_hwndFontItal && g_hFontItal)
                SendMessage(g_hwndFontItal, WM_SETFONT, (WPARAM)g_hFontItal, FALSE);

            g_hwndFontUnder = CreateWindowEx(0, L"BUTTON", L"U",
                WS_CHILD | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                CS_TF_U_X, ctrlY, CS_TF_BTN_W, CS_TF_BTN_H,
                hwnd, (HMENU)ID_TEXT_UNDERLINE, hInst, NULL);
            if (g_hwndFontUnder && g_hFontUndr)
                SendMessage(g_hwndFontUnder, WM_SETFONT, (WPARAM)g_hFontUndr, FALSE);
        }

        // Zoom controls — created here, positioned over the status bar by LayoutChildren
        {
            g_hwndZoomEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"100",
                WS_CHILD | WS_VISIBLE | ES_CENTER,
                0, 0, ZOOM_EDIT_W, ZOOM_BTN_H,
                hwnd, (HMENU)ID_ZOOM_EDIT, GetModuleHandle(NULL), NULL);
            SendMessage(g_hwndZoomEdit, EM_LIMITTEXT, 7, 0);
            if (g_hEditFont)
                SendMessage(g_hwndZoomEdit, WM_SETFONT, (WPARAM)g_hEditFont, FALSE);
            if (g_hwndZoomEdit)
                SetWindowSubclass(g_hwndZoomEdit, DarkEditSubclassProc, 0, 0);

            g_hwndZoomOut = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                0, 0, ZOOM_BTN_W, ZOOM_BTN_H,
                hwnd, (HMENU)ID_ZOOM_OUT, GetModuleHandle(NULL), NULL);
            g_hwndZoomIn = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                0, 0, ZOOM_BTN_W, ZOOM_BTN_H,
                hwnd, (HMENU)ID_ZOOM_IN, GetModuleHandle(NULL), NULL);
            g_hwndZoomFit = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                0, 0, ZOOM_BTN_W, ZOOM_BTN_H,
                hwnd, (HMENU)ID_ZOOM_FIT, GetModuleHandle(NULL), NULL);
            if (g_hwndZoomOut)
                SetWindowSubclass(g_hwndZoomOut, OverlayBtnSubclassProc, 2, (DWORD_PTR)&g_btnZoomOutHot);
            if (g_hwndZoomIn)
                SetWindowSubclass(g_hwndZoomIn,  OverlayBtnSubclassProc, 3, (DWORD_PTR)&g_btnZoomInHot);
            if (g_hwndZoomFit)
                SetWindowSubclass(g_hwndZoomFit, OverlayBtnSubclassProc, 12, (DWORD_PTR)&g_btnZoomFitHot);

            g_hwndFolderBrowserBtn = CreateWindowEx(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
                0, 0, ZOOM_BTN_W, ZOOM_BTN_H,
                hwnd, (HMENU)ID_FOLDER_BROWSER_BTN, GetModuleHandle(NULL), NULL);
            if (g_hwndFolderBrowserBtn)
                SetWindowSubclass(g_hwndFolderBrowserBtn, OverlayBtnSubclassProc, 13, (DWORD_PTR)&g_btnFolderBrowserHot);
        }

        // Alpha (opacity) edit in colour strip
        {
            const int ctrlY = TOOLBAR_HEIGHT + (COLORSTRIP_H - CS_ALPHA_EDIT_H) / 2;
            g_hwndAlphaEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"100",
                WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER,
                CS_ALPHA_EDIT_X, ctrlY, CS_ALPHA_EDIT_W, CS_ALPHA_EDIT_H,
                hwnd, (HMENU)ID_ALPHA_EDIT, GetModuleHandle(NULL), NULL);
            SendMessage(g_hwndAlphaEdit, EM_LIMITTEXT, 3, 0);
            if (g_hEditFont)
                SendMessage(g_hwndAlphaEdit, WM_SETFONT, (WPARAM)g_hEditFont, FALSE);
            SetWindowSubclass(g_hwndAlphaEdit, AlphaEditSubclassProc, 1, 0);
            SetWindowSubclass(g_hwndAlphaEdit, DarkEditSubclassProc,   0, 0);
        }

        LoadSettings();   // restore custom colours, line width, colours from registry
        RecreateFillCursor(); RecreateBrushCursors();  // rebuild with the loaded foreground colour
        UpdateBrushSizeEdit();  // must be AFTER LoadSettings so restored width is shown
        UpdateAlphaEdit();      // restore alpha edit from loaded value
        ApplyDarkMode();        // apply dark/light mode from loaded setting

        // Tooltips for colour strip buttons
        g_hwndColorTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
        if (g_hwndColorTip) {
            TOOLINFO ti = {};
            ti.cbSize   = sizeof(ti);
            ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd     = hwnd;
            if (g_hwndColorEdit) {
                ti.uId      = (UINT_PTR)g_hwndColorEdit;
                ti.lpszText = (LPWSTR)L"Edit Colors";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndReset) {
                ti.uId      = (UINT_PTR)g_hwndReset;
                ti.lpszText = (LPWSTR)L"Reset custom colors to defaults";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndEraserSq) {
                ti.uId      = (UINT_PTR)g_hwndEraserSq;
                ti.lpszText = (LPWSTR)L"Square eraser";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndEraserRd) {
                ti.uId      = (UINT_PTR)g_hwndEraserRd;
                ti.lpszText = (LPWSTR)L"Round eraser";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndSmudgeSq) {
                ti.uId      = (UINT_PTR)g_hwndSmudgeSq;
                ti.lpszText = (LPWSTR)L"Square smudge";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndSmudgeRd) {
                ti.uId      = (UINT_PTR)g_hwndSmudgeRd;
                ti.lpszText = (LPWSTR)L"Round smudge";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndBtnDark) {
                ti.uId      = (UINT_PTR)g_hwndBtnDark;
                ti.lpszText = g_darkMode ? (LPWSTR)L"Toggle light mode (Ctrl+Shift+D)"
                                         : (LPWSTR)L"Toggle dark mode (Ctrl+Shift+D)";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndBtnHelp) {
                ti.uId      = (UINT_PTR)g_hwndBtnHelp;
                ti.lpszText = (LPWSTR)L"Help";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndZoomEdit) {
                ti.uId      = (UINT_PTR)g_hwndZoomEdit;
                ti.lpszText = (LPWSTR)L"Zoom level — type a percentage and press Enter";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndZoomOut) {
                ti.uId      = (UINT_PTR)g_hwndZoomOut;
                ti.lpszText = (LPWSTR)L"Zoom out";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndZoomIn) {
                ti.uId      = (UINT_PTR)g_hwndZoomIn;
                ti.lpszText = (LPWSTR)L"Zoom in";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndZoomFit) {
                ti.uId      = (UINT_PTR)g_hwndZoomFit;
                ti.lpszText = (LPWSTR)L"Toggle zoom to fit / zoom to 100%";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
            if (g_hwndFolderBrowserBtn) {
                ti.uId      = (UINT_PTR)g_hwndFolderBrowserBtn;
                ti.lpszText = (LPWSTR)L"Toggle folder browser";
                SendMessage(g_hwndColorTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
            }
        }

        UpdateTitleBar();
        LayoutChildren(hwnd);   // position zoom controls over status bar before first paint
        // Post a deferred center so it fires after ShowWindow/SetWindowPos settle the final size
        PostMessage(hwnd, WM_APP, 0, 0);
        return 0;
    }

    case WM_ERASEBKGND: {
        if (g_darkMode) {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hwnd, &rc);
            HBRUSH hb = g_hDarkBgBrush ? g_hDarkBgBrush : CreateSolidBrush(RGB(45,45,48));
            FillRect(hdc, &rc, hb);
            if (!g_hDarkBgBrush) DeleteObject(hb);
            return 1;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        if (g_darkMode) {
            HDC hdcCtrl = (HDC)wParam;
            SetTextColor(hdcCtrl, RGB(220, 220, 220));
            SetBkColor(hdcCtrl, RGB(60, 60, 63));
            if (!g_hDarkBgBrush) g_hDarkBgBrush = CreateSolidBrush(RGB(45,45,48));
            return (LRESULT)g_hDarkBgBrush;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_CTLCOLORSTATIC: {
        // Checkbox (BS_AUTOCHECKBOX) sends WM_CTLCOLORSTATIC for its text+background
        if (g_darkMode && (HWND)lParam == g_hwndWandContig) {
            HDC hdcCtrl = (HDC)wParam;
            SetTextColor(hdcCtrl, RGB(220, 220, 220));
            SetBkMode(hdcCtrl, TRANSPARENT);
            if (!g_hDarkBgBrush) g_hDarkBgBrush = CreateSolidBrush(RGB(45,45,48));
            return (LRESULT)g_hDarkBgBrush;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_GETMINMAXINFO: {
        auto* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 315;
        return 0;
    }

    case WM_SIZE: {
        LayoutChildren(hwnd);
        return 0;
    }

    case WM_IMAGE_LOADED:
        FinishImageLoad((AsyncImageLoad*)lParam);
        return 0;

    case WM_DROPFILES: {
        // Open the first dropped image file, as File > Open would
        HDROP drop = (HDROP)wParam;
        wchar_t file[MAX_PATH] = {};
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
        bool found = false;
        for (UINT i = 0; i < count && !found; i++) {
            if (!DragQueryFileW(drop, i, file, MAX_PATH)) continue;
            const wchar_t* dot = wcsrchr(file, L'.');
            found = !(GetFileAttributesW(file) & FILE_ATTRIBUTE_DIRECTORY) && dot && IsImageExtension(dot + 1);
        }
        DragFinish(drop);
        if (!found) {
            MessageBox(hwnd, L"MyPaint can open PNG, JPEG, BMP, GIF, TIFF, WebP, SVG, HEIC and AVIF images.",
                       APP_TITLE, MB_ICONINFORMATION);
            return 0;
        }
        SetForegroundWindow(hwnd);
        if (PromptSaveIfNeeded()) BeginImageLoad(file, false);
        return 0;
    }

    case WM_APP:
        // Deferred from WM_CREATE — runs after ShowWindow/SetWindowPos settle the final layout
        CenterScrollOnCanvas();
        // Open the folder browser if it was open the last time MyPaint was started
        // the same way (with or without an image; it takes focus). This runs after
        // the command-line image is loaded, so it lists that folder
        if ((g_startedWithFile ? g_folderBrowserFromFile : g_folderBrowserFromBlank) && !g_showFolderBrowser)
            ToggleFolderBrowser();
        else if (g_hwndCanvas) SetFocus(g_hwndCanvas);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        PaintColorStrip(hdc, 0, TOOLBAR_HEIGHT, rc.right);
        EndPaint(hwnd, &ps);
        return 0;
    }

    // Left-click on colour strip
    case WM_LBUTTONDOWN: {
        // Clicking anywhere in the main window (outside the canvas child) deselects
        if (g_selFloating) CommitSelectionFloat();
        else if (g_hasSelection) { g_hasSelection = false; InvalidateCanvas(); }

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int hit = HitTestColorStrip(x, y, TOOLBAR_HEIGHT);
        if (hit == 1) {
            // Stroke swatch — select stroke as active
            g_strokeActive = true;
            UpdateAlphaEdit();
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        } else if (hit == 2) {
            // Fill swatch — select fill as active
            g_strokeActive = false;
            UpdateAlphaEdit();
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        } else if (hit >= 3) {
            // Custom colour swatch — apply to active target
            COLORREF col = g_customColors[hit - 3];
            if (g_strokeActive) { g_fgColor = col; g_fgAlpha = 100; RecreateFillCursor(); RecreateBrushCursors(); UpdateAlphaEdit(); }
            else                 { g_bgColor = col; g_bgAlpha = 100; UpdateAlphaEdit(); }
            RefreshEditingPreview();
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        break;
    }

    // Right-click on colour strip — same as left click (apply to active target)
    case WM_RBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int hit = HitTestColorStrip(x, y, TOOLBAR_HEIGHT);
        if (hit == 1) {
            g_strokeActive = true;
            UpdateAlphaEdit();
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        } else if (hit == 2) {
            g_strokeActive = false;
            UpdateAlphaEdit();
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        } else if (hit >= 3) {
            COLORREF col = g_customColors[hit - 3];
            if (g_strokeActive) { g_fgColor = col; g_fgAlpha = 100; RecreateFillCursor(); RecreateBrushCursors(); UpdateAlphaEdit(); }
            else                 { g_bgColor = col; g_bgAlpha = 100; UpdateAlphaEdit(); }
            RefreshEditingPreview();
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        break;
    }

    case WM_SETCURSOR:
        // Keep IDC_ARROW over the colour strip / non-canvas areas
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        // Forward scroll/zoom to canvas so it works without clicking first
        if (!ForwardWheelToHovered(hwnd, msg, wParam, lParam) && g_hwndCanvas)
            SendMessage(g_hwndCanvas, msg, wParam, lParam);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (g_isPasting)         CancelPaste();
            else if (g_hasSelection) { g_hasSelection = false; InvalidateCanvas(); }
        } else if (wParam == VK_RETURN) {
            if (g_isPasting) CommitPaste();
        }
        break;

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        switch (id) {
        // ── File ────────────────────────────────────────────────
        case ID_FILE_NEW:    DoNew();    break;
        case ID_FILE_OPEN:   DoOpen();   break;
        case ID_FILE_SAVE:   DoSave();   break;
        case ID_FILE_SAVEAS: DoSaveAs(); break;
        case ID_FILE_PRINT:  DoPrint();  break;
        case ID_FILE_DELETE: DoDeleteFile(); break;
        case ID_FILE_SAVE_OPTIONS: DoSaveOptions(); break;
        case ID_FILE_INKSCAPE: OpenInInkscape(g_currentFile); break;
        case ID_FILE_RECENT_CLEAR: g_recentFiles.clear(); break;
        case ID_FILE_RECENT_FIRST + 0: case ID_FILE_RECENT_FIRST + 1: case ID_FILE_RECENT_FIRST + 2:
        case ID_FILE_RECENT_FIRST + 3: case ID_FILE_RECENT_FIRST + 4: case ID_FILE_RECENT_FIRST + 5:
        case ID_FILE_RECENT_FIRST + 6: case ID_FILE_RECENT_FIRST + 7: case ID_FILE_RECENT_FIRST + 8:
        case ID_FILE_RECENT_FIRST + 9:
            OpenRecentFile(LOWORD(wParam) - ID_FILE_RECENT_FIRST);
            break;
        case ID_FILE_EXIT:   SendMessage(hwnd, WM_CLOSE, 0, 0); break;

        // ── Edit ────────────────────────────────────────────────
        case ID_EDIT_UNDO:
            if (g_textInline) CancelTextInline();
            else if (g_bezEditing) CancelBezier();
            else PopUndo();
            break;
        case ID_EDIT_REDO:       DoRedo();        break;
        case ID_EDIT_CUT:        DoCut();         break;
        case ID_EDIT_COPY:       DoCopy();        break;
        case ID_EDIT_PASTE:      DoPaste();       break;
        case ID_EDIT_SELECT_ALL: DoSelectAll();   break;

        case ID_ZOOM_IN:
            g_zoomFitMode = false;
            if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
            ZoomIn();
            break;
        case ID_ZOOM_OUT:
            g_zoomFitMode = false;
            if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
            ZoomOut();
            break;
        case ID_FOLDER_BROWSER_BTN:
            ToggleFolderBrowser();
            break;
        case ID_FOLDER_SEARCH:
            OpenFolderSearch();
            break;
        case ID_VIEW_FULLSCREEN:
            if (g_hwndFullscreen) ExitFullscreen(); else EnterFullscreen(false);
            break;
        case ID_VIEW_SLIDESHOW:
            EnterFullscreen(true);
            break;
        case ID_ZOOM_FIT_WINDOW:
            ZoomToFit();
            break;
        case ID_ZOOM_ACTUAL:
            g_zoomFitMode = false;
            if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
            ApplyZoom(1.0f, -1, -1);
            break;

        case ID_ZOOM_FIT:
            if (g_zoomFitMode) {
                // Toggle back to 1:1
                g_zoomFitMode = false;
                ApplyZoom(1.0f, -1, -1);
                if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
            } else {
                ZoomToFit();
            }
            if (g_showFolderBrowser && g_hwndFolderBrowser)
                SetFocus(g_hwndFolderBrowser);
            break;
        case ID_ZOOM_EDIT:
            if (HIWORD(wParam) == EN_CHANGE && !g_updatingZoomEdit) {
                wchar_t buf[16];
                GetWindowText(g_hwndZoomEdit, buf, 16);
                float pct = (float)_wtof(buf);
                if (pct >= 10.0f && pct <= 1000.0f) {
                    g_zoomFitMode = false;
                    if (g_hwndZoomFit) InvalidateRect(g_hwndZoomFit, NULL, FALSE);
                    ApplyZoom(pct / 100.0f, -1, -1);
                }
            }
            break;

        case ID_ALPHA_EDIT:
            if (HIWORD(wParam) == EN_KILLFOCUS) {
                ClampAlphaEdit();
            } else if (HIWORD(wParam) == EN_CHANGE && !g_updatingAlphaEdit) {
                wchar_t buf[8];
                GetWindowText((HWND)lParam, buf, 8);
                int v = _wtoi(buf);
                if (v >= 0 && v <= 100) {
                    if (g_strokeActive) g_fgAlpha = v; else g_bgAlpha = v;
                    RefreshEditingPreview();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            break;

        // ── More toolbar button ──────────────────────────────────
        case ID_MENU_MORE:
            ShowPalette();
            break;

        case ID_MENU_TOOLS:
            ShowToolsPalette();
            break;

        // ── Image ───────────────────────────────────────────────
        case ID_IMAGE_ROTATE_RIGHT: DoRotateCanvas(true);  break;
        case ID_IMAGE_ROTATE_LEFT:  DoRotateCanvas(false); break;
        case ID_IMAGE_FLIP_H:       DoFlipCanvas(true);    break;
        case ID_IMAGE_FLIP_V:       DoFlipCanvas(false);   break;
        case ID_IMAGE_RESIZE:       DoResizeCanvas();      break;
        case ID_IMAGE_CROP:         DoCropToSelection();   break;

        // ── View ────────────────────────────────────────────────
        case ID_VIEW_DARKMODE:
            g_darkMode = !g_darkMode;
            ApplyDarkMode();
            break;

        // ── Help ────────────────────────────────────────────────
        case ID_HELP_ABOUT:
            if (g_hwndHelp) {
                DestroyWindow(g_hwndHelp);   // WM_DESTROY in AboutDlgProc sets g_hwndHelp = NULL
            } else {
                g_hwndHelp = CreateDialog(GetModuleHandle(NULL),
                    MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
                if (g_hwndHelp) ShowWindow(g_hwndHelp, SW_SHOW);
            }
            break;

        // ── Tools ───────────────────────────────────────────────
        case ID_TOOL_PENCIL:  SetTool(TOOL_PENCIL);  break;
        case ID_TOOL_BRUSH:  ShowBrushTypePopup(hwnd);  break;
        case ID_TOOL_ERASER:  SetTool(TOOL_ERASER);  break;
        case ID_TOOL_FILL:    SetTool(TOOL_FILL);    break;
        case ID_TOOL_LINE:    SetTool(TOOL_LINE);    break;
        case ID_TOOL_RECT:    SetTool(TOOL_RECT);    break;
        case ID_TOOL_ELLIPSE: SetTool(TOOL_ELLIPSE); break;
        case ID_TOOL_TEXT:    SetTool(TOOL_TEXT);    break;
        case ID_TOOL_SELECT:  SetTool(TOOL_SELECT);  break;
        case ID_TOOL_WAND:    SetTool(TOOL_WAND);    break;
        case ID_TOOL_LASSO:   SetTool(TOOL_LASSO);   break;
        case ID_TOOL_RRECT:    SetTool(TOOL_RRECT);    break;
        case ID_TOOL_TRIANGLE: SetTool(TOOL_TRIANGLE); break;
        case ID_TOOL_ARROW_L:  SetTool(TOOL_ARROW_L);  break;
        case ID_TOOL_ARROW_R:  SetTool(TOOL_ARROW_R);  break;
        case ID_TOOL_ARROW_U:  SetTool(TOOL_ARROW_U);  break;
        case ID_TOOL_ARROW_D:  SetTool(TOOL_ARROW_D);  break;
        case ID_TOOL_ARROW:     SetTool(TOOL_ARROW);     break;
        case ID_TOOL_STICKMAN:  SetTool(TOOL_STICKMAN);  break;
        case ID_TOOL_SPEECH:    SetTool(TOOL_SPEECH);    break;
        case ID_TOOL_COMPUTER:  SetTool(TOOL_COMPUTER);  break;
        case ID_TOOL_STAR:      SetTool(TOOL_STAR);      break;
        case ID_TOOL_HEART:     SetTool(TOOL_HEART);     break;
        case ID_TOOL_LIGHTNING: SetTool(TOOL_LIGHTNING); break;
        case ID_TOOL_MUSTACHE:  SetTool(TOOL_MUSTACHE);  break;
        case ID_TOOL_DIAMOND:    SetTool(TOOL_DIAMOND);    break;
        case ID_TOOL_PENTAGON:   SetTool(TOOL_PENTAGON);   break;
        case ID_TOOL_HEXAGON_H:  SetTool(TOOL_HEXAGON_H);  break;
        case ID_TOOL_HEXAGON_V:  SetTool(TOOL_HEXAGON_V);  break;
        case ID_TOOL_OCTAGON:    SetTool(TOOL_OCTAGON);    break;
        case ID_TOOL_SPIRAL:    SetTool(TOOL_SPIRAL);    break;
        case ID_TOOL_SPIRAL_R:  SetTool(TOOL_SPIRAL_R);  break;
        case ID_TOOL_CROSS:     SetTool(TOOL_CROSS);     break;
        case ID_TOOL_XMARK:     SetTool(TOOL_XMARK);     break;
        case ID_TOOL_CHECK:     SetTool(TOOL_CHECK);     break;
        case ID_TOOL_SMUDGE:    SetTool(TOOL_SMUDGE);    break;
        case ID_TOOL_DROPPER:   SetTool(TOOL_DROPPER);   break;

        // ── Colour swatches ─────────────────────────────────────
        case ID_COLOR_EDIT:   ChooseActiveColor(); break;
        case ID_COLOR_RESET:  ResetCustomColors(); break;

        // ── Brush size ──────────────────────────────────────────
        case ID_BRUSHSIZE_UP:
            if (g_currentTool == TOOL_ERASER) {
                if (g_eraserSize < MAX_ERASER_SZ) {
                    ++g_eraserSize;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_eraserSize);
                    SetWindowText(g_hwndBrushSz, b);
                    InvalidateCanvas();
                }
            } else if (g_currentTool == TOOL_SMUDGE) {
                if (g_smudgeSize < MAX_SMUDGE_SZ) {
                    ++g_smudgeSize;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_smudgeSize);
                    SetWindowText(g_hwndBrushSz, b);
                    InvalidateCanvas();
                }
            } else if (g_currentTool == TOOL_WAND) {
                if (g_wandTolerance < 255) {
                    ++g_wandTolerance;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_wandTolerance);
                    SetWindowText(g_hwndBrushSz, b);
                }
            } else if (g_currentTool == TOOL_FILL) {
                if (g_fillTolerance < 255) {
                    ++g_fillTolerance;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_fillTolerance);
                    SetWindowText(g_hwndBrushSz, b);
                }
            } else if (g_brushSize < MAX_BRUSH_SZ) {
                g_brushSize = min(g_brushSize + 1.0f, MAX_BRUSH_SZ);
                UpdateBrushSizeEdit();
                RefreshEditingPreview();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case ID_BRUSHSIZE_DOWN:
            if (g_currentTool == TOOL_ERASER) {
                if (g_eraserSize > MIN_ERASER_SZ) {
                    --g_eraserSize;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_eraserSize);
                    SetWindowText(g_hwndBrushSz, b);
                    InvalidateCanvas();
                }
            } else if (g_currentTool == TOOL_SMUDGE) {
                if (g_smudgeSize > MIN_SMUDGE_SZ) {
                    --g_smudgeSize;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_smudgeSize);
                    SetWindowText(g_hwndBrushSz, b);
                    InvalidateCanvas();
                }
            } else if (g_currentTool == TOOL_WAND) {
                if (g_wandTolerance > 0) {
                    --g_wandTolerance;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_wandTolerance);
                    SetWindowText(g_hwndBrushSz, b);
                }
            } else if (g_currentTool == TOOL_FILL) {
                if (g_fillTolerance > 0) {
                    --g_fillTolerance;
                    wchar_t b[16]; StringCchPrintf(b, 16, L"%d", g_fillTolerance);
                    SetWindowText(g_hwndBrushSz, b);
                }
            } else if (g_brushSize > MIN_BRUSH_SZ) {
                g_brushSize = max(g_brushSize - 1.0f, MIN_BRUSH_SZ);
                UpdateBrushSizeEdit();
                RefreshEditingPreview();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case ID_BRUSHSIZE_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t buf[16];
                GetWindowText(g_hwndBrushSz, buf, 16);
                if (g_currentTool == TOOL_ERASER) {
                    int v = _wtoi(buf);
                    if (v >= MIN_ERASER_SZ && v <= MAX_ERASER_SZ) {
                        g_eraserSize = v;
                        InvalidateCanvas();
                    }
                } else if (g_currentTool == TOOL_SMUDGE) {
                    int v = _wtoi(buf);
                    if (v >= MIN_SMUDGE_SZ && v <= MAX_SMUDGE_SZ) {
                        g_smudgeSize = v;
                        InvalidateCanvas();
                    }
                } else if (g_currentTool == TOOL_WAND) {
                    int v = _wtoi(buf);
                    if (v >= 0 && v <= 255)
                        g_wandTolerance = v;
                } else if (g_currentTool == TOOL_FILL) {
                    int v = _wtoi(buf);
                    if (v >= 0 && v <= 255)
                        g_fillTolerance = v;
                } else {
                    float v = (float)_wtof(buf);
                    if (v >= MIN_BRUSH_SZ && v <= MAX_BRUSH_SZ) {
                        g_brushSize = v;
                        RefreshEditingPreview();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
            }
            break;
        case ID_WAND_CONTIG:
            g_wandContiguous = (SendMessage(g_hwndWandContig, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (g_darkMode && g_hwndWandContig) InvalidateRect(g_hwndWandContig, NULL, TRUE);
            break;
        case ID_ERASER_SQUARE:
            g_eraserRound = false;
            if (g_hwndEraserSq) InvalidateRect(g_hwndEraserSq, NULL, TRUE);
            if (g_hwndEraserRd) InvalidateRect(g_hwndEraserRd, NULL, TRUE);
            InvalidateCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case ID_ERASER_ROUND:
            g_eraserRound = true;
            if (g_hwndEraserSq) InvalidateRect(g_hwndEraserSq, NULL, TRUE);
            if (g_hwndEraserRd) InvalidateRect(g_hwndEraserRd, NULL, TRUE);
            InvalidateCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case ID_SMUDGE_SQUARE:
            g_smudgeRound = false;
            if (g_hwndSmudgeSq) InvalidateRect(g_hwndSmudgeSq, NULL, TRUE);
            if (g_hwndSmudgeRd) InvalidateRect(g_hwndSmudgeRd, NULL, TRUE);
            InvalidateCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case ID_SMUDGE_ROUND:
            g_smudgeRound = true;
            if (g_hwndSmudgeSq) InvalidateRect(g_hwndSmudgeSq, NULL, TRUE);
            if (g_hwndSmudgeRd) InvalidateRect(g_hwndSmudgeRd, NULL, TRUE);
            InvalidateCanvas();
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        // ── Text font controls ──────────────────────────────────
        case ID_TEXT_FONT_FACE:
            if ((HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_EDITCHANGE)
                && g_hwndFontFace) {
                GetWindowText(g_hwndFontFace, g_textFontName, LF_FACESIZE);
                if (g_textInline) RefreshEditingPreview();
            }
            break;
        case ID_TEXT_FONT_SIZE:
            if (HIWORD(wParam) == EN_CHANGE && g_hwndFontSize) {
                wchar_t buf[8];
                GetWindowText(g_hwndFontSize, buf, 8);
                int sz = _wtoi(buf);
                if (sz >= 1 && sz <= 999) {
                    g_textFontSize = sz;
                    if (g_textInline) RefreshEditingPreview();
                }
            }
            break;
        case ID_TEXT_BOLD:
            if (g_hwndFontBold) {
                if (SendMessage(g_hwndFontBold, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    g_textFontStyle |= 1;
                else
                    g_textFontStyle &= ~1;
                if (g_textInline) RefreshEditingPreview();
            }
            break;
        case ID_TEXT_ITALIC:
            if (g_hwndFontItal) {
                if (SendMessage(g_hwndFontItal, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    g_textFontStyle |= 2;
                else
                    g_textFontStyle &= ~2;
                if (g_textInline) RefreshEditingPreview();
            }
            break;
        case ID_TEXT_UNDERLINE:
            if (g_hwndFontUnder) {
                if (SendMessage(g_hwndFontUnder, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    g_textFontStyle |= 4;
                else
                    g_textFontStyle &= ~4;
                if (g_textInline) RefreshEditingPreview();
            }
            break;

        default: break;
        }
        return 0;
    }

    case WM_NOTIFY: {
        auto* nmh = (NMHDR*)lParam;
        if (nmh->code == TBN_DROPDOWN && nmh->hwndFrom == g_hwndToolbar) {
            auto* nmtb = (NMTOOLBAR*)lParam;
            int subIdx = -1;
            switch (nmtb->iItem) {
                case ID_MENU_FILE:  subIdx = 0; break;
                case ID_MENU_EDIT:  subIdx = 1; break;
                case ID_MENU_IMAGE: subIdx = 2; break;
                case ID_MENU_VIEW:  subIdx = 3; break;
            }
            if (subIdx >= 0 && g_hMenuMain) {
                // Refresh Edit menu state before showing
                if (subIdx == 2)
                    EnableMenuItem(GetSubMenu(g_hMenuMain, 2), ID_IMAGE_CROP,
                                   MF_BYCOMMAND | (g_hasSelection ? MF_ENABLED : MF_GRAYED));
                if (subIdx == 0) {
                    EnableMenuItem(GetSubMenu(g_hMenuMain, 0), ID_FILE_DELETE,
                                   MF_BYCOMMAND | (g_currentFile[0] ? MF_ENABLED : MF_GRAYED));
                    UpdateFileMenuRecent(GetSubMenu(g_hMenuMain, 0));
                    UpdateFileMenuInkscapeItem(GetSubMenu(g_hMenuMain, 0));   // goes above Open Recent
                }
                if (subIdx == 1) {
                    HMENU hEdit = GetSubMenu(g_hMenuMain, 1);
                    EnableMenuItem(hEdit, ID_EDIT_UNDO,  MF_BYCOMMAND | (!g_undoStack.empty() ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(hEdit, ID_EDIT_REDO,  MF_BYCOMMAND | (!g_redoStack.empty() ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(hEdit, ID_EDIT_CUT,   MF_BYCOMMAND | (g_hasSelection ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(hEdit, ID_EDIT_COPY,  MF_BYCOMMAND | (g_pCanvas ? MF_ENABLED : MF_GRAYED));   // whole image when nothing is selected
                    EnableMenuItem(hEdit, ID_EDIT_PASTE, MF_BYCOMMAND | (IsClipboardFormatAvailable(CF_DIB) ? MF_ENABLED : MF_GRAYED));
                }
                if (subIdx == 3) {
                    HMENU hView = GetSubMenu(g_hMenuMain, 3);
                    CheckMenuItem(hView, ID_FOLDER_BROWSER_BTN, MF_BYCOMMAND | (g_showFolderBrowser ? MF_CHECKED : MF_UNCHECKED));
                    CheckMenuItem(hView, ID_VIEW_DARKMODE,      MF_BYCOMMAND | (g_darkMode ? MF_CHECKED : MF_UNCHECKED));
                }
                HMENU hPopup = GetSubMenu(g_hMenuMain, subIdx);
                RECT rc = {};
                int idx = (int)SendMessage(g_hwndToolbar, TB_COMMANDTOINDEX, nmtb->iItem, 0);
                SendMessage(g_hwndToolbar, TB_GETITEMRECT, (WPARAM)idx, (LPARAM)&rc);
                MapWindowPoints(g_hwndToolbar, NULL, (POINT*)&rc, 2);
                TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, 0, hwnd, NULL);
            }
            return TBDDRET_DEFAULT;
        }
        if (nmh->code == TTN_GETDISPINFO) {
            auto* p = (NMTTDISPINFOW*)lParam;
            switch (p->hdr.idFrom) {
                case ID_MENU_FILE:    p->lpszText = (LPWSTR)L"File";      break;
                case ID_MENU_EDIT:    p->lpszText = (LPWSTR)L"Edit";      break;
                case ID_MENU_IMAGE:   p->lpszText = (LPWSTR)L"Image";     break;
                case ID_MENU_VIEW:    p->lpszText = (LPWSTR)L"View";      break;
                case ID_MENU_MORE:    p->lpszText = (LPWSTR)L"Shapes"; break;
                case ID_MENU_TOOLS:   p->lpszText = (LPWSTR)L"Tools";      break;
                case ID_TOOL_PENCIL:  p->lpszText = (LPWSTR)L"Pencil";    break;
                case ID_TOOL_BRUSH: {
                    static const wchar_t* kBrushNames[BRUSH_COUNT] = {
                        L"Brush: Round", L"Brush: Calligraphy", L"Brush: Airbrush",
                        L"Brush: Marker", L"Brush: Crayon", L"Brush: Oil"
                    };
                    p->lpszText = (LPWSTR)kBrushNames[(int)g_brushType];
                    break;
                }
                case ID_TOOL_ERASER:  p->lpszText = (LPWSTR)L"Eraser";    break;
                case ID_TOOL_FILL:    p->lpszText = (LPWSTR)L"Fill";      break;
                case ID_TOOL_LINE:    p->lpszText = (LPWSTR)L"Line";      break;
                case ID_TOOL_RECT:    p->lpszText = (LPWSTR)L"Rectangle"; break;
                case ID_TOOL_ELLIPSE: p->lpszText = (LPWSTR)L"Ellipse";   break;
                case ID_TOOL_TEXT:    p->lpszText = (LPWSTR)L"Text";      break;
                case ID_TOOL_SELECT:  p->lpszText = (LPWSTR)L"Select";    break;
                case ID_TOOL_WAND:    p->lpszText = (LPWSTR)L"Magic Wand"; break;
                case ID_TOOL_LASSO:   p->lpszText = (LPWSTR)L"Lasso Select"; break;
                case ID_TOOL_RRECT:    p->lpszText = (LPWSTR)L"Rounded Rectangle"; break;
                case ID_TOOL_TRIANGLE: p->lpszText = (LPWSTR)L"Triangle";           break;
                case ID_TOOL_ARROW_L:  p->lpszText = (LPWSTR)L"Left Arrow";         break;
                case ID_TOOL_ARROW_R:  p->lpszText = (LPWSTR)L"Right Arrow";        break;
                case ID_TOOL_ARROW_U:  p->lpszText = (LPWSTR)L"Up Arrow";           break;
                case ID_TOOL_ARROW_D:  p->lpszText = (LPWSTR)L"Down Arrow";         break;
                case ID_TOOL_ARROW:     p->lpszText = (LPWSTR)L"Arrow";         break;
                case ID_TOOL_STICKMAN:  p->lpszText = (LPWSTR)L"Stick Person";  break;
                case ID_TOOL_SPEECH:    p->lpszText = (LPWSTR)L"Speech Bubble"; break;
                case ID_TOOL_COMPUTER:  p->lpszText = (LPWSTR)L"Computer";      break;
                case ID_TOOL_STAR:      p->lpszText = (LPWSTR)L"Star";          break;
                case ID_TOOL_HEART:     p->lpszText = (LPWSTR)L"Heart";         break;
                case ID_TOOL_LIGHTNING: p->lpszText = (LPWSTR)L"Lightning";          break;
                case ID_TOOL_MUSTACHE:  p->lpszText = (LPWSTR)L"Mustache & Glasses"; break;
                case ID_TOOL_DIAMOND:    p->lpszText = (LPWSTR)L"Diamond";                    break;
                case ID_TOOL_PENTAGON:   p->lpszText = (LPWSTR)L"Pentagon";                   break;
                case ID_TOOL_HEXAGON_H:  p->lpszText = (LPWSTR)L"Hexagon (flat top/bottom)";  break;
                case ID_TOOL_HEXAGON_V:  p->lpszText = (LPWSTR)L"Hexagon (flat left/right)";  break;
                case ID_TOOL_OCTAGON:    p->lpszText = (LPWSTR)L"Octagon";                    break;
                case ID_TOOL_SPIRAL:    p->lpszText = (LPWSTR)L"Spiral";             break;
                case ID_TOOL_SPIRAL_R:  p->lpszText = (LPWSTR)L"Reverse Spiral";    break;
                case ID_TOOL_DROPPER:   p->lpszText = (LPWSTR)L"Color Picker (drag for area average)"; break;
            }
            return 0;
        }
        break;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            ClosePalette();
            // Remember the focused child; reactivation focuses the main window itself
            HWND f = GetFocus();
            if (f && IsChild(hwnd, f)) g_hwndFocusBeforeDeactivate = f;
        }
        break;

    case WM_SETFOCUS: {
        HWND f = g_hwndFocusBeforeDeactivate;
        if (!f || !IsWindow(f) || !IsWindowVisible(f))
            f = (g_showFolderBrowser && g_hwndFolderBrowser) ? g_hwndFolderBrowser : g_hwndCanvas;
        if (f) SetFocus(f);
        return 0;
    }

    case WM_CLOSE:
        if (!PromptSaveIfNeeded()) return 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        SaveSettings();   // persist custom colours to registry
        if (g_hMenuCBTHook) { UnhookWindowsHookEx(g_hMenuCBTHook); g_hMenuCBTHook = NULL; }
        if (g_hwndToolbar) RemoveWindowSubclass(g_hwndToolbar, ToolbarSubclassProc, 1);
        if (g_hwndBtnDark)  RemoveWindowSubclass(g_hwndBtnDark,  OverlayBtnSubclassProc, 0);
        if (g_hwndBtnHelp)  RemoveWindowSubclass(g_hwndBtnHelp,  OverlayBtnSubclassProc, 1);
        if (g_hwndZoomOut)    RemoveWindowSubclass(g_hwndZoomOut,    OverlayBtnSubclassProc, 2);
        if (g_hwndZoomIn)     RemoveWindowSubclass(g_hwndZoomIn,     OverlayBtnSubclassProc, 3);
        if (g_hwndZoomFit)       RemoveWindowSubclass(g_hwndZoomFit,       OverlayBtnSubclassProc, 12);
        if (g_hwndFolderBrowserBtn)  RemoveWindowSubclass(g_hwndFolderBrowserBtn,  OverlayBtnSubclassProc, 13);
        FreeFolderThumbs();
        if (g_hwndColorEdit)  RemoveWindowSubclass(g_hwndColorEdit,  OverlayBtnSubclassProc, 4);
        if (g_hwndReset)      RemoveWindowSubclass(g_hwndReset,      OverlayBtnSubclassProc, 5);
        if (g_hwndSzMinus)    RemoveWindowSubclass(g_hwndSzMinus,    OverlayBtnSubclassProc, 6);
        if (g_hwndSzPlus)     RemoveWindowSubclass(g_hwndSzPlus,     OverlayBtnSubclassProc, 7);
        if (g_hwndEraserSq)   RemoveWindowSubclass(g_hwndEraserSq,   OverlayBtnSubclassProc, 8);
        if (g_hwndEraserRd)   RemoveWindowSubclass(g_hwndEraserRd,   OverlayBtnSubclassProc, 9);
        if (g_hwndStatus)    RemoveWindowSubclass(g_hwndStatus,    StatusBarSubclassProc, 0);
        if (g_hwndBrushSz)   RemoveWindowSubclass(g_hwndBrushSz,   DarkEditSubclassProc,  0);
        if (g_hwndZoomEdit)  RemoveWindowSubclass(g_hwndZoomEdit,  DarkEditSubclassProc,  0);
        if (g_hwndAlphaEdit) RemoveWindowSubclass(g_hwndAlphaEdit, DarkEditSubclassProc,  0);
        if (g_hwndFontSize)  RemoveWindowSubclass(g_hwndFontSize,  DarkEditSubclassProc,  0);
        if (g_textBlinkTimer && g_hwndCanvas) { KillTimer(g_hwndCanvas, g_textBlinkTimer); g_textBlinkTimer = 0; }
        for (int i = 0; i < (int)ARRAYSIZE(g_hbmFileMenuIcons); i++)
            if (g_hbmFileMenuIcons[i]) { DeleteObject(g_hbmFileMenuIcons[i]); g_hbmFileMenuIcons[i] = NULL; }
        for (int i = 0; i < (int)ARRAYSIZE(g_hbmEditMenuIcons); i++)
            if (g_hbmEditMenuIcons[i]) { DeleteObject(g_hbmEditMenuIcons[i]); g_hbmEditMenuIcons[i] = NULL; }
        for (int i = 0; i < (int)ARRAYSIZE(g_hbmImageMenuIcons); i++)
            if (g_hbmImageMenuIcons[i]) { DeleteObject(g_hbmImageMenuIcons[i]); g_hbmImageMenuIcons[i] = NULL; }
        if (g_hbmStatusCursor) { DeleteObject(g_hbmStatusCursor); g_hbmStatusCursor = NULL; }
        if (g_hbmStatusBorder) { DeleteObject(g_hbmStatusBorder); g_hbmStatusBorder = NULL; }
        if (g_hbmStatusCrop)   { DeleteObject(g_hbmStatusCrop);   g_hbmStatusCrop   = NULL; }
        if (g_hCurPencil)  { DestroyCursor(g_hCurPencil);  g_hCurPencil  = NULL; }
        if (g_hCurDropper) { DestroyCursor(g_hCurDropper); g_hCurDropper = NULL; }
        if (g_hCurFill)    { DestroyCursor(g_hCurFill);    g_hCurFill    = NULL; }
        if (g_hCurLasso)   { DestroyCursor(g_hCurLasso);   g_hCurLasso   = NULL; }
        if (g_hCurSmudge)  { DestroyCursor(g_hCurSmudge);  g_hCurSmudge  = NULL; }
        if (g_hCurRotate)    { DestroyCursor(g_hCurRotate);    g_hCurRotate    = NULL; }
        if (g_hCurCornerRot) { DestroyCursor(g_hCurCornerRot); g_hCurCornerRot = NULL; }
        delete g_pBmpRotHandle; g_pBmpRotHandle = nullptr;
        for (int i = 0; i < BRUSH_COUNT; ++i) if (g_hCurBrush[i]) { DestroyCursor(g_hCurBrush[i]); g_hCurBrush[i] = NULL; }
        if (g_hSymbolFont)    { DeleteObject(g_hSymbolFont);       g_hSymbolFont    = NULL; }
        if (g_hEditFont)      { DeleteObject(g_hEditFont);         g_hEditFont      = NULL; }
        if (g_hFontBold)      { DeleteObject(g_hFontBold);         g_hFontBold      = NULL; }
        if (g_hFontItal)      { DeleteObject(g_hFontItal);         g_hFontItal      = NULL; }
        if (g_hFontUndr)      { DeleteObject(g_hFontUndr);         g_hFontUndr      = NULL; }
        if (g_hToolImageList) { ImageList_Destroy(g_hToolImageList); g_hToolImageList = NULL; }
        if (g_hMenuMain)      { DestroyMenu(g_hMenuMain);           g_hMenuMain      = NULL; }
        if (g_hIcoColorEdit)  { DestroyIcon(g_hIcoColorEdit);       g_hIcoColorEdit  = NULL; }
        if (g_hIcoZoomOut)    { DestroyIcon(g_hIcoZoomOut);         g_hIcoZoomOut    = NULL; }
        if (g_hIcoZoomIn)     { DestroyIcon(g_hIcoZoomIn);          g_hIcoZoomIn     = NULL; }
        if (g_hIcoMoon)       { DestroyIcon(g_hIcoMoon);            g_hIcoMoon       = NULL; }
        if (g_hIcoSun)        { DestroyIcon(g_hIcoSun);             g_hIcoSun        = NULL; }
        if (g_hIcoHelpBtn)    { DestroyIcon(g_hIcoHelpBtn);         g_hIcoHelpBtn    = NULL; }
        for (auto* b : g_undoStack) delete b;
        g_undoStack.clear();
        for (auto* b : g_redoStack) delete b;
        g_redoStack.clear();
        g_shapeEditing = false;
        CancelPaste();
        delete g_selFloatBitmap; g_selFloatBitmap = nullptr;
        DestroyCanvas();
        GdiplusShutdown(g_gdiplusToken);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================
//  WinMain
// ============================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    // Opt this process into dark mode support (uxtheme ordinals, Win10 1903+).
    // Must be called before any window is created so the system registers the preference.
    {
        HMODULE hUx = GetModuleHandleW(L"uxtheme.dll");
        if (!hUx) hUx = LoadLibraryW(L"uxtheme.dll");
        if (hUx) {
            // ord 135 = SetPreferredAppMode(AllowDark=1) on 18362+,
            //           AllowDarkModeForApp(TRUE)        on earlier builds
            typedef int  (WINAPI* FnAppMode)(int);
            typedef void (WINAPI* FnRefresh)();
            auto fnMode    = (FnAppMode) GetProcAddress(hUx, MAKEINTRESOURCEA(135));
            auto fnRefresh = (FnRefresh) GetProcAddress(hUx, MAKEINTRESOURCEA(104));
            if (fnMode)    fnMode(1);    // 1 = AllowDark
            if (fnRefresh) fnRefresh();
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, NULL);

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIconSm       = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_APPICON),
                           IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszMenuName  = NULL;  // menu bar replaced by toolbar dropdown buttons
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClassEx(&wc)) return 1;

    HACCEL hAccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR));

    HWND hwnd = CreateWindowEx(0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,   // strip paint must not cover child inputs
        CW_USEDEFAULT, CW_USEDEFAULT, 1050, 750,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    // Explicitly set both icon sizes so the taskbar always shows the current icon
    {
        HICON hBig = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_APPICON),
                         IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE);
        HICON hSm  = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_APPICON),
                         IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        if (hBig) SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hBig);
        if (hSm)  SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSm);
    }

    // Restore saved window placement (g_savedWin* filled by LoadSettings in WM_CREATE)
    if (g_savedWinX != INT_MIN && g_savedWinW > 200 && g_savedWinH > 100) {
        // Clamp so at least 50px of the title bar is on the virtual desktop
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (g_savedWinX < vx + vw - 50 && g_savedWinY < vy + vh - 50 &&
            g_savedWinX + g_savedWinW > vx + 50)
        {
            SetWindowPos(hwnd, NULL,
                g_savedWinX, g_savedWinY, g_savedWinW, g_savedWinH,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Open file passed on the command line (e.g. via "Open With" from Explorer)
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc >= 2) { g_startedWithFile = true; LoadImageFromPath(argv[1]); }
        LocalFree(argv);
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // Intercept keys for the help dialog before IsDialogMessage eats them
        if (g_hwndHelp && msg.message == WM_KEYDOWN) {
            // Ctrl+F: open search bar
            if (msg.wParam == 'F' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                HWND hw = msg.hwnd;
                while (hw && hw != g_hwndHelp) hw = GetParent(hw);
                if (hw == g_hwndHelp) {
                    PostMessage(g_hwndHelp, WM_APP, 0, 0);
                    continue;
                }
            }
            // Enter / Escape in search bar: find-next or close bar (don't let IsDialogMessage eat them)
            if (s_searchVisible && s_hwndSearchEdit) {
                bool onSearch   = (msg.hwnd == s_hwndSearchEdit);
                bool onFindNext = (msg.hwnd == s_hwndFindNext);
                if ((onSearch || onFindNext) && msg.wParam == VK_RETURN) {
                    DoHelpFind(g_hwndHelp);
                    continue;
                }
                if (onSearch && msg.wParam == VK_ESCAPE) {
                    PostMessage(g_hwndHelp, WM_COMMAND, IDC_HELP_FINDCLOSE, 0);
                    continue;
                }
            }
        }
        if (g_hwndHelp && IsDialogMessage(g_hwndHelp, &msg))
            continue;
        // Ctrl+C, Ctrl+V, Ctrl+A etc. in the strip's text boxes edit the text,
        // not the image; Ctrl+F still reaches the search
        bool inStripEdit = msg.hwnd && (msg.hwnd == g_hwndFolderSearchEdit || msg.hwnd == g_hwndFolderRename) &&
                           !(msg.message == WM_KEYDOWN && msg.wParam == 'F');
        // Full screen is for looking: its keys move through the folder, and
        // editing shortcuts must not change the hidden image
        if (msg.hwnd && msg.hwnd == g_hwndFullscreen) inStripEdit = true;
        if (inStripEdit || !TranslateAccelerator(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    CoUninitialize();
    return (int)msg.wParam;
}

