#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <ctype.h>

//#define DEBUG
#define CLIENTS_QUEUE_LENGTH 1024
#define BUFFER_SIZE 4096
#define PORT 8971

#include "debug.h"

#define ANSWER_FIRST hello
#define ANSWER_SECOND world

void exec_command(char* command, int fd_output) {
    int n_commands = 0;
    for (int i = 0; i < strlen(command); ++i) {
        if (i == 0 || (command[i - 1] == ' ' && !isspace(command[i]))) {
            ++n_commands;
        }
    }

    char** new_argv = (char**) calloc (sizeof(char*), (size_t)n_commands + 1);
    char* command_ptr = command;

    for (int command_index = 0; ; ++command_index) {
        char* command_end = command_ptr;
        while (*command_end != 0 && *command_end != ' ') {
            ++command_end;
        }

        new_argv[command_index] = (char*) calloc (sizeof(char), command_end - command_ptr + 1);
        strncpy(new_argv[command_index], command_ptr, command_end - command_ptr);

        command_ptr = command_end;
        while(*command_ptr == ' ') {
            ++command_ptr;
        }

        if (*command_ptr == 0) {
            break;
        }
    }

    char* stdin_overwrite = NULL;
    char* stdout_overwrite = NULL;

    char** argv_to_pass = (char**) calloc (sizeof(char*), (size_t)n_commands + 1);
    int argv_to_pass_counter = 0;

    for (int i = 0; new_argv[i] != NULL; ++i) {
        if (!strcmp(new_argv[i], ">")) {
            if (new_argv[i + 1] != NULL) {
                stdout_overwrite = new_argv[i + 1];
                ++i;
            }
        } else if (!strcmp(new_argv[i], "<")) {
            if (new_argv[i + 1] != NULL) {
                stdin_overwrite = new_argv[i + 1];
                ++i;
            }
        } else {
            argv_to_pass[argv_to_pass_counter] = new_argv[i];
            ++argv_to_pass_counter;
        }
    }

    if (!strcmp(new_argv[0], "cd") || !strcmp(new_argv[0], "ls") ||
        !strcmp(new_argv[0], "mkdir") || !strcmp(new_argv[0], "cp") || !strcmp(new_argv[0], "rm") ||
        !strcmp(new_argv[0], "mv") || !strcmp(new_argv[0], "cat") || !strcmp(new_argv[0], "echo") ||
        !strcmp(new_argv[0], "tee") || !strcmp(new_argv[0], "touch")) {

        if (stdin_overwrite == NULL) {
            dup2(fd_output, 0);
        } else {
            int new_stdin_fd = open(stdin_overwrite, O_RDONLY, 0666);
            if (new_stdin_fd > 0) {
                dup2(new_stdin_fd, 0);
            } else {
                dup2(fd_output, 0);
            }
        }

        if (stdout_overwrite == NULL) {
            dup2(fd_output, 1);
        } else {
            int new_stdout_fd = open(stdout_overwrite, O_WRONLY | O_CREAT | O_DSYNC, 0666);
            if (new_stdout_fd > 0) {
                dup2(new_stdout_fd, 1);
            } else {
                dup2(fd_output, 1);
            }
        }

        execvp(argv_to_pass[0], argv_to_pass);
    } else {
        errno = EPERM;
        PERROR("Unknown command \"%s\"", argv_to_pass[0]);
    }
}

int main(int argc, char** argv, char** env) {

    int listen_socket_descriptor = 0;
    int connection_socket_descriptor = 0;
    struct sockaddr_in server_address = {};

    listen_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_descriptor <= 0) {
        PERROR("Socket wasn't opened");
        exit(EXIT_FAILURE);
    } else {
        LOG("Socket was opened, fd = %d", listen_socket_descriptor);
    }

    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);
    server_address.sin_family = AF_INET;

    if (bind(listen_socket_descriptor, (struct sockaddr*)&server_address, sizeof(server_address))
            != EXIT_SUCCESS) {
        PERROR("Listen failed");
        exit(EXIT_FAILURE);
    } else {
        LOG("Bind done");
    }

    listen(listen_socket_descriptor, CLIENTS_QUEUE_LENGTH);

    while (1) {
        connection_socket_descriptor = accept(listen_socket_descriptor, (struct sockaddr*)NULL, NULL);
        if (connection_socket_descriptor <= 0) {
            PERROR("Accept failed");
            break;
        } else {

            int pid = fork();

            if (pid == EXIT_SUCCESS) {
                LOG("Server forked");

                char* command_buffer = (char*) alloca (BUFFER_SIZE);
                memset(command_buffer, 0, BUFFER_SIZE);

                int bytes_read = 0;
                while (bytes_read < BUFFER_SIZE) {
                    int read_result = (int)read(connection_socket_descriptor,
                            command_buffer + bytes_read, 1);

                    if (read_result < 0) {
                        PERROR("READ FAILED");
                    } else if (read_result == 0 || command_buffer[bytes_read] == 0) {
                        break;
                    } else {
                        bytes_read += read_result;
                    }
                }

                LOG("Command: \"%s\"", command_buffer);
                exec_command(command_buffer, connection_socket_descriptor);

                LOG("Forked server finished");
                close(connection_socket_descriptor);

                break;
            } else if (pid > 0) {
                close(connection_socket_descriptor);
                LOG("Let's wait later");
            } else {
                close(connection_socket_descriptor);
                PERROR("Fork failed");
            }

        }
    }

    close(listen_socket_descriptor);
    return EXIT_SUCCESS;
}