#include "allocator.h"
#include <stddef.h>
#include <windows.h>

#define MIN_ALLOCATION_SIZE 16
#define ALIGNMENT 8

typedef struct FreeBlock
{
    size_t size;
    struct FreeBlock *next;
} FreeBlock;

struct Allocator
{
    void *memory;
    size_t size;
    FreeBlock *free_list;
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

Allocator *allocator_create(void *const memory, const size_t size)
{
    if (memory == NULL || size == 0)
    {
        return NULL;
    }

    Allocator *allocator = (Allocator *)memory;
    allocator->memory = (char *)memory + sizeof(Allocator);
    allocator->size = size - sizeof(Allocator);
    allocator->free_list = (FreeBlock *)allocator->memory;
    allocator->free_list->size = allocator->size;
    allocator->free_list->next = NULL;

    return allocator;
}

void allocator_destroy(Allocator *const allocator)
{
}

void *allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (allocator == NULL || size == 0)
    {
        return NULL;
    }

    size_t alloc_size = align(size + sizeof(size_t));

    FreeBlock *current = allocator->free_list;
    FreeBlock *prev = NULL;

    while (current != NULL)
    {
        if (current->size >= alloc_size)
        {

            if (current->size >= alloc_size + sizeof(FreeBlock) + MIN_ALLOCATION_SIZE)
            {
                FreeBlock *new_block = (FreeBlock *)((char *)current + alloc_size);
                new_block->size = current->size - alloc_size;
                new_block->next = current->next;

                if (prev == NULL)
                {
                    allocator->free_list = new_block;
                }
                else
                {
                    prev->next = new_block;
                }
                current->size = alloc_size;
            }
            else
            {
                if (prev == NULL)
                {
                    allocator->free_list = current->next;
                }
                else
                {
                    prev->next = current->next;
                }
            }

            size_t *block_size = (size_t *)current;
            *block_size = current->size - sizeof(size_t);
            return (char *)current + sizeof(size_t);
        }

        prev = current;
        current = current->next;
    }

    return NULL;
}

void allocator_free(Allocator *const allocator, void *const memory)
{
    if (allocator == NULL || memory == NULL)
    {
        return;
    }

    size_t *block_size = (size_t *)((char *)memory - sizeof(size_t));
    FreeBlock *freed_block = (FreeBlock *)((char *)memory - sizeof(size_t));
    freed_block->size = *block_size + sizeof(size_t);

    FreeBlock *current = allocator->free_list;
    FreeBlock *prev = NULL;

    while (current != NULL && current < freed_block)
    {
        prev = current;
        current = current->next;
    }

    if (prev == NULL)
    {
        freed_block->next = allocator->free_list;
        allocator->free_list = freed_block;
    }
    else
    {
        freed_block->next = prev->next;
        prev->next = freed_block;
    }

    current = allocator->free_list;
    prev = NULL;
    while (current != NULL)
    {
        if (prev != NULL && (char *)prev + prev->size == (char *)current)
        {
            prev->size += current->size;
            prev->next = current->next;
            current = prev->next;
        }
        else
        {
            prev = current;
            current = current->next;
        }
    }
}