/*******************************************************************************
 *  Filename: ume.cpp
 *  Description: VTE-based terminal emulator
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#include <fcntl.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <locale.h>
#include <math.h>
#include <pango/pango.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vte/vte.h>
#include <wchar.h>

#include <array>
#include <memory>
#include <string>

#include "config.h"
#include "defaults.h"

#define _(String) gettext(String)
#define N_(String) (String)
#define GETTEXT_PACKAGE "ume"

// TODO template and compile time inline?
#define SAY(format, ...)                                                                                               \
	do {                                                                                                                 \
		if (strcmp("Debug", BUILDTYPE) == 0) {                                                                             \
			fprintf(stderr, "[%d] ", getpid());                                                                              \
			fprintf(stderr, "[%s] ", __FUNCTION__);                                                                          \
			if (format)                                                                                                      \
				fprintf(stderr, format, ##__VA_ARGS__);                                                                        \
			fputc('\n', stderr);                                                                                             \
			fflush(stderr);                                                                                                  \
		}                                                                                                                  \
	} while (0)

// TODO time utils, hooks at times, maybe a background service.
// TODO fix cursor blink and cursor type not working!

#define HIG_DIALOG_CSS                                                                                                 \
	"* {\n"                                                                                                              \
	"-GtkDialog-action-area-border : 12;\n"                                                                              \
	"-GtkDialog-button-spacing : 12;\n"                                                                                  \
	"}"

#define NOTEBOOK_CSS                                                                                                   \
	"* {\n"                                                                                                              \
	"color : rgba(0,0,0,1.0);\n"                                                                                         \
	"background-color : rgba(0,0,0,1.0);\n"                                                                              \
	"border-color : rgba(0,0,0,1.0);\n"                                                                                  \
	"}"

#define TAB_TITLE_CSS                                                                                                  \
	"* {\n"                                                                                                              \
	"padding : 0px;\n"                                                                                                   \
	"}"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

struct g_free_deleter {
	template <class T> void operator()(T *data) {
		g_free((void *)data);
	}
};
template <class T> using unique_g_ptr = std::unique_ptr<T, g_free_deleter>;

struct g_object_unref_deleter {
	template <class T> void operator()(T *data) {
		g_object_unref((void *)data);
	}
};
template <class T> using unique_g_object_ptr = std::unique_ptr<T, g_object_unref_deleter>;

static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GtkWidget *menu;

	GtkWidget *item_copy_link; /* We include here only the items which need to be hidden */
	GtkWidget *item_open_link;
	GtkWidget *item_open_mail;
	GtkWidget *open_link_separator;

	char *current_match;
	char *configfile;

	bool fullscreen;
	bool faded;				/* Fading state */
	bool first_focus; /* First time gtkwindow recieve focus when is created */
	bool focused;			/* For fading feature */

	bool config_modified;			/* Configuration has been modified */
	bool externally_modified; /* Configuration file has been modified by another process */
	bool resized;

	GKeyFile *cfg_file;

	gulong cfg_signal_id = 0;
	GFileMonitor *cfg_monitor;
	config_t config;

	guint width;
	guint height;
	glong columns;
	glong rows;

	GtkCssProvider *provider;
	gint label_count;

	const GdkRGBA *palette;
	char *argv[3];
} ume;

struct terminal {
	GtkWidget *hbox;
	GtkWidget *vte; /* Reference to VTE terminal */
	GPid pid;				/* pid of the forked process */
	GtkWidget *scrollbar;
	GtkWidget *label;
	gchar *label_text;
	bool label_set_byuser;
	GtkBorder padding; /* inner-property data */
	int colorset;
};

/* make this an array instead of #defines to get a compile time
 * error instead of a runtime if NUM_COLORSETS changes */
static int cs_keys[NUM_COLORSETS] = {GDK_KEY_F1, GDK_KEY_F2, GDK_KEY_F3, GDK_KEY_F4, GDK_KEY_F5, GDK_KEY_F6};

#define ERROR_BUFFER_LENGTH 256
const char cfg_group[] = "ume";

static GQuark term_data_id = 0;
// Maybe template this?
#define ume_get_page_term(ume, page_idx)                                                                               \
	(struct terminal *)g_object_get_qdata(G_OBJECT(gtk_notebook_get_nth_page((GtkNotebook *)ume.notebook, page_idx)),    \
																				term_data_id);

#define ume_set_page_term(ume, page_idx, term)                                                                         \
	g_object_set_qdata_full(G_OBJECT(gtk_notebook_get_nth_page((GtkNotebook *)ume.notebook, page_idx)), term_data_id,    \
													term, (GDestroyNotify)g_free);
// Config setters
template <class T> inline void ume_set_config(const gchar *group, const gchar *key, T value);
template <> inline void ume_set_config<gint>(const gchar *group, const gchar *key, gint value) {
	g_key_file_set_integer(ume.cfg_file, group, key, value);
	ume.config_modified = true;
}

// TODO find the line where this is getting called.
template <> inline void ume_set_config<GdkModifierType>(const gchar *group, const gchar *key, GdkModifierType value) {
	g_key_file_set_integer(ume.cfg_file, group, key, value);
	ume.config_modified = true;
}
template <> inline void ume_set_config<const gchar *>(const gchar *group, const gchar *key, const gchar *value) {
	g_key_file_set_string(ume.cfg_file, group, key, value);
	ume.config_modified = true;
}
/*
 *template <> inline void ume_set_config<gchar *>(const gchar *group, const gchar *key, gchar *value) {
 *  g_key_file_set_string(ume.cfg_file, group, key, value);
 *  ume.config_modified = true;
 *}
 */

template <> inline void ume_set_config<bool>(const char *group, const char *key, bool value) {
	g_key_file_set_boolean(ume.cfg_file, group, key, value);
	ume.config_modified = true;
}

// Config getters
template <class T> inline T ume_config_get(const gchar *group, const gchar *key) {
	return g_key_file_get_value(ume.cfg_file, group, key, nullptr);
}
template <> inline gint ume_config_get<gint>(const gchar *group, const gchar *key) {
	return g_key_file_get_integer(ume.cfg_file, group, key, nullptr);
}
template <> inline bool ume_config_get<bool>(const gchar *group, const gchar *key) {
	return g_key_file_get_boolean(ume.cfg_file, group, key, nullptr);
}
template <> inline gchar *ume_config_get<gchar *>(const gchar *group, const gchar *key) {
	return g_key_file_get_string(ume.cfg_file, group, key, nullptr);
}
template <> inline const gchar *ume_config_get<const gchar *>(const gchar *group, const gchar *key) {
	return g_key_file_get_string(ume.cfg_file, group, key, nullptr);
}

template <class T> inline T ume_load_config_or(const gchar *group, const gchar *key, T default_value) {
	if (!g_key_file_has_key(ume.cfg_file, group, key, NULL))
		ume_set_config<T>(group, key, default_value);
	return ume_config_get<T>(group, key);
}

/* Spawn callback */
void ume_spawm_callback(VteTerminal *, GPid, GError, gpointer); // TODO spelling error?
/* Callbacks */
static gboolean ume_key_press(GtkWidget *, GdkEventKey *, gpointer);
static gboolean ume_button_press(GtkWidget *, GdkEventButton *, gpointer);
static void ume_beep(GtkWidget *, void *);
static void ume_increase_font(GtkWidget *, void *);
static void ume_decrease_font(GtkWidget *, void *);
static void ume_child_exited(GtkWidget *, void *);
static void ume_eof(GtkWidget *, void *);
static void ume_title_changed(GtkWidget *, void *);
static gboolean ume_delete_event(GtkWidget *, void *);
static void ume_destroy_window(GtkWidget *, void *);
static gboolean ume_resized_window(GtkWidget *, GdkEventConfigure *, void *);
static gboolean ume_focus_in(GtkWidget *, GdkEvent *, void *);
static gboolean ume_focus_out(GtkWidget *, GdkEvent *, void *);
static void ume_closebutton_clicked(GtkWidget *, void *);
static void ume_conf_changed(GtkWidget *, void *);
static void ume_window_show_event(GtkWidget *, gpointer);
// static gboolean ume_notebook_focus_in (GtkWidget *, void *);
static gboolean ume_notebook_scroll(GtkWidget *, GdkEventScroll *);
static bool ume_close_tab(gint tab);

/* Menuitem callbacks */
static void ume_font_dialog(GtkWidget *, void *);
static void ume_set_name_dialog(GtkWidget *, void *);
static void ume_color_dialog(GtkWidget *, void *);
static void ume_set_title_dialog(GtkWidget *, void *);
static void ume_search_dialog(GtkWidget *, void *);
static void ume_new_tab(GtkWidget *, void *);
static void ume_close_tab_callback(GtkWidget *, void *);
static void ume_fullscreen(GtkWidget *, void *);
static void ume_open_url(GtkWidget *, void *);
static void ume_copy(GtkWidget *, void *);
static void ume_paste(GtkWidget *, void *);
static void ume_show_first_tab(GtkWidget *widget, void *data);
static void ume_tabs_on_bottom(GtkWidget *widget, void *data);
static void ume_less_questions(GtkWidget *widget, void *data);
static void ume_show_close_button(GtkWidget *widget, void *data);
static void ume_show_scrollbar(GtkWidget *, void *);
static void ume_disable_numbered_tabswitch(GtkWidget *, void *);
static void ume_use_fading(GtkWidget *, void *);
static void ume_setname_entry_changed(GtkWidget *, void *);

/* Misc */
static void ume_error(const char *, ...);

/* Functions */
static void ume_init();
static void ume_init_popup();
static void ume_destroy();
static void ume_add_tab();
static void ume_del_tab(gint);
static void ume_move_tab(gint);
static gint ume_find_tab(VteTerminal *);
static void ume_set_font();
static void ume_set_tab_label_text(const gchar *, gint page);
static void ume_set_size(void);
static void ume_set_keybind(const gchar *, guint);
static guint ume_get_keybind(const gchar *);
static guint ume_load_keybind_or(const gchar *, const gchar *, guint);
static void ume_config_done();
static void ume_set_colorset(int);
static void ume_set_colors(void);
static guint ume_tokeycode(guint key);
static void ume_fade_in(void);
static void ume_fade_out(void);
static void ume_reload_config_file(bool readonly);

/* Globals for command line parameters */
static const char *option_font;
static const char *option_workdir;
static const char *option_execute;
static gchar **option_xterm_args;
static gboolean option_xterm_execute = false;
static gboolean option_version = false;
static gint option_ntabs = 1;
static gint option_login = false;
static const char *option_title;
static const char *option_icon;
static int option_rows, option_columns;
static gboolean option_hold = false;
static char *option_config_file;
static gboolean option_fullscreen;
static gboolean option_maximize;
static gint option_colorset;

static GOptionEntry entries[] = { // Command line flags
		{"version", 'v', 0, G_OPTION_ARG_NONE, &option_version, N_("Print version number"), NULL},
		{"font", 'f', 0, G_OPTION_ARG_STRING, &option_font, N_("Select initial terminal font"), NULL},
		{"ntabs", 'n', 0, G_OPTION_ARG_INT, &option_ntabs, N_("Select initial number of tabs"), NULL},
		{"working-directory", 'd', 0, G_OPTION_ARG_STRING, &option_workdir, N_("Set working directory"), NULL},
		{"execute", 'x', 0, G_OPTION_ARG_STRING, &option_execute, N_("Execute command"), NULL},
		{"xterm-execute", 'e', 0, G_OPTION_ARG_NONE, &option_xterm_execute,
		 N_("Execute command (last option in the command line)"), NULL},
		{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &option_xterm_args, NULL, NULL},
		{"login", 'l', 0, G_OPTION_ARG_NONE, &option_login, N_("Login shell"), NULL},
		{"title", 't', 0, G_OPTION_ARG_STRING, &option_title, N_("Set window title"), NULL},
		{"icon", 'i', 0, G_OPTION_ARG_STRING, &option_icon, N_("Set window icon"), NULL},
		{"xterm-title", 'T', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &option_title, NULL, NULL},
		{"columns", 'c', 0, G_OPTION_ARG_INT, &option_columns, N_("Set columns number"), NULL},
		{"rows", 'r', 0, G_OPTION_ARG_INT, &option_rows, N_("Set rows number"), NULL},
		{"hold", 'h', 0, G_OPTION_ARG_NONE, &option_hold, N_("Hold window after execute command"), NULL},
		{"maximize", 'm', 0, G_OPTION_ARG_NONE, &option_maximize, N_("Maximize window"), NULL},
		{"fullscreen", 's', 0, G_OPTION_ARG_NONE, &option_fullscreen, N_("Fullscreen mode"), NULL},
		{"config-file", 0, 0, G_OPTION_ARG_FILENAME, &option_config_file, N_("Use alternate configuration file"), NULL},
		{"colorset", 0, 0, G_OPTION_ARG_INT, &option_colorset, N_("Select initial colorset"), NULL},
		{NULL}};

static guint ume_tokeycode(guint key) {
	GdkKeymap *keymap;
	GdkKeymapKey *keys;
	gint n_keys;
	guint res = 0;
	keymap = gdk_keymap_get_for_display(gdk_display_get_default());

	if (gdk_keymap_get_entries_for_keyval(keymap, key, &keys, &n_keys)) {
		if (n_keys > 0) {
			res = keys[0].keycode;
		}
		g_free(keys);
	}
	return res;
}

