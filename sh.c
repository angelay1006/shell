#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "./jobs.h"
#include <sys/types.h>  // pid_t

#define BUFFER_SIZE 1024
#define TOKEN_SIZE 512

job_list_t *job_list;  // global job list to track background jobs
int next_jid = 1;      // counter for job IDs, increments when new jobs added

/*
 * prints the shell prompt is PROMPT is defined
 *
 * no parameters
 * no return value
 */
void print_prompt(void) {
#ifdef PROMPT
    if (printf("33sh> ") < 0) {
        perror("printf error with print_prompt");
    }
#endif
}

/*
 * reads user input from stdin into the provided buffer
 *
 * buffer - an array where an input of chars will be stored
 * size - the max size of the buffer
 * returns the number of bytes read in or -1 if met with an error
 */
ssize_t read_input(char *buffer, size_t size) {
    ssize_t bytes_read = read(STDIN_FILENO, buffer, size - 1);
    if (bytes_read < 0) {
        perror("read error");
    } else if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // we need to null-terminate the input
    }
    return bytes_read;
}

/*
 * handles edge cases for user input, like EOF or empty lines
 *
 * buffer - the user input buffer, containing the read line
 * bytes_read - the number of bytes read from user input
 * returns 0 to proceed with normal parsing,
 * 1 if the shell should exit, 2 to continue without processing
 */
int handle_input(char *buffer, ssize_t bytes_read) {
    // ctrl-d/EOF check
    if (bytes_read == 0) {
        return 1;  // tells REPL to exit
    }
    // in the case of nothing or only whitespace input
    char *ptr = buffer;
    while (*ptr && isspace((unsigned char)*ptr)) {
        ptr++;  // skip the whitespace
    }
    // start a new line
    if (*ptr == '\0') {
        return 2;  // tells REPL to continue
    }

    return 0;  // tells REPL to proceed with normal parsing
}

/*
 * tokenizes the input buffer and sets up the command, args, and
 * file directions for later execution
 *
 * buffer - the input string containing command & args
 * argv - array to hold args for the command (null terminated for execv)
 * command_path - pointer to the command path string
 * in_file - pointer to the input filepath if redirection is used
 * out_file - pointer to the output filepath if redirection is used
 * append_flag - pointer to a flag for appending output instead of overwriting
 * foreground - pointer to an int (1 if job should run in foreground, 0 for
 * background) returns void, because it just sets up command and args in argv.
 * also modifies in_file, out_file & append_flag for redirection
 */
void parse(char buffer[BUFFER_SIZE], char *argv[TOKEN_SIZE],
           char **command_path, char **in_file, char **out_file,
           int *append_flag, int *foreground) {
    char *token = strtok(buffer, " \t\n");
    int argc = 0;
    *in_file = NULL;
    *out_file = NULL;
    *append_flag = 0;

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            if (*in_file != NULL) {
                fprintf(
                    stderr,
                    "ERROR - Can't have two input redirects on one line.\n");
                *command_path = NULL;
                return;
            }
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "ERROR - No redirection file specified.\n");
                *command_path = NULL;
                return;
            }
            *in_file = token;
        } else if (strcmp(token, ">") == 0) {
            if (*out_file != NULL) {
                fprintf(
                    stderr,
                    "ERROR - Can't have two output redirects on one line.\n");
                *command_path = NULL;  // indicate error
                return;
            }
            *append_flag = 0;
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "ERROR - No redirection file specified.\n");
                *command_path = NULL;
                return;
            }
            *out_file = token;
        } else if (strcmp(token, ">>") == 0) {
            if (*out_file != NULL) {
                fprintf(
                    stderr,
                    "ERROR - Can't have two output redirects on one line.\n");
                *command_path = NULL;  // indicate error
                return;
            }
            *append_flag = 1;
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "ERROR - No redirection file specified.\n");
                *command_path = NULL;
                return;
            }
            *out_file = token;
        } else {
            if (*command_path == NULL) {
                *command_path = token;
                // set argv[0] to the command name
                char *name = strrchr(token, '/');
                if (name != NULL) {
                    argv[argc++] = name + 1;
                } else {
                    argv[argc++] = token;
                }
            } else {
                argv[argc++] = token;
            }
        }
        token = strtok(NULL, " \t\n");
    }

    // now, check if last token is "&" to set foreground/background
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        *foreground = 0;        // set as background job
        argv[argc - 1] = NULL;  // remove "&" from args
    } else {
        *foreground = 1;  // set as foreground job
    }
    argv[argc] = NULL;  // null terminate
}

