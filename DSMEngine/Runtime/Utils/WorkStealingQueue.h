#pragma once
#ifndef __WORK_STEALING_QUEUE_H__
#define __WORK_STEALING_QUEUE_H__

#include <deque>
#include <mutex>
#include "FunctionWrapper.h"


namespace DSM {
    class WorkStealingQueue 
    {
    public:
        WorkStealingQueue() = default;

        WorkStealingQueue(const WorkStealingQueue&) = delete;
        WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

        void Push(FunctionWrapper&& func)
        {
            std::lock_guard<std::mutex> lg(m_Mutex);
            m_Queue.push_front(std::forward<FunctionWrapper>(func));
        }

        bool TryPop(FunctionWrapper& res)
        {
            std::lock_guard<std::mutex> lg(m_Mutex);
            if(m_Queue.empty()){
                return false;
            }
            res = std::move(m_Queue.front());
            m_Queue.pop_front();
            return true;
        }

        bool TrySteal(FunctionWrapper& res)
        {
            std::lock_guard<std::mutex> lg(m_Mutex);
            if(m_Queue.empty()){
                return false;
            }
            res = std::move(m_Queue.back());
            m_Queue.pop_back();
            return true;
        }

        bool Empty() const
        {
            std::lock_guard<std::mutex> lg(m_Mutex);
            return m_Queue.empty();
        }   

    private:
        std::deque<FunctionWrapper> m_Queue;
        mutable std::mutex m_Mutex;
    };
}


#endif // __WORK_STEALING_QUEUE_H__