void search(VteTerminal *vte, const char *pattern, bool reverse) {
	GError *error = NULL;
	VteRegex *regex;

	vte_terminal_search_set_wrap_around(vte, true);

	regex = vte_regex_new_for_search(pattern, (gssize)strlen(pattern), PCRE2_MULTILINE | PCRE2_CASELESS, &error);
	if (!regex) { /* Ubuntu-fucking-morons (17.10 and 18.04) package a broken VTE without PCRE2, and search fails */
		ume_error(error->message);
		g_error_free(error);
	} else {
		vte_terminal_search_set_regex(vte, regex, 0);

		if (!vte_terminal_search_find_next(vte)) {
			vte_terminal_unselect_all(vte);
			vte_terminal_search_find_next(vte);
		}

		if (regex)
			vte_regex_unref(regex);
	}
}

// TODO think of a better way to register keybinds
static gboolean ume_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	if (event->type != GDK_KEY_PRESS)
		return false;

	gint topage = 0;
	gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	/* Use keycodes instead of keyvals. With keyvals, key bindings work only in US/ISO8859-1 and similar locales */
	guint keycode = event->hardware_keycode;

	if ((event->state & ume.config.add_tab_accelerator) == ume.config.add_tab_accelerator &&
			keycode == ume_tokeycode(ume.config.add_tab_key)) {
		ume_add_tab();
		return true;
	} else if ((event->state & ume.config.del_tab_accelerator) == ume.config.del_tab_accelerator &&
						 keycode == ume_tokeycode(ume.config.del_tab_key)) {
		/* Delete current tab */
		ume_close_tab_callback(NULL, NULL);
		return true;
	}

	/* Switch tab keybinding pressed (numbers or next/prev) */
	/* In cases when the user configured accelerators like these ones:
		 switch_tab_accelerator=4  for ctrl+next[prev]_tab_key
		 move_tab_accelerator=5  for ctrl+shift+next[prev]_tab_key
		 move never works, because switch will be processed first, so it needs to be fixed with the following condition */
	if (((event->state & ume.config.switch_tab_accelerator) == ume.config.switch_tab_accelerator) &&
			((event->state & ume.config.move_tab_accelerator) != ume.config.move_tab_accelerator)) {
		if ((keycode >= ume_tokeycode(GDK_KEY_1)) && (keycode <= ume_tokeycode(GDK_KEY_9))) {

			/* User has explicitly disabled this branch, make sure to propagate the event */
			if (ume.config.disable_numbered_tabswitch)
				return false;

			if (ume_tokeycode(GDK_KEY_1) == keycode)
				topage = 0;
			else if (ume_tokeycode(GDK_KEY_2) == keycode)
				topage = 1;
			else if (ume_tokeycode(GDK_KEY_3) == keycode)
				topage = 2;
			else if (ume_tokeycode(GDK_KEY_4) == keycode)
				topage = 3;
			else if (ume_tokeycode(GDK_KEY_5) == keycode)
				topage = 4;
			else if (ume_tokeycode(GDK_KEY_6) == keycode)
				topage = 5;
			else if (ume_tokeycode(GDK_KEY_7) == keycode)
				topage = 6;
			else if (ume_tokeycode(GDK_KEY_8) == keycode)
				topage = 7;
			else if (ume_tokeycode(GDK_KEY_9) == keycode)
				topage = 8;
			if (topage <= npages)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), topage);
			return true;
		} else if (keycode == ume_tokeycode(ume.config.prev_tab_key)) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook)) == 0) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), npages - 1);
			} else {
				gtk_notebook_prev_page(GTK_NOTEBOOK(ume.notebook));
			}
			return true;
		} else if (keycode == ume_tokeycode(ume.config.next_tab_key)) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook)) == (npages - 1)) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), 0);
			} else {
				gtk_notebook_next_page(GTK_NOTEBOOK(ume.notebook));
			}
			return true;
		}
	}

	if ((event->state & ume.config.move_tab_accelerator) == ume.config.move_tab_accelerator) {
		/* Move tab keybinding pressed */
		if (ume_tokeycode(ume.config.prev_tab_key) == keycode) {
			ume_move_tab(BACKWARDS);
			return true;
		}
		if (ume_tokeycode(ume.config.next_tab_key) == keycode) {
			ume_move_tab(FORWARD);
			return true;
		}
	}

	/* Copy/paste keybinding pressed */
	if ((event->state & ume.config.copy_accelerator) == ume.config.copy_accelerator) {
		if (keycode == ume_tokeycode(ume.config.copy_key)) {
			ume_copy(NULL, NULL);
			return true;
		} else if (keycode == ume_tokeycode(ume.config.paste_key)) {
			ume_paste(NULL, NULL);
			return true;
		}
	}

	/* Show scrollbar keybinding pressed */
	if ((event->state & ume.config.scrollbar_accelerator) == ume.config.scrollbar_accelerator) {
		if (keycode == ume_tokeycode(ume.config.scrollbar_key)) {
			ume_show_scrollbar(NULL, NULL);
			return true;
		}
	}

	/* Set tab name keybinding pressed */
	if ((event->state & ume.config.set_tab_name_accelerator) == ume.config.set_tab_name_accelerator) {
		if (keycode == ume_tokeycode(ume.config.set_tab_name_key)) {
			ume_set_name_dialog(NULL, NULL);
			return true;
		}
	}

	/* Search keybinding pressed */
	if ((event->state & ume.config.search_accelerator) == ume.config.search_accelerator) {
		if (keycode == ume_tokeycode(ume.config.search_key)) {
			ume_search_dialog(NULL, NULL);
			return true;
		}
	}

	/* Increase/decrease font size keybinding pressed */
	if ((event->state & ume.config.font_size_accelerator) == ume.config.font_size_accelerator) {
		if (keycode == ume_tokeycode(ume.config.increase_font_size_key)) {
			ume_increase_font(NULL, NULL);
			return true;
		} else if (keycode == ume_tokeycode(ume.config.decrease_font_size_key)) {
			ume_decrease_font(NULL, NULL);
			return true;
		}
	}

	// Scroll up and down with ctrl j, k
	if ((event->state & ume.config.scrollbar_accelerator) == ume.config.scrollbar_accelerator) {
		gint page;
		struct terminal *term;
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
		term = ume_get_page_term(ume, page);
		VteTerminal *vte = (VteTerminal *)term->vte;

		const int scroll_amount = [](guint keycode, VteTerminal *vte) {
			gint rows = (gint)vte_terminal_get_row_count(vte);
			if (keycode == ume_tokeycode(ume.config.scroll_up_key))
				return -ume.config.scroll_amount;
			else if (keycode == ume_tokeycode(ume.config.scroll_down_key))
				return ume.config.scroll_amount;
			else if (keycode == ume_tokeycode(ume.config.page_up_key))
				return -(rows - 1);
			else if (keycode == ume_tokeycode(ume.config.page_down_key))
				return (rows - 1);
			return 0;
		}(keycode, vte);

		if (scroll_amount != 0) {
			GtkAdjustment *adjust = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
			gtk_adjustment_set_value(adjust, gtk_adjustment_get_value(adjust) + scroll_amount);
			return true;
		}
	}

	/* F11 (fullscreen) pressed */
	if (keycode == ume_tokeycode(ume.config.fullscreen_key)) {
		ume_fullscreen(NULL, NULL);
		return true;
	}

	/* Change in colorset */
	if ((event->state & ume.config.set_colorset_accelerator) == ume.config.set_colorset_accelerator) {
		int i;
		for (i = 0; i < NUM_COLORSETS; i++) {
			if (keycode == ume_tokeycode(ume.config.set_colorset_keys[i])) {
				ume_set_colorset(i);
				return true;
			}
		}
	}
	return false;
}

// On click function
static gboolean ume_button_press(GtkWidget *widget, GdkEventButton *button_event, gpointer user_data) {
	struct terminal *term;
	gint page, tag;

	if (button_event->type != GDK_BUTTON_PRESS)
		return false;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	term = ume_get_page_term(ume, page);

	/* Find out if cursor it's over a matched expression...*/
	ume.current_match = vte_terminal_match_check_event(VTE_TERMINAL(term->vte), (GdkEvent *)button_event, &tag);

	/* Left button with accelerator: open the URL if any */
	if (button_event->button == 1 &&
			((button_event->state & ume.config.open_url_accelerator) == ume.config.open_url_accelerator) &&
			ume.current_match) {

		ume_open_url(NULL, NULL);
		return true;
	}

	/* Right button: show the popup menu */
	if (button_event->button == 3) { // TODO break this out somewhere, its showing the right click menu
		GtkMenu *menu;
		menu = GTK_MENU(widget);

		if (ume.current_match) {
			/* Show the extra options in the menu */

			char *matches;
			/* Is it a mail address? */
			if (vte_terminal_event_check_regex_simple(VTE_TERMINAL(term->vte), (GdkEvent *)button_event,
																								&ume.config.mail_vteregexp, 1, 0, &matches)) {
				gtk_widget_show(ume.item_open_mail);
				gtk_widget_hide(ume.item_open_link);
			} else {
				gtk_widget_show(ume.item_open_link);
				gtk_widget_hide(ume.item_open_mail);
			}
			gtk_widget_show(ume.item_copy_link);
			gtk_widget_show(ume.open_link_separator);

			g_free(matches);
		} else {
			/* Hide all the options */
			gtk_widget_hide(ume.item_open_mail);
			gtk_widget_hide(ume.item_open_link);
			gtk_widget_hide(ume.item_copy_link);
			gtk_widget_hide(ume.open_link_separator);
		}

		gtk_menu_popup_at_pointer(menu, (GdkEvent *)button_event);

		return true;
	}

	return false;
}

static gboolean ume_focus_in(GtkWidget *widget, GdkEvent *event, void *data) {
	if (event->type != GDK_FOCUS_CHANGE)
		return false;

	/* Ignore first focus event */
	if (ume.first_focus) {
		ume.first_focus = false;
		return false;
	}

	if (!ume.focused) {
		ume.focused = true;

		if (!ume.first_focus && ume.config.use_fading) {
			ume_fade_in();
		}

		ume_set_colors();
		return true;
	}

	return false;
}

static gboolean ume_focus_out(GtkWidget *widget, GdkEvent *event, void *data) {
	if (event->type != GDK_FOCUS_CHANGE)
		return false;

	if (ume.focused) {
		ume.focused = false;

		if (!ume.first_focus && ume.config.use_fading) {
			ume_fade_out();
		}

		ume_set_colors();
		return true;
	}

	return false;
}

/* Handler for notebook scroll-event - switches tabs by scroll direction
TODO: let scroll directions configurable */
static gboolean ume_notebook_scroll(GtkWidget *widget, GdkEventScroll *event) {
	gint page, npages;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	switch (event->direction) {
		case GDK_SCROLL_DOWN: {
			if (ume.config.stop_tab_cycling_at_end_tabs == 1) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), --page >= 0 ? page : 0);
			} else {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), --page >= 0 ? page : npages - 1);
			}
			break;
		}
		case GDK_SCROLL_UP: {
			if (ume.config.stop_tab_cycling_at_end_tabs == 1) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), ++page < npages ? page : npages - 1);
			} else {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), ++page < npages ? page : 0);
			}
			break;
		}

		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_SMOOTH:
			break;
	}

	return false;
}

static void ume_page_removed(GtkWidget *widget, void *data) {
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) == 1) {
		/* If the first tab is disabled, window size changes and we need
		 * to recalculate its size */
		ume_set_size();
	}
}

static void ume_beep(GtkWidget *widget, void *data) {
	// Remove the urgency hint. This is necessary to signal the window manager
	// that a new urgent event happened when the urgent hint is set after this.
	gtk_window_set_urgency_hint(GTK_WINDOW(ume.main_window), false);

	if (ume.config.urgent_bell) {
		gtk_window_set_urgency_hint(GTK_WINDOW(ume.main_window), true);
	}
}

static void ume_increase_font(GtkWidget *widget, void *data) {
	gint new_size;

	/* Increment font size one unit */
	new_size = pango_font_description_get_size(ume.config.font) + PANGO_SCALE;

	pango_font_description_set_size(ume.config.font, new_size);
	ume_set_font();
	ume_set_size();
	const char *descriptor = pango_font_description_to_string(ume.config.font);
	ume_set_config(cfg_group, "font", descriptor);
	g_free((void *)descriptor);
}

static void ume_decrease_font(GtkWidget *widget, void *data) {
	gint new_size;

	/* Decrement font size one unit */
	new_size = pango_font_description_get_size(ume.config.font) - PANGO_SCALE;

	/* Set a minimal size */
	if (new_size >= FONT_MINIMAL_SIZE) {
		pango_font_description_set_size(ume.config.font, new_size);
		ume_set_font();
		ume_set_size();
		const char *descriptor = pango_font_description_to_string(ume.config.font);
		ume_set_config(cfg_group, "font", descriptor);
		g_free((void *)descriptor);
	}
}

static void ume_child_exited(GtkWidget *widget, void *data) {
	gint page, npages;
	struct terminal *term;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(ume.notebook), gtk_widget_get_parent(widget));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	term = ume_get_page_term(ume, page);

	/* Only write configuration to disk if it's the last tab */
	if (npages == 1) {
		ume_config_done();
	}

	if (option_hold == true) {
		SAY("hold option has been activated");
		return;
	}

	/* Child should be automatically reaped because we don't use G_SPAWN_DO_NOT_REAP_CHILD flag */
	g_spawn_close_pid(term->pid);

	ume_del_tab(page);

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	if (npages == 0)
		ume_destroy();
}

