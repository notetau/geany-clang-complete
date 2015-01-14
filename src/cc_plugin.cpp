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
	_("0.02"), _("Noto, Yuta <nonotetau@gmail.com>"));

#include <string>
#include <vector>
#include <string.h>

#include <SciLexer.h>

#include "completion.hpp"
#include "ui.hpp"
#include "preferences.hpp"

#include "completion_async.hpp"
#include <thread>
#include <chrono>
#include "completion_framework.hpp"
// global variables ////////////////////////////////////////////////////////////////
static cc::SuggestionWindow* suggestWindow;

static cc::CppCompletionFramework* completion_framework;

static struct {
	bool valid;
	int start_pos;
	std::string text;
} edit_tracker;
////////////////////////////////////////////////////////////////////////////////////
/**
	return true if the editing file needs completions, otherwise false. ( is it C/C++ file? )
 */
static bool is_completion_file_now()
{
	GeanyDocument* doc = document_get_current();
	if(doc == NULL) { return false; }
	if( !doc->real_path ) { return false; }

	if(completion_framework) {
		return completion_framework->check_filetype(doc->file_type);
	} else {
		return false;
	}
}

static int get_completion_position(int* flag=NULL)
{
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
	if( completion_framework == NULL ) { return; }
	if( !is_completion_file_now() ) { return; }
	int pos = get_completion_position();
	if( pos == 0 ) { return; } //nothing to complete

	int line= sci_get_line_from_position(editor->sci, pos);
	int ls_pos = sci_get_position_from_line(editor->sci, line);
	int byte_line_len = pos - ls_pos;
	if( byte_line_len < 0 ) { return; }

	char* content = sci_get_contents(editor->sci, sci_get_length(editor->sci)+1+1);
	content[sci_get_length(editor->sci)] = ' '; // replace null -> virtual space for clang
	content[sci_get_length(editor->sci)] = '\0';

	cc::CodeCompletionResults results;

	//TODO clang's col is byte? character?
	completion_framework->complete_async(editor->document->file_name, content, line+1, byte_line_len+1);

	edit_tracker.valid = true;
	edit_tracker.start_pos = pos;
	edit_tracker.text.clear();

	if( pos != sci_get_current_position(editor->sci) ) {
		int len = sci_get_current_position(editor->sci) - pos;
		edit_tracker.text.append(content + pos, len);
	}

	g_free(content);
}

/**
	return true if typed . -> :: except for comments and strings, otherwise false.
 */
static bool check_trigger_char(GeanyEditor *editor)
{
	int pos = sci_get_current_position(editor->sci);
	if( pos < 2 ) { return false; }

	char c1 = sci_get_char_at(editor->sci, pos-1);
	ClangCompletePluginPref* pref = get_ClangCompletePluginPref();

	int style_id = sci_get_style_at(editor->sci, pos);
	switch(style_id){
		case SCE_C_COMMENTLINE:    case SCE_C_COMMENT:
		case SCE_C_COMMENTLINEDOC: case SCE_C_COMMENTDOC:
		case SCE_C_STRINGEOL:
			return false;
	}

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
			int c0_style_id = sci_get_style_at(editor->sci, pos-1);
			if( c0_style_id == SCE_C_NUMBER ) { return false; }
			/* TODO ignore 0 omitted floating number such as ".123" */
			return true;
		}
	}
	return false;
}

static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
								 SCNotification *nt, gpointer* user_data)
{
	if( !is_completion_file_now() ) { return FALSE; }
	switch (nt->nmhdr.code)
	{
		case SCN_UPDATEUI:
			//TODO relocation suggestion window when typings occur scroll (e.g. editting long line)
			if(nt->updated & SC_UPDATE_SELECTION) {
				suggestWindow->close();
			}
			break;
		case SCN_MODIFIED:
			//report before insert position, after delete position
			if( edit_tracker.valid ) {
				if(nt->modificationType & SC_MOD_INSERTTEXT) {
					if( nt->position == edit_tracker.start_pos + edit_tracker.text.length() ) {
						std::string text(nt->text, nt->length); // nt->text is not null term
						edit_tracker.text += text;
						suggestWindow->filter_add(text.c_str());
					}
					else {
						edit_tracker.valid = false;
						suggestWindow->close();
					}
				}
				if(nt->modificationType & SC_MOD_DELETETEXT) {
					//it was caused by backspace?
					if( nt->length == 1 &&
						edit_tracker.text.length() > 0 &&
						nt->position + 1 == edit_tracker.start_pos + edit_tracker.text.length() ) {
						edit_tracker.text.erase(edit_tracker.text.size() - 1);
						suggestWindow->filter_backspace();
					}
					else {
						edit_tracker.valid = false;
						suggestWindow->close();
					}
				}
			}
			break;
		case SCN_CHARADDED:
			if( check_trigger_char(editor) ) {
				send_complete(editor, FALSE);
			}
			break;
		default:
			break;
	}
	return FALSE;
}

static void on_document_activate(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if( suggestWindow ) { suggestWindow->close(); }
}

PluginCallback plugin_callbacks[] = {
	{"editor_notify", (GCallback)&on_editor_notify, FALSE, NULL},
	{"document_activate", (GCallback)&on_document_activate, FALSE, NULL},
	//{"document_open", (GCallback)&on_document_open, FALSE, NULL},
	{NULL,NULL,FALSE,NULL}
};

void update_clang_complete_plugin_state()
{
	if( completion_framework ) {
		completion_framework->set_completion_option(get_ClangCompletePluginPref()->compiler_options);
	}
}

static void force_completion(G_GNUC_UNUSED guint key_id)
{
	if( completion_framework ) {
		GeanyDocument* doc = document_get_current();
		if(doc != NULL) {
			send_complete(doc->editor, 0);
		}
	}
}

static gboolean loop_check_ready(gpointer user_data)
{
	if( !is_completion_file_now() ) { return TRUE; }
	if( completion_framework ) {
		cc::CodeCompletionResults results; // allocate at heap, when init?
		if( completion_framework->try_get_completion_results(results) ) {
			if( edit_tracker.valid ) {
				suggestWindow->show(results, edit_tracker.text.c_str());
			}
		}
	}
	return TRUE;
}


static void init_keybindings()
{
	const int COUNT_KB = 1;
	const int KB_COMPLETE_IDX = 0;
	GeanyKeyGroup* key_group = plugin_set_key_group(geany_plugin,"clang_complete",COUNT_KB,NULL);
	keybindings_set_item(key_group,
		KB_COMPLETE_IDX, force_completion, 0, (GdkModifierType)0, "exec",_("complete"),NULL);
}

extern "C"{
	void plugin_init(GeanyData *data)
	{
		completion_framework = new cc::CppCompletionFramework();
		plugin_timeout_add(geany_plugin, 20, loop_check_ready, NULL);
		suggestWindow = new cc::SuggestionWindow();
		get_ClangCompletePluginPref()->load_preferences();

		init_keybindings();

		edit_tracker.valid = false;
	}

	void plugin_cleanup(void)
	{
		if( completion_framework ) {
			delete completion_framework;
			completion_framework = NULL;
		}
		if( suggestWindow ) {
			delete suggestWindow;
			suggestWindow = NULL;
		}
		cleanup_ClangCompletePluginPref();
	}
}
