/* parent.c
   Родитель:
   - создаёт два pipe
   - форкит два дочерних процесса (child1 и child2)
   - читает строки произвольной длины из stdin (через read)
   - нечётные строки отправляет в pipe1 -> child1
   - чётные строки отправляет в pipe2 -> child2
   - закрывает концы pipe и ждёт завершения детей
   Важно: никаких stdio I/O (printf/getline/fwrite и т.д.) — только read/write.
*/

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

/* read_line_from_fd: читает из fd до '\n' (включительно) или до EOF.
   Возвращает длину прочитанных байт и помещает pointer в *out (malloc — нужно free).
   Возвращает 0 и *out=NULL на EOF (если ничего не прочитано).
   Возвращает -1 при ошибке. */
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
        else if (r == 0)
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
        else
        {
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
}

/* eprint: вывод ошибки в stderr через write */
static void eprint(const char *s)
{
    if (s)
        write_all(2, s, strlen(s));
}

int main(void)
{
    int pipe1[2], pipe2[2];

    if (pipe(pipe1) < 0)
    {
        eprint("pipe1 failed\n");
        return 1;
    }
    if (pipe(pipe2) < 0)
    {
        eprint("pipe2 failed\n");
        return 1;
    }

    pid_t pid1 = fork();
    if (pid1 < 0)
    {
        eprint("fork child1 failed\n");
        return 1;
    }

    if (pid1 == 0)
    {
        /* child1: stdin <- pipe1[0]; parent writes to pipe1[1] */
        /* child1 will be exec'd by parent to ./child1, so here we prepare */
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);
        if (dup2(pipe1[0], 0) == -1)
        {
            eprint("dup2 child1 failed\n");
            _exit(1);
        }
        close(pipe1[0]);
        execl("./child1", "child1", (char *)NULL);
        /* if execl fails */
        eprint("execl child1 failed\n");
        _exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 < 0)
    {
        eprint("fork child2 failed\n");
        return 1;
    }

    if (pid2 == 0)
    {
        /* child2: stdin <- pipe2[0] */
        close(pipe2[1]);
        close(pipe1[0]);
        close(pipe1[1]);
        if (dup2(pipe2[0], 0) == -1)
        {
            eprint("dup2 child2 failed\n");
            _exit(1);
        }
        close(pipe2[0]);
        execl("./child2", "child2", (char *)NULL);
        eprint("execl child2 failed\n");
        _exit(1);
    }

    /* parent process */
    /* close read ends in parent (we write to pipes) */
    close(pipe1[0]);
    close(pipe2[0]);

    /* prompt (via write) */
    write_all(1, "Enter lines (Ctrl+D to finish):\n", 33);

    int line_no = 0;
    while (1)
    {
        char *line = NULL;
        ssize_t rl = read_line_from_fd(0, &line);
        if (rl < 0)
        {
            eprint("Error reading line\n");
            break;
        }
        if (rl == 0)
        { /* EOF */
            free(line);
            break;
        }

        line_no++;
        int dest = (line_no % 2 == 1) ? pipe1[1] : pipe2[1];
        if (write_all(dest, line, (size_t)rl) < 0)
        {
            eprint("Error writing to pipe\n");
            free(line);
            break;
        }
        free(line);
    }

    close(pipe1[1]);
    close(pipe2[1]);

    /* wait children */
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    return 0;
}