static void ume_eof(GtkWidget *widget, void *data) {

	SAY("Got EOF signal");

	gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	/* Only write configuration to disk if it's the last tab */
	if (npages == 1) {
		ume_config_done();
	}

	/* Workaround for libvte strange behaviour. There is not child-exited signal for
		 the last terminal, so we need to kill it here.  Check with libvte authors about
		 child-exited/eof signals */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook)) == 0) {
		struct terminal *term = ume_get_page_term(ume, 0);

		if (option_hold == true) {
			SAY("hold option has been activated");
			return;
		}

		// SAY("waiting for terminal pid (in eof) %d", term->pid);
		// waitpid(term->pid, &status, WNOHANG);
		/* TODO: check wait return */
		/* Child should be automatically reaped because we don't use G_SPAWN_DO_NOT_REAP_CHILD flag */
		g_spawn_close_pid(term->pid);

		ume_del_tab(0);

		npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
		if (npages == 0)
			ume_destroy();
	}
}

/* This handler is called when window title changes, and is used to change window and notebook pages titles */
static void ume_title_changed(GtkWidget *widget, void *data) {
	VteTerminal *vte_term = (VteTerminal *)widget;
	gint modified_page = ume_find_tab(vte_term);
	gint n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, modified_page);

	const char *title = vte_terminal_get_window_title(VTE_TERMINAL(term->vte));

	/* User set values overrides any other one, but title should be changed */
	if (!term->label_set_byuser)
		ume_set_tab_label_text(title, modified_page);

	if (option_title == NULL) {
		if (n_pages == 1) {
			/* Beware: It doesn't work in Unity because there is a Compiz bug: #257391 */
			gtk_window_set_title(GTK_WINDOW(ume.main_window), title);
		} else
			gtk_window_set_title(GTK_WINDOW(ume.main_window), "ume");
	} else {
		gtk_window_set_title(GTK_WINDOW(ume.main_window), option_title);
	}
}

/* Save configuration */
static void ume_config_done() {
	GError *gerror = NULL;
	gsize len = 0;

	gchar *cfgdata = g_key_file_to_data(ume.cfg_file, &len, &gerror);
	if (!cfgdata) {
		fprintf(stderr, "%s\n", gerror->message);
		exit(EXIT_FAILURE);
	}

	/* Write to file IF there's been changes */
	if (ume.config_modified) {
		bool overwrite = true;
		if (ume.config.ignore_overwrite)
			overwrite = false;

		if (ume.externally_modified && !ume.config.ignore_overwrite) { // TODO break this into a confirmation function
			GtkWidget *dialog;
			gint response;

			dialog = gtk_message_dialog_new(GTK_WINDOW(ume.main_window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
																			GTK_BUTTONS_YES_NO,
																			_("Configuration has been modified by another process. Overwrite?"));

			response = gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			if (response == GTK_RESPONSE_YES)
				overwrite = true;
			else
				overwrite = false;
		}

		if (overwrite) {
			GIOChannel *cfgfile = g_io_channel_new_file(ume.configfile, "w", &gerror);
			if (!cfgfile) {
				fprintf(stderr, "%s\n", gerror->message);
				g_error_free(gerror);
				exit(EXIT_FAILURE);
			}

			/* FIXME: if the number of chars written is not "len", something happened.
			 * Check for errors appropriately...*/
			GIOStatus status = g_io_channel_write_chars(cfgfile, cfgdata, len, NULL, &gerror);
			if (status != G_IO_STATUS_NORMAL) {
				// FIXME: we should deal with temporary failures (G_IO_STATUS_AGAIN)
				fprintf(stderr, "%s\n", gerror->message);
				g_error_free(gerror);
				exit(EXIT_FAILURE);
			}
			g_io_channel_shutdown(cfgfile, true, &gerror);
			g_io_channel_unref(cfgfile);
		}
	}
}

static gboolean ume_delete_event(GtkWidget *widget, void *data) {
	if (!ume.config.less_questions) {
		while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) > 0) {
			if (!ume_close_tab(0)) {
				return true;
			}
		}
	}

	ume_config_done();
	return false;
}

static void ume_destroy_window(GtkWidget *widget, void *data) {
	ume_destroy();
}

static void ume_window_show_event(GtkWidget *widget, gpointer data) {
	// set size when the window is first shown
	ume_set_size();
}

static void ume_font_dialog(GtkWidget *widget, void *data) {
	GtkWidget *font_dialog = gtk_font_chooser_dialog_new(_("Select font"), GTK_WINDOW(ume.main_window));
	gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(font_dialog), ume.config.font);

	gint response = gtk_dialog_run(GTK_DIALOG(font_dialog));

	if (response == GTK_RESPONSE_OK) {
		pango_font_description_free(ume.config.font);
		ume.config.font = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(font_dialog));
		ume_set_font();
		ume_set_size();
		const char *descriptor = pango_font_description_to_string(ume.config.font);
		ume_set_config(cfg_group, "font", descriptor);
		g_free((void *)descriptor);
	}

	gtk_widget_destroy(font_dialog);
}

static void ume_set_name_dialog(GtkWidget *widget, void *data) { // TODO break this up a bit
	GtkWidget *input_dialog, *input_header;
	GtkWidget *entry, *label;
	GtkWidget *name_hbox; /* We need this for correct spacing */
	gint response;
	gint page;
	struct terminal *term;
	const gchar *text;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	term = ume_get_page_term(ume, page);

	input_dialog = gtk_dialog_new_with_buttons(_("Set tab name"), GTK_WINDOW(ume.main_window),
																						 (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR),
																						 _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"), GTK_RESPONSE_ACCEPT, NULL);

	/* Configure the new gtk header bar*/
	input_header = gtk_dialog_get_header_bar(GTK_DIALOG(input_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(input_header), false);
	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);

	/* Set style */
	gchar *css = g_strdup_printf(HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(ume.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context(input_dialog);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(ume.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	name_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	entry = gtk_entry_new();
	label = gtk_label_new(_("New text"));
	/* Set tab label as entry default text (when first tab is not displayed, get_tab_label_text
		 returns a null value, so check accordingly */
	text = gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(ume.notebook), term->hbox);
	if (text) {
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	}
	gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
	gtk_box_pack_start(GTK_BOX(name_hbox), label, true, true, 12);
	gtk_box_pack_start(GTK_BOX(name_hbox), entry, true, true, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(input_dialog))), name_hbox, false, false, 12);

	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(ume_setname_entry_changed), input_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, false);

	gtk_widget_show_all(name_hbox);

	response = gtk_dialog_run(GTK_DIALOG(input_dialog));

	if (response == GTK_RESPONSE_ACCEPT) {
		ume_set_tab_label_text(gtk_entry_get_text(GTK_ENTRY(entry)), page);
		term->label_set_byuser = true;
	}

	gtk_widget_destroy(input_dialog);
}

static void ume_set_colorset(int cs) {
	if (cs < 0 || cs >= NUM_COLORSETS)
		return;

	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);
	term->colorset = cs;
	ume.palette = ume.config.colors.palettes[cs].data();

	ume_set_config(cfg_group, "last_colorset", term->colorset + 1);
	ume_set_colors();
}

/* Set the terminal colors for all notebook tabs */
static void ume_set_colors() {
	int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term;
	for (int i = (n_pages - 1); i >= 0; i--) {
		term = ume_get_page_term(ume, i);
		SAY("Setting colorset %d", term->colorset + 1);

		vte_terminal_set_colors(VTE_TERMINAL(term->vte), &ume.config.colors.forecolors[term->colorset],
														&ume.config.colors.backcolors[term->colorset], ume.palette, PALETTE_SIZE);

		// TODO add a way to toggle between the cursor color methods, right now we only allow the inverting method
		// vte_terminal_set_color_cursor(VTE_TERMINAL(term->vte), &ume.config.colors.curscolors[term->colorset]);
		vte_terminal_set_color_cursor((VteTerminal *)term->vte, nullptr);
	}

	/* Main window opacity must be set. Otherwise vte widget will remain opaque */
	gtk_widget_set_opacity(ume.main_window, ume.config.colors.backcolors[term->colorset].alpha);
}

/* Callback from the color change dialog. Updates the contents of that
 * dialog, passed as 'data' from user input. */
#define COLOR_BUTTON_ID "color_button%d"
static void ume_color_dialog_changed(GtkWidget *widget, void *data) {
	GtkDialog *dialog = (GtkDialog *)data;
	GtkColorButton *fore_button = (GtkColorButton *)g_object_get_data(G_OBJECT(dialog), "fg_button");
	GtkColorButton *back_button = (GtkColorButton *)g_object_get_data(G_OBJECT(dialog), "bg_button");
	GtkColorButton *curs_button = (GtkColorButton *)g_object_get_data(G_OBJECT(dialog), "curs_button");
	GtkComboBox *set = (GtkComboBox *)g_object_get_data(G_OBJECT(dialog), "set_combo");
	GtkSpinButton *opacity_spin = (GtkSpinButton *)g_object_get_data(G_OBJECT(dialog), "opacity_spin");
	int selected = gtk_combo_box_get_active(set);

	std::array<GtkWidget *, PALETTE_SIZE> color_buttons;
	for (unsigned i = 0; i < color_buttons.size(); ++i) {
		char temp[64];
		sprintf(temp, COLOR_BUTTON_ID, i);
		color_buttons[i] = (GtkWidget *)g_object_get_data(G_OBJECT(dialog), temp);
	}

	/* if we come here as a result of a change in the active colorset,
	 * load the new colorset to the buttons.
	 * Else, the colorselect buttons or opacity spin have gotten a new
	 * value, store that. */
	if ((GtkWidget *)set == widget) {
		/* Spin opacity is a percentage, convert it*/
		gint new_opacity = (int)(ume.config.colors.backcolors[selected].alpha * 100);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(fore_button), &ume.config.colors.forecolors[selected]);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(back_button), &ume.config.colors.backcolors[selected]);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(curs_button), &ume.config.colors.curscolors[selected]);
		for (int i = 0; i < PALETTE_SIZE; ++i)
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_buttons[i]), &(ume.config.colors.palettes[selected][i]));

		gtk_spin_button_set_value(opacity_spin, new_opacity);
	} else {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(fore_button), &ume.config.colors.forecolors[selected]);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(back_button), &ume.config.colors.backcolors[selected]);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(curs_button), &ume.config.colors.curscolors[selected]);
		for (int i = 0; i < PALETTE_SIZE; ++i)
			gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_buttons[i]), &(ume.config.colors.palettes[selected][i]));

		gtk_spin_button_update(opacity_spin);
		ume.config.colors.backcolors[selected].alpha = gtk_spin_button_get_value(opacity_spin) / 100;
	}

	ume.config.last_colorset = selected + 1;
	ume_set_colorset(selected);
}

static GtkWidget *create_color_button(GtkWidget *dialog, const gchar *label, const GdkRGBA *color,
																			const gchar *button_id) {
	// gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

	if (label != nullptr) {
		GtkWidget *blabel = gtk_label_new(label);
		gtk_box_pack_start(GTK_BOX(hbox), blabel, false, false, 6);
	}

	GtkWidget *button = gtk_color_button_new_with_rgba(color);
	gtk_box_pack_end(GTK_BOX(hbox), button, false, false, 6);

	g_signal_connect(G_OBJECT(button), "color-set", G_CALLBACK(ume_color_dialog_changed), dialog);
	if (button_id != nullptr)
		g_object_set_data(G_OBJECT(dialog), button_id, button);
	return hbox;
}

static GtkWidget *ume_create_color_dialog(GtkWidget *widget, void *data) {
	GtkWidget *color_dialog = gtk_dialog_new_with_buttons(
			_("Select colors"), GTK_WINDOW(ume.main_window), (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR),
			_("_Cancel"), GTK_RESPONSE_CANCEL, _("_Select"), GTK_RESPONSE_ACCEPT, NULL);

	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	std::array<GtkWidget *, PALETTE_SIZE> color_buttons;
	for (int i = 0; i < PALETTE_SIZE; ++i) {
		char button_id[32];
		sprintf(button_id, COLOR_BUTTON_ID, i);
		color_buttons[i] =
				create_color_button(color_dialog, nullptr, &ume.config.colors.palettes[term->colorset][i], button_id);
	}

	std::array<GtkWidget *, 3> special_buttons = {
			create_color_button(color_dialog, "Foreground", &ume.config.colors.forecolors[term->colorset], "fg_button"),
			create_color_button(color_dialog, "Background", &ume.config.colors.backcolors[term->colorset], "bg_button"),
			create_color_button(color_dialog, "Cursor", &ume.config.colors.curscolors[term->colorset], "curs_button")};

	/* Configure the new gtk header bar*/
	GtkWidget *color_header = gtk_dialog_get_header_bar(GTK_DIALOG(color_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(color_header), false);
	gtk_dialog_set_default_response(GTK_DIALOG(color_dialog), GTK_RESPONSE_ACCEPT);

	/* Set style */
	gchar *css = g_strdup_printf(HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(ume.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context(color_dialog);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(ume.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	/* Add the drop-down combobox that selects current colorset to edit. */
	GtkWidget *hbox_sets = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

	GtkWidget *set_label = gtk_label_new("Colorset");
	GtkWidget *set_combo = gtk_combo_box_text_new();
	gchar combo_text[3];
	for (int cs = 0; cs < NUM_COLORSETS; cs++) {
		g_snprintf(combo_text, 2, "%d", cs + 1);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(set_combo), NULL, combo_text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(set_combo), term->colorset);

	/* Foreground and background and cursor color buttons */
	/* Opacity control */
	GtkWidget *hbox_opacity = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	GtkAdjustment *spinner_adj =
			gtk_adjustment_new((ume.config.colors.backcolors[term->colorset].alpha) * 100, 0.0, 99.0, 1.0, 5.0, 0);
	GtkWidget *opacity_spin = gtk_spin_button_new(GTK_ADJUSTMENT(spinner_adj), 1.0, 0);
	GtkWidget *opacity_label = gtk_label_new("Opacity level (%)");

	std::array<GtkWidget *, 4> content_boxes = {
			gtk_box_new(GTK_ORIENTATION_VERTICAL, 12), gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12),
			gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12), gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12)};

	gtk_box_pack_start(GTK_BOX(hbox_sets), set_label, false, false, 12);
	gtk_box_pack_end(GTK_BOX(hbox_sets), set_combo, false, false, 12);
	gtk_box_pack_start(GTK_BOX(hbox_opacity), opacity_label, false, false, 12);
	gtk_box_pack_end(GTK_BOX(hbox_opacity), opacity_spin, false, false, 12);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_sets, false, false, 6);
	for (auto btn : special_buttons)
		gtk_box_pack_start(GTK_BOX(content_boxes[0]), btn, false, false, 0);
	gtk_box_pack_start(GTK_BOX(content_boxes[1]), gtk_label_new("Palette: "), false, false, 6);

	for (int i = 0; i < PALETTE_SIZE / 2; ++i)
		gtk_box_pack_start(GTK_BOX(content_boxes[2]), color_buttons[i], false, false, 0);
	for (int i = PALETTE_SIZE / 2; i < PALETTE_SIZE; ++i)
		gtk_box_pack_start(GTK_BOX(content_boxes[3]), color_buttons[i], false, false, 0);

	for (auto box : content_boxes)
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), box, false, false, 6);
	gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_opacity, false, false, 6);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog)));

	/* When user switches the colorset to change, the callback needs access
	 * to these selector widgets */
	g_object_set_data(G_OBJECT(color_dialog), "set_combo", set_combo);
	g_object_set_data(G_OBJECT(color_dialog), "opacity_spin", opacity_spin);

	g_signal_connect(G_OBJECT(set_combo), "changed", G_CALLBACK(ume_color_dialog_changed), color_dialog);
	g_signal_connect(G_OBJECT(opacity_spin), "changed", G_CALLBACK(ume_color_dialog_changed), color_dialog);
	return color_dialog;
}

