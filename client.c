#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

//#define DEBUG
#include "debug.h"

struct client_config {
    char server_ip[1024];
    uint16_t server_port;
};

struct client_config read_config_from_filename(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        PERROR("File wasn't opened");
        exit(EXIT_FAILURE);
    }

    struct client_config result;
    if (fscanf(file, "%1023s", result.server_ip) != 1) {
        PERROR("Config file isn't correct");
        exit(EXIT_FAILURE);
    }

    if (fscanf(file, "%hu", &result.server_port) != 1) {
        PERROR("Config file isn't correct");
        exit(EXIT_FAILURE);
    }

    return result;
}

int main(int argc, char** argv, char** env) {

    struct client_config minifs_config = read_config_from_filename("minifs_config");

    LOG("ip = %s, port = %hu", minifs_config.server_ip, minifs_config.server_port);

    int is_stdin_needed = 0;
    if (argc >= 2 && !strcmp(argv[1], "-stdin")) {
        is_stdin_needed = 1;
    }

    int args_length = 0;
    for (int i = 1 + is_stdin_needed; i < argc; ++i) {
        args_length += strlen(argv[i]);
    }

    char* command_buffer = (char*) alloca (args_length + 0x10);
    memset(command_buffer, 0, (size_t)args_length + 0x10);

    for (int i = 1 + is_stdin_needed; i < argc; ++i) {
        strcat(command_buffer, argv[i]);
        if (i + 1 < argc) {
            strcat(command_buffer, " ");
        }
    }

    LOG("Command = %s", command_buffer);

    struct sockaddr_in server_address;

    server_address.sin_port = htons(minifs_config.server_port);
    server_address.sin_family = AF_INET;

    if (inet_pton(AF_INET, minifs_config.server_ip, &server_address.sin_addr) <= 0) {
        PERROR("Address isn't correct");
        exit(EXIT_FAILURE);
    }

    int socket_file_descriptor = 0;

    socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_file_descriptor < 0) {
        PERROR("Couldn't open socket");
        exit(EXIT_FAILURE);
    } else {
        LOG("Socket was opened with fd = %d", socket_file_descriptor);
    }

    if(connect(socket_file_descriptor, (struct sockaddr*)&server_address,
            sizeof(server_address)) < 0) {
        PERROR("Connect failed");
        exit(EXIT_FAILURE);
    } else {
        LOG("Connected to server");
    }

    int bytes_count = 0;
    int buffer_length = (int)strlen(command_buffer) + 1;

    while (bytes_count < buffer_length) {
        int write_result = (int)write(socket_file_descriptor,
                command_buffer + bytes_count, (size_t)(buffer_length - bytes_count));

        if (write_result < 0) {
            PERROR("Data wasn't sent");
            exit(EXIT_FAILURE);
        } else {
            bytes_count += write_result;
        }
    }

    if (is_stdin_needed) {
        char *buffer = (char *) alloca (1024);
        while (1) {

            int bytes_read = (int) read(0, buffer, 1024);
            if (bytes_read == 0) {
                break;
            } else if (bytes_read < 0) {
                PERROR("Read from stdin failed");
                break;
            }

            int bytes_write = 0;
            while (bytes_write < bytes_read) {
                int bytes_send = (int) write(socket_file_descriptor, buffer + bytes_write,
                                             (size_t) (bytes_read - bytes_write));
                if (bytes_send == 0) {
                    break;
                } else if (bytes_send < 0) {
                    PERROR("Send from stdin failed");
                } else {
                    bytes_write += bytes_send;
                }
            }
        }
    }

    shutdown(socket_file_descriptor, SHUT_WR);

    const int kBufferSize = 1024;

    char* result_buffer = (char*) alloca (kBufferSize);

    while (1) {
        int bytes_read = 0;
        bytes_read = (int)read(socket_file_descriptor, result_buffer, kBufferSize);
        if (bytes_read < 0) {
            PERROR("Read failed");
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            break;
        } else {
            write(1, result_buffer, (size_t)bytes_read);
        }
    }

    close(socket_file_descriptor);
    LOG("Socket was closed");
    return EXIT_SUCCESS;
}
