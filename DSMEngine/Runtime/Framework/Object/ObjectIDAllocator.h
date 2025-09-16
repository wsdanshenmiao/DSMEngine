#pragma once
#ifndef __OBJECTIDALLOCATOR_H__
#define __OBJECTIDALLOCATOR_H__

#include <atomic>
#include <limits>
#include "Runtime/Core/Macro.h"

namespace DSM {
    using GUID = std::size_t;

    constexpr GUID c_InvalidGUID = std::numeric_limits<GUID>::max();

    class ObjectIDAllocator
    {
    public:
        static GUID AllocateID()
        {
            auto id = sm_NextID.load();
            sm_NextID++;
            if(id >= c_InvalidGUID){
                DSM_CORE_CRITICAL("ObjectIDAllocator: ID allocation failed");
            }
            return id;
        }

    private:
        inline static std::atomic<GUID> sm_NextID = 0;
    };
} // namespace DSM

#endif