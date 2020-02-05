#pragma once

#include <vector>
#include <thread>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <iostream>

namespace SoftRender
{
	using namespace std;

	class ThreadPool
	{
	public:
		ThreadPool();
		virtual ~ThreadPool();

		void add(std::function<void()> aFunction);
		void join();

	protected:
		int m_max_threads = 0;

		mutex m_mutex;
		vector<thread> m_threads;
		vector<function<void()>> m_next_func;
		condition_variable m_cond_start;
		condition_variable m_cond_ready;
		vector<bool> m_is_free;
		bool m_end_threads;

		vector<int> m_called;
	};
}

