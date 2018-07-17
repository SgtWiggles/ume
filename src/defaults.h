#pragma once

#define ICON_FILE "terminal-tango.svg"
#define SCROLL_LINES 4096
#define DEFAULT_SCROLL_LINES 4096
#define HTTP_REGEXP "(ftp|http)s?://[^ \t\n\b()<>{}«»\\[\\]\'\"]+[^.]"
#define MAIL_REGEXP "[^ \t\n\b]+@([^ \t\n\b]+\\.)+([a-zA-Z]{2,4})"
#define DEFAULT_CONFIGFILE "ume.conf"
#define DEFAULT_COLUMNS 80
#define DEFAULT_ROWS 24
#define DEFAULT_FONT "Ubuntu Mono,monospace 12"
#define FONT_MINIMAL_SIZE (PANGO_SCALE * 6)
#define DEFAULT_WORD_CHARS "-,./?%&#_~:"
#define TAB_MAX_SIZE 40
#define TAB_MIN_SIZE 6
#define FORWARD 1
#define BACKWARDS 2
#define FADE_PERCENT 10
#define DEFAULT_ADD_TAB_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_DEL_TAB_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_SWITCH_TAB_MODIFIER (GDK_CONTROL_MASK)
#define DEFAULT_MOVE_TAB_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_COPY_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_SCROLLBAR_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_OPEN_URL_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_FONT_SIZE_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_SET_TAB_NAME_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_SEARCH_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_SELECT_COLORSET_MODIFIER (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define DEFAULT_ADD_TAB_KEY GDK_KEY_T
#define DEFAULT_DEL_TAB_KEY GDK_KEY_W
#define DEFAULT_PREV_TAB_KEY GDK_KEY_Left
#define DEFAULT_NEXT_TAB_KEY GDK_KEY_Right
#define DEFAULT_COPY_KEY GDK_KEY_C
#define DEFAULT_PASTE_KEY GDK_KEY_V

#define DEFAULT_SCROLLBAR_KEY GDK_KEY_S
#define DEFAULT_SCROLL_UP_KEY GDK_KEY_K
#define DEFAULT_SCROLL_DOWN_KEY GDK_KEY_J
#define DEFAULT_PAGE_UP_KEY GDK_KEY_U
#define DEFAULT_PAGE_DOWN_KEY GDK_KEY_D
#define DEFAULT_SCROLL_AMOUNT 10

#define DEFAULT_SET_TAB_NAME_KEY GDK_KEY_N
#define DEFAULT_SEARCH_KEY GDK_KEY_F
#define DEFAULT_FULLSCREEN_KEY GDK_KEY_F11
#define DEFAULT_INCREASE_FONT_SIZE_KEY GDK_KEY_plus
#define DEFAULT_DECREASE_FONT_SIZE_KEY GDK_KEY_minus
#define DEFAULT_SCROLLABLE_TABS TRUE

#define DEFAULT_RELOAD_MODIFIER 5
#define DEFAULT_RELOAD_KEY GDK_KEY_R

#define NUM_COLORSETS 6

#define COLOR_GROUP_KEY "colors%d"
#define COLOR_FOREGROUND_KEY "foreground"
#define COLOR_BACKGROUND_KEY "background"
#define COLOR_CURSOR_KEY "cursor"
#define COLOR_PALETTE_KEY "color%u"
#define COLOR_SWITCH_KEY "colors%u_key"

#define PALETTE_SIZE 16
/* 16 color palettes in GdkRGBA format (red, green, blue, alpha)
 * Text displayed in the first 8 colors (0-7) is meek (uses thin strokes).
 * Text displayed in the second 8 colors (8-15) is bold (uses thick strokes). */
static const char *DEFAULT_PALETTES[NUM_COLORSETS][PALETTE_SIZE] = {
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
