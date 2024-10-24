#include <windows.h>

#define BUFFER_SIZE 1024

int main()
{
    HANDLE pipe_read, pipe_write;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char buffer[BUFFER_SIZE];
    char filename[MAX_PATH];
    DWORD bytes_read, bytes_written;

    // Создаем pipe
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0))
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "Failed to create pipe\n", 20, &bytes_written, NULL);
        return 1;
    }

    // Читаем имя файла
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), filename, sizeof(filename), &bytes_read, NULL))
    {
        CloseHandle(pipe_read);
        CloseHandle(pipe_write);
        return 1;
    }

    // Обработка CRLF
    if (bytes_read >= 2 && filename[bytes_read - 2] == '\r' && filename[bytes_read - 1] == '\n')
    {
        filename[bytes_read - 2] = '\0'; // Убираем \r и \n
    }
    else if (bytes_read >= 1 && (filename[bytes_read - 1] == '\n' || filename[bytes_read - 1] == '\r'))
    {
        filename[bytes_read - 1] = '\0'; // Убираем одиночный символ переноса строки
    }
    else
    {
        filename[bytes_read] = '\0';
    }

    // Получаем полный путь к файлу
    char full_path[MAX_PATH];
    DWORD result = GetFullPathName(filename, MAX_PATH, full_path, NULL);
    if (result == 0 || result > MAX_PATH)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), "Failed to get full path\n", 22, &bytes_written, NULL);
        CloseHandle(pipe_read);
        CloseHandle(pipe_write);
        return 1;
    }

    // Подготавливаем структуры для создания процесса
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdInput = pipe_read;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Формируем командную строку
    char cmd_line[MAX_PATH * 2];
    wsprintf(cmd_line, "child.exe \"%s\"", full_path);

    // Получаем путь к текущей директории
    char module_dir[MAX_PATH];
    GetModuleFileName(NULL, module_dir, MAX_PATH);
    char *last_slash = strrchr(module_dir, '\\');
    if (last_slash)
        *last_slash = '\0';

    // Создаем дочерний процесс
    if (!CreateProcess(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, module_dir, &si, &pi))
    {
        DWORD error = GetLastError();
        char error_msg[256];
        wsprintf(error_msg, "Failed to create process. Error: %d\n", error);
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, lstrlen(error_msg), &bytes_written, NULL);
        CloseHandle(pipe_read);
        CloseHandle(pipe_write);
        return 1;
    }

    CloseHandle(pipe_read);
    CloseHandle(pi.hThread);

    // Читаем ввод пользователя и отправляем в pipe
    while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, BUFFER_SIZE, &bytes_read, NULL) && bytes_read > 0)
    {
        if (!WriteFile(pipe_write, buffer, bytes_read, &bytes_written, NULL))
            break;
    }

    CloseHandle(pipe_write);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    return exit_code;
}