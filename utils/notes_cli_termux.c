
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <errno.h>

#define BUFFER_SIZE 4096

static char NOTES_DIR[PATH_MAX];
static char *EDITOR = NULL;

static void die(const char *msg)
{
    fprintf(stderr, "notes_cli_termux: %s\n", msg);
    exit(1);
}

static void init(void)
{
    if (mkdir(NOTES_DIR, 0755) != 0 && errno != EEXIST)
        die("cannot create notes directory");
}

static void setup(void)
{
    const char *prefix = getenv("PREFIX");

    if (!prefix || !*prefix)
        die("PREFIX is not set");

    if (snprintf(NOTES_DIR, sizeof(NOTES_DIR),
                 "%s/home/.notes_cli/notes", prefix)
        >= sizeof(NOTES_DIR))
        die("notes path too long");
}

static void check_name(const char *name)
{
    if (!name || !*name)
        die("missing note name");

    if (name[0] == '.')
        die("invalid note name");

    for (const char *p = name; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') ||
              (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') ||
              *p == '.' || *p == '_' || *p == '-'))
            die("invalid note name");
    }
}

static char *note_path(const char *name)
{
    size_t size = strlen(NOTES_DIR) + strlen(name) + 2;

    char *path = malloc(size);

    if (!path)
        die("out of memory");

    snprintf(path, size, "%s/%s", NOTES_DIR, name);

    return path;
}

