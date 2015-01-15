#pragma once
#include <string>
#include <vector>
#include "cc_plugin.hpp"

#include "completion_async.hpp"
#include "ui.hpp"
namespace cc
{

	class CppCompletionFramework
	{
		CodeCompletionAsync* completion;
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
