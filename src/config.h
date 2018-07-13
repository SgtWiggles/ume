#pragma once
#include <gdk/gdk.h>

#include <algorithm>
#include <string>

#include "defaults.h"
using palette_t = std::array<GdkRGBA, PALETTE_SIZE>;
struct term_colors_t {
	std::array<GdkRGBA, NUM_COLORSETS> forecolors;
	std::array<GdkRGBA, NUM_COLORSETS> backcolors;
	std::array<GdkRGBA, NUM_COLORSETS> curscolors;
	std::array<palette_t, NUM_COLORSETS> palettes;
};

/*
 *term_colors_t load_colorsets(GKeyFile *keyfile) {
 *  using str_ptr = std::unique_ptr<const gchar, g_free_deleter>;
 *  term_colors_t colors;
 *
 *  for (int i = 0; i < NUM_COLORSETS; i++) {
 *    char group[32];
 *    sprintf(group, "colors%d", i + 1);
 *
 *    auto cfgtmp = str_ptr(ume_load_config_or(group, COLOR_FOREGROUND_KEY, "rgb(192,192,192)"));
 *    gdk_rgba_parse(&colors.forecolors[i], cfgtmp.get());
 *
 *    cfgtmp = str_ptr(ume_load_config_or(group, COLOR_BACKGROUND_KEY, "rgba(0,0,0,1)"));
 *    gdk_rgba_parse(&colors.backcolors[i], cfgtmp.get());
 *
 *    cfgtmp = str_ptr(ume_load_config_or(group, COLOR_CURSOR_KEY, "rgb(255,255,255)"));
 *    gdk_rgba_parse(&colors.curscolors[i], cfgtmp.get());
 *
 *    for (int j = 0; j < PALETTE_SIZE; ++j) {
 *      char key[32];
 *      sprintf(key, COLOR_PALETTE_KEY, j);
 *      auto cfgstr = str_ptr(ume_load_config_or(group, key, DEFAULT_PALETTES[i][j]));
 *      gdk_rgba_parse(&colors.palettes[i][j], cfgstr.get());
 *    }
 *  }
 *  return colors;
 *}
 */

using accel_t = guint;
using key_t = gint;

struct keybind_t {
	key_t key = GDK_KEY_VoidSymbol;
	accel_t accelerator = 0;
};

struct keybind_t parse_keybind(const char *cstr) {
	const std::string sep = "+";
	keybind_t bind;

	auto str = std::string(cstr);
	if (str.size() == 0) // Empty strings are unbound keys
		return bind;

	str.erase(std::remove_if(std::begin(str), std::end(str), isspace), std::end(str));
	std::transform(std::begin(str), std::end(str), std::begin(str), toupper);
	str.push_back('+');

	size_t pos = 0;
	std::string tok;

	while ((pos = str.find(sep)) != std::string::npos) {
		tok = str.substr(0, pos);
		if (tok == "SHIFT") {
			bind.accelerator |= GDK_SHIFT_MASK;
		} else if (tok == "LOCK" || tok == "CAPS") {
			bind.accelerator |= GDK_LOCK_MASK;
		} else if (tok == "CONTROL" || tok == "CTRL") {
			bind.accelerator |= GDK_CONTROL_MASK;
		} else if (tok == "MOD1" || tok == "ALT") {
			bind.accelerator |= GDK_MOD1_MASK;
		} else if (tok == "MOD2") {
			bind.accelerator |= GDK_MOD2_MASK;
		} else if (tok == "MOD3") {
			bind.accelerator |= GDK_MOD3_MASK;
		} else if (tok == "MOD4") {
			bind.accelerator |= GDK_MOD4_MASK;
		} else if (tok == "MOD5") {
			bind.accelerator |= GDK_MOD5_MASK;
		} else if (tok == "BUTTON1") {
			bind.accelerator |= GDK_BUTTON1_MASK;
		} else if (tok == "BUTTON2") {
			bind.accelerator |= GDK_BUTTON2_MASK;
		} else if (tok == "BUTTON3") {
			bind.accelerator |= GDK_BUTTON3_MASK;
		} else if (tok == "BUTTON4") {
			bind.accelerator |= GDK_BUTTON4_MASK;
		} else if (tok == "BUTTON5") {
			bind.accelerator |= GDK_BUTTON5_MASK;
		} else if (tok == "SUPER") {
			bind.accelerator |= GDK_SUPER_MASK;
		} else if (tok == "HYPER") {
			bind.accelerator |= GDK_HYPER_MASK;
		} else if (tok == "META") {
			bind.accelerator |= GDK_META_MASK;
		} else if (bind.key == GDK_KEY_VoidSymbol) {
			bind.key = gdk_keyval_from_name(tok.c_str());
		} else {
			fprintf(stderr, "Error parsing keybind, multiple keys defined or unknown key in string \"%s\" (token: \"%s\")\n",
							cstr, tok.c_str());
		}
		str.erase(0, pos + sep.size());
	}

