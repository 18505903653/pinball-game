#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <list>
#include "locker.h"

// 线程池类，T表示任务类
template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000):
        m_thread_number(thread_number), m_max_requests(max_requests), m_stop(fasle), m_threads(nullptr)
    {
        if (thread_number <= 0 || max_requests <= 0) {
            throw std::exception();
        }
        m_threads = new pthread_t[m_thread_number];
        if (!m_threads) {
            throw std::exception();
        }
        // 创建thread_number 个线程，并设置他们为线程脱离
        for (int i = 0;i < thread_number;i++) {
            if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
                delete[] m_threads;
                throw std::exception();
            }
            if (pthread_detach(m_threads[i]) != 0) {
                delete[] m_threads;
                throw std::exception();
            }
        }
    }
    ~threadpool() {
        delete[] m_threads;
        m_stop = true;
    }

    bool append(T* request) {
        m_queuelocker.lock();
        if (m_workqueue.size() >= m_max_requests) {
            m_queuelocker.unlock();
            return false;
        }

        m_workqueue.push_back(request);
        m_queuestat.post();
        m_queuelocker.unlock();
        return true;
    }
private:
    static void* worker(void* arg) {
        threadpool* pool = (threadpool*)arg;
        pool->run();
        return pool;
    }
    void run() {
        while (!m_stop) {
            m_queuestat.wait();
            m_queuelocker.lock();
            if (m_workqueue.empty()) {
                m_queuelocker.unlock();
                continue;
            }

            T* request = m_workqueue.front();
            m_workqueue.pop_front();
            m_queuelocker.unlock();

            if (!request) {
                continue;
            }

            request->process();
        }
    }
private:
    // 线程的数量
    int m_thread_number;

    // 线程池数组
    pthread_t* m_threads;

    // 请求队列最多允许的等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::list<T*> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量用来判断是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;
};
#endif