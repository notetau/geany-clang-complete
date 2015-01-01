/*
 * preferences.cpp - a Geany plugin to provide code completion using clang
 *
 * Copyright (C) 2014 Noto, Yuta <nonotetau(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "preferences.hpp"

#include "cc_plugin.hpp"

#include <vector>
#include <string>
#include <sstream>

// global preference access interface
ClangCompletePluginPref* global_ClangCompletePluginPref_instance = NULL;

ClangCompletePluginPref* get_ClangCompletePluginPref() {
	if( global_ClangCompletePluginPref_instance == NULL ) {
		global_ClangCompletePluginPref_instance  = new ClangCompletePluginPref();
	}
	return global_ClangCompletePluginPref_instance;
}

void cleanup_ClangCompletePluginPref() {
	if( global_ClangCompletePluginPref_instance ) {
		delete global_ClangCompletePluginPref_instance;
		global_ClangCompletePluginPref_instance = NULL;
	}
}


void ClangCompletePluginPref::load_default_preferences() {
	compiler_options.clear();
	//secondary_compiler_options.clear();
	start_completion_with_dot = true;
	start_completion_with_arrow = true;
	start_completion_with_scope_res = true;
	row_text_max = 120;
	suggestion_window_height_max = 300;
}

static std::string get_config_file() {
	std::string config_file = geany_data->app->configdir;
	config_file += G_DIR_SEPARATOR_S;
	config_file += "plugins";
	config_file += G_DIR_SEPARATOR_S;
	config_file += "clang_complete";
	config_file += G_DIR_SEPARATOR_S;
	config_file += "config.conf";
	return config_file;
}

void ClangCompletePluginPref::load_preferences() {
	std::string config_file = get_config_file();

	/* Initialising options from config file */
	GKeyFile* keyfile = g_key_file_new();
	if( g_key_file_load_from_file(keyfile, config_file.c_str(), G_KEY_FILE_NONE, NULL) ) {

		ClangCompletePluginPref* pref = get_ClangCompletePluginPref();

		pref->start_completion_with_dot = g_key_file_get_boolean(keyfile, "clangcomplete",
			"start_completion_with_dot", NULL);
		pref->start_completion_with_arrow = g_key_file_get_boolean(keyfile, "clangcomplete",
			"start_completion_with_arrow", NULL);
		pref->start_completion_with_scope_res = g_key_file_get_boolean(keyfile, "clangcomplete",
			"start_completion_with_scope_resolution", NULL);
		pref->row_text_max = g_key_file_get_integer(keyfile, "clangcomplete",
			"maximum_char_in_row", NULL);
		pref->suggestion_window_height_max = g_key_file_get_integer(keyfile, "clangcomplete",
			"maximum_sug_window_height", NULL);

		gsize option_num = 0;
		gchar** strs = g_key_file_get_string_list(keyfile,
			"clangcomplete", "compiler_options", &option_num, NULL);
		pref->compiler_options.clear();
		for(gsize i=0; i<option_num; i++) {
			pref->compiler_options.push_back(strs[i]);
		}
		g_strfreev(strs);
	}
	else {
		load_default_preferences();
	}
	g_key_file_free(keyfile);

	update_clang_complete_plugin_state();
}

#include <geanyplugin.h>

static bool pref_dialog_changed = false;

static void on_pref_dialog_value_changed(GtkSpinButton *spinbutton, gpointer user_data) {
	pref_dialog_changed = true;
}

static struct {
	GtkWidget* start_with_dot;
	GtkWidget* start_with_arrow;
	GtkWidget* start_with_scope_res;
	GtkWidget* row_text_max_spinbtn;
	GtkWidget* swin_height_max_spinbtn;
	GtkTextBuffer* options_text_buf;
} pref_widgets;

