#pragma once
#ifndef __THREADSAFE_QUEUE_H__
#define __THREADSAFE_QUEUE_H__

#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

namespace DSM {

    template <typename T>
    class ThreadsafeQueue
    {
    private:
        struct node
        {
            std::shared_ptr<T> data{};
            std::unique_ptr<node> next{};
        };

    public:
        ThreadsafeQueue()
            : m_Head(std::make_unique<node>()), m_Tail(m_Head.get()) {}

        ThreadsafeQueue(const ThreadsafeQueue& other) = delete;
        ThreadsafeQueue& operator=(const ThreadsafeQueue&) = delete;

        void Push(T val)
        {
            auto new_data = std::make_shared<T>(std::move(val));
            auto new_node = std::make_unique<node>();
            {
                std::lock_guard tail_lock{m_TailMutex};
                m_Tail->data = new_data;
                m_Tail->next = std::move(new_node);
                m_Tail = m_Tail->next.get();
            }
            m_DataCond.notify_one();
        }

        std::shared_ptr<T> WaitAndPop()
        {
            const auto old_head = WaitPopHead();
            return old_head->data;
        }

        void WaitAndPop(T& value)
        {
            const auto old_head = WaitPopHead(value);
        }

        std::shared_ptr<T> TryPop()
        {
            const auto old_node = TryPopHead();
            return old_node == nullptr ? nullptr : old_node->data;
        }

        bool TryPop(T& value)
        {
            const auto old_head = TryPopHead(value);
            return old_head != nullptr;
        }

        bool Empty() const
        {
            std::lock_guard head_lock{m_HeadMutex};
            return m_Head.get() == GetTail();
        }


    private:
        node* GetTail() const
        {
            std::lock_guard tail_lock{m_TailMutex};
            return m_Tail;
        }

        std::unique_ptr<node> PopHead()
        {
            auto old_head = std::move(m_Head);
            m_Head = std::move(old_head->next);
            return old_head;
        }

        auto WaitForData()
        {
            std::unique_lock head_lock{m_HeadMutex};
            m_DataCond.wait(head_lock, [this]{ return m_Head.get() != GetTail(); });
            return std::move(head_lock);
        }

        std::unique_ptr<node> WaitPopHead()
        {
            std::unique_lock head_lock{WaitForData()};
            return PopHead();
        }

        std::unique_ptr<node> WaitPopHead(T& value)
        {
            std::unique_lock head_lock{WaitForData()};
            value = std::move(*m_Head->data);
            return PopHead();
        }

        std::unique_ptr<node> TryPopHead()
        {
            std::lock_guard head_lock{m_HeadMutex};
            if(m_Head.get() == GetTail()){
                return nullptr;
            }
            return PopHead();
        }

        std::unique_ptr<node> TryPopHead(T& value)
        {
            std::lock_guard head_lock{m_HeadMutex};
            if(m_Head.get() == GetTail()){
                return nullptr;
            }
            value = std::move(*m_Head->data);
            return PopHead();
        }

    private:
        std::unique_ptr<node> m_Head{};
        node* m_Tail = nullptr;
        mutable std::mutex m_HeadMutex{};
        mutable std::mutex m_TailMutex{};
        std::condition_variable m_DataCond{};
    };

} // namespace dsm

#endif // __THREADSAFE_QUEUE_H__