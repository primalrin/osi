#include <fcntl.h>
#include <stdlib.h>

int main()
{
    char filename[256];
    char buffer[256];
    float sum = 0;
    int fd;

    int bytes_read = read(0, filename, sizeof(filename));
    if (bytes_read <= 0)
        return 1;

    filename[bytes_read] = '\0';

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        return 1;
    }

    while ((bytes_read = read(0, buffer, sizeof(buffer))) > 0)
    {
        char *token;
        char *endptr;
        token = buffer;
        while (*token != '\0')
        {
            sum += strtof(token, &endptr); // Используйте strtof для float
            while (*endptr == ' ' || *endptr == '\n' || *endptr == '\t')
            {
                endptr++;
            }
            token = endptr;
        }

        char result[50];
        snprintf(result, sizeof(result), "%.6f\n", sum); // Форматированный вывод для float
        write(fd, result, strlen(result));               // Используйте snprintf
        sum = 0;
    }

    close(fd);
    return 0;
}