static void save_preferences_state() {
	std::string config_file = get_config_file();
	GKeyFile *keyfile = g_key_file_new();

	ClangCompletePluginPref* pref = get_ClangCompletePluginPref();
	g_key_file_set_boolean(keyfile, "clangcomplete",
		"start_completion_with_dot", pref->start_completion_with_dot);
	g_key_file_set_boolean(keyfile, "clangcomplete",
		"start_completion_with_arrow", pref->start_completion_with_arrow);
	g_key_file_set_boolean(keyfile, "clangcomplete",
		"start_completion_with_scope_resolution", pref->start_completion_with_scope_res);
	g_key_file_set_integer(keyfile, "clangcomplete",
		"maximum_char_in_row", pref->row_text_max);
	g_key_file_set_integer(keyfile, "clangcomplete",
		"maximum_sug_window_height", pref->suggestion_window_height_max);

	int option_num = pref->compiler_options.size();
	gchar** strs = (gchar**)g_malloc0(sizeof(gchar*) * (option_num + 1));
	for(size_t i=0; i<pref->compiler_options.size(); i++) {
		strs[i] = g_strdup(pref->compiler_options[i].c_str());
	}
	g_key_file_set_string_list(keyfile, "clangcomplete", "compiler_options", strs, option_num);
	g_strfreev(strs);

	//TODO use smart_ptr if errors occur, happen memory leak
	gchar *dirname = g_path_get_dirname(config_file.c_str());

	gsize data_length;
	gchar* data = g_key_file_to_data(keyfile, &data_length, NULL);

	int err = utils_mkdir(dirname, TRUE);
	if( err != 0 ) {
		g_critical(_("Failed to create configuration directory \"%s\": %s"),
			dirname, g_strerror(err));
		return;
	}

	GError *error = NULL;
	if( !g_file_set_contents (config_file.c_str(), data, data_length, &error) ) {
		g_critical(_("Failed to save configuration file: %s"), error->message);
		g_error_free(error);
		return;
	}
	g_free(data);
	g_free(dirname);
	g_key_file_free(keyfile);
}

static void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
		g_debug("respon ok");
		if( pref_dialog_changed ) {
			g_debug("modify pref");
			pref_dialog_changed = false;

			ClangCompletePluginPref* pref = get_ClangCompletePluginPref();

			pref->compiler_options.clear();
			GtkTextIter start, end;
			gtk_text_buffer_get_start_iter(pref_widgets.options_text_buf, &start);
			gtk_text_buffer_get_end_iter(pref_widgets.options_text_buf, &end);
			gchar* text = gtk_text_buffer_get_text(
				pref_widgets.options_text_buf, &start, &end, FALSE);
			std::istringstream iss(text);
			std::string tmp;
			while(std::getline(iss, tmp, '\n')) {
				if(tmp.length() != 0) {
					if( tmp[tmp.length()-1] == '\r' ) { tmp = tmp.substr(0,tmp.length()-1); }
					pref->compiler_options.push_back(tmp);
				}
			}
			g_free(text);

			pref->start_completion_with_dot =
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.start_with_dot));
			pref->start_completion_with_arrow =
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.start_with_arrow));
			pref->start_completion_with_scope_res =
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.start_with_scope_res));

			pref->row_text_max = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(pref_widgets.row_text_max_spinbtn));

			pref->suggestion_window_height_max = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(pref_widgets.swin_height_max_spinbtn));

			save_preferences_state();
			update_clang_complete_plugin_state();
		}
	}
}

