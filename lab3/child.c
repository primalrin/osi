#include <windows.h>

#define BUFFER_SIZE 1024
#define SHARED_MEM_SIZE (BUFFER_SIZE * sizeof(char))
#define SHARED_MEM_NAME "Local\\SharedMemBuffer"
#define READ_SEMAPHORE_NAME "Local\\ReadSemaphore"
#define WRITE_SEMAPHORE_NAME "Local\\WriteSemaphore"

float string_to_float(const char *str)
{
    float result = 0;
    float fraction = 0;
    float div = 1;
    int negative = 0;

    while (*str == ' ')
        str++;

    if (*str == '-')
    {
        negative = 1;
        str++;
    }

    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    if (*str == '.')
    {
        str++;
        while (*str >= '0' && *str <= '9')
        {
            div *= 10;
            fraction = fraction * 10 + (*str - '0');
            str++;
        }
    }

    result += fraction / div;
    return negative ? -result : result;
}

void float_to_string(float num, char *str)
{
    int integer_part = (int)num;
    float decimal_part = num - integer_part;
    int idx = 0;
    int temp;

    if (num < 0)
    {
        str[idx++] = '-';
        integer_part = -integer_part;
        decimal_part = -decimal_part;
    }

    temp = integer_part;
    int start_idx = idx;
    do
    {
        str[idx++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);

    int end_idx = idx - 1;
    while (start_idx < end_idx)
    {
        char t = str[start_idx];
        str[start_idx] = str[end_idx];
        str[end_idx] = t;
        start_idx++;
        end_idx--;
    }

    str[idx++] = '.';
    decimal_part *= 100;
    temp = (int)decimal_part;
    str[idx++] = '0' + (temp / 10);
    str[idx++] = '0' + (temp % 10);
    str[idx++] = '\n';
    str[idx] = '\0';
}

BOOL validate_and_fix_path(char *path)
{
    int len = lstrlen(path);
    if (len > 0)
    {
        if (path[0] == '"')
        {
            memmove(path, path + 1, len);
            len--;
        }
        if (len > 0 && path[len - 1] == '"')
        {
            path[len - 1] = '\0';
            len--;
        }
    }

    if (len == 0 || len >= MAX_PATH)
        return FALSE;

    for (int i = 0; i < len; i++)
    {
        if (path[i] < 32 || strchr("<>|?*\"", path[i]))
            return FALSE;
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    char error_msg[256];
    DWORD bytes_written;
    HANDLE hMapFile;
    LPVOID pBuf;
    HANDLE hReadSemaphore, hWriteSemaphore;

    if (argc != 2)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 17, &bytes_written, NULL);
        return 1;
    }

    char file_path[MAX_PATH];
    lstrcpy(file_path, argv[1]);

    if (!validate_and_fix_path(file_path))
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 17, &bytes_written, NULL);
        return 1;
    }

    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        SHARED_MEM_NAME);

    if (hMapFile == NULL)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 27, &bytes_written, NULL);
        return 1;
    }

    pBuf = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        SHARED_MEM_SIZE);

    if (pBuf == NULL)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 26, &bytes_written, NULL);
        CloseHandle(hMapFile);
        return 1;
    }

    hReadSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, READ_SEMAPHORE_NAME);
    hWriteSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, WRITE_SEMAPHORE_NAME);

    if (hReadSemaphore == NULL || hWriteSemaphore == NULL)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 25, &bytes_written, NULL);
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        if (hReadSemaphore)
            CloseHandle(hReadSemaphore);
        if (hWriteSemaphore)
            CloseHandle(hWriteSemaphore);
        return 1;
    }

    HANDLE output_file = CreateFile(
        file_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (output_file == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, lstrlen(error_msg), &bytes_written, NULL);
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        CloseHandle(hReadSemaphore);
        CloseHandle(hWriteSemaphore);
        return 1;
    }

    char num_str[32];
    float sum;
    int num_start = 0;
    BOOL running = TRUE;
    char debug_msg[256];

    while (running)
    {
        WaitForSingleObject(hReadSemaphore, INFINITE);

        DWORD bytes_read = *((DWORD *)((char *)pBuf + BUFFER_SIZE - sizeof(DWORD)));

        WriteFile(GetStdHandle(STD_ERROR_HANDLE), debug_msg, lstrlen(debug_msg), &bytes_written, NULL);

        if (bytes_read == 0)
        {
            WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 19, &bytes_written, NULL);
            running = FALSE;
        }
        else
        {
            sum = 0;
            num_start = 0;
            char *buffer = (char *)pBuf;

            for (int i = 0; i < bytes_read; i++)
            {
                if (buffer[i] == ' ' || buffer[i] == '\n')
                {
                    if (i > num_start)
                    {
                        int len = i - num_start;
                        if (len < sizeof(num_str))
                        {
                            memcpy(num_str, buffer + num_start, len);
                            num_str[len] = '\0';

                            float val = string_to_float(num_str);
                            sum += val;

                            WriteFile(GetStdHandle(STD_ERROR_HANDLE), debug_msg, lstrlen(debug_msg), &bytes_written, NULL);
                        }
                    }
                    num_start = i + 1;

                    if (buffer[i] == '\n')
                    {
                        char result[64];
                        float_to_string(sum, result);

                        WriteFile(GetStdHandle(STD_ERROR_HANDLE), debug_msg, lstrlen(debug_msg), &bytes_written, NULL);

                        if (!WriteFile(output_file, result, lstrlen(result), &bytes_written, NULL))
                        {
                            WriteFile(GetStdHandle(STD_ERROR_HANDLE), "", 22, &bytes_written, NULL);
                            running = FALSE;
                            break;
                        }
                        sum = 0;
                    }
                }
            }
        }

        ReleaseSemaphore(hWriteSemaphore, 1, NULL);
    }

    CloseHandle(output_file);
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hReadSemaphore);
    CloseHandle(hWriteSemaphore);

    return 0;
}