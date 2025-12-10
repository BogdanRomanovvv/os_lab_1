#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#define BUF_CHUNK 1024

/* write_all: безопасная обёртка для write (пишет все байты) */
static ssize_t write_all(int fd, const void *buf, size_t count)
{
    const char *p = buf; // Указатель на текущую позицию в буфере
    size_t left = count; // Количество байт, которые ещё нужно записать
    while (left > 0)     // Цикл продолжается, пока есть данные для записи
    {
        ssize_t w = write(fd, p, left); // Попытка записать оставшиеся байты
        if (w < 0)                      // Если write вернул ошибку
        {
            if (errno == EINTR) // Если вызов был прерван сигналом
                continue;       // Повторяем попытку
            return -1;          // Иначе возвращаем ошибку
        }
        left -= (size_t)w; // Уменьшаем счётчик оставшихся байт
        p += w;            // Сдвигаем указатель на записанные байты
    }
    return (ssize_t)count; // Возвращаем количество записанных байт
}

/* read_line_from_fd: читает из fd до '\n' (включительно) или до EOF.
   Возвращает длину прочитанных байт и помещает pointer в *out (malloc — нужно free).
   Возвращает 0 и *out=NULL на EOF (если ничего не прочитано).
   Возвращает -1 при ошибке. */
static ssize_t read_line_from_fd(int fd, char **out)
{
    size_t cap = BUF_CHUNK;  // Начальная ёмкость буфера
    size_t len = 0;          // Текущая длина прочитанных данных
    char *buf = malloc(cap); // Выделяем память для буфера
    if (!buf)                // Если не удалось выделить память
        return -1;           // Возвращаем ошибку
    char tmp[BUF_CHUNK];     // Временный буфер для чтения

    while (1) // Бесконечный цикл чтения
    {
        ssize_t r = read(fd, tmp, sizeof(tmp)); // Читаем порцию данных
        if (r < 0)                              // Если произошла ошибка чтения
        {
            if (errno == EINTR) // Если чтение прервано сигналом
                continue;       // Повторяем попытку
            free(buf);          // Освобождаем память
            return -1;          // Возвращаем ошибку
        }
        else if (r == 0) // Если достигнут конец файла (EOF)
        {
            if (len == 0) // Если ничего не было прочитано
            {
                free(buf);   // Освобождаем память
                *out = NULL; // Устанавливаем выходной указатель в NULL
                return 0;    // Возвращаем 0 (EOF)
            }
            *out = buf;          // Иначе возвращаем прочитанные данные
            return (ssize_t)len; // Возвращаем длину
        }
        else // Если данные успешно прочитаны
        {
            for (ssize_t i = 0; i < r; ++i) // Обрабатываем каждый прочитанный байт
            {
                if (len + 1 >= cap) // Если буфер заполнен
                {
                    cap *= 2;                     // Удваиваем размер буфера
                    char *nb = realloc(buf, cap); // Перевыделяем память
                    if (!nb)                      // Если не удалось перевыделить
                    {
                        free(buf); // Освобождаем старый буфер
                        return -1; // Возвращаем ошибку
                    }
                    buf = nb; // Обновляем указатель на буфер
                }
                buf[len++] = tmp[i]; // Копируем байт в буфер
                if (tmp[i] == '\n')  // Если встретили символ новой строки
                {
                    *out = buf;          // Возвращаем буфер
                    return (ssize_t)len; // Возвращаем длину строки
                }
            }
        }
    }
}

/* eprint: вывод ошибки в stderr через write */
static void eprint(const char *s)
{
    if (s)                          // Если строка не NULL
        write_all(2, s, strlen(s)); // Выводим её в stderr (файловый дескриптор 2)
}

