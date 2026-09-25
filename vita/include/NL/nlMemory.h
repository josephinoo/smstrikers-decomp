#ifndef _NLMEMORY_H_
#define _NLMEMORY_H_

#include <cstddef>
#include <new> // provides placement new(size_t, void*)

inline unsigned long KB(unsigned long size)
{
    return size << 10;
}

inline unsigned long MB(unsigned long size)
{
    return KB(KB(size));
}

void nlFree(void* ptr);
void* nlMalloc(unsigned long size, unsigned int alignment, bool atEnd);
void* nlMalloc(unsigned long size);

inline void* operator new(size_t size, unsigned int alignment, bool atEnd)
{
    return nlMalloc(size, alignment, atEnd);
}
inline void* operator new[](size_t size, unsigned int alignment, bool atEnd)
{
    return nlMalloc(size, alignment, atEnd);
}
inline void* operator new[](size_t size, unsigned int alignment, bool atEnd, const char*)
{
    return nlMalloc(size, alignment, atEnd);
}

unsigned int nlVirtualTotalFree();
unsigned int nlVirtualLargestBlock();
void nlVirtualFree(void* ptr);
void* nlVirtualAlloc(unsigned long size, bool bZero);
void nlInitMemory();

#endif // _NLMEMORY_H_
