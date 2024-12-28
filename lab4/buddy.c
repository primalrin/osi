#include "allocator.h"
#include <stddef.h>
#include <math.h>
#include <windows.h>

#define MIN_ORDER 4
#define ALIGNMENT 8

typedef struct FreeBlock
{
    struct FreeBlock *next;
} FreeBlock;

struct Allocator
{
    void *memory;
    size_t size;
    int max_order;
    FreeBlock **free_lists;
};

static void *sys_alloc(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static void sys_free(void *ptr, size_t size)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

static size_t align(size_t size)
{
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static int get_order(size_t size)
{
    int order = MIN_ORDER;
    size_t block_size = 1 << MIN_ORDER;
    while (block_size < size)
    {
        block_size <<= 1;
        order++;
    }
    return order;
}

static void split_block(Allocator *allocator, int order)
{
    int higher_order = order + 1;
    FreeBlock *block = allocator->free_lists[higher_order];
    allocator->free_lists[higher_order] = block->next;

    size_t block_size = 1 << order;
    FreeBlock *buddy = (FreeBlock *)((char *)block + block_size);

    block->next = buddy;
    buddy->next = allocator->free_lists[order];
    allocator->free_lists[order] = block;
}

Allocator *allocator_create(void *const memory, const size_t size)
{
    if (memory == NULL || size == 0)
    {
        return NULL;
    }

    Allocator *allocator = (Allocator *)memory;
    allocator->memory = (char *)memory + sizeof(Allocator);
    allocator->size = size - sizeof(Allocator);
    allocator->max_order = get_order(allocator->size);

    allocator->free_lists = (FreeBlock **)sys_alloc(sizeof(FreeBlock *) * (allocator->max_order + 1));
    if (allocator->free_lists == NULL)
    {
        return NULL;
    }

    for (int i = 0; i <= allocator->max_order; i++)
    {
        allocator->free_lists[i] = NULL;
    }

    allocator->free_lists[allocator->max_order] = (FreeBlock *)allocator->memory;
    allocator->free_lists[allocator->max_order]->next = NULL;

    return allocator;
}

void allocator_destroy(Allocator *const allocator)
{
    if (allocator != NULL && allocator->free_lists != NULL)
    {
        sys_free(allocator->free_lists, sizeof(FreeBlock *) * (allocator->max_order + 1));
    }
}

void *allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (allocator == NULL || size == 0)
    {
        return NULL;
    }

    size_t alloc_size = align(size + sizeof(size_t));
    int order = get_order(alloc_size);

    if (order > allocator->max_order)
    {
        return NULL;
    }

    if (allocator->free_lists[order] == NULL)
    {
        int i = order + 1;
        while (i <= allocator->max_order && allocator->free_lists[i] == NULL)
        {
            i++;
        }

        if (i > allocator->max_order)
        {
            return NULL;
        }

        while (i > order)
        {
            split_block(allocator, i - 1);
            i--;
        }
    }

    FreeBlock *block = allocator->free_lists[order];
    allocator->free_lists[order] = block->next;

    size_t *block_size = (size_t *)block;
    *block_size = 1 << order;

    return (char *)block + sizeof(size_t);
}

void *get_buddy(Allocator *allocator, void *block, int order)
{
    size_t block_size = 1 << order;
    size_t block_offset = (char *)block - (char *)allocator->memory;
    size_t buddy_offset = block_offset ^ block_size;
    return (char *)allocator->memory + buddy_offset;
}

void allocator_free(Allocator *const allocator, void *const memory)
{
    if (allocator == NULL || memory == NULL)
    {
        return;
    }

    size_t *block_size_ptr = (size_t *)((char *)memory - sizeof(size_t));
    size_t block_size = *block_size_ptr;
    int order = get_order(block_size);
    FreeBlock *block = (FreeBlock *)((char *)memory - sizeof(size_t));

    while (order < allocator->max_order)
    {
        FreeBlock *buddy = (FreeBlock *)get_buddy(allocator, block, order);

        FreeBlock *current = allocator->free_lists[order];
        FreeBlock *prev = NULL;
        while (current != NULL && current != buddy)
        {
            prev = current;
            current = current->next;
        }

        if (current == NULL)
        {
            break;
        }

        if (prev == NULL)
        {
            allocator->free_lists[order] = current->next;
        }
        else
        {
            prev->next = current->next;
        }

        if (buddy < block)
        {
            block = buddy;
        }

        order++;
        block_size <<= 1;
    }

    block->next = allocator->free_lists[order];
    allocator->free_lists[order] = block;
}