/*
 * checks if a command is a builtin command
 *
 * command_path - the string to check
 * returns 1 if command is a builtin, 0 otherwise
 */
int is_builtin(char *command_path) {
    if (strchr(command_path, '/') != NULL) {
        // command contains '/', so it's a path -- treat as external command
        return 0;
    }
    return (
        strcmp(command_path, "cd") == 0 || strcmp(command_path, "ln") == 0 ||
        strcmp(command_path, "rm") == 0 || strcmp(command_path, "exit") == 0 ||
        strcmp(command_path, "jobs") == 0 || strcmp(command_path, "fg") == 0 ||
        strcmp(command_path, "bg") == 0);
}

/*
 * brings a background job to the foreground, continues execution
 *
 * argv - array of arguments, argv[1] should contain a job ID in the format
 * %<jid> shell_pgid - the process group ID of the shell
 *
 * returns 1 if successful; else, returns 1 with error output to stderr
 */
int handle_fg(char *argv[], pid_t shell_pgid) {
    if (argv[1] == NULL) {
        fprintf(stderr, "fg: syntax error\n");
        return 1;
    }

    char *jid_str = argv[1];
    if (jid_str[0] != '%') {
        fprintf(stderr, "fg: syntax error\n");
        return 1;
    }

    int jid = atoi(jid_str + 1);  // Skip the '%' character
    pid_t pid = get_job_pid(job_list, jid);
    if (pid == -1) {
        fprintf(stderr, "fg: no such job\n");
        return 1;
    }

    // Send SIGCONT to the job's process group
    if (kill(-pid, SIGCONT) < 0) {
        perror("kill(SIGCONT) error");
        return 1;
    }

    // Assign terminal control to the job's process group
    if (tcsetpgrp(STDIN_FILENO, pid) < 0) {
        perror("tcsetpgrp error");
        return 1;
    }

    // update job status to running
    if (update_job_jid(job_list, jid, RUNNING) < 0) {
        fprintf(stderr, "Failed to update job status\n");
        return 1;
    }

    // wait for job to complete/stop
    int status;
    if (waitpid(-pid, &status, WUNTRACED) == -1) {
        perror("waitpid error");
    }

    // after job finishes or stops, return terminal control to shell
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0) {
        perror("tcsetpgrp error");
    }

    if (WIFSTOPPED(status)) {
        // update job status to STOPPED
        if (update_job_jid(job_list, jid, STOPPED) < 0) {
            fprintf(stderr, "Failed to update job status\n");
            return 1;
        }
        printf("[%d] (%d) suspended by signal %d\n", jid, pid,
               WSTOPSIG(status));
    } else if (WIFSIGNALED(status)) {
        // job was terminated by signal
        printf("(%d) terminated by signal %d\n", pid, WTERMSIG(status));
        if (remove_job_jid(job_list, jid) < 0) {
            fprintf(stderr, "Failed to remove job\n");
            return 1;
        }
    } else if (WIFEXITED(status)) {
        // job exited normally
        if (remove_job_jid(job_list, jid) < 0) {
            fprintf(stderr, "Failed to remove job\n");
            return 1;
        }
    }
    return 1;
}

/*
 * continues execution of a background job
 *
 * argv - array of arguments, argv[1] should contain a job ID in the format
 * %<jid>
 *
 * returns 1 if the job is continued successfully, else, error outputs to stderr
 */
int handle_bg(char *argv[]) {
    if (argv[1] == NULL) {
        fprintf(stderr, "bg: syntax error\n");
        return 1;
    }

    char *jid_str = argv[1];
    if (jid_str[0] != '%') {
        fprintf(stderr, "bg: syntax error\n");
        return 1;
    }

    int jid = atoi(jid_str + 1);
    pid_t pid = get_job_pid(job_list, jid);
    if (pid == -1) {
        fprintf(stderr, "bg: no such job\n");
        return 1;
    }

    // send SIGCONT to job's process group
    if (kill(-pid, SIGCONT) < 0) {
        perror("kill(SIGCONT) error");
        return 1;
    }

    // update job status to RUNNING
    if (update_job_jid(job_list, jid, RUNNING) < 0) {
        fprintf(stderr, "Failed to update job status\n");
        return 1;
    }

    // we don't have to wait because job runs in the background
    return 1;
}

