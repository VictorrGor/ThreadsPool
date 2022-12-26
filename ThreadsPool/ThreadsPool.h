#pragma once

#include <thread>
#include <future>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>
#include <unordered_map>
#include <any>
#include <condition_variable>
#include <variant>

namespace ThreadsPool
{
	enum class DestructorBehavior
	{
		JOIN = 0,
		DETACH = 1
	};

	enum class ThreadStatus
	{
		ACTIVE = 0,
		TO_RELEASE = 1
	};

	using Task = std::function<std::any()>;

	using VoidTask = std::function<void()>;

	using u64 = uint64_t;

	//Для возвращения значения потока из пула можно держать пул фьючерсов, возврашая индекс акутального фьючерса по которому можно получить результат(?)
	class ThreadsPool
	{
		std::vector<std::thread> m_threads;

		std::queue<std::pair<u64, std::variant<Task, VoidTask>>> m_tasks;

		std::unordered_map<u64, std::any> m_completed_tasks;

		//cv для оповещения потоков о новых задачах
		std::condition_variable m_threads_cv;

		//cv для оповещения main потока о готовой задаче
		std::condition_variable m_tasks_cv;
		
		DestructorBehavior m_behavior;

		std::mutex m_queue_mutex;

		std::mutex m_results_mutex;

		std::mutex m_complete_mutex;
		
		u64 m_id_counter = 0;

		bool isQuite = false;

		void runThread()
		{
			while (!isQuite) {
				std::variant<Task, VoidTask> task;
				u64 task_id;
				{
					std::unique_lock<std::mutex> lock(m_queue_mutex);
					m_threads_cv.wait(lock, [this] {return !this->isTasksEmpty() || this->isQuiteFlag(); });
					if (isQuite) return;

					std::tie(task_id, task) = m_tasks.front();
					m_tasks.pop();
				}
				std::any res;
				if (task.index()) {
					std::get<VoidTask>(task)();
					{
						std::unique_lock<std::mutex> lock(m_complete_mutex);
						m_completed_tasks[task_id] = std::any();
					}
				}
				else {
					res = std::get<Task>(task)();
					{
						std::unique_lock<std::mutex> lock(m_complete_mutex);
						m_completed_tasks[task_id] = std::move(res);
					}
				}
				m_tasks_cv.notify_one();
			}
		};

	public:
		ThreadsPool() : m_behavior(DestructorBehavior::JOIN) {};

		ThreadsPool(size_t thread_count, DestructorBehavior behavior = DestructorBehavior::JOIN) : m_behavior(behavior)
		{
			for (size_t i_thread_id = 0; i_thread_id < thread_count; ++i_thread_id) {
				m_threads.emplace_back(&ThreadsPool::runThread, this);
			}
		};

		ThreadsPool(const ThreadsPool& obj) = delete;

		ThreadsPool& operator=(const ThreadsPool& obj) = delete;

		~ThreadsPool()
		{
			isQuite = true;
			m_threads_cv.notify_all();
			if (m_behavior == DestructorBehavior::JOIN) {
				for (std::thread& thread : m_threads) {
					if (thread.joinable()) thread.join();
				}
			}
			else {
				for (std::thread& thread : m_threads) {
					if (thread.joinable()) thread.detach();
				}
			}
		};

		u64 addTask(VoidTask task)
		{
			std::unique_lock<std::mutex> queue_mutex(m_queue_mutex);
			u64 res_id = m_id_counter++;
			m_tasks.push({ res_id, task });
			m_threads_cv.notify_one();
			return res_id;
		};

		u64 addTask(Task task) 
		{
			std::unique_lock<std::mutex> queue_mutex(m_queue_mutex);
			u64 res_id = m_id_counter++;
			m_tasks.push({ res_id, task });
			m_threads_cv.notify_one();
			return res_id;
		};

		bool checkTaskStatus(u64 id)
		{
			if (m_completed_tasks.contains(id)) return true;
			return false;
		};

		//Блокирующее получение результатов по id задачи
		void getTaskResult(u64 id, std::any& result)
		{
			std::unique_lock<std::mutex> lock(m_results_mutex);
			m_tasks_cv.wait(lock, std::bind(&ThreadsPool::checkTaskStatus, this, id));
			result = std::move(m_completed_tasks[id]);
		};

		bool isTasksEmpty()
		{
			return m_tasks.empty();
		};

		bool isQuiteFlag()
		{
			return isQuite;
		}

		//Если уменьшается размер пула, то операция блокирующая
		void setPoolSize(size_t size)
		{
			size_t actual_size = m_threads.size();
			if (actual_size < size) {
				m_threads.reserve(actual_size);
				size_t new_threads_count = size - actual_size;
				for (size_t i_thread = 0; i_thread < new_threads_count; ++i_thread) {
					m_threads.emplace_back(&ThreadsPool::runThread, this);
				}
			}
			else {
				size_t released_threads_count = actual_size - size;
				for (size_t i_thread = actual_size - released_threads_count; i_thread < actual_size; ++i_thread) {
					m_threads[i_thread].join();
				}
				m_threads.resize(size);
			}
		}

		void joinQueue()
		{
			std::unique_lock<std::mutex> lock(m_results_mutex);
			m_tasks_cv.wait(lock, std::bind(&ThreadsPool::isTasksEmpty, this));
		}
	};
};