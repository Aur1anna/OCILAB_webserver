#pragma once
#include "./MemoryPool/ThreadCache.h"

namespace My_memoryPool
{

class MemoryPool
{
public:
    static void* allocate(size_t size)
    {
        return ThreadCache::getInstance()->allocate(size);
    }

    static void deallocate(void* ptr, size_t size)
    {
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
};

} // namespace My_memoryPool


/*
// 全局 operator new/delete 重载
inline void* operator new(size_t size) {
    return My_memoryPool::MemoryPool::allocate(size);
}

inline void operator delete(void* ptr) noexcept {
    My_memoryPool::MemoryPool::deallocate(ptr, 0);
}

inline void operator delete[](void* ptr) noexcept {
    My_memoryPool::MemoryPool::deallocate(ptr, 0);
}


*/