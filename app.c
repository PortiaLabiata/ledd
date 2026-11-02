#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "protocol.h"

#define SENDSOCK_NAME "/run/ledsend.sock"

int set_command(const char *cmd, const char *arg, Datagram *dgram)
{
    if (strcmp(cmd, "on") == 0)
    {
        if (!arg)
        {
            dgram->value.argument = 0;
        }
        else
        {
            dgram->value.argument = atoi(arg);
        }
        dgram->value.command = COMMAND_ON;
    }
    else if (strcmp(cmd, "off") == 0)
    {
        dgram->value.command = COMMAND_OFF;
        dgram->value.argument = 0;
    }
    else if (strcmp(cmd, "blink") == 0)
    {
        if (!arg)
        {
            return -1;
        }
        dgram->value.command = COMMAND_BLINK;
        dgram->value.argument = atoi(arg);
    }
    else
    {
        return -1;
    }
    return 0;
}

void cleanup(int sockfd)
{
    if (sockfd > 0)
    {
        close(sockfd);
        unlink(SENDSOCK_NAME);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Invalid number of arguments: at least two needed\n");
        return -1;
    }

    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to init socket");
        return -1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    strcpy(addr.sun_path, SENDSOCK_NAME);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("Failed to bind socket");
        cleanup(sockfd);
        return -1;
    }

    Datagram dgram = {0};

    if (set_command(argv[1], argc > 2 ? argv[2] : NULL, &dgram) < 0)
    {
        fprintf(stderr, "Invalid argument\n");
        cleanup(sockfd);
        return -1;
    }

    struct sockaddr_un peer_addr = {
        .sun_family = AF_UNIX
    };
    strcpy(peer_addr.sun_path, SOCK_NAME);

    if (sendto(sockfd, dgram.buffer, sizeof(dgram.buffer), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
    {
        perror("Failed to send command");
        cleanup(sockfd);
        return -1;
    }

    cleanup(sockfd);

    return 0;
}
