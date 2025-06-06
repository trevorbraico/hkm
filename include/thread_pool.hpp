#include "loggable.hpp"
#include <deque>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>

using std::size_t;

using Func = void(*)(void *);
using Arg = void *;
using Task = std::pair<Func, Arg>;

class ThreadPool : public Loggable
{
public:

    ThreadPool(size_t n_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void run(const Task& task);
    void run(const Func &f, const Arg &arg);

    void stop();
    void resume();

private:
    size_t n_threads_;
    std::vector<std::thread> threads_;
    std::deque<Task> tasks_;

    std::mutex mutex_;
    std::atomic<bool> stop_all_{ false };
    std::condition_variable cv_;
};
