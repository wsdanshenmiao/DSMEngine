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
                push_back(i);
        }

        bool operator==(const StaticVector&) const = default;

        using Base::at;

        reference operator[] (size_type pos)
        {
            return Base::operator[](pos);
        }

        const_reference operator[] (size_type pos) const
        {
            return Base::operator[](pos);
        }

        reference front() noexcept                  { auto tmp = begin(); return *tmp; }
        const_reference front() const noexcept      { auto tmp = begin(); return *tmp; }
        reference back() noexcept                   { auto tmp =  end() - 1; return *tmp; }
        const_reference back() const noexcept       { auto tmp = cend() - 1; return *tmp; }

        value_type* data() noexcept                          { return Base::data(); }
        const value_type* data() const noexcept              { return Base::data(); }
        
        using Base::begin;
        using Base::cbegin;

        iterator end() noexcept                     { return begin() + m_CurrSize; }
        const_iterator end() const noexcept         { return cend(); }
        const_iterator cend() const noexcept        { return cbegin() + m_CurrSize; }

        bool empty() const noexcept                 { return m_CurrSize == 0; }
        size_t size() const noexcept                { return m_CurrSize; }
        constexpr size_t capacity() const noexcept  { return N; }

        void fill(const T& value) noexcept
        {
            Base::fill(value);
            m_CurrSize = N;
        }

        void swap(StaticVector& other) noexcept
        {
            Base::swap(*this);
            std::swap(m_CurrSize, other.m_CurrSize);
        }

        void push_back(const T& value) noexcept
        {
            assert(m_CurrSize < N);
            *(data() + m_CurrSize) = value;
            m_CurrSize++;
        }

        void push_back(T&& value) noexcept
        {
            assert(m_CurrSize < N);
            *(data() + m_CurrSize) = std::move(value);
            m_CurrSize++;
        }

        void pop_back() noexcept
        {
            assert(m_CurrSize > 0);
            m_CurrSize--;
        }

        void resize(size_type new_size) noexcept
        {
            assert(new_size <= N);

            if (m_CurrSize > new_size) {
                for (size_type i = new_size; i < m_CurrSize; i++)
                    *(data() + i) = T{};
            }
            else {
                for (size_type i = m_CurrSize; i < new_size; i++)
                    *(data() + i) = T{};
            }

            m_CurrSize = new_size;
        }

        template <typename... Args>
        reference emplace_back(Args&&... args) noexcept
        {
            assert(m_CurrSize < N);
            ++m_CurrSize;
            back() = T{std::forward<Args>(args)...};
            return back();
        }

    private:
        std::size_t m_CurrSize = 0;
    };


} // namespace DSM 


#endif