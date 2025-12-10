#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUF_CHUNK 1024

/* write_all: гарантированная запись всех байтов в файловый дескриптор */
static ssize_t write_all(int fd, const void *buf, size_t count)
{
    const char *p = buf; // Указатель на текущую позицию в буфере
    size_t left = count; // Количество байт, которые ещё нужно записать
    while (left > 0)     // Пока есть данные для записи
    {
        ssize_t w = write(fd, p, left); // Пытаемся записать оставшиеся байты
        if (w < 0)                      // Если произошла ошибка
        {
            if (errno == EINTR) // Если вызов был прерван сигналом
                continue;       // Повторяем попытку
            return -1;          // Возвращаем ошибку
        }
        left -= (size_t)w; // Уменьшаем счётчик оставшихся байт
        p += w;            // Сдвигаем указатель
    }
    return (ssize_t)count; // Возвращаем количество записанных байт
}

/* read_line_from_fd: читает строку из файлового дескриптора до '\n' или EOF */
static ssize_t read_line_from_fd(int fd, char **out)
{
    size_t cap = BUF_CHUNK;  // Начальная ёмкость буфера
    size_t len = 0;          // Текущая длина прочитанных данных
    char *buf = malloc(cap); // Выделяем память для буфера
    if (!buf)                // Проверка успешности выделения памяти
        return -1;
    char tmp[BUF_CHUNK]; // Временный буфер для чтения
    while (1)            // Читаем до конца строки или файла
    {
        ssize_t r = read(fd, tmp, sizeof(tmp)); // Читаем порцию данных
        if (r < 0)                              // Обработка ошибки чтения
        {
            if (errno == EINTR) // Если прервано сигналом
                continue;       // Повторяем чтение
            free(buf);
            return -1;
        }
        if (r == 0) // Достигнут конец файла (EOF)
        {
            if (len == 0) // Если ничего не прочитано
            {
                free(buf);
                *out = NULL;
                return 0;
            }
            *out = buf; // Возвращаем прочитанные данные
            return (ssize_t)len;
        }
        for (ssize_t i = 0; i < r; ++i) // Обрабатываем каждый прочитанный байт
        {
            if (len + 1 >= cap) // Если буфер заполнен
            {
                cap *= 2;                     // Удваиваем размер
                char *nb = realloc(buf, cap); // Перевыделяем память
                if (!nb)                      // Если не удалось
                {
                    free(buf);
                    return -1;
                }
                buf = nb; // Обновляем указатель
            }
            buf[len++] = tmp[i]; // Копируем байт в буфер
            if (tmp[i] == '\n')  // Если нашли конец строки
            {
                *out = buf;
                return (ssize_t)len; // Возвращаем строку
            }
        }
    }
}

/* reverse_str: инвертирует строку (переворачивает символы в обратном порядке) */
static void reverse_str(char *s, ssize_t len)
{
    if (len <= 0) // Проверка на пустую строку
        return;
    ssize_t i = 0, j = len - 1; // Указатели на начало и конец строки
    int has_nl = 0;             // Флаг наличия символа новой строки
    if (s[j] == '\n')           // Если строка заканчивается на '\n'
    {
        has_nl = 1; // Запоминаем это
        j--;        // Не переворачиваем символ новой строки
    }
    while (i < j) // Меняем местами символы с краёв к центру
    {
        char t = s[i]; // Временная переменная для обмена
        s[i] = s[j];   // Меняем местами
        s[j] = t;
        i++; // Сдвигаем указатели к центру
        j--;
    }
    if (has_nl)            // Если был символ новой строки
        s[len - 1] = '\n'; // Восстанавливаем его в конце
}

/* eprint: выводит сообщение об ошибке в stderr */
static void eprint(const char *s)
{
    if (s)                          // Если строка не NULL
        write_all(2, s, strlen(s)); // Выводим в stderr (fd=2)
}

int main(int argc, char *argv[])
{
    /* Проверяем, что программе передано имя выходного файла */
    if (argc < 2) // argv[0] - имя программы, argv[1] - имя файла
    {
        eprint("child1: no filename provided\n");
        return 1; // Завершаем с ошибкой
    }

    const char *out_file = argv[1]; // Получаем имя файла из аргументов

    /* Открываем файл для записи */
    int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // O_WRONLY - только запись, O_CREAT - создать если нет, O_TRUNC - очистить если есть
    // 0644 - права доступа (rw-r--r--)
    if (fd < 0) // Если не удалось открыть файл
    {
        eprint("child1: open failed\n");
        fd = -1; // Устанавливаем fd в -1 (будем писать только в stdout)
    }

    /* Основной цикл обработки строк */
    while (1) // Читаем строки до EOF
    {
        char *line = NULL;                        // Указатель для очередной строки
        ssize_t rl = read_line_from_fd(0, &line); // Читаем строку из stdin (pipe)
        if (rl < 0)                               // Если произошла ошибка чтения
        {
            eprint("child1: read error\n");
            break; // Прерываем цикл
        }
        if (rl == 0) // Если достигнут EOF (родитель закрыл pipe)
        {
            free(line); // Освобождаем память
            break;      // Выходим из цикла
        }

        reverse_str(line, rl); // Инвертируем строку

        /* Выводим инвертированную строку в stdout */
        if (write_all(1, line, (size_t)rl) < 0)
            eprint("child1: write stdout failed\n");

        /* Если файл открыт, записываем туда же */
        if (fd >= 0) // Проверяем, что файл успешно открыт
        {
            if (write_all(fd, line, (size_t)rl) < 0)
                eprint("child1: write file failed\n");
        }
        free(line); // Освобождаем память строки
    }

    /* Закрываем файл если он был открыт */
    if (fd >= 0)
        close(fd);
    return 0; // Успешное завершение
}
