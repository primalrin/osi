#include <sys/types.h>
#include <fcntl.h>

int main()
{
    int pipe1[2], pipe2[2]; // pipe2 не используется, но оставлен для симметрии
    char filename[256];
    char buffer[256];

    // Создаем pipe
    if (pipe(pipe1) == -1)
    {
        return 1;
    }

    if (pipe(pipe2) == -1)
    {
        return 1;
    }

    int pid = fork();

    if (pid == -1)
    {
        return 1;
    }
    else if (pid == 0)
    { // Дочерний процесс
        close(pipe1[1]);
        close(pipe2[0]); // Закрываем неиспользуемые концы pipe

        dup2(pipe1[0], 0); // Перенаправляем stdin

        close(pipe1[0]);
        close(pipe2[1]); // Закрываем неиспользуемые концы pipe после dup2

        execl("./child", "child", (char *)NULL);

        return 1; // Если execl не сработает
    }
    else
    { // Родительский процесс
        close(pipe1[0]);
        close(pipe2[1]); // Закрываем неиспользуемые концы pipe

        // Чтение имени файла
        int bytes_read = read(0, filename, sizeof(filename));
        if (bytes_read <= 0)
            return 1;

        filename[bytes_read] = '\0'; // Добавляем нулевой символ в конец
        int len = 0;
        while (filename[len] != '\n' && filename[len] != '\0')
            len++;
        filename[len] = '\0';

        write(pipe1[1], filename, len + 1);

        while (1)
        {
            // Чтение данных от пользователя
            bytes_read = read(0, buffer, sizeof(buffer));
            if (bytes_read <= 0)
                break;

            write(pipe1[1], buffer, bytes_read);
        }

        close(pipe1[1]);
        waitpid(pid, NULL, 0); // Ожидаем завершения дочернего процесса.
    }
    return 0;
}