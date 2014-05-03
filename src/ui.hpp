/*
 * ui.hpp - a Geany plugin to provide code completion using clang
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

#pragma once

#include "cc_plugin.hpp"
//for CodeCompletionResults
#include "completion.hpp"

namespace cc {

	class SuggestionWindow {
		GtkWidget* window;
		GtkWidget* tree_view;
		GtkListStore* model;

		std::vector<GdkPixbuf*> icon_pixbufs;

		bool showing_flag;

		std::string filtered_str;
		int pos_start;

		static gboolean signal_key_press_and_release(
			GtkWidget *widget, GdkEventKey *event, SuggestionWindow* self);

		static void signal_tree_selection(
			GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column,
			SuggestionWindow* self);

		void move_cursor(bool down);
		void select_suggestion();
		void filter_backspace();
		void do_filtering();
	public:
		void filter_add(int ch);
		void filter_add(const std::string& str);
		SuggestionWindow();

		~SuggestionWindow() {
			gtk_widget_destroy(window);
		}

		void show(const cc::CodeCompletionResults& results);
		void show_with_filter(const cc::CodeCompletionResults& results, const std::string& filter);
		void close() {
			if( showing_flag ) {
				gtk_widget_hide(tree_view);
				gtk_widget_hide(window);
				showing_flag = false;
			}
		}
		bool isShowing() const { return showing_flag; }

		gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *nt);
	};

}
