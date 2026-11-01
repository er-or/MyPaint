//{{NO_DEPENDENCIES}}
// Resource IDs for MyPaint
//

#pragma once

// Menu IDs
#define IDR_MENU_MAIN           101
#define IDR_ACCELERATOR         102

// Menu item IDs — File
#define ID_FILE_NEW             40001
#define ID_FILE_OPEN            40002
#define ID_FILE_SAVE            40003
#define ID_FILE_SAVEAS          40004
#define ID_FILE_EXIT            40005
#define ID_FILE_PRINT           40007
#define ID_FILE_DELETE          40008
#define ID_FILE_INKSCAPE        40009   // added to the File menu at runtime for SVGs
#define ID_FILE_SAVE_OPTIONS    40030
#define ID_FILE_RECENT_FIRST    41000   // File > Open Recent entries: 41000 + index (up to 10)
#define ID_FILE_RECENT_CLEAR    41020

// Menu item IDs — Edit
#define ID_EDIT_UNDO            40010
#define ID_EDIT_REDO            40011
#define ID_EDIT_CUT             40012
#define ID_EDIT_COPY            40013
#define ID_EDIT_PASTE           40014
#define ID_EDIT_SELECT_ALL      40015

// Zoom controls
#define ID_ZOOM_IN              40501
#define ID_ZOOM_OUT             40502
#define ID_ZOOM_EDIT            40503
#define ID_ZOOM_FIT             40504
#define ID_FOLDER_BROWSER_BTN        40506
#define ID_ZOOM_FIT_WINDOW      40507   // Ctrl+0: always fit (the button toggles)
#define ID_ZOOM_ACTUAL          40508   // Ctrl+1: 100%
#define ID_FOLDER_SEARCH        40509   // Ctrl+F: search box in the folder browser

// Alpha (opacity) edit
#define ID_ALPHA_EDIT           40505

// View menu
#define ID_VIEW_DARKMODE        40601
#define ID_VIEW_FULLSCREEN      40602   // F11
#define ID_VIEW_SLIDESHOW       40603   // F5

// Toolbar menu-bar replacement button IDs
#define ID_MENU_FILE            40801
#define ID_MENU_EDIT            40802
#define ID_MENU_IMAGE           40803
#define ID_MENU_VIEW            40804
#define ID_MENU_HELP            40805
#define ID_MENU_MORE            40806
#define ID_MENU_TOOLS           40808

// Toolbar separator IDs (needed so TB_HIDEBUTTON can target them)
#define ID_SEP_EXTENDED         40900

// Menu item IDs — Image
#define ID_IMAGE_RESIZE         40401
#define ID_IMAGE_ROTATE_RIGHT   40402
#define ID_IMAGE_ROTATE_LEFT    40403
#define ID_IMAGE_FLIP_H         40404
#define ID_IMAGE_FLIP_V         40405
#define ID_IMAGE_CROP           40406

// Menu item IDs — Help
#define ID_HELP_ABOUT           40020

// Toolbar button command IDs — Tools
#define ID_TOOL_PENCIL          40101
#define ID_TOOL_BRUSH           40102
#define ID_TOOL_ERASER          40103
#define ID_TOOL_FILL            40104
#define ID_TOOL_LINE            40105
#define ID_TOOL_RECT            40106
#define ID_TOOL_ELLIPSE         40107
#define ID_TOOL_TEXT            40108
#define ID_TOOL_SELECT          40109
#define ID_TOOL_RRECT       40110
#define ID_TOOL_TRIANGLE    40111
#define ID_TOOL_ARROW_L     40112
#define ID_TOOL_ARROW_R     40113
#define ID_TOOL_ARROW_U     40114
#define ID_TOOL_ARROW_D     40115
#define ID_TOOL_ARROW       40116
#define ID_TOOL_STICKMAN    40117
#define ID_TOOL_SPEECH      40118
#define ID_TOOL_COMPUTER    40119
#define ID_TOOL_STAR        40120
#define ID_TOOL_HEART       40121
#define ID_TOOL_LIGHTNING   40122
#define ID_TOOL_MUSTACHE    40123
#define ID_TOOL_DIAMOND     40135
#define ID_TOOL_PENTAGON    40136
#define ID_TOOL_HEXAGON_H   40137
#define ID_TOOL_HEXAGON_V   40138
#define ID_TOOL_OCTAGON     40139
#define ID_TOOL_SPIRAL      40133
#define ID_TOOL_SPIRAL_R    40134
#define ID_TOOL_DROPPER     40124
#define ID_TOOL_WAND        40131
#define ID_TOOL_LASSO       40132
#define ID_TOOL_SMUDGE      40143
#define ID_TOOL_HAND        40144
#define ID_TOOL_CROSS       40140
#define ID_TOOL_XMARK       40141
#define ID_TOOL_CHECK       40142

