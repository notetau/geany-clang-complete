
#include "completion_async.hpp"
#include <thread>
#include <mutex>
#include <list>
#include <algorithm>
#include <memory>
#include <future>
#include <iostream>
#include <chrono>
namespace cc
{
	class CodeCompletionAsync::CodeCompletionAsyncImpl
	{
	public:
		CodeCompletionAsyncImpl()
		{
		}

		~CodeCompletionAsyncImpl()
		{
		}

		void set_option(std::vector<std::string>& options)
		{
			std::lock_guard<std::mutex> lock(comp_mutex);
			completion.setOption(options);
		}
		void complete_async(
			const char* filename, const char* content, int line, int col, int flag)
		{
			std::string filename_(filename);
			std::string content_(content);

			std::shared_ptr<CodeCompletionResults> p(new CodeCompletionResults());

			std::future< std::shared_ptr<CodeCompletionResults> > f =
				std::async( std::launch::async,
				[=](std::shared_ptr<CodeCompletionResults> results)
					{
						std::lock_guard<std::mutex> lock(comp_mutex);

						completion.complete(
							*results,
							filename_.c_str(), content_.c_str(), line, col, flag);
						return results;
					}, p);

			//push front (stack)
			future_list.push_front(std::move(f));
		}

		bool try_get_results(CodeCompletionResults& results)
		{
			if( future_list.empty() ) {
				return false;
			}
			else {
				bool ret = 0;
				auto iter = future_list.begin();

				if( iter->wait_for(std::chrono::seconds(0)) == std::future_status::ready ) {
					results = *(iter->get()); //copy
					ret = true;
					future_list.erase(iter);

					future_list.remove_if(
					[](std::future< std::shared_ptr<CodeCompletionResults> >& f)
					{
						return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
					});
				}
				else {
					ret = false;
				}
				return ret;
			}
		}
	private:
		CodeCompletion completion;

		std::list< std::future< std::shared_ptr<CodeCompletionResults> > > future_list;

		std::mutex comp_mutex;
	};



	CodeCompletionAsync::CodeCompletionAsync() : pimpl(new CodeCompletionAsyncImpl()) {}

	CodeCompletionAsync::~CodeCompletionAsync() { delete pimpl; }

	void CodeCompletionAsync::set_option(std::vector<std::string>& options)
	{
		pimpl->set_option(options);
	}

	void CodeCompletionAsync::complete_async(
		const char* filename, const char* content, int line, int col, int flag)
	{
		pimpl->complete_async(filename, content, line, col, flag);
	}

	bool CodeCompletionAsync::try_get_results(CodeCompletionResults& results)
	{
		return pimpl->try_get_results(results);
	}
}
