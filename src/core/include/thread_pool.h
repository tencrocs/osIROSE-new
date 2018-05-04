// Copyright (c) 2012 Jakob Progsch, Václav Zeman
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//   1. The origin of this software must not be misrepresented; you must not
//   claim that you wrote the original software. If you use this software
//   in a product, an acknowledgment in the product documentation would be
//   appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//   3. This notice may not be removed or altered from any source
//   distribution.
// https://github.com/progschj/ThreadPool

/*!
 *  \file thread_pool.h
 *
 *  \author Jakob Progsch, Vaclav Zeman
 *  \date 2012
 *
 *  A thread pool implementation from https://github.com/progschj/ThreadPool
 *  Modified by RavenX8 and L3nn0x
 *
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace Core {
/*!
 *  \class ThreadPool
 *
 *  \brief A thread pool implementation.
 *
 *  This class implements a thread pool that holds onto a
 *  queue of jobs and a pool of workers.
 *  The workers get assigned a job in a first come first server
 *  manner.
 *
 */
class ThreadPool {
 public:
  /*!
   * \brief The constructor. Takes the number of threads to launch
   *
   * \param[in] nb_threads The number of threads to launch in the thread pool
   */
  ThreadPool(size_t nb_threads);
  ~ThreadPool();
  /*!
   * \brief Enqueue a job with arguments to be executed by the thread pool
   *
   * \param[in] f The callable to execute
   * \param[in] args The arguments to pass to f. They are binded using std::bind
   * \param[out] std::future<> The result is a future on the result of
   * f(args...)
   */
  template <class F, class... Args>
  auto enqueue(F &&f, Args &&... args)
      -> std::future<std::invoke_result_t<F(Args...)>>;

 private:
  // need to keep track of threads so we can join them
  std::vector<std::thread> workers;
  // the task queue
  std::queue<std::function<void()>> tasks;

  // synchronization
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
  for (size_t i = 0; i < threads; ++i)
    workers.emplace_back([this] {
      for (;;) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(this->queue_mutex);
          this->condition.wait(
              lock, [this] { return this->stop || !this->tasks.empty(); });
          if (this->stop && this->tasks.empty()) return;
          task = std::move(this->tasks.front());
          this->tasks.pop();
        }

        task();
      }
    });
}

// add new work item to the pool
template <class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&... args)
    -> std::future<std::invoke_result_t<F(Args...)>> {
  using return_type = std::invoke_result_t<F(Args...)>;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex);

    // don't allow enqueueing after stopping the pool
    if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

    tasks.emplace([task]() { (*task)(); });
  }
  condition.notify_one();
  return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) worker.join();
}
}  // namespace Core

#endif