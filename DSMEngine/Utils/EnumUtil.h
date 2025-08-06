#pragma once
#ifndef __ENUMUTIL_H__
#define __ENUMUTIL_H__

#include <concepts>

namespace DSM {
    template <typename T> requires std::is_enum_v<T>
    struct EnumBitOperators
    {
        static constexpr bool enable = false;
    };

    template <typename T> requires EnumBitOperators<T>::enable
    inline constexpr T operator|(T l, T r)
    {
		using E = std::underlying_type_t<T>;
		return static_cast<T>(static_cast<E>(l) | static_cast<E>(r));
	}

    template <typename T> requires EnumBitOperators<T>::enable
    inline constexpr T operator&(T l, T r)
    {
		using E = std::underlying_type_t<T>;
		return static_cast<T>(static_cast<E>(l) & static_cast<E>(r));
	}

    template <typename T> requires EnumBitOperators<T>::enable
    inline constexpr T operator^(T l, T r)
    {
		using E = std::underlying_type_t<T>;
		return static_cast<T>(static_cast<E>(l) ^ static_cast<E>(r));
	}

    template <typename T> requires EnumBitOperators<T>::enable
    inline constexpr T operator~(T l)
    {
		using E = std::underlying_type_t<T>;
		return static_cast<T>(~static_cast<E>(l));
	}

    template <typename T> requires EnumBitOperators<T>::enable
    inline constexpr bool operator!(T l)
    {
        return static_cast<bool>(!static_cast<std::underlying_type_t<T>>(l));
    }
        
    template <typename T> requires EnumBitOperators<T>::enable
    inline T& operator|=(T& l, T r) { return l = l | r; }
        
    template <typename T> requires EnumBitOperators<T>::enable
    inline T& operator&=(T& l, T r) { return l = l & r; }
        
    template <typename T> requires EnumBitOperators<T>::enable
    inline T& operator^=(T& l, T r) { return l = l ^ r; }

    template <typename T> requires std::is_enum_v<T>
    inline constexpr bool HasAllFlags(T value, T flags) { return (value & flags) == flags; }

    template <typename T> requires std::is_enum_v<T>
    inline constexpr bool HasFlags(T value, T flags) 
    { 
        return static_cast<std::underlying_type_t<T>>(value & flags) != 0; 
    }

#define ENABLE_ENUM_BIT_OPERATOR(EnumType)  \
    template<> struct EnumBitOperators<EnumType>    \
    { static constexpr bool enable = true; };
    

    
} // namespace DSM 


#endif