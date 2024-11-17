    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <errno.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <string.h>
    #include <signal.h>

    #define MAX_VARS 100

    typedef struct {
        char name[50];
        char value[100];
    } ShellVariable;

    ShellVariable variables[MAX_VARS];
    int var_count = 0;
    pid_t child_pid = -1;  // Global variable to keep track of the child process ID
    int last_command_status = 0; // Variable to store the exit status of the last command

    void handle_sigint(int sig) {
        if (child_pid != -1) {
            // If a child process is running, kill it
            kill(child_pid, SIGINT);
        } else {
            // Print message if no child process is running
            printf("\nyou typed control-c\n");
            fflush(stdout);  // Ensure the message is displayed immediately
        }
    }

    void set_variable(char *name, char *value) {
        for (int i = 0; i < var_count; i++) {
            if (strcmp(variables[i].name, name) == 0) {
                strcpy(variables[i].value, value);
                return;
            }
        }
        if (var_count < MAX_VARS) {
            strcpy(variables[var_count].name, name);
            strcpy(variables[var_count].value, value);
            var_count++;
        } else {
            fprintf(stderr, "Error: Maximum number of variables reached.\n");
        }
    }

    char* get_variable(char *name) {
        for (int i = 0; i < var_count; i++) {
            if (strcmp(variables[i].name, name) == 0) {
                return variables[i].value;
            }
        }
        return NULL;
    }

    void replace_variables(char *command, int last_command_status) {
        char result[1024] = "";
        char temp[1024];
        char *token;
        char *value;

        strcpy(temp, command);
        token = strtok(temp, " ");
        while (token != NULL) {
            if (strcmp(token, "$?") == 0) {
                char status_str[20];
                sprintf(status_str, "%d", last_command_status);
                strcat(result, status_str);
            } else if (token[0] == '$') {
                value = get_variable(token + 1);
                if (value != NULL) {
                    strcat(result, value);
                } else {
                    strcat(result, token);
                }
            } else {
                strcat(result, token);
            }
            token = strtok(NULL, " ");
            if (token != NULL) {
                strcat(result, " ");
            }
        }
        strcpy(command, result);
    }

    int main() {
        char command[1024];
        char last_command[1024];
        char *token;
        int i;
        char *outfile = NULL;
        int fd, amper, redirect, append, piping, redirect_stderr, retid, status, argc1;
        int fildes[2];
        char *argv1[10], *argv2[10];

        // Set up the signal handler for SIGINT
        struct sigaction sa;
        sa.sa_handler = handle_sigint;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);

        while (1) {
            printf("hello: ");
            fgets(command, 1024, stdin);
            command[strlen(command) - 1] = '\0';
            // q7
            if (strcmp(command, "quit") == 0) {
                printf("Exiting shell...\n");
                break; // Terminate the shell process
            }
            // q6
                if (strcmp(command, "!!") == 0) {
                    if (strlen(last_command) > 0) {
                        strcpy(command, last_command);
                        printf("%s\n", command);
                } else {
                    printf("No previous command to repeat\n");
                        continue;
                }
            }
            strcpy(last_command, command); // Store the current command as the last command

   if (strncmp(command, "if ", 3) == 0) {
            char *condition = strtok(command + 3, "then");
            char *if_branch = strtok(NULL, "else");
            char *else_branch = strtok(NULL, "fi");

            // Remove leading and trailing whitespaces
            if_branch = strtok(if_branch, " \t");
            else_branch = strtok(else_branch, " \t");

            int status;
            pid_t pid = fork();

            if (pid == 0) {
                // Child process
                execl("/bin/sh", "sh", "-c", condition, (char *)NULL);
                perror("execl");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                // Parent process
                waitpid(pid, &status, 0);

                // Check the exit status of the condition command
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    // Condition is true: execute if branch
                    system(if_branch);
                } else {
                    // Condition is false: execute else branch
                    system(else_branch);
                }
            } else {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            continue;
        }





            // Check for variable assignment
            if (strchr(command, '=') != NULL) {
                token = strtok(command, " =");
                char *name = token + 1;  // Skip the '$' character
                token = strtok(NULL, " =");
                char *value = token;
                set_variable(name, value);
                continue;
            }
            
            // Check for read command
            if (strncmp(command, "read ", 5) == 0) {
                char *var_name = command + 5; // Get the variable name after "read "
                char input[100];
                printf(" ");
                fgets(input, 100, stdin);
                input[strlen(input) - 1] = '\0'; // Remove the newline character
                set_variable(var_name, input);
                continue;
            }
            
            // Check for echo $?
            if (strcmp(command, "echo $?") == 0) {
                printf("%d\n", last_command_status);
                continue;
            }
            
            replace_variables(command, last_command_status);

            
            piping = 0;
            redirect_stderr = 0;
            append = 0;
            redirect = 0;

            /* parse command line */
            i = 0;
            token = strtok(command, " ");
            while (token != NULL) {
                argv1[i] = token;
                token = strtok(NULL, " ");
                i++;
                if (token && !strcmp(token, "|")) {
                    piping = 1;
                    break;
                }
            }
            argv1[i] = NULL;
            argc1 = i;

            /* Is command empty */
            if (argv1[0] == NULL)
                continue;

            /* Does command contain pipe */
            if (piping) {
                i = 0;
                while (token != NULL) {
                    token = strtok(NULL, " ");
                    argv2[i] = token;
                    i++;
                }
                argv2[i] = NULL;
            }

            /* Check for '>>' for appending output to a file */
            for (int j = 0; j < argc1; j++) {
                if (strcmp(argv1[j], ">>") == 0) {
                    append = 1;
                    if (j + 1 < argc1) {
                        outfile = argv1[j + 1];
                        argv1[j] = NULL;  // Remove '>>' and filename from command
                        argc1 = j;  // Adjust argc1
                    } else {
                        fprintf(stderr, "Syntax error: no file specified for '>>'\n");
                        append = 0;
                    }
                    break;
                }
            }

            /* Check for '2>' for redirecting stderr to a file */
            for (int j = 0; j < argc1; j++) {
                if (strcmp(argv1[j], "2>") == 0) {
                    redirect_stderr = 1;
                    if (j + 1 < argc1) {
                        outfile = argv1[j + 1];
                        argv1[j] = NULL;  // Remove '2>' and filename from command
                        argc1 = j;  // Adjust argc1
                    } else {
                        fprintf(stderr, "Syntax error: no file specified for '2>'\n");
                        redirect_stderr = 0;
                    }
                    break;
                }
            }

            /* Does command line end with & */
            if (argc1 > 0 && !strcmp(argv1[argc1 - 1], "&")) {
                amper = 1;
                argv1[argc1 - 1] = NULL;
            } else
             {
                amper = 0;
            }

            if (argc1 > 1 && !strcmp(argv1[argc1 - 2], ">")) {
                redirect = 1
                ;
                argv1[argc1 - 2] = NULL;
                outfile = argv1[argc1 - 1];
            } else {
                redirect = 0;
            }

            /* for commands not part of the shell command language */
            if ((child_pid = fork()) == 0) {
                /* redirection of IO ? */
                if (append) {
                    fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
                    if (fd < 0) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    close(STDOUT_FILENO);
                    dup(fd);
                    close(fd);
                    /* stdout is now redirected */
                } else if (redirect) {
                    fd = creat(outfile, 0660);
                    if (fd < 0) {
                        perror("creat");
                        exit(EXIT_FAILURE);
                    }
                    close(STDOUT_FILENO);
                    dup(fd);
                    close(fd);
                    /* stdout is now redirected */
                } else if (redirect_stderr) {
                    fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                    if (fd < 0) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    close(STDERR_FILENO);
                    dup(fd);
                    close(fd);
                    /* stderr is now redirected */
                }
if (piping) {
    // Handle multiple pipes
    int num_pipes = 1; // Count the number of pipes
    int command_index = 0; // Index to track the current command
    int pipe_index = 0; // Index to track the current pipe
    int command_start = 0; // Index to mark the start of each command
    char *argv[MAX_VARS]; // Temporary array to hold command arguments

    // Loop through the command line arguments
    while (argv1[command_index] != NULL) {
        // Check for pipe symbol
        if (strcmp(argv1[command_index], "|") == 0) {
            // Set the end of the current command
            argv[command_index - command_start] = NULL;
            
            // Create a pipe
            pipe(fildes);

            // Fork a child process to execute the command
            if (fork() == 0) {
                // Close stdin and duplicate pipe write end to stdin
                close(STDIN_FILENO);
                dup(fildes[0]);
                close(fildes[0]);
                close(fildes[1]);

                // Execute the command
                execvp(argv[0], argv);
                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // Close stdout and duplicate pipe read end to stdout
            close(STDOUT_FILENO);
            dup(fildes[1]);
            close(fildes[0]);
            close(fildes[1]);

            // Move to the next command
            command_start = command_index + 1;
            pipe_index++;
        }

        // Store the command arguments
        argv[command_index - command_start] = argv1[command_index];

        // Move to the next argument
        command_index++;
    }

    // Execute the last command in the pipeline
    argv[command_index - command_start] = NULL;
    execvp(argv[0], argv);
    perror("execvp");
    exit(EXIT_FAILURE);
} else {
    // Execute a single command
    execvp(argv1[0], argv1);
    perror("execvp");
    exit(EXIT_FAILURE);
}

    }
    }
    }
        