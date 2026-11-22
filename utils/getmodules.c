
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_DEPTH 32
#define INITIAL_HASH_SIZE 64

typedef struct Module {
    char *name;
    char *path;
    struct Module *next;
} Module;

static Module **hash_table;
static Module **modules;

static size_t hash_capacity;
static size_t module_count;
static size_t module_capacity;

static size_t hash_collision_events;
static size_t path_collisions;
static size_t missing;

/* Normalize Linux module names */
static void normalize(char *name) {
    for (size_t i = 0; name[i]; i++) {
        if (name[i] == '-')
            name[i] = '_';
    }
}

/* Duplicate a string */
static char *duplicate_string(const char *str) {
    size_t len = strlen(str);

    if (len == SIZE_MAX)
        return NULL;

    char *copy = malloc(len + 1);

    if (copy)
        memcpy(copy, str, len + 1);

    return copy;
}

/* FNV-1a 64-bit hash */
static uint64_t hash_name(const char *name) {
    uint64_t hash = UINT64_C(14695981039346656037);

    while (*name) {
        hash ^= (unsigned char)*name++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

/* Find module */
static Module *find_module(const char *name) {
    if (!hash_capacity)
        return NULL;

    size_t index = (size_t)(hash_name(name) % hash_capacity);

    for (Module *m = hash_table[index]; m; m = m->next) {
        if (strcmp(m->name, name) == 0)
            return m;
    }

    return NULL;
}

/* Resize hash table */
static int resize_hash_table(void) {
    size_t new_capacity;

    if (!hash_capacity) {
        new_capacity = INITIAL_HASH_SIZE;
    } else {
        if (hash_capacity > SIZE_MAX / 2)
            return -1;

        new_capacity = hash_capacity * 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(Module *))
        return -1;

    Module **new_table = calloc(new_capacity, sizeof(Module *));

    if (!new_table)
        return -1;

    for (size_t i = 0; i < module_count; i++) {
        Module *module = modules[i];
        size_t index = (size_t)(
            hash_name(module->name) % new_capacity
        );

        module->next = new_table[index];
        new_table[index] = module;
    }

    free(hash_table);
    hash_table = new_table;
    hash_capacity = new_capacity;

    return 0;
}

/* Ensure hash table capacity */
static int ensure_hash_capacity(void) {
    if (!hash_capacity ||
        module_count >= hash_capacity - hash_capacity / 4) {
        return resize_hash_table();
    }

    return 0;
}

/* Ensure module array capacity */
static int ensure_module_capacity(void) {
    if (module_count < module_capacity)
        return 0;

    size_t new_capacity;

    if (!module_capacity) {
        new_capacity = 32;
    } else {
        if (module_capacity > SIZE_MAX / 2)
            return -1;

        new_capacity = module_capacity * 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(Module *))
        return -1;

    Module **tmp = realloc(
        modules,
        new_capacity * sizeof(Module *)
    );

    if (!tmp)
        return -1;

    modules = tmp;
    module_capacity = new_capacity;

    return 0;
}

/* Add loaded module */
static int add_module(const char *name) {
    char *copy = duplicate_string(name);

    if (!copy)
        return -1;

    normalize(copy);

    if (find_module(copy)) {
        free(copy);
        return 0;
    }

    if (ensure_hash_capacity() != 0 ||
        ensure_module_capacity() != 0) {
        free(copy);
        return -1;
    }

    Module *module = calloc(1, sizeof(Module));

    if (!module) {
        free(copy);
        return -1;
    }

    module->name = copy;

    size_t index = (size_t)(hash_name(copy) % hash_capacity);

    if (hash_table[index])
        hash_collision_events++;

    module->next = hash_table[index];
    hash_table[index] = module;

    modules[module_count++] = module;

    return 0;
}

/* Identify supported module extensions */
static size_t extension_length(const char *name) {
    size_t len = strlen(name);

    if (len >= 7 && strcmp(name + len - 7, ".ko.zst") == 0)
        return 7;

    if (len >= 6 && strcmp(name + len - 6, ".ko.xz") == 0)
        return 6;

    if (len >= 6 && strcmp(name + len - 6, ".ko.gz") == 0)
        return 6;

    if (len >= 3 && strcmp(name + len - 3, ".ko") == 0)
        return 3;

    return 0;
}

/* Extract module name */
static char *get_module_name(const char *filename) {
    size_t len = strlen(filename);
    size_t ext = extension_length(filename);

    if (!ext || len == ext)
        return NULL;

    size_t name_len = len - ext;
    char *name = malloc(name_len + 1);

    if (!name)
        return NULL;

    memcpy(name, filename, name_len);
    name[name_len] = '\0';

    normalize(name);

    return name;
}

/* Read /proc/modules */
static int read_modules(void) {
    FILE *file = fopen("/proc/modules", "r");

    if (!file) {
        perror("getmodules: /proc/modules");
        return -1;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    while ((length = getline(&line, &capacity, file)) != -1) {
        (void)length;

        char *end = line + strcspn(line, " \t\r\n");

        if (end == line)
            continue;

        *end = '\0';

        if (add_module(line) != 0) {
            fprintf(stderr, "getmodules: allocation failure\n");
            free(line);
            fclose(file);
            return -1;
        }
    }

    int failed = ferror(file);

    free(line);

    if (fclose(file) != 0) {
        perror("getmodules: fclose");
        failed = 1;
    }

    if (failed) {
        fprintf(stderr, "getmodules: error reading /proc/modules\n");
        return -1;
    }

    return 0;
}

/* Report filesystem errors */
static void report_error(const char *operation, const char *path) {
    int saved_errno = errno;

    fprintf(stderr, "getmodules: %s %s: %s\n",
            operation, path, strerror(saved_errno));
}

/*
 * Open an absolute directory path component by component.
 * No component may be a symbolic link.
 */
static int open_directory_path(const char *path) {
    if (!path || path[0] != '/') {
        errno = EINVAL;
        return -1;
    }

    char *copy = duplicate_string(path);

    if (!copy)
        return -1;

    int current_fd = open(
        "/",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC
    );

    if (current_fd < 0) {
        free(copy);
        return -1;
    }

    char *saveptr = NULL;
    char *component = strtok_r(copy, "/", &saveptr);

    while (component) {
        if (strcmp(component, ".") == 0 ||
            strcmp(component, "..") == 0) {
            close(current_fd);
            free(copy);
            errno = EINVAL;
            return -1;
        }

        int next_fd = openat(
            current_fd,
            component,
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
        );

        if (next_fd < 0) {
            int saved_errno = errno;
            close(current_fd);
            free(copy);
            errno = saved_errno;
            return -1;
        }

        close(current_fd);
        current_fd = next_fd;

        component = strtok_r(NULL, "/", &saveptr);
    }

    free(copy);
    return current_fd;
}

/* Scan an already opened directory */
static void scan_directory_fd(
    int fd,
    const char *path,
    unsigned int depth
) {
    DIR *dir = fdopendir(fd);

    if (!dir) {
        report_error("fdopendir:", path);
        close(fd);
        return;
    }

    int parent_fd = dirfd(dir);
    struct dirent *entry;

    for (;;) {
        errno = 0;
        entry = readdir(dir);

        if (!entry) {
            if (errno != 0)
                report_error("readdir:", path);
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[MAX_PATH];

        int written = snprintf(
            fullpath,
            sizeof(fullpath),
            "%s/%s",
            path,
            entry->d_name
        );

        if (written < 0 || (size_t)written >= sizeof(fullpath))
            continue;

        struct stat st;

        if (fstatat(
            parent_fd,
            entry->d_name,
            &st,
            AT_SYMLINK_NOFOLLOW
        ) != 0) {
            if (errno != ENOENT)
                report_error("fstatat:", fullpath);
            continue;
        }

        if (S_ISLNK(st.st_mode))
            continue;

        if (S_ISDIR(st.st_mode)) {
            if (depth >= MAX_DEPTH) {
                fprintf(stderr,
                        "getmodules: depth limit reached: %s\n",
                        fullpath);
                continue;
            }

            int child_fd = openat(
                parent_fd,
                entry->d_name,
                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
            );

            if (child_fd < 0) {
                if (errno != ENOENT && errno != ENOTDIR)
                    report_error("openat:", fullpath);
                continue;
            }

            struct stat opened_st;

            if (fstat(child_fd, &opened_st) != 0 ||
                !S_ISDIR(opened_st.st_mode) ||
                opened_st.st_dev != st.st_dev ||
                opened_st.st_ino != st.st_ino) {
                close(child_fd);
                continue;
            }

            scan_directory_fd(child_fd, fullpath, depth + 1);
            continue;
        }

        if (!S_ISREG(st.st_mode))
            continue;

        if (!extension_length(entry->d_name))
            continue;

        char *name = get_module_name(entry->d_name);

        if (!name) {
            fprintf(stderr,
                    "getmodules: cannot extract module name: %s\n",
                    fullpath);
            continue;
        }

        Module *module = find_module(name);
        free(name);

        if (!module)
            continue;

        if (module->path) {
            path_collisions++;
            continue;
        }

        module->path = duplicate_string(fullpath);

        if (!module->path)
            fprintf(stderr,
                    "getmodules: allocation failure: %s\n",
                    fullpath);
    }

    if (closedir(dir) != 0)
        report_error("closedir:", path);
}

/* Open and scan root directory */
static void scan_root(const char *path) {
    int fd = open_directory_path(path);

    if (fd < 0) {
        if (errno != ENOENT && errno != ENOTDIR)
            report_error("cannot open root:", path);
        return;
    }

    scan_directory_fd(fd, path, 0);
}

/* Print a path */
static int print_path(const char *path, int nul_delimited) {
    size_t len = strlen(path);

    if (fwrite(path, 1, len, stdout) != len)
        return -1;

    if (fputc(nul_delimited ? '\0' : '\n', stdout) == EOF)
        return -1;

    return 0;
}

/* Cleanup */
static void cleanup(void) {
    for (size_t i = 0; i < module_count; i++) {
        free(modules[i]->name);
        free(modules[i]->path);
        free(modules[i]);
    }

    free(modules);
    free(hash_table);
}

int main(int argc, char **argv) {
    int nul_delimited = 0;

    if (argc == 2 && strcmp(argv[1], "-0") == 0) {
        nul_delimited = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: getmodules [-0]\n");
        return EXIT_FAILURE;
    }

    if (resize_hash_table() != 0) {
        fprintf(stderr, "getmodules: cannot initialize hash table\n");
        return EXIT_FAILURE;
    }

    if (read_modules() != 0) {
        cleanup();
        return EXIT_FAILURE;
    }

    const char *directories[] = {
        "/vendor/lib/modules",
        "/vendor_dlkm/lib/modules",
        "/system_dlkm/lib/modules",
        "/odm/lib/modules",
        "/system/lib/modules",
        "/lib/modules",
        NULL
    };

    for (size_t i = 0; directories[i]; i++)
        scan_root(directories[i]);

    for (size_t i = 0; i < module_count; i++) {
        if (modules[i]->path) {
            if (print_path(modules[i]->path, nul_delimited) != 0) {
                perror("getmodules: stdout");
                cleanup();
                return EXIT_FAILURE;
            }
        } else {
            missing++;
        }
    }

    if (fflush(stdout) != 0) {
        perror("getmodules: stdout");
        cleanup();
        return EXIT_FAILURE;
    }

    fprintf(stderr,
        "getmodules: %zu unique modules, %zu found, %zu missing\n",
        module_count,
        module_count - missing,
        missing
    );

    fprintf(stderr,
        "getmodules: %zu historical hash collision events, "
        "%zu additional path matches\n",
        hash_collision_events,
        path_collisions
    );

    cleanup();

    return EXIT_SUCCESS;
}

