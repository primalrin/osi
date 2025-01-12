#include "allocator.h"
#include <stddef.h>
#include <windows.h>

#define MIN_ORDER 3
#define MAX_ORDER 20

typedef struct Block
{
    unsigned char order;
    unsigned char is_free;
    struct Block *next;
    struct Block *prev;
} Block;

struct Allocator
{
    void *memory;
    size_t size;
    Block *free_lists[MAX_ORDER - MIN_ORDER + 1];
};

static size_t align(size_t size)
{
    return (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
}

static size_t get_block_size(int order)
{
    return 1 << order;
}

static int get_order(size_t size)
{
    int order = MIN_ORDER;
    size_t block_size = get_block_size(order);
    while (block_size < size)
    {
        order++;
        block_size = get_block_size(order);
    }
    return order;
}

static Block *get_buddy(Allocator *allocator, Block *block)
{
    size_t block_size = get_block_size(block->order);
    size_t offset = (char *)block - (char *)allocator->memory;
    size_t buddy_offset = offset ^ block_size;
    return (Block *)((char *)allocator->memory + buddy_offset);
}

static void add_to_free_list(Allocator *allocator, Block *block)
{
    int order = block->order;
    block->is_free = 1;
    block->next = allocator->free_lists[order - MIN_ORDER];
    block->prev = NULL;
    if (allocator->free_lists[order - MIN_ORDER] != NULL)
    {
        allocator->free_lists[order - MIN_ORDER]->prev = block;
    }
    allocator->free_lists[order - MIN_ORDER] = block;
}

static void remove_from_free_list(Allocator *allocator, Block *block)
{
    int order = block->order;
    if (block->prev != NULL)
    {
        block->prev->next = block->next;
    }
    else
    {
        allocator->free_lists[order - MIN_ORDER] = block->next;
    }
    if (block->next != NULL)
    {
        block->next->prev = block->prev;
    }
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

    for (int i = 0; i <= MAX_ORDER - MIN_ORDER; i++)
    {
        allocator->free_lists[i] = NULL;
    }

    int order = MAX_ORDER;
    size_t block_size = get_block_size(order);
    while (block_size > allocator->size && order > MIN_ORDER)
    {
        order--;
        block_size = get_block_size(order);
    }

    Block *initial_block = (Block *)allocator->memory;
    initial_block->order = order;
    add_to_free_list(allocator, initial_block);

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

    size_t alloc_size = align(size + sizeof(Block));
    int order = get_order(alloc_size);

    if (order > MAX_ORDER)
    {
        return NULL;
    }

    for (int i = order; i <= MAX_ORDER; i++)
    {
        if (allocator->free_lists[i - MIN_ORDER] != NULL)
        {
            Block *block = allocator->free_lists[i - MIN_ORDER];
            remove_from_free_list(allocator, block);

            while (block->order > order)
            {
                block->order--;
                Block *buddy = get_buddy(allocator, block);
                buddy->order = block->order;
                add_to_free_list(allocator, buddy);
            }

            block->is_free = 0;
            return (char *)block + sizeof(Block);
        }
    }

    return NULL;
}

void allocator_free(Allocator *const allocator, void *const memory)
{
    if (allocator == NULL || memory == NULL)
    {
        return;
    }

    Block *block = (Block *)((char *)memory - sizeof(Block));
    block->is_free = 1;

    while (block->order < MAX_ORDER)
    {
        Block *buddy = get_buddy(allocator, block);
        if (!buddy->is_free || buddy->order != block->order)
        {
            break;
        }

        remove_from_free_list(allocator, buddy);

        if (block > buddy)
        {
            Block *temp = block;
            block = buddy;
            buddy = temp;
        }

        block->order++;
    }

    add_to_free_list(allocator, block);
}