int main(void)
{
    /* Запрашиваем имя файла для первого дочернего процесса */
    write_all(1, "Enter filename for child1: ", 27); // Выводим приглашение в stdout
    char *filename1 = NULL;                          // Указатель для имени файла
    ssize_t len1 = read_line_from_fd(0, &filename1); // Читаем строку из stdin (fd=0)
    if (len1 <= 0)                                   // Если не удалось прочитать
    {
        eprint("Failed to read filename for child1\n"); // Выводим ошибку
        return 1;                                       // Завершаем программу с ошибкой
    }
    if (filename1[len1 - 1] == '\n') // Если в конце есть символ новой строки
        filename1[len1 - 1] = '\0';  // Заменяем его на нулевой терминатор

    /* Запрашиваем имя файла для второго дочернего процесса */
    write_all(1, "Enter filename for child2: ", 27); // Выводим приглашение
    char *filename2 = NULL;                          // Указатель для второго имени
    ssize_t len2 = read_line_from_fd(0, &filename2); // Читаем вторую строку
    if (len2 <= 0)                                   // Если не удалось прочитать
    {
        eprint("Failed to read filename for child2\n"); // Выводим ошибку
        free(filename1);                                // Освобождаем первое имя
        return 1;                                       // Завершаем программу
    }
    if (filename2[len2 - 1] == '\n') // Убираем символ новой строки
        filename2[len2 - 1] = '\0';

    /* Создаём два канала (pipe) для межпроцессного взаимодействия */
    int pipe1[2], pipe2[2]; // pipe1[0] - чтение, pipe1[1] - запись

    if (pipe(pipe1) < 0) // Создаём первый канал
    {
        eprint("pipe1 failed\n"); // Если не удалось создать канал
        free(filename1);          // Освобождаем память
        free(filename2);
        return 1; // Завершаем программу
    }
    if (pipe(pipe2) < 0) // Создаём второй канал
    {
        eprint("pipe2 failed\n");
        free(filename1);
        free(filename2);
        return 1;
    }

    /* Создаём первый дочерний процесс */
    pid_t pid1 = fork(); // fork() создаёт копию процесса
    if (pid1 < 0)        // Если fork() вернул ошибку
    {
        eprint("fork child1 failed\n");
        free(filename1);
        free(filename2);
        return 1;
    }

    if (pid1 == 0) // Код выполняется только в дочернем процессе child1
    {
        /* Закрываем ненужные дескрипторы */
        close(pipe1[1]); // Закрываем конец записи первого канала (child1 только читает)
        close(pipe2[0]); // Закрываем второй канал (child1 его не использует)
        close(pipe2[1]);

        /* Перенаправляем stdin на чтение из pipe1 */
        if (dup2(pipe1[0], 0) == -1) // Делаем pipe1[0] новым stdin (fd=0)
        {
            eprint("dup2 child1 failed\n");
            _exit(1); // _exit() не вызывает обработчики atexit()
        }
        close(pipe1[0]); // Закрываем старый дескриптор (теперь он дублирован в stdin)

        /* Заменяем текущий процесс на программу child1 */
        execl("./child1", "child1", filename1, (char *)NULL); // Передаём имя файла как аргумент
        /* Если execl() вернул управление, значит произошла ошибка */
        eprint("execl child1 failed\n");
        _exit(1); // Завершаем дочерний процесс
    }

    /* Создаём второй дочерний процесс */
    pid_t pid2 = fork(); // Снова создаём копию родительского процесса
    if (pid2 < 0)        // Проверяем на ошибку
    {
        eprint("fork child2 failed\n");
        free(filename1);
        free(filename2);
        return 1;
    }

    if (pid2 == 0) // Код выполняется только в дочернем процессе child2
    {
        /* Закрываем ненужные дескрипторы */
        close(pipe2[1]); // Закрываем конец записи второго канала
        close(pipe1[0]); // Закрываем первый канал (child2 его не использует)
        close(pipe1[1]);

        /* Перенаправляем stdin на чтение из pipe2 */
        if (dup2(pipe2[0], 0) == -1) // Делаем pipe2[0] новым stdin
        {
            eprint("dup2 child2 failed\n");
            _exit(1);
        }
        close(pipe2[0]); // Закрываем старый дескриптор

        /* Заменяем процесс на программу child2 */
        execl("./child2", "child2", filename2, (char *)NULL); // Передаём имя файла
        eprint("execl child2 failed\n");
        _exit(1);
    }

    /* Родительский процесс: закрываем концы чтения каналов */
    close(pipe1[0]); // Родитель не будет читать из pipe1
    close(pipe2[0]); // Родитель не будет читать из pipe2

    /* Выводим приглашение для ввода строк */
    write_all(1, "Enter lines (Ctrl+D to finish):\n", 33);

    int line_no = 0; // Счётчик строк (для распределения между процессами)
    while (1)        // Бесконечный цикл чтения строк
    {
        char *line = NULL;                        // Указатель для очередной строки
        ssize_t rl = read_line_from_fd(0, &line); // Читаем строку из stdin
        if (rl < 0)                               // Если произошла ошибка чтения
        {
            eprint("Error reading line\n");
            break; // Выходим из цикла
        }
        if (rl == 0) // Если достигнут конец ввода (EOF, Ctrl+D)
        {
            free(line); // Освобождаем память
            break;      // Выходим из цикла
        }

        line_no++; // Увеличиваем номер строки
        /* Определяем, в какой канал отправить строку */
        int dest = (line_no % 2 == 1) ? pipe1[1] : pipe2[1]; // Нечётные в pipe1, чётные в pipe2
        if (write_all(dest, line, (size_t)rl) < 0)           // Отправляем строку в канал
        {
            eprint("Error writing to pipe\n");
            free(line);
            break;
        }
        free(line); // Освобождаем память после отправки
    }

    /* Закрываем концы записи каналов */
    close(pipe1[1]); // Сигнализируем child1, что больше данных не будет
    close(pipe2[1]); // Сигнализируем child2, что больше данных не будет

    /* Ожидаем завершения дочерних процессов */
    int status;                // Переменная для хранения статуса завершения
    waitpid(pid1, &status, 0); // Ждём завершения child1
    waitpid(pid2, &status, 0); // Ждём завершения child2

    /* Освобождаем выделенную память */
    free(filename1);
    free(filename2);

    return 0; // Успешное завершение программы
}
