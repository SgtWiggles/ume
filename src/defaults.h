#pragma once
#include <gdk/gdk.h>

static constexpr const char *ICON_FILE = "terminal-tango.svg";
static constexpr int SCROLL_LINES = 4096;
static constexpr int DEFAULT_SCROLL_LINES = 4096;
static constexpr const char *HTTP_REGEXP = "(ftp|http)s?://[^ \t\n\b()<>{}«»\\[\\]\'\"]+[^.]";
static constexpr const char *MAIL_REGEXP = "[^ \t\n\b]+@([^ \t\n\b]+\\.)+([a-zA-Z]{2,4})";
static constexpr const char *DEFAULT_CONFIGFILE = "ume.conf";
static constexpr int DEFAULT_COLUMNS = 80;
static constexpr int DEFAULT_ROWS = 24;
static constexpr const char *DEFAULT_FONT = "Ubuntu Mono,monospace 12";
static constexpr int FONT_MINIMAL_SIZE = (PANGO_SCALE * 6);
static constexpr const char *DEFAULT_WORD_CHARS = "-,./?%&#_~:";
static constexpr int TAB_MAX_SIZE = 40;
static constexpr int TAB_MIN_SIZE = 6;
static constexpr int FORWARD = 1;
static constexpr int BACKWARDS = 2;
static constexpr int FADE_PERCENT = 10;
static constexpr guint DEFAULT_ADD_TAB_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_DEL_TAB_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_SWITCH_TAB_MODIFIER = (GDK_CONTROL_MASK);
static constexpr guint DEFAULT_MOVE_TAB_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_COPY_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_SCROLLBAR_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_OPEN_URL_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_FONT_SIZE_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_SET_TAB_NAME_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_SEARCH_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_SELECT_COLORSET_MODIFIER = (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
static constexpr guint DEFAULT_ADD_TAB_KEY = GDK_KEY_T;
static constexpr guint DEFAULT_DEL_TAB_KEY = GDK_KEY_W;
static constexpr guint DEFAULT_PREV_TAB_KEY = GDK_KEY_Left;
static constexpr guint DEFAULT_NEXT_TAB_KEY = GDK_KEY_Right;
static constexpr guint DEFAULT_COPY_KEY = GDK_KEY_C;
static constexpr guint DEFAULT_PASTE_KEY = GDK_KEY_V;

static constexpr guint DEFAULT_SCROLLBAR_KEY = GDK_KEY_S;
static constexpr guint DEFAULT_SCROLL_UP_KEY = GDK_KEY_K;
static constexpr guint DEFAULT_SCROLL_DOWN_KEY = GDK_KEY_J;
static constexpr guint DEFAULT_PAGE_UP_KEY = GDK_KEY_U;
static constexpr guint DEFAULT_PAGE_DOWN_KEY = GDK_KEY_D;
static constexpr int DEFAULT_SCROLL_AMOUNT = 10;

static constexpr guint DEFAULT_SET_TAB_NAME_KEY = GDK_KEY_N;
static constexpr guint DEFAULT_SEARCH_KEY = GDK_KEY_F;
static constexpr guint DEFAULT_FULLSCREEN_KEY = GDK_KEY_F11;
static constexpr guint DEFAULT_INCREASE_FONT_SIZE_KEY = GDK_KEY_plus;
static constexpr guint DEFAULT_DECREASE_FONT_SIZE_KEY = GDK_KEY_minus;
static constexpr bool DEFAULT_SCROLLABLE_TABS = false;

static constexpr int DEFAULT_RELOAD_MODIFIER = 5;
static constexpr guint DEFAULT_RELOAD_KEY = GDK_KEY_R;

static constexpr int NUM_COLORSETS = 6;

static constexpr const char *COLOR_GROUP_KEY = "colors%d";
static constexpr const char *COLOR_FOREGROUND_KEY = "foreground";
static constexpr const char *COLOR_BACKGROUND_KEY = "background";
static constexpr const char *COLOR_CURSOR_KEY = "cursor";
static constexpr const char *COLOR_PALETTE_KEY = "color%u";
static constexpr const char *COLOR_SWITCH_KEY = "colors%u_key";

static constexpr int PALETTE_SIZE = 16;
/* 16 color palettes in GdkRGBA format (red, green, blue, alpha)
 * Text displayed in the first 8 colors (0-7) is meek (uses thin strokes).
 * Text displayed in the second 8 colors (8-15) is bold (uses thick strokes). */
static constexpr const char *DEFAULT_PALETTES[NUM_COLORSETS][PALETTE_SIZE] = {
		{"rgba(33,33,33,1)", "rgba(221,50,90,1)", "rgba(69,123,36,1)", "rgba(255,172,120,1)", "rgba(19,78,178,1)",
		 "rgba(86,0,136,1)", "rgba(14,113,124,1)", "rgba(239,239,239,1)", "rgba(125,125,125,1)", "rgba(232,59,63,1)",
		 "rgba(122,186,58,1)", "rgba(255,133,55,1)", "rgba(84,164,243,1)", "rgba(170,77,188,1)", "rgba(38,187,209,1)",
		 "rgba(217,217,217,1)"},
		{"rgba(0,0,0,1)", "rgba(204,0,0,1)", "rgba(77,154,5,1)", "rgba(195,160,0,1)", "rgba(52,100,163,1)",
		 "rgba(117,79,123,1)", "rgba(5,151,154,1)", "rgba(211,214,207,1)", "rgba(84,86,82,1)", "rgba(239,40,40,1)",
		 "rgba(137,226,52,1)", "rgba(251,232,79,1)", "rgba(114,158,207,1)", "rgba(172,126,168,1)", "rgba(52,226,226,1)",
		 "rgba(237,237,235,1)"},
		{"rgba(0,0,0,1)", "rgba(170,0,0,1)", "rgba(0,170,0,1)", "rgba(170,84,0,1)", "rgba(0,0,170,1)", "rgba(170,0,170,1)",
		 "rgba(0,170,170,1)", "rgba(170,170,170,1)", "rgba(84,84,84,1)", "rgba(255,84,84,1)", "rgba(84,255,84,1)",
		 "rgba(255,255,84,1)", "rgba(84,84,255,1)", "rgba(255,84,255,1)", "rgba(84,255,255,1)", "rgba(255,255,255,1)"},
		{"rgba(7,54,66,1)", "rgba(219,49,47,1)", "rgba(133,153,0,1)", "rgba(181,137,0,1)", "rgba(38,138,209,1)",
		 "rgba(211,54,130,1)", "rgba(42,161,151,1)", "rgba(237,232,212,1)", "rgba(0,42,54,1)", "rgba(202,75,22,1)",
		 "rgba(87,110,117,1)", "rgba(100,123,130,1)", "rgba(130,147,149,1)", "rgba(107,112,195,1)", "rgba(147,161,161,1)",
		 "rgba(253,246,226,1)"},
		{"rgba(237,232,212,1)", "rgba(219,49,47,1)", "rgba(133,153,0,1)", "rgba(181,137,0,1)", "rgba(38,138,209,1)",
		 "rgba(211,54,130,1)", "rgba(42,161,151,1)", "rgba(7,54,66,1)", "rgba(253,246,226,1)", "rgba(202,75,22,1)",
		 "rgba(147,161,161,1)", "rgba(130,147,149,1)", "rgba(100,123,130,1)", "rgba(107,112,195,1)", "rgba(87,110,117,1)",
		 "rgba(0,42,54,1)"},
		{"rgba(0,0,0,1)", "rgba(205,0,0,1)", "rgba(0,205,0,1)", "rgba(205,205,0,1)", "rgba(29,144,255,1)",
		 "rgba(205,0,205,1)", "rgba(0,205,205,1)", "rgba(228,228,228,1)", "rgba(75,75,75,1)", "rgba(255,0,0,1)",
		 "rgba(0,255,0,1)", "rgba(255,255,0,1)", "rgba(70,130,179,1)", "rgba(255,0,255,1)", "rgba(0,255,255,1)",
		 "rgba(255,255,255,1)"}};
