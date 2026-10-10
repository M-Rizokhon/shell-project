#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

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

enum Proc_kind {BACKGROUND, FOREGROUND};



static int is_redir_op(const char* s) {
    return strcmp(s, ">") == 0 || strcmp(s, ">>") == 0 || strcmp(s, "<") == 0;
}


// Applies every > and >> in args, removing the operator and filename
// tokens so only the command and its real arguments remain.
// Returns 0 on success, -1 on error (message already printed).
static int apply_redirections(char **args) {
    int w = 0;

    for (int r = 0; args[r] != NULL; r++) {


        if (!is_redir_op(args[r])) {
            args[w++] = args[r];
            continue;
        }

        if (args[r + 1] == NULL) {
            fprintf(stderr, "syntax error: filename expected after %s\n", args[r]);
            return -1;
        }

        if (is_redir_op(args[r + 1])) {
            fprintf(stderr, "syntax error near unexpected token '%s'\n", args[r + 1]);
            return -1;
        }

        int is_append = strcmp(args[r], ">>") == 0;
        int is_in = strcmp(args[r], "<") == 0;

        int flags = is_in ? O_RDONLY : O_WRONLY | O_CREAT | (is_append ? O_APPEND : O_TRUNC);
        int target = is_in ? STDIN_FILENO : STDOUT_FILENO;

        int fd = open(args[r + 1], flags, 0644);
        if (fd == -1) {
            perror(args[r + 1]);
            return -1;
        }

        if (dup2(fd, target) == -1) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);

        r++;  // skip the filename too
    }

    args[w] = NULL;
    return 0;
}   


static void exec_cmd(char **argv) {
    if (apply_redirections(argv) == -1) _exit(1);
    if (argv[0] == NULL) {
        fprintf(stderr, "syntax error: missing command\n");
        _exit(1);
    }
    execvp(argv[0], argv);
    perror(argv[0]);
    _exit(127);
}





int main(void) {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {
        }

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

        enum Proc_kind proc = FOREGROUND;
        if (strcmp(args[i-1], "&") == 0) {
            proc = BACKGROUND;
            args[i-1] = NULL;
            i--;
        }

        if (args[0] == NULL) {
            continue;
        }

        int j = 0;
        int is_pipe = 0;
        while (args[j] != NULL) {
            if (strcmp(args[j], "|") == 0) {
                is_pipe = 1;
                break;
            }
            j++;
        }

        if (is_pipe) {

            char* left_argv[i], *right_argv[i];
            int k = 0;
            while (strcmp(args[k], "|") != 0) {
                left_argv[k] = args[k];
                k++;
            }
            left_argv[k] = NULL;
            k++;

            int m = 0;
            while (args[k] != NULL) {
                right_argv[m++] = args[k++];
            }
            right_argv[m] = NULL;

            if (left_argv[0] == NULL || right_argv[0] == NULL) {
                fprintf(stderr, "syntax error near '|'\n");
                continue;
            }

            
            int pfd[2];
            if (pipe(pfd) == -1) {
                perror("pipe");
                continue;
            }

            pid_t left = fork();
            if (left == 0) {
                dup2(pfd[1], STDOUT_FILENO);
                close(pfd[0]);
                close(pfd[1]);
                exec_cmd(left_argv);
            }
            
            pid_t right = fork();
            if (right == 0) {
                dup2(pfd[0], STDIN_FILENO);
                close(pfd[1]);
                close(pfd[0]);
                exec_cmd(right_argv);
            }

            close(pfd[0]);
            close(pfd[1]);
            waitpid(left, NULL, 0);
            waitpid(right, NULL, 0);
            continue;
        }


        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            if (apply_redirections(args) == -1) {
                exit(1);
            }

            if (args[0] == NULL) {
                fprintf(stderr, "syntax error: missing command\n");
                exit(1);
            }

            execvp(args[0], args);

            perror("execvp");
            exit(1);
        } else {
            if (proc == FOREGROUND)
                waitpid(pid, NULL, 0);
            else {
                printf("%d\n", pid);
            }
        }
    }

    return 0;
}