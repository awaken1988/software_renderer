#include "render_threading.h"

SoftRender::ThreadPool::ThreadPool()
{
	m_max_threads = std::thread::hardware_concurrency();
	if (m_max_threads < 1)
		m_max_threads = 1;
	m_next_func.resize(m_max_threads);
	m_is_free.resize(m_max_threads);
	m_called.resize(m_max_threads);
	m_end_threads = false;

	for (int iThread = 0; iThread < m_max_threads; iThread++) {
		m_is_free[iThread] = false;
		m_threads.push_back(thread([this, iThread]() {	
			{
				unique_lock lck(this->m_mutex);
				this->m_is_free[iThread] = true;
				this->m_cond_ready.notify_all();
			}
			while (true) {	
				{
					unique_lock lck(this->m_mutex);
					this->m_cond_start.wait(lck, [this, iThread]() {
						return !this->m_is_free[iThread];
					});	

					//std::cout << "threadpool: start  " << iThread << std::endl;
					m_called[iThread]++;
				}

				this->m_next_func[iThread]();

				{
					scoped_lock slck(this->m_mutex);
					m_is_free[iThread] = true;
					if (m_end_threads)
						return;
				}
				m_cond_ready.notify_all();
			}
		}));
	}

	//wait until all threads are ready
	unique_lock lck(m_mutex);
	m_cond_ready.wait(lck, [this]() {
		for (const auto& iIsFree : m_is_free) {
			if (!iIsFree)
				return false;
		}
		return true;
	});
}

SoftRender::ThreadPool::~ThreadPool()
{
	join();

	m_end_threads = true;

	for (int iThread = 0; iThread < m_max_threads; iThread++) {
		m_next_func[iThread] = []() -> void {
			return;
		};
		m_is_free[iThread] = false;
	}

	m_cond_start.notify_all();

	for (int iThread = 0; iThread < m_max_threads; iThread++) {
		m_threads[iThread].join();
	}
}

void SoftRender::ThreadPool::add(std::function<void()> aFunction)
{
	int idx = 0;
	auto check_fun = [this, &idx]() {
		idx = 0;
		for (; idx < m_max_threads; idx++) {
			if (m_is_free[idx])
				return true;
		}
		return false;
	};

	unique_lock lck(m_mutex);
	m_cond_ready.wait(lck, check_fun);
	m_next_func[idx] = aFunction;
	m_is_free[idx] = false;
	lck.unlock();
	m_cond_start.notify_all();
}

void SoftRender::ThreadPool::join()
{
	auto check_fun = [this]() {
		for (int i=0; i < m_max_threads; i++) {
			if (!m_is_free[i])
				return false;
		}
		return true;
	};

	unique_lock lck(m_mutex);
	m_cond_ready.wait(lck, check_fun);
}
