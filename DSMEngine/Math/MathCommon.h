#pragma once
#ifndef __MATHCOMMON_H__
#define __MATHCOMMON_H__

#include <concepts>

namespace DSM {
    // 获取
    template <std::unsigned_integral T>
    T NextPowerOf2(T val)
    {
        val--;
        val |= val >> 1;
        val |= val >> 2;
        val |= val >> 4;
        val |= val >> 8;
        val |= val >> 16;
        val++;

        return val;
    }


} // namespace DSM 

#endif