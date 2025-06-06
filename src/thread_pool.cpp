#include "thread_pool.hpp"
#include <atomic>

ThreadPool::ThreadPool(size_t n_threads)
    : Loggable{ "ThreadPool" }
    , n_threads_{ n_threads }
    , stop_all_{ false }
{
    for (size_t i = 0; i < n_threads_; ++i)
    {
        threads_.emplace_back([this] {
            while (true)
            {
                Task *task = nullptr;
                {
                    std::unique_lock lock{ mutex_ };
                    cv_.wait(lock, [this] { return !tasks_.empty() || stop_all_.load(std::memory_order_acquire); });

                    task = &tasks_.front();
                    log("Got task @ {}", (void *)task);
                    tasks_.pop_front();
                }

                if (task != nullptr && !stop_all_.load(std::memory_order_acquire))
                {
                    task->first(task->second);  // actually run the task
                    task = nullptr;
                }
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    stop_all_.store(true, std::memory_order_release);
    cv_.notify_all();
    for (auto &t : threads_)
        t.join();
}

void ThreadPool::run(const Task& t)
{
    std::lock_guard<std::mutex> guard{ mutex_ };
    tasks_.emplace_back(t.first, t.second);
    cv_.notify_one();
}

void ThreadPool::run(const Func &f, const Arg &arg)
{
    std::lock_guard<std::mutex> guard{ mutex_ };
    tasks_.emplace_back(f, arg);
    cv_.notify_one();
}

void ThreadPool::stop()
{
    stop_all_.store(true, std::memory_order_release);
}

void ThreadPool::resume()
{
    stop_all_.store(false, std::memory_order_release);
}
