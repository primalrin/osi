#include "allocator.h"
#include <stddef.h>
#include <math.h>
#include <stdint.h>

#define BLOCK_SIZE 16

struct Allocator
{
    void *memory;
    size_t size;
    size_t max_order;
    uint8_t *free_blocks;
};

size_t get_order(size_t size)
{
    return (size_t)ceil(log2(size));
}

Allocator *allocator_create(void *memory, size_t size)
{
    Allocator *allocator = (Allocator *)memory;
    allocator->memory = (uint8_t *)memory + sizeof(Allocator);
    allocator->size = size - sizeof(Allocator);
    allocator->max_order = (size_t)log2(allocator->size);
    size_t bitmap_size = (size / BLOCK_SIZE) / 8;
    if ((size / BLOCK_SIZE) % 8 != 0)
    {
        bitmap_size++;
    }

    allocator->free_blocks = (uint8_t *)allocator->memory + allocator->size - bitmap_size;
    for (size_t i = 0; i < bitmap_size; i++)
    {
        allocator->free_blocks[i] = 0xFF;
    }

    return allocator;
}

void allocator_destroy(Allocator *allocator)
{
}

void *allocator_alloc(Allocator *allocator, size_t size)
{
    size_t num_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    size_t total_blocks = allocator->size / BLOCK_SIZE;

    for (size_t i = 0; i <= total_blocks - num_blocks; i++)
    {
        int found = 1;
        for (size_t j = 0; j < num_blocks; j++)
        {
            size_t byte_index = (i + j) / 8;
            size_t bit_index = (i + j) % 8;
            if (!(allocator->free_blocks[byte_index] & (1 << bit_index)))
            {
                found = 0;
                break;
            }
        }

        if (found)
        {
            for (size_t j = 0; j < num_blocks; j++)
            {
                size_t byte_index = (i + j) / 8;
                size_t bit_index = (i + j) % 8;
                allocator->free_blocks[byte_index] &= ~(1 << bit_index);
            }
            return (uint8_t *)allocator->memory + i * BLOCK_SIZE;
        }
    }

    return NULL;
}

void allocator_free(Allocator *allocator, void *memory)
{
    if (memory == NULL)
    {
        return;
    }
    size_t offset = (uint8_t *)memory - (uint8_t *)allocator->memory;
    size_t index = offset / BLOCK_SIZE;

    if (index < (allocator->size / BLOCK_SIZE))
    {
        size_t byte_index = index / 8;
        size_t bit_index = index % 8;

        allocator->free_blocks[byte_index] |= (1 << bit_index);
    }
}