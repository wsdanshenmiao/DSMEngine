#pragma once
#ifndef __CONRAINER_H__
#define __CONRAINER_H__

#include <array>
#include <cassert>

namespace DSM {
    template <typename T, std::size_t N>
    class StaticVector : private std::array<T, N>
    {
    public:
        using Base = std::array<T, N>;

        using typename Base::value_type;
        using typename Base::size_type;
        using typename Base::difference_type;
        using typename Base::reference;
        using typename Base::const_reference;
        using typename Base::pointer;
        using typename Base::const_pointer;
        using typename Base::iterator;
        using typename Base::const_iterator;

        StaticVector() : Base(), m_CurrSize(0){ }

        StaticVector(size_t size) : Base(), m_CurrSize(size)
        {
            assert(size <= N);
        }

        StaticVector(std::initializer_list<T> il)
            : m_CurrSize(0)
        {
            for(auto i : il)
                PushBack(i);
        }

        using Base::at;

        reference operator[] (size_type pos)
        {
            return Base::operator[](pos);
        }

        const_reference operator[] (size_type pos) const
        {
            return Base::operator[](pos);
        }

        reference Front() noexcept                  { auto tmp = begin(); return *tmp; }
        const_reference Front() const noexcept      { auto tmp = begin(); return *tmp; }
        reference Back() noexcept                   { auto tmp =  end() - 1; return *tmp; }
        const_reference Back() const noexcept       { auto tmp = cend() - 1; return *tmp; }

        value_type* Data() noexcept                          { return Base::data(); }
        const value_type* Data() const noexcept              { return Base::data(); }
        
        using Base::begin;
        using Base::cbegin;

        iterator end() noexcept                     { return begin() + m_CurrSize; }
        const_iterator end() const noexcept         { return cend(); }
        const_iterator cend() const noexcept        { return cbegin() + m_CurrSize; }

        bool Empty() const noexcept                 { return m_CurrSize == 0; }
        size_t Size() const noexcept                { return m_CurrSize; }
        constexpr size_t Capacity() const noexcept  { return N; }

        void Fill(const T& value) noexcept
        {
            Base::fill(value);
            m_CurrSize = N;
        }

        void Swap(StaticVector& other) noexcept
        {
            Base::swap(*this);
            std::swap(m_CurrSize, other.m_CurrSize);
        }

        void PushBack(const T& value) noexcept
        {
            assert(m_CurrSize < N);
            *(Data() + m_CurrSize) = value;
            m_CurrSize++;
        }

        void PushBack(T&& value) noexcept
        {
            assert(m_CurrSize < N);
            *(Data() + m_CurrSize) = std::move(value);
            m_CurrSize++;
        }

        void PopBack() noexcept
        {
            assert(m_CurrSize > 0);
            m_CurrSize--;
        }

        void Resize(size_type new_size) noexcept
        {
            assert(new_size <= N);

            if (m_CurrSize > new_size)
            {
                for (size_type i = new_size; i < m_CurrSize; i++)
                    *(Data() + i) = T{};
            }
            else
            {
                for (size_type i = m_CurrSize; i < new_size; i++)
                    *(Data() + i) = T{};
            }

            m_CurrSize = new_size;
        }

        template <typename... Args>
        reference EmplaceBack(Args&&... args) noexcept
        {
            assert(m_CurrSize < N);
            ++m_CurrSize;
            Back() = T{std::forward<Args>(args)...};
            return Back();
        }

    private:
        std::size_t m_CurrSize = 0;
    };


} // namespace DSM 


#endif