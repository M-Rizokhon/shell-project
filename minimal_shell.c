#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

typedef struct {
    const char* name;
    const char* desc;
} builtin_t;

builtin_t builtins[] = {
    {"cd",   "Change the current directory"},
    {"exit", "Exit the shell"},
    {"help", "Show this help message"},
};


int main(void) {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        printf("myshell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        int i = 0;
        char *token = strtok(line, " ");

        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }

        args[i] = NULL;

        if (args[0] == NULL) {
            continue;
        }

        if (strcmp(args[0], "man") == 0) {
            continue;
        }

        if (strcmp(args[0], "help") == 0) {
            size_t builtin_count = sizeof(builtins) / sizeof(builtins[0]);
            if (args[1] != NULL) {
                const char* command = args[1];
                int found = 0;
                for (size_t j = 0; j < builtin_count; j++) {
                    if (strcmp(builtins[j].name,command) == 0) {
                        printf("Built-in command: %s\n", command);
                        printf("  %-10s %s\n", command, builtins[j].desc);
                        found = 1;
                        break;
                    }
                } 
                if (!found) 
                    fprintf(stderr, "help: no such command as %s\n", command);
                continue;
                
            }

            printf("myshell: a simple command shell\n\n");
            printf("Built-in commands:\n");
            for (size_t j = 0; j < builtin_count; j++) {
                printf("  %-10s %s\n", builtins[j].name, builtins[j].desc);
            }
            printf("\nAny other input is treated as an external command and run via execvp.\n");
            continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (i > 2) {
                fprintf(stderr, "cd: too many arguments\n");
                continue;
            }

            const char* path = (i == 1) ? getenv("HOME"): args[1];

            if (path == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                continue;
            }

            if (chdir(path) == -1) {
                perror("cd");
            }

            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            execvp(args[0], args);

            perror("execvp");
            exit(1);
        } else {
            waitpid(pid, NULL, 0);
        }
    }

    return 0;
}