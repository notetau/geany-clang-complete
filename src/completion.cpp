/*
 * completion.cpp - a Geany plugin to provide code completion using clang
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

#include "completion.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstring>

#include <clang-c/Index.h>

using namespace cc;

static CompleteResultType getCursorType(CXCompletionResult& result)
{
	switch(result.CursorKind) {
	case CXCursor_StructDecl: case CXCursor_UnionDecl: case CXCursor_ClassDecl:
	case CXCursor_TypedefDecl: case CXCursor_ClassTemplate: case CXCursor_Constructor:
		return COMPLETE_RESULT_CLASS;
	case CXCursor_CXXMethod: case CXCursor_Destructor:
		return COMPLETE_RESULT_METHOD;
	case CXCursor_FunctionDecl : case CXCursor_FunctionTemplate:
		return COMPLETE_RESULT_FUNCTION;
	case CXCursor_VarDecl:
		return COMPLETE_RESULT_VAR;
	case CXCursor_FieldDecl:
		return COMPLETE_RESULT_MEMBER;
	case CXCursor_Namespace:
		return COMPLETE_RESULT_NAMESPACE;

	default:
		//std::cout<<"unknown chunk"<<  result.CursorKind <<std::endl;
		return COMPLETE_RESULT_NONE;
	}
}

static void getExpression(CXCompletionString str, std::string& typedText, std::string& signature)
{
	int N = clang_getNumCompletionChunks(str);
	std::string returnType;
	std::string& exp = signature;
	for(int i=0; i<N; i++) {
        CXCompletionChunkKind kind = clang_getCompletionChunkKind(str, i);

        switch(kind) {
		case CXCompletionChunk_Optional:
			exp += "{";
			getExpression( clang_getCompletionChunkCompletionString(str, i), typedText, signature );
            exp += "}";
            break;
        case CXCompletionChunk_VerticalSpace:
			exp += "{VerticalSpace  }";
            break;
		case CXCompletionChunk_TypedText:
			{
				CXString text = clang_getCompletionChunkText(str, i);
				const char* cstr = clang_getCString(text);
				typedText = cstr;
				exp += cstr;
				clang_disposeString(text);
			} break;
		case CXCompletionChunk_Placeholder:
		     {
				CXString text = clang_getCompletionChunkText(str, i);
				const char *cstr = clang_getCString(text);
				exp += cstr ? cstr : "";
				clang_disposeString(text);
			} break;
		case CXCompletionChunk_ResultType:
			{
				CXString text = clang_getCompletionChunkText(str, i);
				const char *cstr = clang_getCString(text);
				returnType = cstr ? cstr : "";
				//exp += ' ';
				clang_disposeString(text);
			} break;
		case CXCompletionChunk_Text:
		case CXCompletionChunk_Informative:
		case CXCompletionChunk_CurrentParameter:
             {
				CXString text = clang_getCompletionChunkText(str, i);
				const char *cstr = clang_getCString(text);
				exp += cstr ? cstr : "";
				exp += ' ';
				clang_disposeString(text);
			} break;
		case CXCompletionChunk_LeftParen:        exp +='('; break;
		case CXCompletionChunk_RightParen:       exp +=')'; break;
		case CXCompletionChunk_LeftBracket:      exp +='['; break;
		case CXCompletionChunk_RightBracket:     exp +=']'; break;
		case CXCompletionChunk_LeftBrace:        exp +='{'; break;
		case CXCompletionChunk_RightBrace:       exp +='}'; break;
		case CXCompletionChunk_LeftAngle:        exp +='<'; break;
		case CXCompletionChunk_RightAngle:       exp +='>'; break;
		case CXCompletionChunk_Comma:            exp +=", "; break;
		case CXCompletionChunk_Colon:            exp +=':'; break;
		case CXCompletionChunk_SemiColon:        exp +=';'; break;
		case CXCompletionChunk_Equal:            exp +='='; break;
		case CXCompletionChunk_HorizontalSpace:  exp +=' '; break;
		}
	}
	signature = exp + " : " + returnType;
	//signature = exp;
}


class CodeCompletion::CodeCompletionImpl {
public:
	CXIndex index;
	std::map<std::string, CXTranslationUnit> tuCache;
	std::vector<std::string> commandLineArgs;

	CodeCompletionImpl() {
		index = clang_createIndex(0, 1); /* (excludeDeclarationsFromPCH, displayDiagnostics) */
		if(!index) {
			std::cerr<< "an unexpected error @ clang_createIndex" <<std::endl;
		}
		std::cout<<"created codecomplete"<<std::endl;
	}
	~CodeCompletionImpl() {
		clearTranslationUnitCache();
		clang_disposeIndex(index);
		std::cout<<"destroyed codecomplete"<<std::endl;
	}

	void clearTranslationUnitCache() {
		std::map<std::string, CXTranslationUnit>::iterator it = tuCache.begin();
		while( it != tuCache.end() ) {
			if( it->second ) {
				clang_disposeTranslationUnit(it->second);
			}
			++it;
		}
		tuCache.clear();
	}

	CXTranslationUnit getTranslationUnit(const char* filename, const char* content) {
		CXTranslationUnit tu;
		if( tuCache.find(filename) == tuCache.end() ) {

			const char** argv = new const char*[commandLineArgs.size()];

			for(size_t i=0; i<commandLineArgs.size(); i++) {
				//argv[i] = commandLineArgs[i].c_str();
				argv[i] = commandLineArgs.at(i).c_str();
			}

			CXUnsavedFile f[1];
			f[0].Filename = filename;
			f[0].Contents = content;
			f[0].Length   = strlen(content);

			tu = clang_parseTranslationUnit(index, filename, argv, commandLineArgs.size(),
											f, 1, clang_defaultEditingTranslationUnitOptions() );
			if(tu) {
				tuCache.insert(std::pair<std::string, CXTranslationUnit>(filename, tu));
			}

			delete [] argv;
		} else {
			tu = tuCache[filename];
		}
		return tu;
	}
	void setOption(std::vector<std::string>& options) {
		commandLineArgs.clear();
		for(size_t i=0; i<options.size(); i++) {
			commandLineArgs.push_back(options[i]);
		}
		clearTranslationUnitCache();
	}
	void complete(CodeCompletionResults& result,
		const char* filename, const char* content, int line, int col, int flag) {

		if(index == NULL) { return; }
		result.clear();

		CXTranslationUnit tu = getTranslationUnit(filename, content);
		if(!tu) {
			std::cerr<< "an unexpected error @ clang_parseTranslationUnit" <<std::endl;
			return;
		}
		CXUnsavedFile f[1];
		f[0].Filename = filename;
		f[0].Contents = content;
		f[0].Length   = strlen(content);

		/* NEED reparse! */
		clang_reparseTranslationUnit(tu, 0, 0, clang_defaultReparseOptions(tu));

		CXCodeCompleteResults* results = clang_codeCompleteAt(tu, filename, line, col,
			f, 1, clang_defaultCodeCompleteOptions() );

		if(!results) {
			std::cerr<< "an unexpected error @ clang_codeCompleteAt" <<std::endl;
			return;
		}
		if( results->NumResults == 0 ) {
			std::cerr<< "no code completion!!!" <<std::endl;
			return;
		} else {
			clang_sortCodeCompletionResults(results->Results, results->NumResults);
			for(int i=0; i<results->NumResults; i++) {
				CompleteResultRow r;
				r.type = getCursorType(results->Results[i]);

				if(r.type != COMPLETE_RESULT_NONE) {
					getExpression(results->Results[i].CompletionString, r.typedText, r.label);
					//r.label = r.typedText + ": " + r.label;
					result.push_back(r);
				}
			}
		}
		clang_disposeCodeCompleteResults(results);
	}

	//int completeAsync(const char* filename, const char* content, int line, int col);
private:
	CodeCompletionImpl(const CodeCompletionImpl&);
	void operator=(const CodeCompletionImpl&);
};


//CodeCompletion////////////////////////////////////////////////////////////////
CodeCompletion::CodeCompletion() : pimpl(new CodeCompletionImpl()) {}
CodeCompletion::~CodeCompletion() {delete pimpl;}

void CodeCompletion::setOption(std::vector<std::string>& options) {
	pimpl->setOption(options);
}
void CodeCompletion::complete(CodeCompletionResults& result,
	const char* filename, const char* content, int line, int col, int flag) {

	pimpl->complete(result, filename, content, line, col, flag);
}

int CodeCompletion::completeAsync(const char* filename, const char* content, int line, int col) {
	return 0;
}

