#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "roaring/roaring.h"
#ifdef __GLIBC__
#include <malloc.h>
#endif

#define FILENAME "/tmp/roaring.bin"

void die(const char *func) {
    fprintf(stderr, "%s(%s): %s\n", func, FILENAME, strerror(errno));
    exit(1);
}

void save(const char *buf, size_t len) {
    int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        die("open");
    }
    while (len > 0) {
        ssize_t res = write(fd, buf, len);
        if (res == -1) {
            die("write");
        }
        buf += res;
        len -= res;
    }
    close(fd);
}

void populate(roaring_bitmap_t *r, int n, char **paths) {
    for (int i = 0; i < n; i++) {
        size_t count = 0;
        uint32_t *values = read_integer_file(paths[i], &count);
        if (count == 0) {
            fprintf(stderr, "No integers found in %s\n", paths[i]);
            exit(1);
        }
        for (size_t j = 0; j < count; j++) {
            roaring_bitmap_add(r, values[j]);
        }
        free(values);
    }
}

int main(int argc, char *argv[]) {
    (void)&read_all_integer_files;  // suppress unused warning

    if (argc < 2) {
        printf("Usage: %s <comma_separated_integers_file> ...\n", argv[0]);
        return 1;
    }

    {
        roaring_bitmap_t *r = roaring_bitmap_create();
        populate(r, argc - 1, argv + 1);
        printf("Cardinality: %" PRId64 "\n", roaring_bitmap_get_cardinality(r));
        size_t len = roaring_bitmap_frozen_size_in_bytes(r);
        printf("Serialized size [bytes]: %zu\n", len);
        char *buf = malloc(len);
        roaring_bitmap_frozen_serialize(r, buf);
        save(buf, len);
        free(buf);
        free(r);
    }
    {
        int fd = open(FILENAME, O_RDONLY);
        if (fd == -1) {
            die("open");
        }

        struct stat st;
        if (fstat(fd, &st) == -1) {
            die("fstat");
        }

        char *ptr =
            (char *)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (!ptr) {
            die("mmap");
        }
        const roaring_bitmap_t *r = roaring_bitmap_frozen_view(ptr, st.st_size);
#ifdef __GLIBC__
        printf("Allocation size [bytes]: %zu\n", malloc_usable_size((void *)r));
#endif
        roaring_bitmap_free(r);
        munmap(ptr, st.st_size);
        close(fd);
    }

    return 0;
}