static void ume_color_dialog(GtkWidget *widget, void *data) {
	struct term_colors_t temp_colors = ume.config.colors;
	int prev_colorset = ume.config.last_colorset - 1;

	GtkWidget *color_dialog = ume_create_color_dialog(widget, data);
	gint response = gtk_dialog_run(GTK_DIALOG(color_dialog)); // Loop on and update the dialog menu

	if (response != GTK_RESPONSE_ACCEPT) {
		ume.config.colors = temp_colors;
		ume_set_colorset(prev_colorset);
	} else {
		// NOTE: config writing goes here, maybe break all this out somewhere else
		/* Save all colorsets to both the global struct and configuration.*/
		for (int i = 0; i < NUM_COLORSETS; i++) {
			char group[20];
			sprintf(group, COLOR_GROUP_KEY, i + 1);

			auto cfgtmp = std::unique_ptr<const gchar, g_free_deleter>(gdk_rgba_to_string(&ume.config.colors.forecolors[i]));
			ume_set_config(group, COLOR_FOREGROUND_KEY, cfgtmp.get());

			cfgtmp = std::unique_ptr<const gchar, g_free_deleter>(gdk_rgba_to_string(&ume.config.colors.backcolors[i]));
			ume_set_config(group, COLOR_BACKGROUND_KEY, cfgtmp.get());

			cfgtmp = std::unique_ptr<const gchar, g_free_deleter>(gdk_rgba_to_string(&ume.config.colors.curscolors[i]));
			ume_set_config(group, COLOR_CURSOR_KEY, cfgtmp.get());

			for (int j = 0; j < PALETTE_SIZE; ++j) {
				char temp_name[32];
				sprintf(temp_name, COLOR_PALETTE_KEY, j);
				auto cfgstr =
						std::unique_ptr<const gchar, g_free_deleter>(gdk_rgba_to_string(&ume.config.colors.palettes[i][j]));
				ume_set_config(group, temp_name, cfgstr.get());
			}
		}

		/* Apply the new colorsets to all tabs
		 * Set the current tab's colorset to the last selected one in the dialog.
		 * This is probably what the new user expects, and the experienced user
		 * hopefully will not mind. */
		ume_set_colorset(ume.config.last_colorset - 1);
	}
	gtk_widget_destroy(color_dialog);
}

// Fading
static void ume_fade_out() { // NOTE:  maybe we can make this fade between color switching
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	if (!ume.faded) {
		ume.faded = true;
		GdkRGBA x = ume.config.colors.forecolors[term->colorset];
		// SAY("fade out red %f to %f", x.red, x.red/100.0*FADE_PERCENT);
		x.red = x.red / 100.0 * FADE_PERCENT;
		x.green = x.green / 100.0 * FADE_PERCENT;
		x.blue = x.blue / 100.0 * FADE_PERCENT;
		if ((x.red >= 0 && x.red <= 1.0) && (x.green >= 0 && x.green <= 1.0) && (x.blue >= 0 && x.blue <= 1.0)) {
			ume.config.colors.forecolors[term->colorset] = x;
		} else {
			SAY("Forecolor value out of range");
		}
	}
}
static void ume_fade_in() { // NOTE: maybe we can make this fade between color switching
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	if (ume.faded) {
		ume.faded = false;
		GdkRGBA x = ume.config.colors.forecolors[term->colorset];
		// SAY("fade in red %f to %f", x.red, x.red/FADE_PERCENT*100.0);
		x.red = x.red / FADE_PERCENT * 100.0;
		x.green = x.green / FADE_PERCENT * 100.0;
		x.blue = x.blue / FADE_PERCENT * 100.0;
		if ((x.red >= 0 && x.red <= 1.0) && (x.green >= 0 && x.green <= 1.0) && (x.blue >= 0 && x.blue <= 1.0)) {
			ume.config.colors.forecolors[term->colorset] = x;
		} else {
			SAY("Forecolor value out of range");
		}
	}
}

static void ume_search_dialog(GtkWidget *widget, void *data) { // TODO: (low) inline into terminal itself? clean up
	GtkWidget *title_dialog, *title_header;
	GtkWidget *entry, *label;
	GtkWidget *title_hbox;
	gint response;

	title_dialog = gtk_dialog_new_with_buttons(_("Search"), GTK_WINDOW(ume.main_window),
																						 (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR),
																						 _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"), GTK_RESPONSE_ACCEPT, NULL);

	/* Configure the new gtk header bar*/
	title_header = gtk_dialog_get_header_bar(GTK_DIALOG(title_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(title_header), false);
	gtk_dialog_set_default_response(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT);

	/* Set style */
	gchar *css = g_strdup_printf(HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(ume.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context(title_dialog);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(ume.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	entry = gtk_entry_new();
	label = gtk_label_new(_("Search"));
	title_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
	gtk_box_pack_start(GTK_BOX(title_hbox), label, true, true, 12);
	gtk_box_pack_start(GTK_BOX(title_hbox), entry, true, true, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(title_dialog))), title_hbox, false, false, 12);

	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(ume_setname_entry_changed), title_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, false);

	gtk_widget_show_all(title_hbox);

	response = gtk_dialog_run(GTK_DIALOG(title_dialog));
	if (response == GTK_RESPONSE_ACCEPT) {
		gint page;
		struct terminal *term;
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
		term = ume_get_page_term(ume, page);
		search(VTE_TERMINAL(term->vte), gtk_entry_get_text(GTK_ENTRY(entry)), 0);
	}
	gtk_widget_destroy(title_dialog);
}

// TODO clean up
static void ume_set_title_dialog(GtkWidget *widget, void *data) {
	GtkWidget *title_dialog, *title_header;
	GtkWidget *entry, *label;
	GtkWidget *title_hbox;
	gint response;

	title_dialog = gtk_dialog_new_with_buttons(_("Set window title"), GTK_WINDOW(ume.main_window),
																						 (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR),
																						 _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"), GTK_RESPONSE_ACCEPT, NULL);

	/* Configure the new gtk header bar*/
	title_header = gtk_dialog_get_header_bar(GTK_DIALOG(title_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(title_header), false);
	gtk_dialog_set_default_response(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT);

	/* Set style */
	gchar *css = g_strdup_printf(HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(ume.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context(title_dialog);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(ume.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	entry = gtk_entry_new();
	label = gtk_label_new(_("New window title"));
	title_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	/* Set window label as entry default text */
	gtk_entry_set_text(GTK_ENTRY(entry), gtk_window_get_title(GTK_WINDOW(ume.main_window)));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
	gtk_box_pack_start(GTK_BOX(title_hbox), label, true, true, 12);
	gtk_box_pack_start(GTK_BOX(title_hbox), entry, true, true, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(title_dialog))), title_hbox, false, false, 12);

	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(ume_setname_entry_changed), title_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, false);

	gtk_widget_show_all(title_hbox);

	response = gtk_dialog_run(GTK_DIALOG(title_dialog));
	if (response == GTK_RESPONSE_ACCEPT) {
		/* Bug #257391 shadow reachs here too... */
		gtk_window_set_title(GTK_WINDOW(ume.main_window), gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(title_dialog);
}

static void ume_copy_url(GtkWidget *widget, void *data) {
	GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clip, ume.current_match, -1);
	clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clip, ume.current_match, -1);
}

static void ume_open_url(GtkWidget *widget, void *data) {
	GError *error = NULL;
	gchar *browser = NULL;

	SAY("Opening %s", ume.current_match);

	browser = g_strdup(g_getenv("BROWSER"));

	if (!browser) {
		if (!(browser = g_find_program_in_path("xdg-open"))) {
			/* TODO: Legacy for systems without xdg-open. This should be removed */
			browser = g_strdup("firefox");
		}
	}

	gchar *argv[] = {browser, ume.current_match, NULL};
	if (!g_spawn_async(".", argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
		ume_error("Couldn't exec \"%s %s\": %s", browser, ume.current_match, error->message);
		g_error_free(error);
	}

	g_free(browser);
}

static void ume_open_mail(GtkWidget *widget, void *data) {
	GError *error = NULL;
	gchar *program = NULL;

	if ((program = g_find_program_in_path("xdg-email"))) {
		gchar *argv[] = {program, ume.current_match, NULL};
		if (!g_spawn_async(".", argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
			ume_error("Couldn't exec \"%s %s\": %s", program, ume.current_match, error->message);
		}
		g_free(program);
	}
}

static void ume_show_first_tab(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), true);
		ume_set_config(cfg_group, "show_always_first_tab", "Yes");
		ume.config.first_tab = true;
	} else {
		/* Only hide tabs if the notebook has one page */
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) == 1) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), false);
		}
		ume_set_config(cfg_group, "show_always_first_tab", "No");
		ume.config.first_tab = false;
	}
	ume_set_size();
}

// TODO these are just config setters, maybe we can just use lambdas or something
static void ume_tabs_on_bottom(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(ume.notebook), GTK_POS_BOTTOM);
		ume_set_config(cfg_group, "tabs_on_bottom", true);
	} else {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(ume.notebook), GTK_POS_TOP);
		ume_set_config(cfg_group, "tabs_on_bottom", false);
	}
}

static void ume_less_questions(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		ume.config.less_questions = true;
		ume_set_config(cfg_group, "less_questions", true);
	} else {
		ume.config.less_questions = false;
		ume_set_config(cfg_group, "less_questions", false);
	}
}

static void ume_show_close_button(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		ume_set_config(cfg_group, "closebutton", true);
	} else {
		ume_set_config(cfg_group, "closebutton", false);
	}
}

static void ume_show_scrollbar(GtkWidget *widget, void *data) {
	ume.config.keep_fc = 1;

	gint n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	if (!g_key_file_get_boolean(ume.cfg_file, cfg_group, "scrollbar", NULL)) {
		ume.config.show_scrollbar = true;
		ume_set_config(cfg_group, "scrollbar", true);
	} else {
		ume.config.show_scrollbar = false;
		ume_set_config(cfg_group, "scrollbar", false);
	}

	/* Toggle/Untoggle the scrollbar for all tabs */
	for (int i = (n_pages - 1); i >= 0; i--) {
		term = ume_get_page_term(ume, i);
		if (!ume.config.show_scrollbar)
			gtk_widget_hide(term->scrollbar);
		else
			gtk_widget_show(term->scrollbar);
	}
	ume_set_size();
}

static void ume_urgent_bell(GtkWidget *widget, void *data) {
	ume.config.urgent_bell = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	if (ume.config.urgent_bell) {
		ume_set_config(cfg_group, "urgent_bell", "Yes");
	} else {
		ume_set_config(cfg_group, "urgent_bell", "No");
	}
}

static void ume_audible_bell(GtkWidget *widget, void *data) {
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	term = ume_get_page_term(ume, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_audible_bell(VTE_TERMINAL(term->vte), true);
		ume_set_config(cfg_group, "audible_bell", "Yes");
	} else {
		vte_terminal_set_audible_bell(VTE_TERMINAL(term->vte), false);
		ume_set_config(cfg_group, "audible_bell", "No");
	}
}

static void ume_blinking_cursor(GtkWidget *widget, void *data) {
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	term = ume_get_page_term(ume, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term->vte), VTE_CURSOR_BLINK_ON);
		ume_set_config(cfg_group, "blinking_cursor", "Yes");
	} else {
		vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term->vte), VTE_CURSOR_BLINK_OFF);
		ume_set_config(cfg_group, "blinking_cursor", "No");
	}
}

static void ume_allow_bold(GtkWidget *widget, void *data) {
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_allow_bold(VTE_TERMINAL(term->vte), true);
		ume_set_config(cfg_group, "allow_bold", "Yes");
	} else {
		vte_terminal_set_allow_bold(VTE_TERMINAL(term->vte), false);
		ume_set_config(cfg_group, "allow_bold", "No");
	}
}

