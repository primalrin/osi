#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#define MEMORY_SIZE (1 << 20)
#define ALLOCATIONS 10000
#define MAX_ALLOC_SIZE 4096

typedef Allocator *(*allocator_create_func)(void *const memory, const size_t size);
typedef void (*allocator_destroy_func)(Allocator *const allocator);
typedef void *(*allocator_alloc_func)(Allocator *const allocator, const size_t size);
typedef void (*allocator_free_func)(Allocator *const allocator, void *const memory);

static void *sys_alloc(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static void sys_free(void *ptr, size_t size)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

long long get_current_time_ns()
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (long long)((double)count.QuadPart / freq.QuadPart * 1000000000.0);
}

int main(int argc, char *argv[])
{
    void *memory = sys_alloc(MEMORY_SIZE);
    if (memory == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    allocator_create_func create_func = NULL;
    allocator_destroy_func destroy_func = NULL;
    allocator_alloc_func alloc_func = NULL;
    allocator_free_func free_func = NULL;

    HMODULE lib = NULL;

    if (argc > 1)
    {
        lib = LoadLibraryA(argv[1]);

        if (lib == NULL)
        {
            fprintf(stderr, "Failed to load library: %s\n", argv[1]);
            fprintf(stderr, "Error code: %lu\n", GetLastError());
        }
        else
        {
            create_func = (allocator_create_func)GetProcAddress(lib, "allocator_create");
            destroy_func = (allocator_destroy_func)GetProcAddress(lib, "allocator_destroy");
            alloc_func = (allocator_alloc_func)GetProcAddress(lib, "allocator_alloc");
            free_func = (allocator_free_func)GetProcAddress(lib, "allocator_free");

            if (create_func == NULL || destroy_func == NULL || alloc_func == NULL || free_func == NULL)
            {
                fprintf(stderr, "Failed to get function addresses\n");
                FreeLibrary(lib);
                lib = NULL;
            }
        }
    }

    if (create_func == NULL)
    {
        create_func = (allocator_create_func)sys_alloc;
        destroy_func = (allocator_destroy_func)sys_free;
        alloc_func = (allocator_alloc_func)sys_alloc;
        free_func = (allocator_free_func)sys_free;
    }

    Allocator *allocator = create_func(memory, MEMORY_SIZE);
    if (allocator == NULL && lib != NULL)
    {
        fprintf(stderr, "Failed to create allocator\n");
        FreeLibrary(lib);
        sys_free(memory, MEMORY_SIZE);
        return 1;
    }

    void *allocations[ALLOCATIONS];
    size_t allocation_sizes[ALLOCATIONS];
    size_t total_allocated = 0;

    long long start_alloc = get_current_time_ns();
    for (int i = 0; i < ALLOCATIONS; i++)
    {
        size_t size = rand() % MAX_ALLOC_SIZE + 1;
        allocations[i] = alloc_func(allocator, size);
        if (allocations[i] != NULL)
        {
            allocation_sizes[i] = size;
            total_allocated += size;
        }
        else
        {
            allocation_sizes[i] = 0;
        }
    }
    long long end_alloc = get_current_time_ns();

    long long start_free = get_current_time_ns();
    for (int i = 0; i < ALLOCATIONS; i++)
    {
        if (allocations[i] != NULL)
        {
            free_func(allocator, allocations[i]);
        }
    }
    long long end_free = get_current_time_ns();

    if (allocator != NULL && create_func != (allocator_create_func)sys_alloc)
    {
        destroy_func(allocator);
    }

    if (lib != NULL)
    {
        FreeLibrary(lib);
    }

    if (create_func == (allocator_create_func)sys_alloc)
    {
        sys_free(memory, MEMORY_SIZE);
    }

    printf("Total allocated: %zu bytes\n", total_allocated);
    printf("Allocation time: %f seconds\n", (double)(end_alloc - start_alloc) / 1000000000.0);
    printf("Free time: %f seconds\n", (double)(end_free - start_free) / 1000000000.0);
    if (total_allocated > 0)
    {
        printf("Usage factor: %f\n", (double)total_allocated / MEMORY_SIZE);
    }
    else
    {
        printf("Usage factor: N/A (no successful allocations)\n");
    }

    return 0;
}