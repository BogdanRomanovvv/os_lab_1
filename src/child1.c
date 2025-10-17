/* child1.c
   читает строки из stdin (pipe), инвертирует каждую строку
   и записывает инвертированную строку в stdout и в файл child1.txt
   Все I/O — системные вызовы (read/write/open/close).
*/

#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUF_CHUNK 1024
#define OUT_FILE "child1.txt"

static ssize_t write_all(int fd, const void *buf, size_t count)
{
    const char *p = buf;
    size_t left = count;
    while (left > 0)
    {
        ssize_t w = write(fd, p, left);
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)count;
}

static ssize_t read_line_from_fd(int fd, char **out)
{
    size_t cap = BUF_CHUNK;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf)
        return -1;
    char tmp[BUF_CHUNK];
    while (1)
    {
        ssize_t r = read(fd, tmp, sizeof(tmp));
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            free(buf);
            return -1;
        }
        if (r == 0)
        {
            if (len == 0)
            {
                free(buf);
                *out = NULL;
                return 0;
            }
            *out = buf;
            return (ssize_t)len;
        }
        for (ssize_t i = 0; i < r; ++i)
        {
            if (len + 1 >= cap)
            {
                cap *= 2;
                char *nb = realloc(buf, cap);
                if (!nb)
                {
                    free(buf);
                    return -1;
                }
                buf = nb;
            }
            buf[len++] = tmp[i];
            if (tmp[i] == '\n')
            {
                *out = buf;
                return (ssize_t)len;
            }
        }
    }
}

static void reverse_str(char *s, ssize_t len)
{
    if (len <= 0)
        return;
    ssize_t i = 0, j = len - 1;
    int has_nl = 0;
    if (s[j] == '\n')
    {
        has_nl = 1;
        j--;
    }
    while (i < j)
    {
        char t = s[i];
        s[i] = s[j];
        s[j] = t;
        i++;
        j--;
    }
    if (has_nl)
        s[len - 1] = '\n';
}

static void eprint(const char *s)
{
    if (s)
        write_all(2, s, strlen(s));
}

int main(void)
{
    /* открываем файл через open (fcntl.h) */
    int fd = open(OUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        eprint("child1: open failed\n");
        /* но продолжаем писать только в stdout */
        fd = -1;
    }

    while (1)
    {
        char *line = NULL;
        ssize_t rl = read_line_from_fd(0, &line);
        if (rl < 0)
        {
            eprint("child1: read error\n");
            break;
        }
        if (rl == 0)
        {
            free(line);
            break;
        }

        reverse_str(line, rl);
        if (write_all(1, line, (size_t)rl) < 0)
            eprint("child1: write stdout failed\n");
        if (fd >= 0)
        {
            if (write_all(fd, line, (size_t)rl) < 0)
                eprint("child1: write file failed\n");
        }
        free(line);
    }

    if (fd >= 0)
        close(fd);
    return 0;
}