static void ume_stop_tab_cycling_at_end_tabs(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		ume_set_config(cfg_group, "stop_tab_cycling_at_end_tabs", "Yes");
		ume.config.stop_tab_cycling_at_end_tabs = true;
	} else {
		ume_set_config(cfg_group, "stop_tab_cycling_at_end_tabs", "No");
		ume.config.stop_tab_cycling_at_end_tabs = false;
	}
}

static void ume_set_cursor(GtkWidget *widget, void *data) {

	char *cursor_string = (char *)data;
	int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		if (strcmp(cursor_string, "block") == 0) {
			ume.config.cursor_type = VTE_CURSOR_SHAPE_BLOCK;
			ume_set_config(cfg_group, "cursor_type", "block");
		} else if (strcmp(cursor_string, "underline") == 0) {
			ume.config.cursor_type = VTE_CURSOR_SHAPE_UNDERLINE;
			ume_set_config(cfg_group, "cursor_type", "underline");
		} else if (strcmp(cursor_string, "ibeam") == 0) {
			ume.config.cursor_type = VTE_CURSOR_SHAPE_IBEAM;
			ume_set_config(cfg_group, "cursor_type", "ibeam");
		}

		for (int i = (n_pages - 1); i >= 0; i--) {
			struct terminal *term = ume_get_page_term(ume, i);
			vte_terminal_set_cursor_shape(VTE_TERMINAL(term->vte), ume.config.cursor_type);
		}
	}
}

/* Retrieve the cwd of the specified term page.
 * Original function was from terminal-screen.c of gnome-terminal, copyright (C) 2001 Havoc Pennington
 * Adapted by Hong Jen Yee, non-linux shit removed by David Gómez */
static char *ume_get_term_cwd(struct terminal *term) {
	char *cwd = NULL;

	if (term->pid >= 0) {
		char *file, *buf;
		struct stat sb;
		int len;

		file = g_strdup_printf("/proc/%d/cwd", term->pid);

		if (g_stat(file, &sb) == -1) {
			g_free(file);
			return cwd;
		}

		buf = (char *)malloc(sb.st_size + 1);

		if (buf == NULL) {
			g_free(file);
			return cwd;
		}

		len = readlink(file, buf, sb.st_size + 1);

		if (len > 0 && buf[0] == '/') {
			buf[len] = '\0';
			cwd = g_strdup(buf);
		}

		g_free(buf);
		g_free(file);
	}

	return cwd;
}

static gboolean ume_resized_window(GtkWidget *widget, GdkEventConfigure *event, void *data) {
	if ((guint)event->width != ume.width || (guint)event->height != ume.height) { // NOTE: configure events are?
		// SAY("Configure event received. Current w %d h %d ConfigureEvent w %d h %d",
		// ume.width, ume.height, event->width, event->height);
		ume.resized = true;
	}
	return false;
}

static void ume_setname_entry_changed(GtkWidget *widget, void *data) {
	GtkDialog *title_dialog = (GtkDialog *)data;

	if (strcmp(gtk_entry_get_text(GTK_ENTRY(widget)), "") == 0) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, false);
	} else {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, true);
	}
}

/* Parameters are never used */
static void ume_copy(GtkWidget *widget, void *data) {
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	vte_terminal_copy_clipboard_format(VTE_TERMINAL(term->vte), VTE_FORMAT_TEXT);
}

/* Parameters are never used */
static void ume_paste(GtkWidget *widget, void *data) {
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	struct terminal *term = ume_get_page_term(ume, page);

	vte_terminal_paste_clipboard(VTE_TERMINAL(term->vte));
}

static void ume_new_tab(GtkWidget *widget, void *data) {
	ume_add_tab();
}

static bool ume_close_tab(gint page) {
	struct terminal *term = ume_get_page_term(ume, page);
	SAY("Destroying tab %d\n", page);
	/* Check if there are running processes for this tab. Use tcgetpgrp to compare to the shell PGID */
	pid_t pgid = tcgetpgrp(vte_pty_get_fd(vte_terminal_get_pty(VTE_TERMINAL(term->vte))));
	if ((pgid != -1) && (pgid != term->pid) && (!ume.config.less_questions)) {
		GtkWidget *dialog =
				gtk_message_dialog_new(GTK_WINDOW(ume.main_window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
															 _("There is a running process in this terminal.\n\nDo you really want to close it?"));

		gint response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (response == GTK_RESPONSE_YES) {
			ume_del_tab(page);
			return true;
		}
	} else {
		ume_del_tab(page);
		return true;
	}

	SAY("Failed to destroy tab %d\n", page);
	return false;
}

static void ume_close_tab_callback(GtkWidget *, void *) {
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	/* Only write configuration to disk if it's the last tab */
	if (npages == 1) {
		ume_config_done();
	}

	ume_close_tab(page);

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	if (npages == 0)
		ume_destroy();
}

static void ume_fullscreen(GtkWidget *widget, void *data) {
	if (ume.fullscreen != true) {
		ume.fullscreen = true;
		gtk_window_fullscreen(GTK_WINDOW(ume.main_window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(ume.main_window));
		ume.fullscreen = false;
	}
}

/* Callback for the tabs close buttons */
static void ume_closebutton_clicked(GtkWidget *widget, void *data) {
	gint page;
	GtkWidget *hbox = (GtkWidget *)data;
	struct terminal *term;
	pid_t pgid;
	GtkWidget *dialog;
	gint npages, response;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(ume.notebook), hbox);
	term = ume_get_page_term(ume, page);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	/* Only write configuration to disk if it's the last tab */
	if (npages == 1) {
		ume_config_done();
	}

	/* Check if there are running processes for this tab. Use tcgetpgrp to compare to the shell PGID */
	pgid = tcgetpgrp(vte_pty_get_fd(vte_terminal_get_pty(VTE_TERMINAL(term->vte))));
	if ((pgid != -1) && (pgid != term->pid) && (!ume.config.less_questions)) {
		dialog =
				gtk_message_dialog_new(GTK_WINDOW(ume.main_window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
															 _("There is a running process in this terminal.\n\nDo you really want to close it?"));

		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (response == GTK_RESPONSE_YES) {
			ume_del_tab(page);

			if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) == 0)
				ume_destroy();
		}
	} else { /* No processes, hell with tab */
		ume_del_tab(page);

		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) == 0)
			ume_destroy();
	}
}

/* Callback called when ume configuration file is modified by an external process */
// TODO create setting which on modification it reads from the file!
static void ume_conf_changed(GtkWidget *widget, void *data) {
	SAY("Config externally modified");
	ume.externally_modified = true;
	if (ume.config.reload_config_on_modify) {
		ume_reload_config_file(true);
		SAY("Reloading config, last_colorset is %d", ume.config.last_colorset - 1);
		ume_set_colorset(ume.config.last_colorset - 1);
	}
}

static void ume_disable_numbered_tabswitch(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		ume.config.disable_numbered_tabswitch = true;
		ume_set_config(cfg_group, "disable_numbered_tabswitch", true);
	} else {
		ume.config.disable_numbered_tabswitch = false;
		ume_set_config(cfg_group, "disable_numbered_tabswitch", false);
	}
}

static void ume_use_fading(GtkWidget *widget, void *data) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		ume.config.use_fading = true;
		ume_set_config(cfg_group, "use_fading", true);
	} else {
		ume.config.use_fading = false;
		ume_set_config(cfg_group, "use_fading", false);
		ume_fade_in();
		ume_set_colors();
	}
}

/******* Functions ********/
static term_colors_t ume_load_colorsets() {
	using str_ptr = std::unique_ptr<const gchar, g_free_deleter>;
	term_colors_t colors;

	for (int i = 0; i < NUM_COLORSETS; i++) {
		char group[32];
		sprintf(group, "colors%d", i + 1);

		auto cfgtmp = str_ptr(ume_load_config_or(group, COLOR_FOREGROUND_KEY, "rgb(192,192,192)"));
		gdk_rgba_parse(&colors.forecolors[i], cfgtmp.get());

		cfgtmp = str_ptr(ume_load_config_or(group, COLOR_BACKGROUND_KEY, "rgba(0,0,0,1)"));
		gdk_rgba_parse(&colors.backcolors[i], cfgtmp.get());

		cfgtmp = str_ptr(ume_load_config_or(group, COLOR_CURSOR_KEY, "rgb(255,255,255)"));
		gdk_rgba_parse(&colors.curscolors[i], cfgtmp.get());

		for (int j = 0; j < PALETTE_SIZE; ++j) {
			char key[32];
			sprintf(key, COLOR_PALETTE_KEY, j);
			auto cfgstr = str_ptr(ume_load_config_or(group, key, DEFAULT_PALETTES[i][j]));
			gdk_rgba_parse(&colors.palettes[i][j], cfgstr.get());
		}
	}
	return colors;
}

