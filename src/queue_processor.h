#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>

template<typename T>
class QueueProcessor
{
private:
    std::queue<std::thread> _threads;
    std::queue<T> _tasks;
    size_t _active_count;
    bool _terminate;

    std::mutex _thread_mutex;
    std::condition_variable _thread_cv;

    virtual void act(const T&, size_t qid) = 0;

public:

    QueueProcessor() : _active_count(0), _terminate(false) {};
    virtual ~QueueProcessor() = default;

    void start(size_t thread_count = std::thread::hardware_concurrency())
    {
        size_t qid = 0;
        while(_threads.empty() || _threads.size() < thread_count) {
            _threads.push(std::thread([this,qid]() {
                while(true) {
                    std::unique_lock<std::mutex> lock_thread(_thread_mutex);
                    _thread_cv.wait(lock_thread, [this]() {
                        return !_tasks.empty() || _terminate;
                    });
                    if(_tasks.empty() && _terminate)
                        break;

                    T task = _tasks.front();
                    _tasks.pop();
                    ++_active_count;
                    lock_thread.unlock();

                    act(task, qid);

                    lock_thread.lock();
                    if(--_active_count == 0 && _tasks.empty())
                        _thread_cv.notify_all();
                }
            }));
            ++qid;
        }
    }

    void add(const T& task, bool now)
    {
        std::unique_lock<std::mutex> lock_thread(_thread_mutex);
        _tasks.push(task);
        if(now)
            _thread_cv.notify_all();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock_thread(_thread_mutex);
        _thread_cv.wait(lock_thread, [&]() {
            return _active_count == 0 && _tasks.empty();
        });
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock_thread(_thread_mutex);
        _terminate = true;
        _thread_cv.notify_all();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock_thread(_thread_mutex);
        while(!_tasks.empty())
            _tasks.pop();
    }

    void finish()
    {
        while(!_threads.empty()) {
            _threads.front().join();
            _threads.pop();
        }
    }

    virtual void done()
    {
        stop();
        wait();
        finish();
    }
};