extern "C" {
	GtkWidget* plugin_configure(GtkDialog* dialog)
	{
		g_debug("code complete: plugin_configure");
		pref_dialog_changed = false;
		ClangCompletePluginPref* pref = get_ClangCompletePluginPref();

		GtkWidget* vbox = gtk_vbox_new(FALSE,8);

		GtkWidget* start_with_box = gtk_hbox_new(FALSE,4);
		{
			GtkWidget* start_with_label = gtk_label_new(_("start completion with"));

			pref_widgets.start_with_dot = gtk_check_button_new_with_label(_("."));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.start_with_dot),
				pref->start_completion_with_dot);
			g_signal_connect(pref_widgets.start_with_dot, "toggled",
				G_CALLBACK(on_pref_dialog_value_changed), NULL);

			pref_widgets.start_with_arrow = gtk_check_button_new_with_label(_("->"));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.start_with_arrow),
				pref->start_completion_with_arrow);
			g_signal_connect(pref_widgets.start_with_arrow, "toggled",
				G_CALLBACK(on_pref_dialog_value_changed), NULL);

			pref_widgets.start_with_scope_res = gtk_check_button_new_with_label(_("::"));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.start_with_scope_res),
				pref->start_completion_with_scope_res);
			g_signal_connect(pref_widgets.start_with_scope_res, "toggled",
				G_CALLBACK(on_pref_dialog_value_changed), NULL);

			gtk_box_pack_start(GTK_BOX(start_with_box),
				start_with_label, FALSE, FALSE, 5);
			gtk_box_pack_start(GTK_BOX(start_with_box),
				pref_widgets.start_with_dot, FALSE, FALSE, 8);
			gtk_box_pack_start(GTK_BOX(start_with_box),
				pref_widgets.start_with_arrow, FALSE, FALSE, 8);
			gtk_box_pack_start(GTK_BOX(start_with_box),
				pref_widgets.start_with_scope_res, FALSE, FALSE, 8);
		}
		gtk_box_pack_start(GTK_BOX(vbox), start_with_box, FALSE, FALSE, 1);

		GtkWidget* row_text_max_box = gtk_hbox_new(FALSE, 2);
		{
			GtkWidget* row_text_max_label = gtk_label_new(_("row text max"));

			GtkObject* row_text_max_adj = gtk_adjustment_new(pref->row_text_max, 10, 512, 1, 10, 0);
			pref_widgets.row_text_max_spinbtn =
				gtk_spin_button_new(GTK_ADJUSTMENT(row_text_max_adj), 1.0, 0);
			g_signal_connect(pref_widgets.row_text_max_spinbtn, "value-changed",
				G_CALLBACK(on_pref_dialog_value_changed), NULL);

			gtk_box_pack_start(GTK_BOX(row_text_max_box),
				row_text_max_label, FALSE, FALSE, 5);
			gtk_box_pack_start(GTK_BOX(row_text_max_box),
				pref_widgets.row_text_max_spinbtn, FALSE, FALSE, 5);
		}
		gtk_box_pack_start(GTK_BOX(vbox), row_text_max_box, FALSE, FALSE, 1);

		GtkWidget* swin_height_max_box = gtk_hbox_new(FALSE,2);
		{
			GtkWidget* swin_height_max_label = gtk_label_new(_("suggestion window height max"));

			GtkObject* swin_height_max_adj =
				gtk_adjustment_new(pref->suggestion_window_height_max, 100, 1080, 1, 10, 0);
			pref_widgets.swin_height_max_spinbtn =
				gtk_spin_button_new(GTK_ADJUSTMENT(swin_height_max_adj), 1.0, 0);
			g_signal_connect(pref_widgets.swin_height_max_spinbtn, "value-changed",
				G_CALLBACK(on_pref_dialog_value_changed), NULL);

			gtk_box_pack_start(GTK_BOX(swin_height_max_box),
				swin_height_max_label, FALSE, FALSE, 5);
			gtk_box_pack_start(GTK_BOX(swin_height_max_box),
				pref_widgets.swin_height_max_spinbtn, FALSE, FALSE, 5);
		}
		gtk_box_pack_start (GTK_BOX(vbox), swin_height_max_box, FALSE, FALSE, 1);

		GtkWidget* compiler_options_label =
			gtk_label_new(_("compiler options (one option per a line)"));
		gtk_misc_set_alignment(GTK_MISC(compiler_options_label), 0, 0.5);
		gtk_box_pack_start (GTK_BOX(vbox), compiler_options_label, FALSE, FALSE, 1);

		GtkWidget* copt_scrolled_window = gtk_scrolled_window_new(NULL,NULL);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(copt_scrolled_window),
			GTK_SHADOW_ETCHED_OUT);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(copt_scrolled_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_set_border_width(GTK_CONTAINER(copt_scrolled_window), 5);
		GtkWidget* options_text_view = gtk_text_view_new();
		gtk_widget_set_size_request(options_text_view, 480, 250);
		gtk_container_add(GTK_CONTAINER(copt_scrolled_window), options_text_view);
		// load compiler options
		pref_widgets.options_text_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(options_text_view));
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_offset(pref_widgets.options_text_buf, &iter, 0);
		for(size_t i=0; i<pref->compiler_options.size(); i++) {
			gtk_text_buffer_insert(
				pref_widgets.options_text_buf, &iter, pref->compiler_options[i].c_str(), -1);
			gtk_text_buffer_insert(pref_widgets.options_text_buf, &iter, "\n", -1);
		}
		g_signal_connect(pref_widgets.options_text_buf, "changed",
			G_CALLBACK(on_pref_dialog_value_changed), NULL);

		gtk_box_pack_start(GTK_BOX(vbox), copt_scrolled_window, TRUE, TRUE, 5);

		g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

		return vbox;
	}
}