// TODO make this return a config struct!
static void ume_reload_config_file(bool readonly) {
	term_data_id = g_quark_from_static_string("ume_term");

	/* Config file initialization*/
	ume.cfg_file = g_key_file_new();
	ume.config_modified = false;

	std::string configdir = g_build_filename(g_get_user_config_dir(), "ume", NULL);
	if (!g_file_test(g_get_user_config_dir(), G_FILE_TEST_EXISTS))
		g_mkdir(g_get_user_config_dir(), 0755);
	if (!g_file_test(configdir.data(), G_FILE_TEST_EXISTS))
		g_mkdir(configdir.data(), 0755);
	if (option_config_file) {
		ume.configfile = g_build_filename(configdir.data(), option_config_file, NULL);
	} else { /* Use more standard-conforming path for config files, if available. */
		ume.configfile = g_build_filename(configdir.data(), DEFAULT_CONFIGFILE, NULL);
	}

	GError *error = NULL;
	/* Open config file */
	// TODO debug further regarding mass edits and config file getting cleared!
	if (!g_key_file_load_from_file(ume.cfg_file, ume.configfile, G_KEY_FILE_NONE, &error)) {
		/* If there's no file, ignore the error. A new one is created */
		if (error->code == G_KEY_FILE_ERROR_UNKNOWN_ENCODING || error->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
			fprintf(stderr, "Not valid config file format: %s, (%s)\n", error->message, ume.configfile);
			g_error_free(error);
			exit(EXIT_FAILURE);
		}
	}

	if (ume.cfg_signal_id == 0) { // Only load the monitor signal once!
		/* Add GFile monitor to control file external changes */
		GFile *cfgfile = g_file_new_for_path(ume.configfile);
		ume.cfg_monitor = g_file_monitor_file(cfgfile, (GFileMonitorFlags)0, nullptr, nullptr);
		ume.cfg_signal_id = g_signal_connect(G_OBJECT(ume.cfg_monitor), "changed", G_CALLBACK(ume_conf_changed), NULL);
	}

	SAY("Reloading config file");
	using cfgtmp_t = unique_g_ptr<const gchar>;
	cfgtmp_t cfgtmp = nullptr;

	/* We can safely ignore errors from g_key_file_get_value(), since if the
	 * call to g_key_file_has_key() was successful, the key IS there. From the
	 * glib docs I don't know if we can ignore errors from g_key_file_has_key,
	 * too. I think we can: the only possible error is that the config file
	 * doesn't exist, but we have just read it!
	 */

	ume.config.colors = ume_load_colorsets();
	ume.config.last_colorset = ume_load_config_or<gint>(cfg_group, "last_colorset", 1);
	ume.palette = ume.config.colors.palettes[ume.config.last_colorset - 1].data();

	ume.config.scroll_lines = ume_load_config_or(cfg_group, "scroll_lines", DEFAULT_SCROLL_LINES);
	ume.config.scroll_amount = ume_load_config_or(cfg_group, "scroll_amount", DEFAULT_SCROLL_AMOUNT);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "font", DEFAULT_FONT));
	ume.config.font = pango_font_description_from_string(cfgtmp.get());

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "show_always_first_tab", "No"));
	ume.config.first_tab = (strcmp(cfgtmp.get(), "Yes") == 0) ? true : false;

	ume.config.show_scrollbar = ume_load_config_or(cfg_group, "scrollbar", false);
	ume.config.show_closebutton = ume_load_config_or(cfg_group, "closebutton", true);
	ume.config.tabs_on_bottom = ume_load_config_or(cfg_group, "tabs_on_bottom", false);

	ume.config.less_questions = ume_load_config_or(cfg_group, "less_questions", false);
	ume.config.disable_numbered_tabswitch = ume_load_config_or(cfg_group, "disable_numbered_tabswitch", false);
	ume.config.use_fading = ume_load_config_or(cfg_group, "use_fading", false);
	ume.config.scrollable_tabs = ume_load_config_or(cfg_group, "scrollable_tabs", true);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "urgent_bell", "Yes"));
	ume.config.urgent_bell = (strcmp(cfgtmp.get(), "Yes") == 0);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "audible_bell", "Yes"));
	ume.config.audible_bell = (strcmp(cfgtmp.get(), "Yes") == 0);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "blinking_cursor", "No"));
	ume.config.blinking_cursor = (strcmp(cfgtmp.get(), "Yes") == 0);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "stop_tab_cycling_at_end_tabs", "No"));
	ume.config.stop_tab_cycling_at_end_tabs = (strcmp(cfgtmp.get(), "Yes") == 0);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "allow_bold", "Yes"));
	ume.config.allow_bold = (strcmp(cfgtmp.get(), "Yes") == 0);

	cfgtmp = cfgtmp_t(ume_load_config_or(cfg_group, "cursor_type", "block"));
	if (strcmp(cfgtmp.get(), "block") == 0) {
		ume.config.cursor_type = VTE_CURSOR_SHAPE_BLOCK;
	} else if (strcmp(cfgtmp.get(), "underline") == 0) {
		ume.config.cursor_type = VTE_CURSOR_SHAPE_UNDERLINE;
	} else if (strcmp(cfgtmp.get(), "ibeam") == 0) {
		ume.config.cursor_type = VTE_CURSOR_SHAPE_IBEAM;
	} else {
		ume.config.cursor_type = VTE_CURSOR_SHAPE_BLOCK;
	}

	ume.config.word_chars = ume_load_config_or(cfg_group, "word_chars", DEFAULT_WORD_CHARS);

	// TODO accelerator better strings or merge with keybinds!

	// ----- Begin of keybinds -----
	// TODO make a keybind struct with accelerator and key bundled together.
	// ume.config.add_tab_accelerator = ume_load_config_or(cfg_group, "add_tab_accelerator",
	// DEFAULT_ADD_TAB_ACCELERATOR);
	ume.config.add_tab_accelerator = ume_load_config_or(cfg_group, "add_tab_accelerator", DEFAULT_ADD_TAB_ACCELERATOR);
	ume.config.add_tab_key = ume_load_keybind_or(cfg_group, "add_tab_key", DEFAULT_ADD_TAB_KEY);

	ume.config.del_tab_accelerator = ume_load_config_or(cfg_group, "del_tab_accelerator", DEFAULT_DEL_TAB_ACCELERATOR);
	ume.config.del_tab_key = ume_load_keybind_or(cfg_group, "del_tab_key", DEFAULT_DEL_TAB_KEY);

	ume.config.move_tab_accelerator = ume_load_config_or(cfg_group, "move_tab_accelerator", DEFAULT_MOVE_TAB_ACCELERATOR);
	ume.config.switch_tab_accelerator =
			ume_load_config_or<gint>(cfg_group, "switch_tab_accelerator", DEFAULT_SWITCH_TAB_ACCELERATOR);
	ume.config.prev_tab_key = ume_load_keybind_or(cfg_group, "prev_tab_key", DEFAULT_PREV_TAB_KEY);
	ume.config.next_tab_key = ume_load_keybind_or(cfg_group, "next_tab_key", DEFAULT_NEXT_TAB_KEY);

	ume.config.copy_accelerator = ume_load_config_or(cfg_group, "copy_accelerator", DEFAULT_COPY_ACCELERATOR);
	ume.config.copy_key = ume_load_keybind_or(cfg_group, "copy_key", DEFAULT_COPY_KEY);
	ume.config.paste_key = ume_load_keybind_or(cfg_group, "paste_key", DEFAULT_PASTE_KEY);

	ume.config.scrollbar_accelerator =
			ume_load_config_or(cfg_group, "scrollbar_accelerator", DEFAULT_SCROLLBAR_ACCELERATOR);
	ume.config.scrollbar_key = ume_load_keybind_or(cfg_group, "scrollbar_key", DEFAULT_SCROLLBAR_KEY);
	ume.config.scroll_up_key = ume_load_keybind_or(cfg_group, "scroll_up_key", DEFAULT_SCROLL_UP_KEY);
	ume.config.scroll_down_key = ume_load_keybind_or(cfg_group, "scroll_down_key", DEFAULT_SCROLL_DOWN_KEY);
	ume.config.page_up_key = ume_load_keybind_or(cfg_group, "page_up_key", DEFAULT_PAGE_UP_KEY);
	ume.config.page_down_key = ume_load_keybind_or(cfg_group, "page_down_key", DEFAULT_PAGE_DOWN_KEY);

	ume.config.set_tab_name_accelerator =
			ume_load_config_or(cfg_group, "set_tab_name_accelerator", DEFAULT_SET_TAB_NAME_ACCELERATOR);
	ume.config.set_tab_name_key = ume_load_keybind_or(cfg_group, "set_tab_name_key", DEFAULT_SET_TAB_NAME_KEY);

	ume.config.search_accelerator = ume_load_config_or(cfg_group, "search_accelerator", DEFAULT_SEARCH_ACCELERATOR);
	ume.config.search_key = ume_load_keybind_or(cfg_group, "search_key", DEFAULT_SEARCH_KEY);

	ume.config.font_size_accelerator =
			ume_load_config_or(cfg_group, "font_size_accelerator", DEFAULT_FONT_SIZE_ACCELERATOR);
	ume.config.increase_font_size_key =
			ume_load_keybind_or(cfg_group, "increase_font_size_key", DEFAULT_INCREASE_FONT_SIZE_KEY);
	ume.config.decrease_font_size_key =
			ume_load_keybind_or(cfg_group, "decrease_font_size_key", DEFAULT_DECREASE_FONT_SIZE_KEY);

	ume.config.fullscreen_key = ume_load_keybind_or(cfg_group, "fullscreen_key", DEFAULT_FULLSCREEN_KEY);

	ume.config.set_colorset_accelerator =
			ume_load_config_or(cfg_group, "set_colorset_accelerator", DEFAULT_SELECT_COLORSET_ACCELERATOR);
	for (int i = 0; i < NUM_COLORSETS; ++i) {
		char key_name[32];
		sprintf(key_name, COLOR_SWITCH_KEY, i + 1);
		ume.config.set_colorset_keys[i] = ume_load_keybind_or(cfg_group, key_name, cs_keys[i]);
	}

	ume.config.open_url_accelerator = ume_load_config_or(cfg_group, "open_url_accelerator", DEFAULT_OPEN_URL_ACCELERATOR);

	// ------ End of keybindings -----
	ume.config.icon = ume_load_config_or(cfg_group, "icon_file", ICON_FILE);

	/* set default title pattern from config or NULL */
	ume.config.tab_default_title = g_key_file_get_string(ume.cfg_file, cfg_group, "tab_default_title", NULL);
	ume.config.reload_config_on_modify = ume_load_config_or(cfg_group, "reload_config_on_modify", false);
	ume.config.ignore_overwrite = ume_load_config_or(cfg_group, "ignore_overwrite", false);
}

static void ume_init() { // TODO break this glorious mega function .
	ume_reload_config_file(false);

	/* Use always GTK header bar*/
	g_object_set(gtk_settings_get_default(), "gtk-dialogs-use-header", true, NULL);
	ume.provider = gtk_css_provider_new();

	ume.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(ume.main_window), "ume");

	/* Default terminal size*/
	ume.columns = DEFAULT_COLUMNS;
	ume.rows = DEFAULT_ROWS;

	/* Create notebook and set style */
	ume.notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable((GtkNotebook *)ume.notebook, ume.config.scrollable_tabs);

	gchar *css = g_strdup_printf(NOTEBOOK_CSS);
	gtk_css_provider_load_from_data(ume.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context(ume.notebook);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(ume.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	/* Adding mask, for handle scroll events */
	gtk_widget_add_events(ume.notebook, GDK_SCROLL_MASK);

	/* Figure out if we have rgba capabilities. FIXME: Is this really needed? */
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(ume.main_window));
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	if (visual != NULL && gdk_screen_is_composited(screen)) {
		gtk_widget_set_visual(GTK_WIDGET(ume.main_window), visual);
	}

	/* Command line options initialization */

	/* Set argv for forked childs. Real argv vector starts at argv[1] because we're
		 using G_SPAWN_FILE_AND_ARGV_ZERO to be able to launch login shells */
	ume.argv[0] = g_strdup(g_getenv("SHELL"));
	if (option_login) {
		ume.argv[1] = g_strdup_printf("-%s", g_getenv("SHELL"));
	} else {
		ume.argv[1] = g_strdup(g_getenv("SHELL"));
	}
	ume.argv[2] = NULL;

	if (option_title) {
		gtk_window_set_title(GTK_WINDOW(ume.main_window), option_title);
	}

	if (option_columns) {
		ume.columns = option_columns;
	}

	if (option_rows) {
		ume.rows = option_rows;
	}

	/* Add datadir path to icon name and set icon */
	gchar *icon_path;
	GError *error = NULL;
	if (option_icon) {
		icon_path = g_strdup_printf("%s", option_icon);
	} else {
		icon_path = g_strdup_printf(DATADIR "/pixmaps/%s", ume.config.icon);
	}
	gtk_window_set_icon_from_file(GTK_WINDOW(ume.main_window), icon_path, &error);
	g_free(icon_path);
	icon_path = NULL;
	if (error)
		g_error_free(error);

	if (option_font) {
		ume.config.font = pango_font_description_from_string(option_font);
	}

	if (option_colorset && option_colorset > 0 && option_colorset <= NUM_COLORSETS) {
		ume.config.last_colorset = option_colorset;
	}

	/* These options are exclusive */
	if (option_fullscreen) {
		ume_fullscreen(NULL, NULL);
	} else if (option_maximize) {
		gtk_window_maximize(GTK_WINDOW(ume.main_window));
	}

	ume.label_count = 1;
	ume.fullscreen = false;
	ume.resized = false;
	ume.config.keep_fc = false;
	ume.externally_modified = false;

	error = NULL;
	ume.config.http_vteregexp = vte_regex_new_for_match(HTTP_REGEXP, strlen(HTTP_REGEXP), 0, &error);
	if (!ume.config.http_vteregexp) {
		SAY("http_regexp: %s", error->message);
		g_error_free(error);
	}
	error = NULL;
	ume.config.mail_vteregexp = vte_regex_new_for_match(MAIL_REGEXP, strlen(MAIL_REGEXP), 0, &error);
	if (!ume.config.mail_vteregexp) {
		SAY("mail_regexp: %s", error->message);
		g_error_free(error);
	}

	gtk_container_add(GTK_CONTAINER(ume.main_window), ume.notebook);

	/* Adding mask to see wheter ume window is focused or not */
	// gtk_widget_add_events(ume.main_window, GDK_FOCUS_CHANGE_MASK);
	ume.focused = true;
	ume.first_focus = true;
	ume.faded = false;

	ume_init_popup();

	g_signal_connect(G_OBJECT(ume.main_window), "delete_event", G_CALLBACK(ume_delete_event), NULL);
	g_signal_connect(G_OBJECT(ume.main_window), "destroy", G_CALLBACK(ume_destroy_window), NULL);
	g_signal_connect(G_OBJECT(ume.main_window), "key-press-event", G_CALLBACK(ume_key_press), NULL);
	g_signal_connect(G_OBJECT(ume.main_window), "configure-event", G_CALLBACK(ume_resized_window), NULL);
	g_signal_connect(G_OBJECT(ume.main_window), "focus-out-event", G_CALLBACK(ume_focus_out), NULL);
	g_signal_connect(G_OBJECT(ume.main_window), "focus-in-event", G_CALLBACK(ume_focus_in), NULL);
	g_signal_connect(G_OBJECT(ume.main_window), "show", G_CALLBACK(ume_window_show_event), NULL);
	// g_signal_connect(G_OBJECT(ume.notebook), "focus-in-event", G_CALLBACK(ume_notebook_focus_in), NULL);
	g_signal_connect(ume.notebook, "scroll-event", G_CALLBACK(ume_notebook_scroll), NULL);
}

