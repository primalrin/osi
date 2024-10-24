#include <windows.h>

#define BUFFER_SIZE 1024

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

// Функция для проверки и исправления пути
BOOL validate_and_fix_path(char *path)
{
    // Удаляем начальные и конечные кавычки
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

    // Проверяем базовую валидность пути
    if (len == 0 || len >= MAX_PATH)
        return FALSE;

    // Проверяем на недопустимые символы
    for (int i = 0; i < len; i++)
    {
        if (path[i] < 32 || strchr("<>|?*\"", path[i]))
            return FALSE;
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    DWORD bytes_written;

    if (argc != 2)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "Invalid arguments\n", 17, &bytes_written, NULL);
        return 1;
    }

    char file_path[MAX_PATH];
    lstrcpy(file_path, argv[1]);

    // Проверяем и исправляем путь
    if (!validate_and_fix_path(file_path))
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "Invalid file path\n", 17, &bytes_written, NULL);
        return 1;
    }

    // Создаем или перезаписываем файл
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
        char error_msg[256];
        wsprintf(error_msg, "Failed to create file '%s'. Error: %d\n", file_path, error);
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, lstrlen(error_msg), &bytes_written, NULL);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    char num_str[32];
    float sum;
    int num_start = 0;
    DWORD bytes_read;

    while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, BUFFER_SIZE, &bytes_read, NULL) && bytes_read > 0)
    {
        sum = 0;
        num_start = 0;

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
                        sum += string_to_float(num_str);
                    }
                }
                num_start = i + 1;

                if (buffer[i] == '\n')
                {
                    char result[64];
                    float_to_string(sum, result);
                    if (!WriteFile(output_file, result, lstrlen(result), &bytes_written, NULL))
                    {
                        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "Failed to write to file\n", 22, &bytes_written, NULL);
                        CloseHandle(output_file);
                        return 1;
                    }
                    sum = 0;
                }
            }
        }
    }

    CloseHandle(output_file);
    return 0;
}