static int command_exists(const char *cmd)
{
    const char *path = getenv("PATH");

    if (!path)
        return 0;

    char *copy = strdup(path);

    if (!copy)
        die("out of memory");

    char *saveptr;
    char *dir = strtok_r(copy, ":", &saveptr);

    while (dir) {
        char candidate[PATH_MAX];

        snprintf(candidate, sizeof(candidate),
                 "%s/%s", *dir ? dir : ".", cmd);

        if (access(candidate, X_OK) == 0) {
            free(copy);
            return 1;
        }

        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(copy);
    return 0;
}

static void get_editor(void)
{
    const char *env = getenv("EDITOR");

    if (env && *env) {
        if (!command_exists(env))
            die("EDITOR not found");

        EDITOR = strdup(env);
        return;
    }

    const char *editors[] = {"nano", "vim", "vi"};

    for (int i = 0; i < 3; i++) {
        if (command_exists(editors[i])) {
            EDITOR = strdup(editors[i]);
            return;
        }
    }

    die("no supported editor found");
}

static void run_editor(const char *file)
{
    pid_t pid = fork();

    if (pid < 0)
        die("cannot start editor");

    if (pid == 0) {
        char *args[] = {EDITOR, (char *)file, NULL};

        execvp(EDITOR, args);

        perror("notes_cli_termux: editor");
        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
        die("cannot wait for editor");

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        die("editor failed");
}

static int regular_file(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void create_note(const char *name)
{
    check_name(name);
    init();

    char *file = note_path(name);

    if (access(file, F_OK) == 0)
        die("note already exists");

    get_editor();
    run_editor(file);

    if (!regular_file(file))
        die("note was not created");

    printf("Created note: %s\n", name);

    free(file);
}

static void edit_note(const char *name)
{
    check_name(name);

    char *file = note_path(name);

    if (!regular_file(file))
        die("note does not exist");

    get_editor();
    run_editor(file);

    printf("Edited note: %s\n", name);

    free(file);
}

static void read_note(const char *name)
{
    check_name(name);

    char *file = note_path(name);

    FILE *fp = fopen(file, "r");

    if (!fp)
        die("note does not exist");

    char buffer[BUFFER_SIZE];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        fwrite(buffer, 1, n, stdout);

    fclose(fp);
    free(file);
}

static void delete_note(const char *name)
{
    check_name(name);

    char *file = note_path(name);

    if (!regular_file(file))
        die("note does not exist");

    if (unlink(file) != 0)
        die("cannot delete note");

    printf("Deleted note: %s\n", name);

    free(file);
}

static void list_notes(void)
{
    init();

    DIR *dir = opendir(NOTES_DIR);

    if (!dir)
        die("cannot open notes directory");

    int found = 0;
    struct dirent *entry;

    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.')
            continue;

        char *file = note_path(entry->d_name);

        if (regular_file(file)) {
            puts(entry->d_name);
            found = 1;
        }

        free(file);
    }

    closedir(dir);

    if (!found)
        puts("No notes found.");
}

static void search_notes(const char *term)
{
    if (!term || !*term)
        die("missing search term");

    DIR *dir = opendir(NOTES_DIR);

    if (!dir)
        die("notes directory does not exist");

    int found = 0;
    struct dirent *entry;

    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.')
            continue;

        char *file = note_path(entry->d_name);

        if (!regular_file(file)) {
            free(file);
            continue;
        }

        FILE *fp = fopen(file, "r");

        if (!fp) {
            free(file);
            continue;
        }

        char *line = NULL;
        size_t capacity = 0;
        ssize_t length;
        unsigned long line_number = 0;

        while ((length = getline(&line, &capacity, fp)) != -1) {
            line_number++;

            if (strstr(line, term)) {
                printf("%s:%lu:", entry->d_name, line_number);

                fwrite(line, 1, length, stdout);

                if (length == 0 || line[length - 1] != '\n')
                    putchar('\n');

                found = 1;
            }
        }

        free(line);
        fclose(fp);
        free(file);
    }

    closedir(dir);

    if (!found)
        die("no matches found");
}

static void rename_note(const char *old_name,
                        const char *new_name)
{
    check_name(old_name);
    check_name(new_name);

    char *old = note_path(old_name);
    char *new = note_path(new_name);

    if (!regular_file(old))
        die("source note does not exist");

    if (access(new, F_OK) == 0)
        die("destination already exists");

    if (rename(old, new) != 0)
        die("cannot rename note");

    printf("Renamed %s to %s\n", old_name, new_name);

    free(old);
    free(new);
}

static void help_list(void)
{
    puts(
        "notes_cli_termux\n\n"
        "Usage: notes_cli_termux <command> [arguments]\n\n"
        "Commands:\n"
        "  create <name>       Create a note\n"
        "  edit <name>         Edit a note\n"
        "  read <name>         Read a note\n"
        "  delete <name>       Delete a note\n"
        "  list                List notes\n"
        "  search <term>       Search notes\n"
        "  rename <old> <new>  Rename a note\n"
        "  help                Show help"
    );
}

int main(int argc, char **argv)
{
    setup();

    if (argc < 2) {
        help_list();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "create") == 0) {
        if (argc < 3) die("missing note name");
        create_note(argv[2]);

    } else if (strcmp(cmd, "edit") == 0) {
        if (argc < 3) die("missing note name");
        edit_note(argv[2]);

    } else if (strcmp(cmd, "read") == 0) {
        if (argc < 3) die("missing note name");
        read_note(argv[2]);

    } else if (strcmp(cmd, "delete") == 0) {
        if (argc < 3) die("missing note name");
        delete_note(argv[2]);

    } else if (strcmp(cmd, "list") == 0) {
        list_notes();

    } else if (strcmp(cmd, "search") == 0) {
        if (argc < 3) die("missing search term");
        search_notes(argv[2]);

    } else if (strcmp(cmd, "rename") == 0) {
        if (argc < 4) die("missing rename arguments");
        rename_note(argv[2], argv[3]);

    } else if (
        strcmp(cmd, "help") == 0 ||
        strcmp(cmd, "-h") == 0 ||
        strcmp(cmd, "--help") == 0
    ) {
        help_list();

    } else {
        fprintf(stderr,
                "notes_cli_termux: unknown command: %s\n",
                cmd);
        return 1;
    }

    return 0;
}