static void ume_init_popup() {
	GtkWidget *item_new_tab, *item_set_name, *item_close_tab, *item_copy, *item_paste, *item_select_font,
			*item_select_colors, *item_set_title, *item_fullscreen, *item_toggle_scrollbar, *item_options,
			*item_show_first_tab, *item_urgent_bell, *item_audible_bell, *item_blinking_cursor, *item_allow_bold,
			*item_other_options, *item_cursor, *item_cursor_block, *item_cursor_underline, *item_cursor_ibeam,
			*item_show_close_button, *item_tabs_on_bottom, *item_less_questions, *item_disable_numbered_tabswitch,
			*item_use_fading, *item_stop_tab_cycling_at_end_tabs;
	GtkWidget *options_menu, *other_options_menu, *cursor_menu;

	ume.item_open_mail = gtk_menu_item_new_with_label(_("Open mail"));
	ume.item_open_link = gtk_menu_item_new_with_label(_("Open link"));
	ume.item_copy_link = gtk_menu_item_new_with_label(_("Copy link"));
	item_new_tab = gtk_menu_item_new_with_label(_("New tab"));
	item_set_name = gtk_menu_item_new_with_label(_("Set tab name..."));
	item_close_tab = gtk_menu_item_new_with_label(_("Close tab"));
	item_fullscreen = gtk_menu_item_new_with_label(_("Full screen"));
	item_copy = gtk_menu_item_new_with_label(_("Copy"));
	item_paste = gtk_menu_item_new_with_label(_("Paste"));
	item_select_font = gtk_menu_item_new_with_label(_("Select font..."));
	item_select_colors = gtk_menu_item_new_with_label(_("Select colors..."));
	item_set_title = gtk_menu_item_new_with_label(_("Set window title..."));

	item_options = gtk_menu_item_new_with_label(_("Options"));

	item_other_options = gtk_menu_item_new_with_label(_("More"));
	item_show_first_tab = gtk_check_menu_item_new_with_label(_("Always show tab bar"));
	item_tabs_on_bottom = gtk_check_menu_item_new_with_label(_("Tabs at bottom"));
	item_show_close_button = gtk_check_menu_item_new_with_label(_("Show close button on tabs"));
	item_toggle_scrollbar = gtk_check_menu_item_new_with_label(_("Show scrollbar"));
	item_less_questions = gtk_check_menu_item_new_with_label(_("Don't show exit dialog"));
	item_urgent_bell = gtk_check_menu_item_new_with_label(_("Set urgent bell"));
	item_audible_bell = gtk_check_menu_item_new_with_label(_("Set audible bell"));
	item_blinking_cursor = gtk_check_menu_item_new_with_label(_("Set blinking cursor"));
	item_allow_bold = gtk_check_menu_item_new_with_label(_("Enable bold font"));
	item_stop_tab_cycling_at_end_tabs = gtk_check_menu_item_new_with_label(_("Stop tab cycling at end tabs"));
	item_disable_numbered_tabswitch = gtk_check_menu_item_new_with_label(_("Disable numbered tabswitch"));
	item_use_fading = gtk_check_menu_item_new_with_label(_("Enable focus fade"));
	item_cursor = gtk_menu_item_new_with_label(_("Set cursor type"));
	item_cursor_block = gtk_radio_menu_item_new_with_label(NULL, _("Block"));
	item_cursor_underline =
			gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_cursor_block), _("Underline"));
	item_cursor_ibeam =
			gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_cursor_block), _("IBeam"));

	/* Show defaults in menu items */
	if (ume.config.first_tab) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), false);
	}

	if (ume.config.show_closebutton) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_close_button), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_close_button), false);
	}

	if (ume.config.tabs_on_bottom) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_tabs_on_bottom), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_tabs_on_bottom), false);
	}

	if (ume.config.less_questions) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_less_questions), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_less_questions), false);
	}

	if (ume.config.show_scrollbar) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), false);
	}

	if (ume.config.disable_numbered_tabswitch) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_disable_numbered_tabswitch), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_disable_numbered_tabswitch), false);
	}

	if (ume.config.use_fading) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_use_fading), true);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_use_fading), false);
	}

	if (ume.config.urgent_bell) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_urgent_bell), true);
	}

	if (ume.config.audible_bell) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_audible_bell), true);
	}

	if (ume.config.blinking_cursor) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_blinking_cursor), true);
	}

	if (ume.config.allow_bold) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_allow_bold), true);
	}

	if (ume.config.stop_tab_cycling_at_end_tabs) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_stop_tab_cycling_at_end_tabs), true);
	}

	switch (ume.config.cursor_type) {
		case VTE_CURSOR_SHAPE_BLOCK:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_block), true);
			break;
		case VTE_CURSOR_SHAPE_UNDERLINE:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_underline), true);
			break;
		case VTE_CURSOR_SHAPE_IBEAM:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_ibeam), true);
	}

	ume.open_link_separator = gtk_separator_menu_item_new();

	ume.menu = gtk_menu_new();
	// ume.labels_menu=gtk_menu_new();

	/* Add items to popup menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), ume.item_open_mail);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), ume.item_open_link);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), ume.item_copy_link);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), ume.open_link_separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_new_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_set_name);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_close_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_fullscreen);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_copy);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_paste);
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(ume.menu), item_options);

	options_menu = gtk_menu_new();
	other_options_menu = gtk_menu_new();
	cursor_menu = gtk_menu_new();

	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_set_title);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_colors);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_font);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_other_options);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_show_first_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_tabs_on_bottom);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_show_close_button);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_toggle_scrollbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_less_questions);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_urgent_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_audible_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_disable_numbered_tabswitch);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_use_fading);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_blinking_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_allow_bold);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_stop_tab_cycling_at_end_tabs);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_block);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_underline);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_ibeam);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_options), options_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_other_options), other_options_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_cursor), cursor_menu);

	/* ... and finally assign callbacks to menuitems */
	g_signal_connect(G_OBJECT(item_new_tab), "activate", G_CALLBACK(ume_new_tab), NULL);
	g_signal_connect(G_OBJECT(item_set_name), "activate", G_CALLBACK(ume_set_name_dialog), NULL);
	g_signal_connect(G_OBJECT(item_close_tab), "activate", G_CALLBACK(ume_close_tab_callback), NULL);
	g_signal_connect(G_OBJECT(item_select_font), "activate", G_CALLBACK(ume_font_dialog), NULL);
	g_signal_connect(G_OBJECT(item_copy), "activate", G_CALLBACK(ume_copy), NULL);
	g_signal_connect(G_OBJECT(item_paste), "activate", G_CALLBACK(ume_paste), NULL);
	g_signal_connect(G_OBJECT(item_select_colors), "activate", G_CALLBACK(ume_color_dialog), NULL);

	g_signal_connect(G_OBJECT(item_show_first_tab), "activate", G_CALLBACK(ume_show_first_tab), NULL);
	g_signal_connect(G_OBJECT(item_tabs_on_bottom), "activate", G_CALLBACK(ume_tabs_on_bottom), NULL);
	g_signal_connect(G_OBJECT(item_less_questions), "activate", G_CALLBACK(ume_less_questions), NULL);
	g_signal_connect(G_OBJECT(item_show_close_button), "activate", G_CALLBACK(ume_show_close_button), NULL);
	g_signal_connect(G_OBJECT(item_toggle_scrollbar), "activate", G_CALLBACK(ume_show_scrollbar), NULL);
	g_signal_connect(G_OBJECT(item_urgent_bell), "activate", G_CALLBACK(ume_urgent_bell), NULL);
	g_signal_connect(G_OBJECT(item_audible_bell), "activate", G_CALLBACK(ume_audible_bell), NULL);
	g_signal_connect(G_OBJECT(item_blinking_cursor), "activate", G_CALLBACK(ume_blinking_cursor), NULL);
	g_signal_connect(G_OBJECT(item_allow_bold), "activate", G_CALLBACK(ume_allow_bold), NULL);
	g_signal_connect(G_OBJECT(item_stop_tab_cycling_at_end_tabs), "activate",
									 G_CALLBACK(ume_stop_tab_cycling_at_end_tabs), NULL);
	g_signal_connect(G_OBJECT(item_disable_numbered_tabswitch), "activate", G_CALLBACK(ume_disable_numbered_tabswitch),
									 NULL);
	g_signal_connect(G_OBJECT(item_use_fading), "activate", G_CALLBACK(ume_use_fading), NULL);
	g_signal_connect(G_OBJECT(item_set_title), "activate", G_CALLBACK(ume_set_title_dialog), NULL);

	g_signal_connect(G_OBJECT(item_cursor_block), "activate", G_CALLBACK(ume_set_cursor), (void *)"block");
	g_signal_connect(G_OBJECT(item_cursor_underline), "activate", G_CALLBACK(ume_set_cursor), (void *)"underline");
	g_signal_connect(G_OBJECT(item_cursor_ibeam), "activate", G_CALLBACK(ume_set_cursor), (void *)"ibeam");

	g_signal_connect(G_OBJECT(ume.item_open_mail), "activate", G_CALLBACK(ume_open_mail), NULL);
	g_signal_connect(G_OBJECT(ume.item_open_link), "activate", G_CALLBACK(ume_open_url), NULL);
	g_signal_connect(G_OBJECT(ume.item_copy_link), "activate", G_CALLBACK(ume_copy_url), NULL);
	//	g_signal_connect(G_OBJECT(item_fullscreen), "activate", G_CALLBACK(ume_fullscreen), NULL);

	gtk_widget_show_all(ume.menu);
}

static void ume_destroy() {
	/* Delete all existing tabs */
	while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) > 0) {
		ume_del_tab(-1);
	}

	g_key_file_free(ume.cfg_file);

	pango_font_description_free(ume.config.font);

	free(ume.configfile);

	gtk_main_quit();
}

static void ume_set_size(void) {
	struct terminal *term;
	gint pad_x, pad_y;
	gint char_width, char_height;
	guint npages;
	gint min_width, natural_width;
	gint page;

	term = ume_get_page_term(ume, 0);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	/* Mayhaps an user resize happened. Check if row and columns have changed */
	if (ume.resized) {
		ume.columns = vte_terminal_get_column_count(VTE_TERMINAL(term->vte));
		ume.rows = vte_terminal_get_row_count(VTE_TERMINAL(term->vte));
		SAY("New columns %ld and rows %ld", ume.columns, ume.rows);
		ume.resized = false;
	}

	gtk_style_context_get_padding(gtk_widget_get_style_context(term->vte), gtk_widget_get_state_flags(term->vte),
																&term->padding);
	pad_x = term->padding.left + term->padding.right;
	pad_y = term->padding.top + term->padding.bottom;
	// SAY("padding x %d y %d", pad_x, pad_y);
	char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
	char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));

	ume.width = pad_x + (char_width * ume.columns);
	ume.height = pad_y + (char_height * ume.rows);

	if (npages >= 2 || ume.config.first_tab) {

		/* TODO: Yeah i know, this is utter shit. Remove this ugly hack and set geometry hints*/
		if (!ume.config.show_scrollbar)
			// ume.height += min_height - 10;
			ume.height += 10;
		else
			// ume.height += min_height - 47;
			ume.height += 47;

		ume.width += 8;
		ume.width += /* (hb*2)+*/ (pad_x * 2);
	}

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	term = ume_get_page_term(ume, page);

	gtk_widget_get_preferred_width(term->scrollbar, &min_width, &natural_width);
	// SAY("SCROLLBAR min width %d natural width %d", min_width, natural_width);
	if (ume.config.show_scrollbar) {
		ume.width += min_width;
	}

	/* GTK does not ignore resize for maximized windows on some systems,
		 so we do need check if it's maximized or not */
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(ume.main_window));
	if (gdk_window != NULL) {
		if (gdk_window_get_state(gdk_window) & GDK_WINDOW_STATE_MAXIMIZED) {
			SAY("window is maximized, will not resize");
			return;
		}
	}

	gtk_window_resize(GTK_WINDOW(ume.main_window), ume.width, ume.height);
	SAY("Resized to %d %d", ume.width, ume.height);
}

static void ume_set_font() {
	gint n_pages;
	struct terminal *term;
	int i;

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	/* Set the font for all tabs */
	for (i = (n_pages - 1); i >= 0; i--) {
		term = ume_get_page_term(ume, i);
		vte_terminal_set_font(VTE_TERMINAL(term->vte), ume.config.font);
	}
}

static void ume_move_tab(gint direction) {
	gint page, n_pages;
	GtkWidget *child;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ume.notebook), page);

	if (direction == FORWARD) {
		if (page != n_pages - 1)
			gtk_notebook_reorder_child(GTK_NOTEBOOK(ume.notebook), child, page + 1);
	} else {
		if (page != 0)
			gtk_notebook_reorder_child(GTK_NOTEBOOK(ume.notebook), child, page - 1);
	}
}

/* Find the notebook page for the vte terminal passed as a parameter */
static gint ume_find_tab(VteTerminal *vte_term) {
	gint matched_page, page, n_pages;
	struct terminal *term;

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	matched_page = -1;
	page = 0;

	do {
		term = ume_get_page_term(ume, page);
		if ((VteTerminal *)term->vte == vte_term) {
			matched_page = page;
		}
		page++;
	} while (page < n_pages);

	return (matched_page);
}

static void ume_set_tab_label_text(const gchar *title, gint page) {
	struct terminal *term;
	gchar *chopped_title;

	term = ume_get_page_term(ume, page);

	if ((title != NULL) && (g_strcmp0(title, "") != 0)) {
		/* Chop to max size. TODO: Should it be configurable by the user? */
		chopped_title = g_strndup(title, TAB_MAX_SIZE);
		/* Honor the minimum tab label size */
		while (strlen(chopped_title) < TAB_MIN_SIZE) {
			char *old_ptr = chopped_title;
			chopped_title = g_strconcat(chopped_title, " ", NULL);
			free(old_ptr);
		}
		gtk_label_set_text(GTK_LABEL(term->label), chopped_title);
		free(chopped_title);
	} else { /* Use the default values */
		gtk_label_set_text(GTK_LABEL(term->label), term->label_text);
	}
}

/* Callback for vte_terminal_spawn_async */
void ume_spawn_callback(VteTerminal *vte, GPid pid, GError *error, gpointer user_data) {
	struct terminal *term = (struct terminal *)user_data;
	// term = ume_get_page_term(ume, page);
	if (pid == -1) { /* Fork has failed */
		SAY("Error: %s", error->message);
	} else {
		term->pid = pid;
	}
}

