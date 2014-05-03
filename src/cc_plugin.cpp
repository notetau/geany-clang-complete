/*
 * cc_plugin.cpp - a Geany plugin to provide code completion using clang
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

#include "cc_plugin.hpp"

extern "C" {
	GeanyPlugin *geany_plugin;
	GeanyData *geany_data;
	GeanyFunctions *geany_functions;
}

PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO(_("clang-complete"), _("code completion by clang"),
	_("0.01"), _("Noto, Yuta <nonotetau@gmail.com>"));

#include <string>
#include <vector>
#include <string.h>

#include "completion.hpp"
#include "ui.hpp"
#include "preferences.hpp"

// global variables ////////////////////////////////////////////////////////////////
cc::SuggestionWindow* suggestWindow;
cc::CodeCompletion* codeCompletion;
////////////////////////////////////////////////////////////////////////////////////

static int get_completion_position(int* flag=NULL) {
	const char* stop_token = "{}[]#()<>%:;.?*+-/^&∼!=,\\\"\'\t\n ";
	float xxx;
	GeanyDocument* doc = document_get_current();

	ScintillaObject *sci = doc->editor->sci;
	int cur_pos = sci_get_current_position(sci);
	if( cur_pos == 0 ) return 0;
	int cur_token_started_pos = 0;
	//g_print("char at (%d) %c ", sci_get_char_at(sci, cur_pos), sci_get_char_at(sci, cur_pos));
	for(int pos = cur_pos-1; pos >= 0; pos--) {
		if( strchr(stop_token, sci_get_char_at(sci, pos)) ) {
			cur_token_started_pos = pos + 1; break;
		}
	}
	//g_print("cpos %d token %d", cur_pos, cur_token_started_pos);
	return cur_token_started_pos;
}

static void send_complete(GeanyEditor *editor, int flag)
{
	if( codeCompletion == NULL ) { return; }
	int pos = get_completion_position();
	if( pos == 0 ) { return; } //nothing to complete

	int line= sci_get_line_from_position(editor->sci, pos);
	int ls_pos = sci_get_position_from_line(editor->sci, line);
	int byte_line_len = pos - ls_pos;
	if( byte_line_len < 0 ) { return; }

	//ui_set_statusbar(FALSE, _("f=%s, l%d, c%d"),
		//editor->document->file_name, line+1, byte_line_len+1 );

	char* content = sci_get_contents(editor->sci, sci_get_length(editor->sci)+1+1);
	content[sci_get_length(editor->sci)] = ' '; // replace null -> virtual space for clang
	content[sci_get_length(editor->sci)] = '\0';
	cc::CodeCompletionResults results;
	codeCompletion->complete(results,
		editor->document->file_name, content, line+1, byte_line_len+1);
	//TODO clang's col is byte? character?

	if( pos == sci_get_current_position(editor->sci) ) {
		suggestWindow->show(results);
	}
	else {
		int len = sci_get_current_position(editor->sci) - pos;
		char* filter = new char[len+1];
		std::copy(content + pos, content + pos + len, filter);
		filter[len] = '\0';
		//g_print("%s", filter);
		suggestWindow->show_with_filter(results, filter);
		delete [] filter;
	}

	g_free(content);
}

static bool check_trigger_char(GeanyEditor *editor) {
	GeanyFiletype *ft = editor->document->file_type;

	if (ft->id != GEANY_FILETYPES_C && ft->id != GEANY_FILETYPES_CPP) return false;

	int pos = sci_get_current_position(editor->sci);
	if( pos < 2 ) { return false; }
	//TODO check pos is in comment
	char c1 = sci_get_char_at(editor->sci, pos-1);
	//g_print("typed %d %c",c1, c1);
	ClangCompletePluginPref* pref = get_ClangCompletePluginPref();
	//triggered . -> ::
	//g_print("setting start_with %d %d %d",
		//pref->start_completion_with_dot,
		//pref->start_completion_with_arrow,
		//pref->start_completion_with_scope_res);

	if( pref->start_completion_with_scope_res ) {
		if(c1 == ':') {
			char c0 = sci_get_char_at(editor->sci, pos-2);
			if( c0 == ':' ) { return true; }
		}
	}
	if( pref->start_completion_with_arrow ) {
		if( c1 == '>' ) {
			char c0 = sci_get_char_at(editor->sci, pos-2);
			if( c0 == '-' ) { return true; }
		}
	}
	if( pref->start_completion_with_dot ) {
		 if(c1 == '.') {
			//int c0styleID = sci_get_style_at(editor->sci, pos-2);
			/* TODO ignore 0 omitted floating number such as ".123" */
			return true;
		}
	}
	return false;
}

static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
								 SCNotification *nt, gpointer* user_data)
{
	//if( !suggestWindow->isShowing() ) { return FALSE; }
	switch (nt->nmhdr.code)
	{
		case SCN_UPDATEUI:
			//TODO relocation suggestion window when typings occur scroll (e.g. editting long line)
			if(nt->updated & SC_UPDATE_SELECTION) {
				suggestWindow->close();
			}
			break;
		case SCN_MODIFIED:
			break;
		case SCN_CHARADDED:
			{
				suggestWindow->filter_add(nt->ch);
				if( check_trigger_char(editor) ) {
					send_complete(editor, FALSE);
				}
			}
			break;
		case SCN_CALLTIPCLICK:
			break;
		default:
			break;
	}
	return FALSE;
	//need ? proxy function
	//return suggestWindow->on_editor_notify(obj, editor, nt);
}

static void on_document_activate(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	//g_print("change doc");
	/* TODO check this file is C/C++ */
	suggestWindow->close();
}

PluginCallback plugin_callbacks[] = {
	{"editor_notify", (GCallback)&on_editor_notify, FALSE, NULL},
	{"document_activate", (GCallback)&on_document_activate, FALSE, NULL},
	//{"document_open", (GCallback)&on_document_open, FALSE, NULL},
	{NULL,NULL,FALSE,NULL}
};



void update_clang_complete_plugin_state() {
	if( codeCompletion ) {
		codeCompletion->setOption( get_ClangCompletePluginPref()->compiler_options );
	}
}

static void force_completion(G_GNUC_UNUSED guint key_id)
{
	if( codeCompletion == NULL ) { return; }

	GeanyDocument* doc = document_get_current();
	if(doc != NULL) {
		send_complete(doc->editor, 0);
	}
}

static void init_keybindings()
{
	const int COUNT_KB = 1;
	const int KB_COMPLETE_IDX = 0;

	GeanyKeyGroup* key_group = plugin_set_key_group(geany_plugin,"clang_complete",COUNT_KB,NULL);
	keybindings_set_item(key_group,
		KB_COMPLETE_IDX, force_completion, 0, (GdkModifierType)0,
		"exec",_("complete"),NULL);
}

extern "C"{
	void plugin_init(GeanyData *data)
	{
		codeCompletion = new cc::CodeCompletion();
		suggestWindow = new cc::SuggestionWindow();
		get_ClangCompletePluginPref()->load_preferences();

		init_keybindings();
	}
	void plugin_cleanup(void)
	{
		if( codeCompletion ) {
			delete codeCompletion;
			codeCompletion = NULL;
		}
		if( suggestWindow ) {
			delete suggestWindow;
			suggestWindow = NULL;
		}
		cleanup_ClangCompletePluginPref();
	}
}


