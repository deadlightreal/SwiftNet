#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

int run_and_check(const char *path) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        execl(path, path, NULL);
        // If execl fails
        perror("execl");
        exit(127);
    } else {
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }
}

int main() {

    const char *dir_path = "./output";
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    char full_path[4096];
    int all_ok = 1;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            printf("Running: %s\n", full_path);
            int exit_code = run_and_check(full_path);
            if (exit_code == 0) {
                fprintf(stdout, "successfully completed test: %s\n", full_path);
            } else {
                fprintf(stderr, "Error: %s exited with code %d\n", full_path, exit_code);
                all_ok = 0;
            }
        }
    }

    closedir(dir);

    if (all_ok) {
        printf("All tests ran successfully.\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
