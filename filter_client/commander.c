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

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	perror("socket");
	exit(1);
    }

    printf("Trying to connect...\n");
 
    /* assign the socket path */
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
	perror("connect");
	exit(1);
    }

    printf("Socket Connected.\n");

    while(printf("command> "), fgets(command, sizeof(command), stdin)) {

	/* send message */
	/* implement API interface here? */
	
	if (send(s, command, strlen(command), 0) == -1) {
	    perror("send");
	    exit(1);
	}

	if ((t=recv(s, command, 100, 0)) > 0) {
	    command[t] = '\0';
	    printf("echo> %s", command);
	} else {
	    if (t < 0) perror("recv");
	    else printf("Server closed connection\n");
	    exit(1);
	}
    }

    close(s);

    return 0;
}
