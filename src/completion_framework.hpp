/*
 * completion_framework.hpp - a Geany plugin to provide code completion using clang
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
#include <string>
#include <vector>
#include "cc_plugin.hpp"

#include "completion_async.hpp"
#include "suggestion_window.hpp"
namespace cc
{

	class CppCompletionFramework
	{
		CodeCompletionAsyncWrapper* completion;
		SuggestionWindow* suggestion_window;
	public:
		CppCompletionFramework();
		virtual ~CppCompletionFramework();

		void set_suggestion_window(SuggestionWindow* window) {
			suggestion_window = window;
		}


		bool check_filetype(GeanyFiletype *ft) const;
		/**
			return true if typed . -> :: except for comments and strings, otherwise false.
		 */
		bool check_trigger_char(GeanyEditor *editor);

		void set_completion_option(std::vector<std::string>& options);
		void complete_async(
			const char* filename, const char* content, int line, int col, int flag=0);
		bool try_get_completion_results(CodeCompletionResults& result);

		GtkWidget* create_config_widget(GtkDialog* dialog);

		void load_preferences();
		void updated_preferences();
		void save_preferences();
	};
}
