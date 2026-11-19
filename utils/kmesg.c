
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/klog.h>
#include <unistd.h>

int main(void)
{
    int size = klogctl(10, NULL, 0);

    if (size < 0) {
        perror("klogctl");
        return 1;
    }

    if (size == 0) {
        puts("kmesg: empty kernel buffer");
        return 0;
    }

    char *buf = malloc((size_t)size);

    if (!buf) {
        perror("malloc");
        return 1;
    }

    int n = klogctl(2, buf, size);

    if (n < 0) {
        perror("klogctl");
        free(buf);
        return 1;
    }

    if (n > 0 && write(STDOUT_FILENO, buf, (size_t)n) < 0) {
        perror("write");
        free(buf);
        return 1;
    }

    free(buf);
    return 0;
}

