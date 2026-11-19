
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#define BUFFER_SIZE 256

static void die(const char *message)
{
    fprintf(stderr, "sysinfo: %s\n", message);
    exit(1);
}

static int command_exists(const char *command)
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
        char fullpath[BUFFER_SIZE];

        snprintf(fullpath, sizeof(fullpath),
                 "%s/%s", *dir ? dir : ".", command);

        if (access(fullpath, X_OK) == 0) {
            free(copy);
            return 1;
        }

        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(copy);
    return 0;
}

static void get_property(const char *property, char *output,
                         size_t output_size)
{
    FILE *fp = popen(
        "getprop ro.build.version.release",
        "r"
    );

    if (!fp) {
        output[0] = '\0';
        return;
    }

    if (!fgets(output, output_size, fp))
        output[0] = '\0';

    pclose(fp);

    output[strcspn(output, "\r\n")] = '\0';
}

int main(void)
{
    struct utsname system_info;

    char android[BUFFER_SIZE] = "";
    char api_level[BUFFER_SIZE] = "";

    if (!command_exists("getprop"))
        die("getprop not found!");

    if (uname(&system_info) != 0)
        die("cannot retrieve system information");

    get_property("ro.build.version.release",
                 android, sizeof(android));

    FILE *fp = popen("getprop ro.build.version.sdk", "r");

    if (fp) {
        if (fgets(api_level, sizeof(api_level), fp)) {
            api_level[strcspn(api_level, "\r\n")] = '\0';
        }

        pclose(fp);
    }

    struct passwd *user = getpwuid(getuid());

    const char *username = user ? user->pw_name : "unknown";

    char hostname[BUFFER_SIZE] = "unknown";

    if (gethostname(hostname, sizeof(hostname) - 1) != 0)
        strcpy(hostname, "unknown");

    hostname[sizeof(hostname) - 1] = '\0';

    printf("%s@%s\n", username, hostname);

    printf("\n");
    printf("--------------\n");
    printf("\n");

    printf("Host: %s\n", hostname);
    printf("Kernel: %s\n", system_info.release);
    printf("OS: Android %s %s\n",
           android, system_info.machine);
    printf("Android API level: %s\n", api_level);

    return 0;
}