/*
 * executes a builtin command: "cd" "ln" "rm" or "exit"
 *
 * argv - array of arguments for the builtin command where argv[0] is
 *        the command name
 * shell_pgid - the process group ID of the shell, used for "fg" and "bg"
 *
 * returns 1 if the builtin command was handled successfully, -1 if "exit" was
 * called, or 0 if the command isn't a recognized builtin command
 */
int handle_builtin(char *argv[], pid_t shell_pgid) {
    char *name = strrchr(argv[0], '/');
    if (name != NULL) {
        name++;  // move past the '/'
    } else {
        name = argv[0];
    }

    if (strcmp(name, "cd") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else if (argv[2] != NULL) {
            fprintf(stderr, "cd: syntax error\n");
        } else if (chdir(argv[1]) != 0) {
            perror("cd error");
        }
        return 1;
    }
    if (strcmp(name, "ln") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "ln: missing arguments\n");
        } else if (link(argv[1], argv[2]) != 0) {
            perror("ln error");
        }
        return 1;
    }
    if (strcmp(name, "rm") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "rm: missing argument\n");
        } else if (unlink(argv[1]) != 0) {
            perror("rm error");
        }
        return 1;
    }
    if (strcmp(name, "jobs") == 0) {
        jobs(job_list);
        return 1;
    }
    if (strcmp(name, "fg") == 0) {
        return handle_fg(argv, shell_pgid);
    }
    if (strcmp(name, "bg") == 0) {
        return handle_bg(argv);
    }
    if (strcmp(name, "exit") == 0) {
        // cleanup background jobs before exit
        cleanup_job_list(job_list);
        job_list = NULL;
        return -1;  // exit shell
    }

    return 0;
}

/*
 * runs an external command by creating a new process, redirecting input/output
 * if that was specified
 *
 * command_path - path to the executable command
 * argv - array of arguments to pass to the command
 * in_file - filepath for input redirection/NULL if not used
 * out_file - filepath for output redirection/NULL if not used
 * append_flag - 1 if output should be appended or 0 if it should overwrite
 *
 * returns void & handles errors by printing to stderr
 */
void execute_command(char *command_path, char *argv[], char *in_file,
                     char *out_file, int append_flag) {
    if (command_path == NULL) {
        // error was already printed during parsing
        return;
    }

    if (argv[0] == NULL) {
        fprintf(stderr, "ERROR - No command.\n");
        return;
    }

    // handle input redirection
    if (in_file != NULL) {
        int fd_in = open(in_file, O_RDONLY);
        if (fd_in < 0) {
            perror("Failed to open input file");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("Failed to redirect stdin");
            exit(EXIT_FAILURE);
        }
        close(fd_in);
    }

    // handle output redirection
    if (out_file != NULL) {
        int fd_out;
        if (append_flag) {
            fd_out = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
        } else {
            fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        }
        if (fd_out < 0) {
            perror("Failed to open output file");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("Failed to redirect stdout");
            exit(EXIT_FAILURE);
        }
        close(fd_out);
    }
    // Execute the command
    if (execv(command_path, argv) == -1) {
        perror("execv error");
        exit(EXIT_FAILURE);
    }
}

/*
 * handles status changes in background jobs like stopping or terminating
 *
 * job_list - the global job list structure, to manage jobs in background
 *
 * no return value - prints job status updates directly to stdout
 */
void check_background_jobs(job_list_t *job_list) {
    int status;
    pid_t pid;
    // add WNOHANG, WUNTRACED, WCONTINUED flags
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int jid = get_job_jid(job_list, pid);

        if (WIFEXITED(status)) {
            // normal termination
            printf("[%d] (%d) terminated with exit status %d\n", jid, pid,
                   WEXITSTATUS(status));
            remove_job_pid(job_list, pid);
        } else if (WIFSIGNALED(status)) {
            // termination via signal
            printf("[%d] (%d) terminated by signal %d\n", jid, pid,
                   WTERMSIG(status));
            remove_job_pid(job_list, pid);
        } else if (WIFSTOPPED(status)) {
            // job is stopped
            printf("[%d] (%d) suspended by signal %d\n", jid, pid,
                   WSTOPSIG(status));
            update_job_pid(job_list, pid, STOPPED);
        } else if (WIFCONTINUED(status)) {
            // job is resumed
            printf("[%d] (%d) resumed\n", jid, pid);
            update_job_pid(job_list, pid, RUNNING);
        }
    }
}

