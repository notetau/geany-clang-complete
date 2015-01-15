/*
 * preferences.hpp - a Geany plugin to provide code completion using clang
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

#include <vector>
#include <string>

struct ClangCompletePluginPref {
	std::vector<std::string> compiler_options;
	//std::vector<std::string> secondary_compiler_options;
	bool start_completion_with_dot;
	bool start_completion_with_arrow;
	bool start_completion_with_scope_res;
	int row_text_max;
	int suggestion_window_height_max;
	//bool hide_filtered_row;
	//bool replace_well_known_template_typedef;
	//bool use_async_completion;

	void load_preferences();
};


ClangCompletePluginPref* get_ClangCompletePluginPref();

void cleanup_ClangCompletePluginPref();
