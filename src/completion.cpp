/*
 * completion.cpp - a Geany plugin to provide code completion using clang
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

#include "completion.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstring>

#include <clang-c/Index.h>

using namespace geanycc;

static CompleteResultType getCursorType(const CXCompletionResult& result)
{
	switch (result.CursorKind) {
		case CXCursor_StructDecl:
		case CXCursor_UnionDecl:
		case CXCursor_ClassDecl:
		case CXCursor_TypedefDecl:
		case CXCursor_ClassTemplate:
		case CXCursor_Constructor:
			return COMPLETE_RESULT_CLASS;
		case CXCursor_CXXMethod:
		case CXCursor_Destructor:
			return COMPLETE_RESULT_METHOD;
		case CXCursor_FunctionDecl:
		case CXCursor_FunctionTemplate:
			return COMPLETE_RESULT_FUNCTION;
		case CXCursor_VarDecl:
		case CXCursor_EnumConstantDecl:
			return COMPLETE_RESULT_VAR;
		case CXCursor_FieldDecl:
			return COMPLETE_RESULT_MEMBER;
		case CXCursor_Namespace:
			return COMPLETE_RESULT_NAMESPACE;
		case CXCursor_MacroDefinition:
			return COMPLETE_RESULT_MACRO;  // return COMPLETE_RESULT_NONE;
		case CXCursor_EnumDecl:
			return COMPLETE_RESULT_OTHER;
		case CXCursor_NotImplemented:  // keywords?
			// case CXCursor_ParmDecl: //function arg?
			return COMPLETE_RESULT_NONE;
		default:
			std::cout << "unknown chunk" << result.CursorKind << std::endl;
			return COMPLETE_RESULT_NONE;
	}
}

struct CompletionStringParser
{
	CompleteResultRow* r;
	std::string text;
	int enter_optional_count;
	int enter_arguments;

	void setAveailability(CXCompletionString comp_str)
	{
		CXAvailabilityKind kind = clang_getCompletionAvailability(comp_str);
		switch (kind) {
			case CXAvailability_Available:
				r->availability = COMPLETE_RESULT_AVAIL_AVAIL;
				break;
			case CXAvailability_Deprecated:
				r->availability = COMPLETE_RESULT_AVAIL_DEPRECATED;
				break;
			case CXAvailability_NotAvailable:
				r->availability = COMPLETE_RESULT_AVAIL_NOTAVAIL;
				break;
			case CXAvailability_NotAccessible:
				r->availability = COMPLETE_RESULT_AVAIL_NOTACCESS;
				break;
			default:
				r->availability = COMPLETE_RESULT_AVAIL_NOTAVAIL;
				break;
		}
	}

	void append(std::string& stdstr, CXCompletionString comp_str, unsigned chunk_idx)
	{
		CXString text = clang_getCompletionChunkText(comp_str, chunk_idx);
		const char* cstr = clang_getCString(text);
		stdstr += cstr ? cstr : "";
		clang_disposeString(text);
	}

	void look(CXCompletionString comp_str, unsigned chunk_idx)
	{
		CXString text = clang_getCompletionChunkText(comp_str, chunk_idx);
		const char* cstr = clang_getCString(text);
		if (cstr) {
			r->signature += cstr;
			if (enter_arguments) {
				r->arguments += cstr;
			}
		}
		clang_disposeString(text);
	}

	template <typename T>
	void look(T val)
	{
		r->signature += val;
		if (enter_arguments) {
			r->arguments += val;
		}
	}

	void do_parse(CXCompletionString comp_str)
	{
		int N = clang_getNumCompletionChunks(comp_str);
		for (int i = 0; i < N; i++) {
			CXCompletionChunkKind kind = clang_getCompletionChunkKind(comp_str, i);
			switch (kind) {
				case CXCompletionChunk_Optional:
					if (enter_optional_count == 0) {
						look("{");
					}
					enter_optional_count += 1;
					do_parse(clang_getCompletionChunkCompletionString(comp_str, i));
					enter_optional_count -= 1;
					if (enter_optional_count == 0) {
						look("}");
					}
					break;
				case CXCompletionChunk_TypedText:
					append(r->typed_text, comp_str, i);
					break;
				case CXCompletionChunk_ResultType:
					append(r->return_type, comp_str, i);
					break;
				case CXCompletionChunk_Placeholder:
					look(comp_str, i);
					break;
				case CXCompletionChunk_Text:
					append(text, comp_str, i);
					break;
				case CXCompletionChunk_Informative:
					look(comp_str, i);
					break;
				case CXCompletionChunk_CurrentParameter:
					look(comp_str, i);
					break;
				case CXCompletionChunk_LeftParen:
					enter_arguments += 1;
					look(" (");
					break;
				case CXCompletionChunk_RightParen:
					look(')');
					enter_arguments -= 1;
					break;
				case CXCompletionChunk_LeftBracket:
					look('[');
					break;
				case CXCompletionChunk_RightBracket:
					look(']');
					break;
				case CXCompletionChunk_LeftBrace:
					look('{');
					break;
				case CXCompletionChunk_RightBrace:
					look('}');
					break;
				case CXCompletionChunk_LeftAngle:
					look('<');
					break;
				case CXCompletionChunk_RightAngle:
					look('>');
					break;
				case CXCompletionChunk_Comma:
					look(", ");
					break;
				case CXCompletionChunk_Colon:
					look(':');
					break;
				case CXCompletionChunk_SemiColon:
					look(';');
					break;
				case CXCompletionChunk_Equal:
					look('=');
					break;
				case CXCompletionChunk_HorizontalSpace:
					look(' ');
					break;
				case CXCompletionChunk_VerticalSpace:
					look('\n');
					break;
			}
		}
	}

	void parse(CompleteResultRow* r, const CXCompletionResult& result)
	{
		this->r = r;
		r->type = getCursorType(result);
		CXCompletionString comp_str = result.CompletionString;
		setAveailability(comp_str);
		enter_optional_count = 0;
		enter_arguments = 0;
		text = "";
		do_parse(comp_str);
		r->signature.insert(0, r->typed_text);
		if (r->return_type != "") {
			r->signature += " -> ";
			r->signature += r->return_type;
		}
		if (text != "") {
			r->signature += " {";
			r->signature += text;
			r->signature += "}";
		}
	}
};

class CppCodeCompletion::CodeCompletionImpl
{
   public:
	CXIndex index;
	std::map<std::string, CXTranslationUnit> tu_cache;
	std::vector<std::string> commandline_args;

	CodeCompletionImpl() : index(NULL)
	{
		CXString version = clang_getClangVersion();
		std::cout << clang_getCString(version) << std::endl;
		clang_disposeString(version);
		create_index();
		std::cout << "created codecomplete" << std::endl;
	}

	~CodeCompletionImpl()
	{
		clear_translation_unit_cache();
		clang_disposeIndex(index);
		std::cout << "destroyed codecomplete" << std::endl;
	}

	void create_index()
	{
		if (index) {
			clear_translation_unit_cache();
			clang_disposeIndex(index);
			index = NULL;
		}
		index = clang_createIndex(0, 0); /* (excludeDeclarationsFromPCH, displayDiagnostics) */
		if (!index) {
			std::cerr << "an unexpected error @ clang_createIndex" << std::endl;
		}
	}

	CXTranslationUnit get_translation_unit(const char* filename, const char* content)
	{
		CXTranslationUnit tu = NULL;
		if (tu_cache.find(filename) != tu_cache.end()) {
			tu = tu_cache[filename];
		} else {  // not found -> create
			const char** argv = new const char* [commandline_args.size()];
			for (size_t i = 0; i < commandline_args.size(); i++) {
				argv[i] = commandline_args[i].c_str();
			}
			CXUnsavedFile f[1];
			f[0].Filename = filename;
			f[0].Contents = content;
			f[0].Length = strlen(content);
			tu = clang_parseTranslationUnit(index, filename, argv, commandline_args.size(), f, 1,
			                                clang_defaultEditingTranslationUnitOptions());
			if (tu) {
				tu_cache.insert(std::pair<std::string, CXTranslationUnit>(filename, tu));
			}
			delete[] argv;
		}
		return tu;
	}

	void clear_translation_unit_cache()
	{
		std::map<std::string, CXTranslationUnit>::iterator it = tu_cache.begin();
		while (it != tu_cache.end()) {
			if (it->second) {
				clang_disposeTranslationUnit(it->second);
			}
			++it;
		}
		tu_cache.clear();
	}

	void set_option(std::vector<std::string>& options)
	{
		commandline_args.clear();
		for (size_t i = 0; i < options.size(); i++) {
			commandline_args.push_back(options[i]);
		}
		create_index();
	}

	void complete(CodeCompletionResults& result, const char* filename, const char* content,
	              int line, int col, int flag)
	{
		if (index == NULL) {
			return;
		}
		result.clear();

		CXTranslationUnit tu = get_translation_unit(filename, content);
		if (!tu) {
			std::cerr << "an unexpected error @ clang_parseTranslationUnit" << std::endl;
			return;
		}
		CXUnsavedFile f[1];
		f[0].Filename = filename;
		f[0].Contents = content;
		f[0].Length = strlen(content);

		unsigned comp_flag = clang_defaultCodeCompleteOptions();
		CXCodeCompleteResults* results =
		    clang_codeCompleteAt(tu, filename, line, col, f, 1, comp_flag);

		if (!results) {
			std::cerr << "an unexpected error @ clang_codeCompleteAt" << std::endl;
			return;
		}

		if (results->NumResults == 0) {
			std::cerr << "no code completion!!!" << std::endl;
			return;
		} else {
			result.reserve(results->NumResults);
			clang_sortCodeCompletionResults(results->Results, results->NumResults);
			for (int i = 0; i < results->NumResults; i++) {
				geanycc::CompleteResultType type = getCursorType(results->Results[i]);

				if (type != COMPLETE_RESULT_NONE) {
					result.push_back(CompleteResultRow());
					CompleteResultRow& pr = result[result.size() - 1];
					pr.type = type;
					CompletionStringParser().parse(&pr, results->Results[i]);
				}
			}
		}

		clang_disposeCodeCompleteResults(results);
	}

   private:
	CodeCompletionImpl(const CodeCompletionImpl&);
	void operator=(const CodeCompletionImpl&);
};

