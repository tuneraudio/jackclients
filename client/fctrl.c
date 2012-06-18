#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK_PATH "command_socket"
#define BUFF_SIZE (1 << 6)

static inline ssize_t
prompt(char *buf, size_t size)
{
    printf("command> ");
    fgets(buf, size, stdin);
    return buf ? (ssize_t)strlen(buf) : -1;
}

int
main(void)
{
    struct sockaddr_un remote;
    char command[BUFF_SIZE];
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* assign the socket path */
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    size_t len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(fd, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    ssize_t ret;
    while ((ret = prompt(command, BUFF_SIZE))) {
        /* remove \n character */
        command[--ret] = '\0';

        /* send message */
        if (send(fd, command, ret, 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }

        ret = recv(fd, command, BUFF_SIZE, 0);

        if (ret == 0)
            break;
        else if (ret == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        command[ret] = '\0';
        printf("status> %s\n", command);
    }

    printf("Server closed connection\n");
    close(fd);
    return 0;
}

// vim: et:sts=4:sw=4:cino=(0
