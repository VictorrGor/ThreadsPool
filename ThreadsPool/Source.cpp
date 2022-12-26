#include <iostream>
#include <limits>
#include "ThreadsPool.h"
#include <thread>
#include <omp.h>

void pool_test()
{
	volatile size_t accum = 0;
	size_t max = std::numeric_limits<size_t>().max();
	max = 1000; //max >> 35;
	for (size_t i = 0; i != max; ++i) {
		++accum;
		accum *= 2;
		accum /= 2;
	}
}

void pool_vs_threads_vector()
{
	size_t task_count = 1000;
	size_t max_threads_size = 8;
	std::cout << "Pool of threads:\n";
	double start_time = omp_get_wtime();
	{
		ThreadsPool::ThreadsPool pool;

		pool.setPoolSize(max_threads_size);
		for (size_t i_task = 0; i_task < task_count; ++i_task) {
			pool.addTask(pool_test);
		}
		pool.joinQueue();
	}
	double end_time = omp_get_wtime();
	std::cout << "\tTime: " << end_time - start_time << ";\n";

	std::cout << "\nThreads vector:\n";
	start_time = omp_get_wtime();
	{
		std::vector<std::thread> v;
		v.reserve(task_count);
		for (size_t i_thread = 0; i_thread < task_count; ++i_thread) {
			v.emplace_back(pool_test);
		}
		for (std::thread& i_thread : v) i_thread.join();
	}
	end_time = omp_get_wtime();
	std::cout << "\tTime: " << end_time - start_time << ";\n";
}


int main()
{
	pool_vs_threads_vector();
}