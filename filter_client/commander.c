#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK_PATH "command_socket"

int main(void)
{
    int s, len;
    socklen_t t;
    struct sockaddr_un remote;
    char command[64];
    char *nl;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    printf("Trying to connect...\n");

    /* assign the socket path */
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Socket Connected.\n");

    while(printf("command> "), fgets(command, sizeof(command), stdin)) {
        /* remove NL character */
        nl = strchr(command,'\n');
        if (nl != NULL)
            *nl = '\0';

        /* implement API interface here? */

        /* send message */
        if (send(s, command, strlen(command), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }

        if ((t=recv(s, command, 100, 0)) > 0) {
            command[t] = '\0';
            printf("status> %s\n", command);
        } else {
            if (t < 0)
                perror("recv");
            else
                printf("Server closed connection\n");
            exit(EXIT_FAILURE);
        }
    }

    close(s);

    return 0;
}
