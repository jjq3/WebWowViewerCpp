//
// Created by Deamon on 12/13/2023.
//

#ifndef AWEBWOWVIEWERCPP_FRAMEBASEDSTACKALLOCATOR_H
#define AWEBWOWVIEWERCPP_FRAMEBASEDSTACKALLOCATOR_H

#undef max

#include <new>
#include <limits>
#include <stdint.h>
#include "tbb/tbb.h"
#include "tbb/scalable_allocator.h"

#ifndef TEST_APP_ALLOCATION

void * frameLinearAllocate(size_t size);
void frameDeAllocate(void *p, size_t size);
void allocatorBeginFrame(unsigned int frameNum);

template<class T>
struct FrameBasedStackAllocator
{
    typedef T value_type;

    FrameBasedStackAllocator() = default;

    template<class U>
    constexpr FrameBasedStackAllocator(const FrameBasedStackAllocator <U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n)
    {
        //if (n > (std::numeric_limits<std::size_t>::max() / sizeof(T)))
        //    throw std::bad_array_new_length();


        if (auto p = static_cast<T*>(frameLinearAllocate(n * sizeof(T))))
        {
//            report(p, n);
            return p;
        }

        return nullptr;
    }

    void deallocate(T* p, std::size_t n) noexcept
    {
//        report(p, n, 0);
        frameDeAllocate(p, n * sizeof(T));
    }
private:
//    void report(T* p, std::size_t n, bool alloc = true) const
//    {
//        std::cout << (alloc ? "Alloc: " : "Dealloc: ") << sizeof(T) * n
//                  << " bytes at " << std::hex << std::showbase
//                  << reinterpret_cast<void*>(p) << std::dec << '\n';
//    }
};

template<class T, class U>
bool operator==(const FrameBasedStackAllocator <T>&, const FrameBasedStackAllocator <U>&) { return true; }

template<class T, class U>
bool operator!=(const FrameBasedStackAllocator <T>&, const FrameBasedStackAllocator <U>&) { return false; }

template <class T>
using transp_vec = std::vector<T, FrameBasedStackAllocator<T>>;

namespace framebased {

#if ((defined (_GLIBCXX_VECTOR) \
      || defined (_LIBCPP_VECTOR) \
      || defined (_VECTOR_)))
#define ARENA_HAS_VECTOR_DEF
    template <class T>
    using vector = std::vector<T, FrameBasedStackAllocator<T>>;
#endif

#if ((defined (_GLIBCXX_UNORDERED_MAP) \
      || defined (_LIBCPP_UNORDERED_MAP) \
      || defined (_UNORDERED_MAP_)) \
     && !defined (ARENA_HAS_UNORDERED_MAP_DEF))
#define ARENA_HAS_UNORDERED_MAP_DEF
    template <class Key, class Value, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using unordered_map = std::unordered_map<Key, Value, Hash, KeyEqual, FrameBasedStackAllocator<std::pair<const Key, Value>>>;
#endif
}
#else
namespace framebased {
    template <class T>
    using vector = std::vector<T>;

    template <class Key, class Value, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using unordered_map = std::unordered_map<Key, Value, Hash, KeyEqual>;
}
#endif

#endif //AWEBWOWVIEWERCPP_FRAMEBASEDSTACKALLOCATOR_H