// CodeCompletion //////////////////////////////////////////////////////////////
CppCodeCompletion::CppCodeCompletion() : pimpl(new CodeCompletionImpl()) {}
CppCodeCompletion::~CppCodeCompletion() { delete pimpl; }

void CppCodeCompletion::set_option(std::vector<std::string>& options)
{
	pimpl->set_option(options);
}

void CppCodeCompletion::complete(CodeCompletionResults& result, const char* filename,
                                 const char* content, int line, int col, int flag)
{
	pimpl->complete(result, filename, content, line, col, flag);
}

// misc ////////////////////////////////////////////////////////////////////////
struct CompletionStringParserDebugPrinter
{
	void print_cchunk(const char* chunk_name, CXCompletionString comp_str, unsigned chunk_idx)
	{
		CXString text = clang_getCompletionChunkText(comp_str, chunk_idx);
		const char* cstr = clang_getCString(text);
		printf("{%s, %s}, ", chunk_name, cstr);
		clang_disposeString(text);
	}

	void do_parse(CXCompletionString comp_str)
	{
		int N = clang_getNumCompletionChunks(comp_str);
		for (int i = 0; i < N; i++) {
			CXCompletionChunkKind kind = clang_getCompletionChunkKind(comp_str, i);
			switch (kind) {
				case CXCompletionChunk_Optional:
					printf("{Optional, S}, ");
					do_parse(clang_getCompletionChunkCompletionString(comp_str, i));
					printf("{Optional, E}, ");
					break;
				case CXCompletionChunk_TypedText:
					print_cchunk("TypedText", comp_str, i);
					break;
				case CXCompletionChunk_ResultType:
					print_cchunk("ResultType", comp_str, i);
					break;
				case CXCompletionChunk_Placeholder:
					print_cchunk("Placeholder", comp_str, i);
					break;
				case CXCompletionChunk_Text:
					print_cchunk("Text", comp_str, i);
					break;
				case CXCompletionChunk_Informative:
					print_cchunk("Info", comp_str, i);
					break;
				case CXCompletionChunk_CurrentParameter:
					print_cchunk("CurParam", comp_str, i);
					break;
				case CXCompletionChunk_LeftParen:
					print_cchunk("L-Paren", comp_str, i);
					break;
				case CXCompletionChunk_RightParen:
					print_cchunk("R-Paren", comp_str, i);
					break;
				case CXCompletionChunk_LeftBracket:
					print_cchunk("L-Bracket", comp_str, i);
					break;
				case CXCompletionChunk_RightBracket:
					print_cchunk("R-Bracket", comp_str, i);
					break;
				case CXCompletionChunk_LeftBrace:
					print_cchunk("L-Brace", comp_str, i);
					break;
				case CXCompletionChunk_RightBrace:
					print_cchunk("R-Brace", comp_str, i);
					break;
				case CXCompletionChunk_LeftAngle:
					print_cchunk("L-Angle", comp_str, i);
					break;
				case CXCompletionChunk_RightAngle:
					print_cchunk("R-Angle", comp_str, i);
					break;
				case CXCompletionChunk_Comma:
					print_cchunk("Comma", comp_str, i);
					break;
				case CXCompletionChunk_Colon:
					print_cchunk("Colon", comp_str, i);
					break;
				case CXCompletionChunk_SemiColon:
					print_cchunk("SemiColon", comp_str, i);
					break;
				case CXCompletionChunk_Equal:
					print_cchunk("Equal", comp_str, i);
					break;
				case CXCompletionChunk_HorizontalSpace:
					print_cchunk("H-Space", comp_str, i);
					break;
				case CXCompletionChunk_VerticalSpace:
					print_cchunk("V-Space", comp_str, i);
					break;
			}
		}
	}

	void parse(const CXCompletionResult& result)
	{
		CXCompletionString comp_str = result.CompletionString;
		do_parse(comp_str);
		printf("\n");
	}
};
