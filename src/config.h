#pragma once
#include <gdk/gdk.h>

#include <algorithm>
#include <assert.h>
#include <string>

#include "defaults.h"
using palette_t = std::array<GdkRGBA, PALETTE_SIZE>;
struct term_colors_t {
	std::array<GdkRGBA, NUM_COLORSETS> forecolors;
	std::array<GdkRGBA, NUM_COLORSETS> backcolors;
	std::array<GdkRGBA, NUM_COLORSETS> curscolors;
	std::array<palette_t, NUM_COLORSETS> palettes;
};

using accel_t = guint;
using keycode_t = guint;

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

	// bool reload_config_on_modify;
	bool ignore_overwrite;

	const char *icon;
	const char *word_chars; /* Exceptions for word selection */

	PangoFontDescription *font;

	gchar *tab_default_title;
	gint last_colorset;

	accel_t open_url_accelerator;

	accel_t add_tab_accelerator;
	keycode_t add_tab_key;
	accel_t del_tab_accelerator;
	keycode_t del_tab_key;

	accel_t switch_tab_accelerator;
	accel_t move_tab_accelerator;
	gint prev_tab_key;
	gint next_tab_key;

	accel_t copy_accelerator;
	gint copy_key;
	gint paste_key;

	accel_t scrollbar_accelerator;
	gint scrollbar_key;
	gint scroll_up_key;
	gint scroll_down_key;
	gint page_up_key;
	gint page_down_key;

	accel_t set_tab_name_accelerator;
	gint set_tab_name_key;

	accel_t search_accelerator;
	gint search_key;

	gint fullscreen_key;

	accel_t font_size_accelerator;
	gint increase_font_size_key;
	gint decrease_font_size_key;

	accel_t set_colorset_accelerator;
	std::array<gint, NUM_COLORSETS> set_colorset_keys;

	VteRegex *http_vteregexp, *mail_vteregexp;

	term_colors_t colors;
};

/*
 *struct keybind_t parse_keybind(const char *cstr) {
 *  assert(cstr != nullptr);
 *  const std::string sep = "+";
 *  keybind_t bind;
 *
 *  auto str = std::string(cstr);
 *  if (str.size() == 0) // Empty strings are unbound keys
 *    return bind;
 *
 *  str.erase(std::remove_if(std::begin(str), std::end(str), isspace), std::end(str));
 *  std::transform(std::begin(str), std::end(str), std::begin(str), toupper);
 *  str.push_back('+');
 *
 *  size_t pos = 0;
 *  std::string tok;
 *
 *  while ((pos = str.find(sep)) != std::string::npos) {
 *    tok = str.substr(0, pos);
 *    if (tok == "SHIFT") {
 *      bind.accelerator |= GDK_SHIFT_MASK;
 *    } else if (tok == "LOCK" || tok == "CAPS") {
 *      bind.accelerator |= GDK_LOCK_MASK;
 *    } else if (tok == "CONTROL" || tok == "CTRL") {
 *      bind.accelerator |= GDK_CONTROL_MASK;
 *    } else if (tok == "MOD1" || tok == "ALT") {
 *      bind.accelerator |= GDK_MOD1_MASK;
 *    } else if (tok == "MOD2") {
 *      bind.accelerator |= GDK_MOD2_MASK;
 *    } else if (tok == "MOD3") {
 *      bind.accelerator |= GDK_MOD3_MASK;
 *    } else if (tok == "MOD4") {
 *      bind.accelerator |= GDK_MOD4_MASK;
 *    } else if (tok == "MOD5") {
 *      bind.accelerator |= GDK_MOD5_MASK;
 *    } else if (tok == "BUTTON1") {
 *      bind.accelerator |= GDK_BUTTON1_MASK;
 *    } else if (tok == "BUTTON2") {
 *      bind.accelerator |= GDK_BUTTON2_MASK;
 *    } else if (tok == "BUTTON3") {
 *      bind.accelerator |= GDK_BUTTON3_MASK;
 *    } else if (tok == "BUTTON4") {
 *      bind.accelerator |= GDK_BUTTON4_MASK;
 *    } else if (tok == "BUTTON5") {
 *      bind.accelerator |= GDK_BUTTON5_MASK;
 *    } else if (tok == "SUPER") {
 *      bind.accelerator |= GDK_SUPER_MASK;
 *    } else if (tok == "HYPER") {
 *      bind.accelerator |= GDK_HYPER_MASK;
 *    } else if (tok == "META") {
 *      bind.accelerator |= GDK_META_MASK;
 *    } else if (bind.key == GDK_KEY_VoidSymbol) {
 *      bind.key = gdk_keyval_from_name(tok.c_str());
 *    } else {
 *      fprintf(stderr, "Error parsing keybind, multiple keys defined or unknown key in string \"%s\" (token:
 *\"%s\")\n", cstr, tok.c_str());
 *    }
 *    str.erase(0, pos + sep.size());
 *  }
 *
 *  return bind;
 *}
 */
