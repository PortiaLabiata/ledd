#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "protocol.h"

void draw_form()
{
    puts("<form method=\"get\">");
        puts("<label for=\"command\">Command</label><br>");
        puts("<input id=\"command\" type=\"radio\" name=\"command\" value=\"on\">On</input><br>");
        puts("<input id=\"command\" type=\"radio\" name=\"command\" value=\"off\">Off</input><br>");
        puts("<input id=\"command\" type=\"radio\" name=\"command\" value=\"blink\">Blink</input><br>");

        puts("<label for=\"argument\">Argument</label>");
        puts("<input id=\"argument\" type=\"text\" name=\"argument\"></input>");
        puts("<br><input type=\"submit\" value=\"Send\"></input>");
    puts("</form>");
}

typedef enum {
    KEY,
    VALUE,
} ParserState;

Command str2cmd(const char *s)
{
    if (strcmp(s, "on") == 0)
    {
        return COMMAND_ON;
    }
    else if (strcmp(s, "off") == 0)
    {
        return COMMAND_OFF;
    }
    else if (strcmp(s, "blink") == 0)
    {
        return COMMAND_BLINK;
    }
    else
    {
        return COMMAND_INVALID;
    }
}

int handle_get(char *request, Command *cmd, unsigned int *arg)
{
    static const size_t MAX_SUBTOKEN_LEN = 64;

    char *str1, *str2;
    char *save1, *save2;

    for (str1 = request; ; str1 = NULL)
    {
        ParserState state = KEY;
        char *token = strtok_r(str1, "&", &save1);
        if (!token)
        {
            break;
        }

        char key[MAX_SUBTOKEN_LEN];
        char value[MAX_SUBTOKEN_LEN];

        memset(key, 0, sizeof(key));
        memset(value, 0, sizeof(value));

        for (str2 = token; ; str2 = NULL)
        {
            char *subtoken = strtok_r(str2, "=", &save2);
            if (!subtoken)
            {
                break;
            }

            if (state == KEY)
            {
                state = VALUE;
                strncpy(key, subtoken, MAX_SUBTOKEN_LEN);
            }
            else
            {
                state = KEY;
                strncpy(value, subtoken, MAX_SUBTOKEN_LEN);
            }
        }

        if (strcmp(key, "command") == 0)
        {
            *cmd = str2cmd(value); 
        }
        else if (strcmp(key, "argument") == 0)
        {
            *arg = atoi(value);            
        }
    }

    return 0;
}

int send_command(Command cmd, unsigned int arg)
{
    static const char *SENDSOCK_NAME = "/run/ledserv.sock";

    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        return -1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    strcpy(addr.sun_path, SENDSOCK_NAME);

    unlink(SENDSOCK_NAME);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(sockfd);
        unlink(SENDSOCK_NAME);
        return -1;
    }

    Datagram dgram = {
        .value = {
            .command = cmd,
            .argument = arg
        }
    };

    struct sockaddr_un peer_addr = {
        .sun_family = AF_UNIX
    };
    strcpy(peer_addr.sun_path, SOCK_NAME);

    if (sendto(sockfd, dgram.buffer, DGRAM_SIZE, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
    {
        close(sockfd);
        unlink(SENDSOCK_NAME);
        return -1;
    }

    close(sockfd);
    unlink(SENDSOCK_NAME);
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    printf("Content-type: text/html\n\n");

    // Render form either way
    draw_form();

    const char *method = getenv("REQUEST_METHOD");
    if (!method)
    {
        return 0;
    }

    if (strcmp(method, "GET") == 0)
    {
        char *request = getenv("QUERY_STRING");

        Command cmd = COMMAND_INVALID;
        unsigned int arg = 0;

        if (!request)
        {
            printf("Failed to read request\n");
        } 
        else if (handle_get(request, &cmd, &arg) < 0 &&
                cmd != COMMAND_INVALID)
        {
            printf("Failed to execute command: %s<br>",
                   strerror(errno));
        }
        else if (send_command(cmd, arg) < 0)
        {
            printf("Failed to send command: %s<br>",
                   strerror(errno));
        }
    }
    
    return 0;
}
