#include "allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define PAGE_SIZE 4096
#define MAX_ORDER 12

typedef struct Page
{
    union
    {
        struct Page *next_free;
        size_t block_size;
    };
} Page;

typedef struct FreeBlock
{
    struct FreeBlock *next;
} FreeBlock;

struct Allocator
{
    size_t num_pages;
    Page *pages;
    FreeBlock *free_lists[MAX_ORDER + 1];
    Page *free_pages;
};

static size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

static int get_order(size_t size)
{
    for (int order = 0; order <= MAX_ORDER; ++order)
    {
        if ((1UL << order) >= size)
        {
            return order;
        }
    }
    return -1;
}

Allocator *allocator_create(void *const memory, const size_t size)
{
    if (size < sizeof(Allocator))
        return NULL;
    Allocator *allocator = (Allocator *)memory;
    allocator->num_pages = size / PAGE_SIZE;
    allocator->pages = (Page *)((char *)memory + sizeof(Allocator));
    allocator->free_pages = NULL;

    for (size_t i = 0; i < allocator->num_pages; ++i)
    {
        allocator->pages[i].next_free = allocator->free_pages;
        allocator->free_pages = &allocator->pages[i];
    }

    for (int i = 0; i <= MAX_ORDER; ++i)
    {
        allocator->free_lists[i] = NULL;
    }

    return allocator;
}

void allocator_destroy(Allocator *const allocator)
{
}

void *allocator_alloc(Allocator *const allocator, const size_t size)
{
    if (size == 0 || size > (1UL << MAX_ORDER))
        return NULL;

    size_t aligned_size = align_up(size, sizeof(void *));
    int order = get_order(aligned_size);
    if (order == -1)
        return NULL;

    if (allocator->free_lists[order] != NULL)
    {
        FreeBlock *block = allocator->free_lists[order];
        allocator->free_lists[order] = block->next;
        return block;
    }

    if (allocator->free_pages == NULL)
    {
        return NULL;
    }

    Page *free_page = allocator->free_pages;
    allocator->free_pages = free_page->next_free;

    size_t block_size = 1UL << order;
    free_page->block_size = block_size;
    size_t num_blocks = PAGE_SIZE / block_size;
    char *page_start = (char *)free_page;

    for (size_t i = 0; i < num_blocks; ++i)
    {
        FreeBlock *block = (FreeBlock *)(page_start + i * block_size);
        if (i == 0)
        {
            allocator->free_lists[order] = block;
        }
        if (i < num_blocks - 1)
        {
            block->next = (FreeBlock *)(page_start + (i + 1) * block_size);
        }
        else
        {
            block->next = NULL;
        }
    }

    FreeBlock *block = allocator->free_lists[order];
    allocator->free_lists[order] = block->next;
    return block;
}

void allocator_free(Allocator *const allocator, void *const memory)
{
    if (memory == NULL)
        return;
    Page *page = NULL;
    for (size_t i = 0; i < allocator->num_pages; ++i)
    {
        char *page_start = (char *)&allocator->pages[i];
        char *page_end = page_start + PAGE_SIZE;
        if (memory >= (void *)page_start && memory < (void *)page_end)
        {
            page = &allocator->pages[i];
            break;
        }
    }

    if (page == NULL || page->block_size == 0)
    {
        return;
    }

    int order = get_order(page->block_size);
    if (order == -1)
        return;
    FreeBlock *block_to_free = (FreeBlock *)memory;
    block_to_free->next = allocator->free_lists[order];
    allocator->free_lists[order] = block_to_free;

    size_t block_size = 1UL << order;
    size_t num_blocks = PAGE_SIZE / block_size;
    char *page_start = (char *)page;
    int is_page_free = 1;
    for (size_t i = 0; i < num_blocks; ++i)
    {
        FreeBlock *block = allocator->free_lists[order];
        while (block != NULL)
        {
            if ((void *)block == (void *)(page_start + i * block_size))
            {
                break;
            }
            block = block->next;
        }
        if (block == NULL)
        {
            is_page_free = 0;
            break;
        }
    }

    if (is_page_free)
    {

        FreeBlock **head = &allocator->free_lists[order];
        while (*head != NULL)
        {
            if (*head >= (FreeBlock *)page_start && *head < (FreeBlock *)(page_start + PAGE_SIZE))
            {
                *head = (*head)->next;
            }
            else
            {
                head = &(*head)->next;
            }
        }

        page->next_free = allocator->free_pages;
        allocator->free_pages = page;
        page->block_size = 0;
    }
}