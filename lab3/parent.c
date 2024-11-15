
#include <windows.h>

#define BUFFER_SIZE 1024
#define SHARED_MEM_SIZE (BUFFER_SIZE * sizeof(char))
#define SHARED_MEM_NAME "Local\\SharedMemBuffer"
#define READ_SEMAPHORE_NAME "Local\\ReadSemaphore"
#define WRITE_SEMAPHORE_NAME "Local\\WriteSemaphore"

int main()
{
    char error_msg[256];
    HANDLE hMapFile;
    LPVOID pBuf;
    HANDLE hReadSemaphore, hWriteSemaphore;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char buffer[BUFFER_SIZE];
    char filename[MAX_PATH];
    DWORD bytes_read, bytes_written;

    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), filename, sizeof(filename), &bytes_read, NULL))
        return 1;

    if (bytes_read >= 2 && filename[bytes_read - 2] == '\r' && filename[bytes_read - 1] == '\n')
        filename[bytes_read - 2] = '\0';
    else if (bytes_read >= 1 && (filename[bytes_read - 1] == '\n' || filename[bytes_read - 1] == '\r'))
        filename[bytes_read - 1] = '\0';
    else
        filename[bytes_read] = '\0';

    char full_path[MAX_PATH];
    DWORD result = GetFullPathName(filename, MAX_PATH, full_path, NULL);
    if (result == 0 || result > MAX_PATH)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 22, &bytes_written, NULL);
        return 1;
    }

    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        SHARED_MEM_SIZE,
        SHARED_MEM_NAME);

    if (hMapFile == NULL)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 29, &bytes_written, NULL);
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

    hReadSemaphore = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE_NAME);
    hWriteSemaphore = CreateSemaphore(NULL, 1, 1, WRITE_SEMAPHORE_NAME);

    if (hReadSemaphore == NULL || hWriteSemaphore == NULL)
    {
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, 27, &bytes_written, NULL);
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        if (hReadSemaphore)
            CloseHandle(hReadSemaphore);
        if (hWriteSemaphore)
            CloseHandle(hWriteSemaphore);
        return 1;
    }

    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    char cmd_line[MAX_PATH * 2];
    wsprintf(cmd_line, "child.exe \"%s\"", full_path);

    char module_dir[MAX_PATH];
    GetModuleFileName(NULL, module_dir, MAX_PATH);
    char *last_slash = strrchr(module_dir, '\\');
    if (last_slash)
        *last_slash = '\0';

    if (!CreateProcess(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, module_dir, &si, &pi))
    {
        DWORD error = GetLastError();
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), error_msg, lstrlen(error_msg), &bytes_written, NULL);
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        CloseHandle(hReadSemaphore);
        CloseHandle(hWriteSemaphore);
        return 1;
    }

    CloseHandle(pi.hThread);

    while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, BUFFER_SIZE - sizeof(DWORD), &bytes_read, NULL) && bytes_read > 0)
    {

        WaitForSingleObject(hWriteSemaphore, INFINITE);

        memcpy(pBuf, buffer, bytes_read);
        *((DWORD *)((char *)pBuf + BUFFER_SIZE - sizeof(DWORD))) = bytes_read;

        ReleaseSemaphore(hReadSemaphore, 1, NULL);
    }

    WaitForSingleObject(hWriteSemaphore, INFINITE);
    *((DWORD *)((char *)pBuf + BUFFER_SIZE - sizeof(DWORD))) = 0;
    ReleaseSemaphore(hReadSemaphore, 1, NULL);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hReadSemaphore);
    CloseHandle(hWriteSemaphore);

    return exit_code;
}