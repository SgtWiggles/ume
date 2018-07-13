#pragma once
#include <gdk/gdk.h>

#include "defaults.h"
struct term_colors {
	GdkRGBA forecolors[NUM_COLORSETS];
	GdkRGBA backcolors[NUM_COLORSETS];
	GdkRGBA curscolors[NUM_COLORSETS];
	GdkRGBA palettes[NUM_COLORSETS][PALETTE_SIZE];

	term_colors &operator=(const term_colors &other) {
		for (int i = 0; i < NUM_COLORSETS; ++i) {
			forecolors[i] = other.forecolors[i];
			backcolors[i] = other.backcolors[i];
			curscolors[i] = other.curscolors[i];
			for (int j = 0; j < PALETTE_SIZE; ++j)
				palettes[i][j] = other.palettes[i][j];
		}
		return *this;
	}
};

struct config_t {
	gint scroll_lines;
	gint scroll_amount;

	VteCursorShape cursor_type;

	bool first_tab;
	bool show_scrollbar;
	bool show_closebutton;
	bool tabs_on_bottom;
	bool less_questions;
	bool urgent_bell;
	bool audible_bell;
	bool blinking_cursor;
	bool stop_tab_cycling_at_end_tabs;
	bool allow_bold;

	bool keep_fc; /* Global flag to indicate that we don't want changes in the files and columns values */
	bool disable_numbered_tabswitch; /* For disabling direct tabswitching key */

	bool use_fading;
	bool scrollable_tabs;

	bool reload_config_on_modify;
	bool ignore_overwrite;

	char *icon;
	char *word_chars; /* Exceptions for word selection */

	PangoFontDescription *font;

	gchar *tab_default_title;
	gint last_colorset;
	gint add_tab_accelerator;
	gint del_tab_accelerator;
	gint switch_tab_accelerator;
	gint move_tab_accelerator;
	gint copy_accelerator;
	gint scrollbar_accelerator;
	gint open_url_accelerator;
	gint font_size_accelerator;
	gint set_tab_name_accelerator;
	gint search_accelerator;
	gint set_colorset_accelerator;
	gint add_tab_key;
	gint del_tab_key;
	gint prev_tab_key;
	gint next_tab_key;
	gint copy_key;
	gint paste_key;

	gint scrollbar_key;
	gint scroll_up_key;
	gint scroll_down_key;
	gint page_up_key;
	gint page_down_key;

	gint set_tab_name_key;
	gint search_key;
	gint fullscreen_key;
	gint increase_font_size_key;
	gint decrease_font_size_key;
	gint set_colorset_keys[NUM_COLORSETS];

	VteRegex *http_vteregexp, *mail_vteregexp;

	term_colors colors;
};
