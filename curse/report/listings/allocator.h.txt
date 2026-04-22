#pragma once

#include <cstddef>

enum class AllocatorKind {
    FirstFit,
    PowerOfTwo
};

struct AllocatorStats {
    size_t managed_memory = 0;
    size_t used_memory = 0;
    size_t free_memory = 0;
    size_t largest_free_block = 0;
    size_t allocation_count = 0;
};

struct Allocator {
    AllocatorKind kind = AllocatorKind::FirstFit;
    void* state = nullptr;
};

Allocator* createFirstFitAllocator(void* realMemory, size_t memorySize);
Allocator* createPowerOfTwoAllocator(void* realMemory, size_t memorySize);
void destroyAllocator(Allocator* allocator);

void* alloc(Allocator* allocator, size_t blockSize);
void freeBlock(Allocator* allocator, void* block);

AllocatorStats getAllocatorStats(const Allocator* allocator);
size_t getAllocatedBlockFootprint(const Allocator* allocator, const void* block);
const char* getAllocatorName(const Allocator* allocator);