// Brush sub-type popup menu IDs
#define ID_BRUSH_ROUND       40125
#define ID_BRUSH_CALLIGRAPHY 40126
#define ID_BRUSH_AIRBRUSH    40127
#define ID_BRUSH_MARKER      40128
#define ID_BRUSH_CRAYON      40129
#define ID_BRUSH_OIL         40130

// Toolbar — colour pickers (also in Edit menu context)
#define ID_COLOR_FOREGROUND     40201
#define ID_COLOR_BACKGROUND     40202
#define ID_COLOR_RESET          40203
#define ID_COLOR_EDIT           40204

// Toolbar — brush size
#define ID_BRUSHSIZE_UP         40301
#define ID_BRUSHSIZE_DOWN       40302
#define ID_BRUSHSIZE_EDIT       40303
#define ID_ERASER_SQUARE        40304
#define ID_ERASER_ROUND         40305
#define ID_WAND_CONTIG          40306
#define ID_SMUDGE_SQUARE        40307
#define ID_SMUDGE_ROUND         40308

// Dialog IDs
#define IDD_TEXT_INPUT          201
#define IDD_ABOUT               202
#define IDD_RESIZE_CANVAS       203
#define IDD_SAVE_OPTIONS        204

// Standard control IDs
#define IDC_STATIC              -1

// Dialog control IDs — Text tool
#define IDC_TEXT_EDIT           1001
#define IDC_TEXT_FONT           1002
#define IDC_TEXT_SIZE           1003

// Dialog control IDs — Resize image
#define IDC_RESIZE_W            1010
#define IDC_RESIZE_H            1011
#define IDC_RESIZE_W_PCT        1012
#define IDC_RESIZE_H_PCT        1013
#define IDC_RESIZE_LOCK_ASPECT  1014
#define IDC_RESIZE_CANVAS_ONLY  1015

// Dialog control IDs — File > Save Options
#define IDC_SAVEOPT_SLIDER      1030
#define IDC_SAVEOPT_QUALITY     1031
#define IDC_SAVEOPT_LOSSLESS    1032
#define IDC_SAVEOPT_SIZE        1033
#define IDC_SAVEOPT_HINT        1035
#define IDC_SAVEOPT_TAB_JPEG    1036
#define IDC_SAVEOPT_TAB_PNG     1037
#define IDC_SAVEOPT_TAB_WEBP    1038
#define IDC_SAVEOPT_QLABEL      1039
#define IDC_SAVEOPT_PNG_FULL    1040
#define IDC_SAVEOPT_PNG_256     1041
#define IDC_SAVEOPT_PNG_ALPHA   1042
#define IDC_SAVEOPT_PNG_HINT    1043
#define IDC_SAVEOPT_DEFAULTS    1044
#define IDC_SAVEOPT_WEBP_ALPHA  1045

// Dialog control IDs — Help/About
#define IDC_HELP_TEXT           1020
#define IDC_HELP_SEARCH         1021
#define IDC_HELP_FINDCLOSE      1022
#define IDC_HELP_FINDNEXT       1023

// String IDs
#define IDS_APP_TITLE           1
#define IDS_UNTITLED            2
#define IDS_FILTER_ALL          3
#define IDS_FILTER_PNG          4

// Icon
#define IDI_APPICON             1

// Text tool font controls (in colour strip, visible when text tool active)
#define ID_TEXT_FONT_FACE   40701
#define ID_TEXT_FONT_SIZE   40702
#define ID_TEXT_BOLD        40703
#define ID_TEXT_ITALIC      40704
#define ID_TEXT_UNDERLINE   40705

// Status bar pane indices (not resource IDs, just constants)
#define STATUS_PANE_POS         0
#define STATUS_PANE_SEL         1
#define STATUS_PANE_SIZE        2
#define STATUS_PANE_COLOR       3
#define STATUS_PANE_HEX         4
#define STATUS_PANE_ZOOM        5
