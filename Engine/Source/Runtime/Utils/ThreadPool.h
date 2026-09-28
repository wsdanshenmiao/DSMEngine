#pragma once
#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__


#include <thread>
#include <functional>
#include <future>
#include <type_traits>

#include "ThreadsafeQueue.h"
#include "WorkStealingQueue.h"

namespace DSM {

    class ThreadPool {
    public:
        explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
        {
            thread_count = std::max(size_t(1), thread_count);
            
            try {
                m_Queues.reserve(thread_count);
                m_Threads.reserve(thread_count);
                for(size_t i = 0; i < thread_count; ++i) {
                    m_Queues.emplace_back(std::make_unique<WorkStealingQueue>());
                    m_Threads.emplace_back(&ThreadPool::WorkerThread, this, i);
                }
            }
            catch(const std::exception& e) {
                m_Done = true;
                throw;
            }
        }

        ~ThreadPool()
        {
            m_Done = true;
        }

        template <typename Func>
        auto Submit(Func&& f)
        {
            using result_type = std::invoke_result_t<Func>;

            std::packaged_task<result_type()> task(std::forward<Func>(f));
            std::future<result_type> res = task.get_future();
            if(sm_LocalWorkQueue != nullptr){
                sm_LocalWorkQueue->Push(std::move(task));
            }
            else {
                m_WorkQueue.push(std::move(task));
            }
            return res;
        }

        void RunPendingTask()
        {
            FunctionWrapper task;
            // 一次从本地队列，全局队列，其他线程队列中获取任务
            if(PopTaskFromLocalQueue(task) || 
                PopTaskFromGlobalQueue(task) || 
                PopTaskFromOtherThreadQueue(task)){
                task();
            }
            else {
                std::this_thread::yield();
            }
        }

    private:
        void WorkerThread(size_t index) 
        {
            sm_Index = index;
            sm_LocalWorkQueue = m_Queues[index].get();
            while(!m_Done) {
                RunPendingTask();
            }
        }

        bool PopTaskFromLocalQueue(FunctionWrapper& task)
        {
            return sm_LocalWorkQueue != nullptr && sm_LocalWorkQueue->TryPop(task);
        }

        bool PopTaskFromGlobalQueue(FunctionWrapper& task)
        {
            return m_WorkQueue.TryPop(task);
        }

        bool PopTaskFromOtherThreadQueue(FunctionWrapper& task)
        {
            for(size_t i = 0; i < m_Queues.size(); ++i) {
                const size_t index = (sm_Index + i + 1) % m_Queues.size();
                if(m_Queues[index]->TrySteal(task)){
                    return true;
                }
            }
            return false;
        }

    private:
        // 减少不同线程的任务竞争，每个线程拥有独立的任务队列
        std::atomic<bool> m_Done{false};
        ThreadsafeQueue<FunctionWrapper> m_WorkQueue;
        std::vector<std::unique_ptr<WorkStealingQueue>> m_Queues;
        std::vector<std::jthread> m_Threads;

        inline static thread_local WorkStealingQueue* sm_LocalWorkQueue = nullptr;
        inline static thread_local size_t sm_Index = 0;
    };
}


#endif // __THREAD_POOL_H__