/*
 * handles initialization, REPL, parsing input, job control, cleanup on exit
 *
 * initializes the global job list and shell process group ID.
 * sets up signal handlers, reads user input, processes commands, manages
 * job status updates, cleans up resources before exit.
 *
 * returns 0, if successfully completed
 */
int main() {
    job_list = init_job_list();

    // get shell's process group ID
    pid_t shell_pgid = getpgrp();

    // ignore signals in shell process
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    char buffer[BUFFER_SIZE];
    char *argv[TOKEN_SIZE];
    char *command_path = NULL;

    char *in_file = NULL;
    char *out_file = NULL;
    int append_flag = 0;

    int foreground = 1;

    while (1) {
        // before each prompt, reap background jobs
        check_background_jobs(job_list);
        print_prompt();

        // read input
        ssize_t bytes_read = read_input(buffer, BUFFER_SIZE);

        // special case handling
        int special_case = handle_input(buffer, bytes_read);
        if (special_case == 1) {
            // clean up background jobs before exiting
            cleanup_job_list(job_list);
            job_list = NULL;
            break;  // exit shell
        }
        if (special_case == 2) {
            continue;
        }

        // parse input into command & arguments
        command_path = NULL;
        in_file = NULL;
        out_file = NULL;
        append_flag = 0;
        parse(buffer, argv, &command_path, &in_file, &out_file, &append_flag,
              &foreground);

        // check for builtin commands
        if (command_path != NULL && is_builtin(command_path)) {
            int builtin_result = handle_builtin(argv, shell_pgid);
            if (builtin_result == -1) {
                // Clean up background jobs before exiting
                cleanup_job_list(job_list);
                job_list = NULL;  // avoid using job_list after its freed
                break;
            }

            continue;  // need to skip execution if built-in command
        }

        // handle external commands
        if (argv[0] != NULL) {
            pid_t pid = fork();
            if (pid == 0) {  // this is the child process
                // create a unique pgid, set to child's pid
                if (setpgid(0, 0) == -1) {
                    perror("setpgid error in child process");
                    exit(EXIT_FAILURE);
                }

                if (foreground) {
                    // give terminal control to child process group
                    if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
                        perror("tcsetpgrp error in child process");
                        exit(EXIT_FAILURE);
                    }
                }

                // restore default behavior for signals in child
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);

                execute_command(command_path, argv, in_file, out_file,
                                append_flag);
                exit(EXIT_FAILURE);  // if exec fails, exit
            } else if (pid > 0) {    // this is the parent process
                // child should be in its own process group
                if (setpgid(pid, pid) == -1) {
                    perror("setpgid error in parent process");
                }

                if (foreground) {
                    if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
                        perror("tcsetpgrp error in parent process");
                    }

                    // wait for child to complete
                    int status;
                    if (waitpid(pid, &status, WUNTRACED) == -1) {
                        perror("wait error");
                    }

                    // if foreground job was stopped, add to job list
                    if (WIFSTOPPED(status)) {
                        add_job(job_list, next_jid++, pid, STOPPED,
                                command_path);
                        printf("[%d] (%d) suspended by signal %d\n",
                               next_jid - 1, pid, WSTOPSIG(status));
                    } else if (WIFSIGNALED(status)) {
                        printf("(%d) terminated by signal %d\n", pid,
                               WTERMSIG(status));
                    }

                    // job is completed, return terminal control to shell
                    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
                        perror("tcsetpgrp error returning control to shell");
                    }
                } else {  // its a background job
                    add_job(job_list, next_jid, pid, RUNNING, command_path);
                    printf("[%d] (%d)\n", next_jid, pid);
                    next_jid++;
                }

            } else {
                perror("fork error");
            }
        }
    }
    return 0;
}
