#include "completion_framework.hpp"

#include "preferences.hpp"

#include <SciLexer.h>

namespace cc
{

CppCompletionFramework::CppCompletionFramework()
{
	completion = new CodeCompletionAsync();
	suggestion_window = nullptr;
}
CppCompletionFramework::~CppCompletionFramework()
{
	if(completion) {
		delete completion;
		completion = NULL;
	}
}

bool CppCompletionFramework::check_filetype(GeanyFiletype *ft) const
{
	if(ft == NULL) { return false; }
	return (ft->id == GEANY_FILETYPES_C || ft->id == GEANY_FILETYPES_CPP);
}

bool CppCompletionFramework::check_trigger_char(GeanyEditor *editor)
{
	int pos = sci_get_current_position(editor->sci);
	if( pos < 2 ) { return false; }

	char c1 = sci_get_char_at(editor->sci, pos-1);
	ClangCompletePluginPref* pref = ClangCompletePluginPref::instance();

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

void CppCompletionFramework::set_completion_option(std::vector<std::string>& options)
{
	if(completion) {
		completion->set_option(options);
	}
}

void CppCompletionFramework::complete_async(
	const char* filename, const char* content, int line, int col, int flag)
{
	if(completion) {
		completion->complete_async(filename, content, line, col, flag);
	}
}

bool CppCompletionFramework::try_get_completion_results(CodeCompletionResults& result)
{
	if(completion) {
		return completion->try_get_results(result);
	} else {
		return false;
	}
}


}
