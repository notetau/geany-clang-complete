/*
 * completion_framework.cpp - a Geany plugin to provide code completion using clang
 *
 * Copyright (C) 2014-2015 Noto, Yuta <nonotetau(at)gmail(dot)com>
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


#include "completion_framework.hpp"

#include "preferences.hpp"
#include "completion.hpp"

#include <SciLexer.h>

namespace geanycc
{

CppCompletionFramework::CppCompletionFramework()
{
	completion = new CodeCompletionAsyncWrapper(new CppCodeCompletion());
}
CppCompletionFramework::~CppCompletionFramework()
{
	if (completion) {
		delete completion;
		completion = NULL;
	}
}

bool CppCompletionFramework::check_filetype(GeanyFiletype* ft) const
{
	if (ft == NULL) {
		return false;
	}
	return (ft->id == GEANY_FILETYPES_C || ft->id == GEANY_FILETYPES_CPP);
}

bool CppCompletionFramework::check_trigger_char(GeanyEditor* editor)
{
	int pos = sci_get_current_position(editor->sci);
	if (pos < 2) {
		return false;
	}

	char c1 = sci_get_char_at(editor->sci, pos - 1);
	ClangCompletePluginPref* pref = ClangCompletePluginPref::instance();

	int style_id = sci_get_style_at(editor->sci, pos);
	switch (style_id) {
		case SCE_C_COMMENTLINE:
		case SCE_C_COMMENT:
		case SCE_C_COMMENTLINEDOC:
		case SCE_C_COMMENTDOC:
		case SCE_C_STRINGEOL:
			return false;
	}

	if (pref->start_completion_with_scope_res) {
		if (c1 == ':') {
			char c0 = sci_get_char_at(editor->sci, pos - 2);
			if (c0 == ':') {
				return true;
			}
		}
	}
	if (pref->start_completion_with_arrow) {
		if (c1 == '>') {
			char c0 = sci_get_char_at(editor->sci, pos - 2);
			if (c0 == '-') {
				return true;
			}
		}
	}
	if (pref->start_completion_with_dot) {
		if (c1 == '.') {
			int c0_style_id = sci_get_style_at(editor->sci, pos - 1);
			if (c0_style_id == SCE_C_NUMBER) {
				return false;
			}
			// TODO ignore 0 omitted floating number such as ".123"
			return true;
		}
	}
	return false;
}

} // namespace geanycc
