// Copyright 2016 Chirstopher Torres (Raven), L3nn0x
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*!
 *  \file network_thread_pool.h
 *
 *  \author Raven
 *  \date march 2016
 *
 *  The network thread pool
 */

#ifndef _thread_pool_h_
#define _thread_pool_h_
#include "thread_pool.h"
#include <asio.hpp>
#include <atomic>
#include <bitset>
#include <mutex>
#include <queue>
#include <thread>

namespace Core {
#define MAX_NETWORK_THREADS 512

/*!
 * \class NetworkThreadPool
 *
 * \brief This class is used to spawn a network thread pool that handles all of the work that comes to and from the network.
 */

class NetworkThreadPool {
  typedef std::unique_ptr<asio::io_context::work> asio_worker;

 public:
  /*!
   * \brief Forces a maximum amount of threads when launching the pool
   * \param[in] maximum number of threads
   */
  NetworkThreadPool(uint16_t max_threads)
      : io_work(new asio_worker::element_type(io_service)),
        pool(std::thread::hardware_concurrency()) {
    uint16_t core_count = std::thread::hardware_concurrency()
                              ? std::thread::hardware_concurrency()
                              : 1;

    if (max_threads != 0 && core_count > max_threads) core_count = max_threads;

    if (core_count > MAX_NETWORK_THREADS)
      core_count = MAX_NETWORK_THREADS;
    else if (core_count <= 0)
      core_count = 1;

    for (uint32_t idx = 0; idx < core_count; ++idx) {
      threads_active.set(idx);
      pool.enqueue([this, idx]() { (*this)(idx); });
    }
  }

  ~NetworkThreadPool() { shutdown(); }
  /*!
   * \brief get the io_service
   */
  asio::io_context* get_io_service() { return &io_service; }
  /*!
   * \brief Get the number of active threads
   */
  uint16_t get_thread_count() const {
    return static_cast<uint16_t>(threads_active.count());
  }

 private:
  void shutdown() {
    threads_active.reset();
    io_work.reset();
    io_service.stop();
  }

  NetworkThreadPool& operator()(uint32_t id) {
    io_service.run_one();
    if (threads_active.test(id)) pool.enqueue([this, id]() { (*this)(id); });
    return *this;
  }

  std::bitset<MAX_NETWORK_THREADS> threads_active;
  asio::io_context io_service;
  asio_worker io_work;
  ThreadPool pool;
};
}  // namespace Core

#endif  // __thread_pool_h__
