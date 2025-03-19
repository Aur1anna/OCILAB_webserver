#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace My_memoryPool
{

void* PageCache::allocateSpan(size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找合适的空闲span
    // lower_bound函数返回第一个大于等于numPages的元素的迭代器
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end())
    {
        Span* span = it->second;

        // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
        if (span->next)
        {
            freeSpans_[it->first] = span->next; //如果有下一个，就把下一个设为freeSpans_的second值，也就是此页数的头节点
        }
        else
        {
            freeSpans_.erase(it);   //如果没有下一个，也就是拿走这个就没有这个页数的span了，就从freeSpans_中删除此页数的span
        }

        // 如果找到的span大于需要的numPages则进行分割
        if (span->numPages > numPages) 
        {
            //多余span部分命名为newSpan,初始化其三个参数
            Span* newSpan = ::new Span;   
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            // 将超出部分放回对应span下的空闲Span*链，具体放在另一个newSpan->numPages对应节点的前面，然后保存新的freeSpans_头节点。
            //如果map里本没有这个span，则自动创建freeSpans_[newSpan->numPages]为nullptr，然后next=nullptr，这一步相当于没有，然后再头节点设为newSpan自己.
                    //这一步相当于一段代码两用巧妙是巧妙，但是不是自己写的人很难读得懂，其实应该做一个if判断是否map里已经存在这个newspan大小的链表了。
            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan; //这里是引用，等同于freeSpans_[newSpan->numPages] = newSpan；

            span->numPages = numPages;
        }

        // 记录span信息用于回收
        spanMap_[span->pageAddr] = span;
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请
    void* memory = systemAlloc(numPages);
    if (!memory) return nullptr;

    // 创建新的span
    Span* span = ::new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    // 记录span信息用于回收
    spanMap_[memory] = span;
    return memory;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) return;

    Span* span = it->second;

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);
    
    if (nextIt != spanMap_.end())
    {
        Span* nextSpan = nextIt->second;
        
        // 1. 首先检查nextSpan是否在空闲链表中
        bool found = false;
        auto& nextList = freeSpans_[nextSpan->numPages];
        
        // 检查是否是头节点
        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            found = true;
        }
        else if (nextList) // 只有在链表非空时才遍历
        {
            Span* prev = nextList;
            while (prev->next)
            {
                if (prev->next == nextSpan)
                {   
                    // 将nextSpan从空闲链表中移除
                    prev->next = nextSpan->next;
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }

        // 2. 只有在找到nextSpan的情况下才进行合并
        if (found)
        {
            // 合并span
            span->numPages += nextSpan->numPages;
            spanMap_.erase(nextAddr);
            delete nextSpan;
        }
    }

    // 将合并后的span通过头插法插入空闲列表
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;
}

void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;

    // 使用mmap分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;

    // 内存区域清零
    memset(ptr, 0, size);
    return ptr;
}

} // namespace My_memoryPool