#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
    int *data;
    int length;
} Array;

typedef struct
{
    Array *arrays;
    int *result;
    int start;
    int end;
    int array_count;
    HANDLE mutex;
} ThreadData;

DWORD WINAPI ThreadFunction(LPVOID arg)
{
    ThreadData *data = (ThreadData *)arg;
    for (int i = data->start; i < data->end; ++i)
    {
        int sum = 0;
        for (int j = 0; j < data->array_count; ++j)
        {
            sum += data->arrays[j].data[i];
        }
        WaitForSingleObject(data->mutex, INFINITE);
        data->result[i] = sum;
        ReleaseMutex(data->mutex);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {

        MessageBoxW(NULL, L"Not enough arguments", L"Error", MB_OK);
        return 1;
    }

    int max_threads = atoi(argv[1]);
    int array_count = atoi(argv[2]);
    int array_length = 1000;

    Array *arrays = (Array *)malloc(array_count * sizeof(Array));
    for (int i = 0; i < array_count; ++i)
    {
        arrays[i].data = (int *)malloc(array_length * sizeof(int));
        arrays[i].length = array_length;

        for (int j = 0; j < array_length; ++j)
        {
            arrays[i].data[j] = rand() % 100;
        }
    }

    int *result = (int *)malloc(array_length * sizeof(int));

    HANDLE mutex = CreateMutex(NULL, FALSE, NULL);

    LARGE_INTEGER frequency;
    LARGE_INTEGER start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    HANDLE *threads = (HANDLE *)malloc(max_threads * sizeof(HANDLE));
    ThreadData *threadData = (ThreadData *)malloc(max_threads * sizeof(ThreadData));

    int chunk_size = array_length / max_threads;
    for (int i = 0; i < max_threads; ++i)
    {
        threadData[i].arrays = arrays;
        threadData[i].result = result;
        threadData[i].start = i * chunk_size;
        threadData[i].end = (i == max_threads - 1) ? array_length : (i + 1) * chunk_size;
        threadData[i].array_count = array_count;
        threadData[i].mutex = mutex;
        threads[i] = CreateThread(NULL, 0, ThreadFunction, &threadData[i], 0, NULL);
        if (threads[i] == NULL)
        {
            MessageBoxW(NULL, L"Could not create thread", L"Error", MB_OK);

            free(threads);
            free(threadData);

            return 1;
        }
    }

    WaitForMultipleObjects(max_threads, threads, TRUE, INFINITE);

    QueryPerformanceCounter(&end);
    double elapsed_time = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;

    wchar_t message[256];
    swprintf_s(message, 256, L"Elapsed time: %f seconds", elapsed_time);
    MessageBoxW(NULL, message, L"Time", MB_OK);

    for (int i = 0; i < array_count; ++i)
    {
        free(arrays[i].data);
    }
    free(arrays);
    free(result);
    free(threads);
    free(threadData);
    CloseHandle(mutex);

    return 0;
}