// TODO break this up
static void ume_add_tab() {
	GtkWidget *tab_label_hbox;
	GtkWidget *close_button;
	int index;
	int npages;
	gchar *cwd = NULL;
	gchar *label_text = _("Terminal %d");

	struct terminal *term = g_new0(struct terminal, 1);

	/* Create label for tabs */
	term->label_set_byuser = false;

	/* appling tab title pattern from config (https://answers.launchpad.net/ume/+question/267951) */
	if (ume.config.tab_default_title != NULL) {
		label_text = ume.config.tab_default_title;
		term->label_set_byuser = true;
	}

	term->label_text = g_strdup_printf(label_text, ume.label_count++);
	term->label = gtk_label_new(term->label_text);

	tab_label_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_hexpand(tab_label_hbox, true);
	gtk_label_set_ellipsize(GTK_LABEL(term->label), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start(GTK_BOX(tab_label_hbox), term->label, true, false, 0);

	/* If the tab close button is enabled, create and add it to the tab */
	if (ume.config.show_closebutton) {
		close_button = gtk_button_new();
		/* Adding scroll-event to button, to propagate it to notebook (fix for scroll event when pointer is above the
		 * button) */
		gtk_widget_add_events(close_button, GDK_SCROLL_MASK);

		gtk_widget_set_name(close_button, "closebutton");
		gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);

		GtkWidget *image = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);
		gtk_container_add(GTK_CONTAINER(close_button), image);
		gtk_box_pack_start(GTK_BOX(tab_label_hbox), close_button, false, false, 0);
	}

	if (ume.config.tabs_on_bottom) {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(ume.notebook), GTK_POS_BOTTOM);
	}

	/* Set tab title style */
	gchar *css = g_strdup_printf(TAB_TITLE_CSS);
	gtk_css_provider_load_from_data(ume.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context(tab_label_hbox);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(ume.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	gtk_widget_show_all(tab_label_hbox);

	/* Create new vte terminal, scrollbar, and pack it */
	term->vte = vte_terminal_new();
	term->scrollbar =
			gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(term->vte)));
	term->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(term->hbox), term->vte, true, true, 0);
	gtk_box_pack_start(GTK_BOX(term->hbox), term->scrollbar, false, false, 0);

	term->colorset = ume.config.last_colorset - 1;

	/* Select the directory to use for the new tab */
	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
	if (index >= 0) {
		struct terminal *prev_term;
		prev_term = ume_get_page_term(ume, index);
		cwd = ume_get_term_cwd(prev_term);

		term->colorset = prev_term->colorset;
	}
	if (!cwd)
		cwd = g_get_current_dir();

	/* Keep values when adding tabs */
	ume.config.keep_fc = true;

	if ((index = gtk_notebook_append_page(GTK_NOTEBOOK(ume.notebook), term->hbox, tab_label_hbox)) == -1) {
		ume_error("Cannot create a new tab");
		exit(1);
	}

	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(ume.notebook), term->hbox, true);
	// TODO: Set group id to support detached tabs
	// gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(ume.notebook), term->hbox, true);

	ume_set_page_term(ume, index, term);

	/* vte signals */
	g_signal_connect(G_OBJECT(term->vte), "bell", G_CALLBACK(ume_beep), NULL);
	g_signal_connect(G_OBJECT(term->vte), "increase-font-size", G_CALLBACK(ume_increase_font), NULL);
	g_signal_connect(G_OBJECT(term->vte), "decrease-font-size", G_CALLBACK(ume_decrease_font), NULL);
	g_signal_connect(G_OBJECT(term->vte), "child-exited", G_CALLBACK(ume_child_exited), NULL);
	g_signal_connect(G_OBJECT(term->vte), "eof", G_CALLBACK(ume_eof), NULL);
	g_signal_connect(G_OBJECT(term->vte), "window-title-changed", G_CALLBACK(ume_title_changed), NULL);
	g_signal_connect_swapped(G_OBJECT(term->vte), "button-press-event", G_CALLBACK(ume_button_press), ume.menu);

	/* Notebook signals */
	g_signal_connect(G_OBJECT(ume.notebook), "page-removed", G_CALLBACK(ume_page_removed), NULL);
	if (ume.config.show_closebutton) {
		g_signal_connect(G_OBJECT(close_button), "clicked", G_CALLBACK(ume_closebutton_clicked), term->hbox);
	}

	/* Since vte-2.91 env is properly overwritten */
	const char *mode = "TERM=xterm-256color";
	char *command_env[2] = {g_strdup(mode), 0};
	/* First tab */
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));
	if (npages == 1) {
		if (ume.config.first_tab) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), true);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), false);
		}

		gtk_notebook_set_show_border(GTK_NOTEBOOK(ume.notebook), false);
		ume_set_font();
		ume_set_colors();
		/* Set size before showing the widgets but after setting the font */
		ume_set_size();

		gtk_widget_show_all(ume.notebook);
		if (!ume.config.show_scrollbar) {
			gtk_widget_hide(term->scrollbar);
		}

		gtk_widget_show(ume.main_window);

#ifdef GDK_WINDOWING_X11
		/* Set WINDOWID env variable */
		GdkDisplay *display = gdk_display_get_default();

		if (GDK_IS_X11_DISPLAY(display)) {
			GdkWindow *gwin = gtk_widget_get_window(ume.main_window);
			if (gwin != NULL) {
				guint winid = gdk_x11_window_get_xid(gwin);
				gchar *winidstr = g_strdup_printf("%d", winid);
				g_setenv("WINDOWID", winidstr, false);
				g_free(winidstr);
			}
		}
#endif

		int command_argc = 0;
		char **command_argv;
		if (option_execute || option_xterm_execute) {
			GError *gerror = NULL;
			gchar *path;

			if (option_execute) {
				/* -x option */
				if (!g_shell_parse_argv(option_execute, &command_argc, &command_argv, &gerror)) {
					switch (gerror->code) {
						case G_SHELL_ERROR_EMPTY_STRING:
							ume_error("Empty exec string");
							exit(1);
							break;
						case G_SHELL_ERROR_BAD_QUOTING:
							ume_error("Cannot parse command line arguments: mangled quoting");
							exit(1);
							break;
						case G_SHELL_ERROR_FAILED:
							ume_error("Error in exec option command line arguments");
							exit(1);
					}
					g_error_free(gerror);
				}
			} else {
				/* -e option - last in the command line, takes all extra arguments */
				if (option_xterm_args) {
					gchar *command_joined;
					command_joined = g_strjoinv(" ", option_xterm_args);
					if (!g_shell_parse_argv(command_joined, &command_argc, &command_argv, &gerror)) {
						switch (gerror->code) {
							case G_SHELL_ERROR_EMPTY_STRING:
								ume_error("Empty exec string");
								exit(1);
								break;
							case G_SHELL_ERROR_BAD_QUOTING:
								ume_error("Cannot parse command line arguments: mangled quoting");
								exit(1);
							case G_SHELL_ERROR_FAILED:
								ume_error("Error in exec option command line arguments");
								exit(1);
						}
					}
					if (gerror != NULL)
						g_error_free(gerror);
					g_free(command_joined);
				}
			}

			/* Check if the command is valid */
			if (command_argc > 0) {
				path = g_find_program_in_path(command_argv[0]);
				if (path) {
					vte_terminal_spawn_async(VTE_TERMINAL(term->vte), VTE_PTY_NO_HELPER, NULL, command_argv, command_env,
																	 G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, ume_spawn_callback, term);
				} else {
					ume_error("%s command not found", command_argv[0]);
					command_argc = 0;
					// exit(1);
				}
				free(path);
				g_strfreev(command_argv);
				g_strfreev(option_xterm_args);
				g_free(command_env[0]);
			}
		} // else { /* No execute option */

		/* Only fork if there is no execute option or if it has failed */
		if ((!option_execute && !option_xterm_args) || (command_argc == 0)) {
			if (option_hold == true) {
				ume_error("Hold option given without any command");
				option_hold = false;
			}
			vte_terminal_spawn_async(VTE_TERMINAL(term->vte), VTE_PTY_NO_HELPER, cwd, ume.argv, command_env,
															 (GSpawnFlags)(G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO), NULL, NULL, NULL, -1,
															 NULL, ume_spawn_callback, term);
		}
		/* Not the first tab */
	} else {
		ume_set_font();
		ume_set_colors();
		gtk_widget_show_all(term->hbox);
		if (!ume.config.show_scrollbar) {
			gtk_widget_hide(term->scrollbar);
		}

		if (npages == 2) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), true);
			ume_set_size();
		}
		/* Call set_current page after showing the widget: gtk ignores this
		 * function in the window is not visible *sigh*. Gtk documentation
		 * says this is for "historical" reasons. Me arse */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(ume.notebook), index);
		vte_terminal_spawn_async(VTE_TERMINAL(term->vte), VTE_PTY_NO_HELPER, cwd, ume.argv, command_env,
														 (GSpawnFlags)(G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO), NULL, NULL, NULL, -1,
														 NULL, ume_spawn_callback, term);
	}

	free(cwd);

	/* Init vte terminal */
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(term->vte), ume.config.scroll_lines);
	vte_terminal_match_add_regex(VTE_TERMINAL(term->vte), ume.config.http_vteregexp, PCRE2_CASELESS);
	vte_terminal_match_add_regex(VTE_TERMINAL(term->vte), ume.config.mail_vteregexp, PCRE2_CASELESS);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(term->vte), true);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(term->vte), VTE_ERASE_ASCII_DELETE);
	vte_terminal_set_word_char_exceptions(VTE_TERMINAL(term->vte), ume.config.word_chars);
	vte_terminal_set_audible_bell(VTE_TERMINAL(term->vte), ume.config.audible_bell ? true : false);
	vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term->vte),
																		 ume.config.blinking_cursor ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
	vte_terminal_set_allow_bold(VTE_TERMINAL(term->vte), ume.config.allow_bold ? true : false);
	vte_terminal_set_cursor_shape(VTE_TERMINAL(term->vte), ume.config.cursor_type);

	// ume_set_colors();

	/* FIXME: Possible race here. Find some way to force to process all configure
	 * events before setting keep_fc again to false */
	ume.config.keep_fc = false;
}

/* Delete the notebook tab passed as a parameter */
static void ume_del_tab(gint page) {
	struct terminal *term;
	gint npages;

	term = ume_get_page_term(ume, page);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook));

	/* When there's only one tab use the shell title, if provided */
	if (npages == 2) {
		const char *title;
		term = ume_get_page_term(ume, 0);
		title = vte_terminal_get_window_title(VTE_TERMINAL(term->vte));
		if (title != NULL)
			gtk_window_set_title(GTK_WINDOW(ume.main_window), title);
	}

	term = ume_get_page_term(ume, page);

	/* Do the first tab checks BEFORE deleting the tab, to ensure correct
	 * sizes are calculated when the tab is deleted */
	if (npages == 2) {
		if (ume.config.first_tab) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), true);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ume.notebook), false);
		}
		ume.config.keep_fc = true;
	}

	gtk_widget_hide(term->hbox);
	gtk_notebook_remove_page(GTK_NOTEBOOK(ume.notebook), page);

	/* Find the next page, if it exists, and grab focus */
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ume.notebook)) > 0) {
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ume.notebook));
		term = ume_get_page_term(ume, page);
		gtk_widget_grab_focus(term->vte);
	}
}

static void ume_set_keybind(const gchar *key, guint value) {
	char *valname;
	valname = gdk_keyval_name(value);
	g_key_file_set_string(ume.cfg_file, cfg_group, key, valname);
	ume.config_modified = true;
	// FIXME: free() valname?
}

static guint ume_get_keybind(const gchar *key) {
	guint retval = GDK_KEY_VoidSymbol;
	std::unique_ptr<gchar, g_free_deleter> value(g_key_file_get_string(ume.cfg_file, cfg_group, key, nullptr));

	if (value != nullptr) {
		if (strcmp(value.get(), "") == 0)
			return GDK_KEY_VoidSymbol;
		retval = gdk_keyval_from_name(value.get());
	}

	/* For backwards compatibility with integer values */
	/* If gdk_keyval_from_name fail, it seems to be integer value*/
	if ((retval == GDK_KEY_VoidSymbol) || (retval == 0))
		retval = g_key_file_get_integer(ume.cfg_file, cfg_group, key, nullptr);

	/* Always use uppercase value as keyval */
	return gdk_keyval_to_upper(retval);
}

static guint ume_load_keybind_or(const gchar *group, const gchar *key, guint default_value) {
	if (!g_key_file_has_key(ume.cfg_file, group, key, nullptr))
		ume_set_keybind(key, default_value);
	return ume_get_keybind(key);
}

static void ume_error(const char *format, ...) {
	GtkWidget *dialog;
	va_list args;

	va_start(args, format);
	char *buff = (char *)malloc(sizeof(char) * ERROR_BUFFER_LENGTH);
	vsnprintf(buff, sizeof(char) * ERROR_BUFFER_LENGTH, format, args);
	va_end(args);

	dialog = gtk_message_dialog_new(GTK_WINDOW(ume.main_window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
																	GTK_BUTTONS_CLOSE, "%s", buff);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error message"));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	free(buff);
}

/* This function is used to fix bug #1393939 */
static void ume_sanitize_working_directory() {
	const gchar *home_directory = g_getenv("HOME");
	if (home_directory == NULL) {
		home_directory = g_get_home_dir();
	}

	if (home_directory != NULL) {
		if (chdir(home_directory)) {
			fprintf(stderr, _("Cannot change working directory\n"));
			exit(1);
		}
	}
}

int main(int argc, char **argv) {
	/* Localization */
	setlocale(LC_ALL, "");
	gchar *localedir = g_strdup_printf("%s/locale", DATADIR);
	textdomain(GETTEXT_PACKAGE);
	bindtextdomain(GETTEXT_PACKAGE, localedir);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	g_free(localedir);

	/* Rewrites argv to include a -- after the -e argument this is required to make
	 * sure GOption doesn't grab any arguments meant for the command being called */

	/* Initialize nargv */
	char **nargv = (char **)calloc((argc + 1), sizeof(char *));
	int n = 0;
	int nargc = argc;
	bool have_e = false;

	for (int i = 0; i < argc; ++i) {
		if (!have_e && g_strcmp0(argv[i], "-e") == 0) {
			nargv[n] = g_strdup("-e");
			++n;
			nargv[n] = g_strdup("--");
			++nargc;
			have_e = true;
		} else {
			nargv[n] = g_strdup(argv[i]);
		}
		++n;
	}

	/* Options parsing */
	GError *error = NULL;
	GOptionContext *context;
	GOptionGroup *option_group;

	context = g_option_context_new(_("- vte-based terminal emulator"));
	option_group = gtk_get_option_group(true);
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
	g_option_context_add_group(context, option_group);
	if (!g_option_context_parse(context, &nargc, &nargv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		g_error_free(error);
		exit(1);
	}

	g_option_context_free(context);

	if (option_workdir && chdir(option_workdir)) {
		fprintf(stderr, _("Cannot change working directory\n"));
		exit(1);
	}

	if (option_version) {
		fprintf(stderr, _("ume version is %s\n"), VERSION);
		exit(1);
	}

	if (option_ntabs <= 0) {
		option_ntabs = 1;
	}

	/* Init stuff */
	gtk_init(&nargc, &nargv);
	g_strfreev(nargv);
	ume_init();

	/* Add initial tabs (1 by default) */
	for (int i = 0; i < option_ntabs; i++)
		ume_add_tab();

	ume_sanitize_working_directory();

	gtk_main();

	return 0;
}