	return bind;
}

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

	const char *icon;
	const char *word_chars; /* Exceptions for word selection */

	PangoFontDescription *font;

	gchar *tab_default_title;
	gint last_colorset;

	accel_t open_url_accelerator;

	accel_t add_tab_accelerator;
	gint add_tab_key;

	accel_t del_tab_accelerator;
	gint del_tab_key;

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

// config_t load_from_keyfile(GKeyFile *keyfile, bool readonly) {
//	using cfgtmp_t = unique_g_ptr<const gchar>;
//	cfgtmp_t cfgtmp = nullptr;
//	config_t config;
//
//	/* We can safely ignore errors from g_key_file_get_value(), since if the
//	 * call to g_key_file_has_key() was successful, the key IS there. From the
//	 * glib docs I don't know if we can ignore errors from g_key_file_has_key,
//	 * too. I think we can: the only possible error is that the config file
//	 * doesn't exist, but we have just read it!
//	 */
//
//	config.colors = ume_load_colorsets();
//	config.last_colorset = ume_load_config_or<gint>(cfg_group, "last_colorset", 1);
//
//	config.scroll_lines = ume_load_config_or(cfg_group, "scroll_lines", DEFAULT_SCROLL_LINES);
//	config.scroll_amount = ume_load_config_or(cfg_group, "scroll_amount", DEFAULT_SCROLL_AMOUNT);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "font", DEFAULT_FONT));
//	config.font = pango_font_description_from_string(cfgtmp.get());
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "show_always_first_tab", "No"));
//	config.first_tab = (strcmp(cfgtmp.get(), "Yes") == 0) ? true : false;
//
//	config.show_scrollbar = ume_load_config_or(cfg_group, "scrollbar", false);
//	config.show_closebutton = ume_load_config_or(cfg_group, "closebutton", true);
//	config.tabs_on_bottom = ume_load_config_or(cfg_group, "tabs_on_bottom", false);
//
//	config.less_questions = ume_load_config_or(cfg_group, "less_questions", false);
//	config.disable_numbered_tabswitch = ume_load_config_or(cfg_group, "disable_numbered_tabswitch", false);
//	config.use_fading = ume_load_config_or(cfg_group, "use_fading", false);
//	config.scrollable_tabs = ume_load_config_or(cfg_group, "scrollable_tabs", true);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "urgent_bell", "Yes"));
//	config.urgent_bell = (strcmp(cfgtmp.get(), "Yes") == 0);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "audible_bell", "Yes"));
//	config.audible_bell = (strcmp(cfgtmp.get(), "Yes") == 0);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "blinking_cursor", "No"));
//	config.blinking_cursor = (strcmp(cfgtmp.get(), "Yes") == 0);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "stop_tab_cycling_at_end_tabs", "No"));
//	config.stop_tab_cycling_at_end_tabs = (strcmp(cfgtmp.get(), "Yes") == 0);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "allow_bold", "Yes"));
//	config.allow_bold = (strcmp(cfgtmp.get(), "Yes") == 0);
//
//	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "cursor_type", "block"));
//	if (strcmp(cfgtmp.get(), "block") == 0) {
//		config.cursor_type = VTE_CURSOR_SHAPE_BLOCK;
//	} else if (strcmp(cfgtmp.get(), "underline") == 0) {
//		config.cursor_type = VTE_CURSOR_SHAPE_UNDERLINE;
//	} else if (strcmp(cfgtmp.get(), "ibeam") == 0) {
//		config.cursor_type = VTE_CURSOR_SHAPE_IBEAM;
//	} else {
//		config.cursor_type = VTE_CURSOR_SHAPE_BLOCK;
//	}
//
//	config.word_chars = ume_load_config_or(cfg_group, "word_chars", DEFAULT_WORD_CHARS);
//
//	// TODO accelerator better strings or merge with keybinds!
//
//	// ----- Begin of keybinds -----
//	// TODO make a keybind struct with accelerator and key bundled together.
//	config.add_tab_accelerator = ume_load_config_or(cfg_group, "add_tab_accelerator", DEFAULT_ADD_TAB_ACCELERATOR);
//	config.add_tab_key = ume_load_keybind_or(cfg_group, "add_tab_key", DEFAULT_ADD_TAB_KEY);
//
//	config.del_tab_accelerator = ume_load_config_or(cfg_group, "del_tab_accelerator", DEFAULT_DEL_TAB_ACCELERATOR);
//	config.del_tab_key = ume_load_keybind_or(cfg_group, "del_tab_key", DEFAULT_DEL_TAB_KEY);
//
//	config.move_tab_accelerator = ume_load_config_or(cfg_group, "move_tab_accelerator", DEFAULT_MOVE_TAB_ACCELERATOR);
//	config.switch_tab_accelerator =
//			ume_load_config_or<gint>(cfg_group, "switch_tab_accelerator", DEFAULT_SWITCH_TAB_ACCELERATOR);
//	config.prev_tab_key = ume_load_keybind_or(cfg_group, "prev_tab_key", DEFAULT_PREV_TAB_KEY);
//	config.next_tab_key = ume_load_keybind_or(cfg_group, "next_tab_key", DEFAULT_NEXT_TAB_KEY);
//
//	config.copy_accelerator = ume_load_config_or(cfg_group, "copy_accelerator", DEFAULT_COPY_ACCELERATOR);
//	config.copy_key = ume_load_keybind_or(cfg_group, "copy_key", DEFAULT_COPY_KEY);
//	config.paste_key = ume_load_keybind_or(cfg_group, "paste_key", DEFAULT_PASTE_KEY);
//
//	config.scrollbar_accelerator = ume_load_config_or(cfg_group, "scrollbar_accelerator",
// DEFAULT_SCROLLBAR_ACCELERATOR); 	config.scrollbar_key = ume_load_keybind_or(cfg_group, "scrollbar_key",
// DEFAULT_SCROLLBAR_KEY); 	config.scroll_up_key = ume_load_keybind_or(cfg_group, "scroll_up_key",
// DEFAULT_SCROLL_UP_KEY); 	config.scroll_down_key = ume_load_keybind_or(cfg_group, "scroll_down_key",
// DEFAULT_SCROLL_DOWN_KEY); 	config.page_up_key = ume_load_keybind_or(cfg_group, "page_up_key", DEFAULT_PAGE_UP_KEY);
//	config.page_down_key = ume_load_keybind_or(cfg_group, "page_down_key", DEFAULT_PAGE_DOWN_KEY);
//
//	config.set_tab_name_accelerator =
//			ume_load_config_or(cfg_group, "set_tab_name_accelerator", DEFAULT_SET_TAB_NAME_ACCELERATOR);
//	config.set_tab_name_key = ume_load_keybind_or(cfg_group, "set_tab_name_key", DEFAULT_SET_TAB_NAME_KEY);
//
//	config.search_accelerator = ume_load_config_or(cfg_group, "search_accelerator", DEFAULT_SEARCH_ACCELERATOR);
//	config.search_key = ume_load_keybind_or(cfg_group, "search_key", DEFAULT_SEARCH_KEY);
//
//	config.font_size_accelerator = ume_load_config_or(cfg_group, "font_size_accelerator",
// DEFAULT_FONT_SIZE_ACCELERATOR); 	config.increase_font_size_key = 			ume_load_keybind_or(cfg_group,
//"increase_font_size_key", DEFAULT_INCREASE_FONT_SIZE_KEY); 	config.decrease_font_size_key =
//			ume_load_keybind_or(cfg_group, "decrease_font_size_key", DEFAULT_DECREASE_FONT_SIZE_KEY);
//
//	config.fullscreen_key = ume_load_keybind_or(cfg_group, "fullscreen_key", DEFAULT_FULLSCREEN_KEY);
//
//	config.set_colorset_accelerator =
//			ume_load_config_or(cfg_group, "set_colorset_accelerator", DEFAULT_SELECT_COLORSET_ACCELERATOR);
//	for (int i = 0; i < NUM_COLORSETS; ++i) {
//		char key_name[32];
//		sprintf(key_name, COLOR_SWITCH_KEY, i + 1);
//		config.set_colorset_keys[i] = ume_load_keybind_or(cfg_group, key_name, cs_keys[i]);
//	}
//
//	config.open_url_accelerator = ume_load_config_or(cfg_group, "open_url_accelerator", DEFAULT_OPEN_URL_ACCELERATOR);
//
//	// ------ End of keybindings -----
//	config.icon = ume_load_config_or(cfg_group, "icon_file", ICON_FILE);
//
//	/* set default title pattern from config or NULL */
//	config.tab_default_title = g_key_file_get_string(ume.cfg_file, cfg_group, "tab_default_title", NULL);
//	config.reload_config_on_modify = ume_load_config_or(cfg_group, "reload_config_on_modify", false);
//	config.ignore_overwrite = ume_load_config_or(cfg_group, "ignore_overwrite", false);
//}
