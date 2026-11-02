#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/poll.h>
#include <sys/un.h>

#include "led.h"
#include "protocol.h"

typedef enum {
    BLINKING,
    IDLE,
} State;

typedef struct {
    State state;
    int timerfd;
    Value value;
} Context;

static volatile sig_atomic_t term_flag = 0;

static void cleanup(int sockfd)
{
    led_write(0);
    led_end();
    
    if (sockfd > 0)
    {
        close(sockfd);
        unlink(SOCK_NAME);
    }
}

void sighandler(int signum)
{
    (void)signum;
    term_flag = 1;
}

void tms2timespec(struct timespec *ts, unsigned int tms)
{
    ts->tv_sec = tms / 1000;
    ts->tv_nsec = (tms % 1000) * 1000000;
}

int timer_disarm(int timerfd)
{
    assert(timerfd > 0);
    struct itimerspec ts = {
        .it_value = {
            .tv_sec = 0,
            .tv_nsec = 0
        },
        .it_interval = {
            .tv_nsec = 0,
            .tv_sec = 0
        }
    };

    int ret = timerfd_settime(timerfd, 0, &ts, NULL);
    if (ret < 0)
    {
        perror("Failed to set timer period");
        return -1;
    }
    return 0;
}

int handle_dgram(Context *ctx, Datagram *dgram)
{
    switch (dgram->value.command)
    {

    case COMMAND_BLINK:
    {
        if (dgram->value.argument == 0)
        {
            errno = EINVAL;
            return -1;
        }
        ctx->state = BLINKING;

        struct timespec dt = {0};
        tms2timespec(&dt, dgram->value.argument);

        struct itimerspec ts = {
            .it_value = dt,
            .it_interval = dt,
        };
        return timerfd_settime(ctx->timerfd, 0, &ts, NULL);
    }

    case COMMAND_ON:
    {
        if (dgram->value.argument != 0)
        {
            struct timespec dt = {0};
            tms2timespec(&dt, dgram->value.argument);

            struct itimerspec ts = {
                .it_value = dt,
                .it_interval = {
                    .tv_sec = 0,
                    .tv_nsec = 0
                }
            };

            if (timerfd_settime(ctx->timerfd, 0, &ts, NULL) < 0)
            {
                return -1;
            }
        }
        ctx->value = 1;
        ctx->state = IDLE;
        return 0;
    }

    case COMMAND_OFF:
    {
        if (dgram->value.argument != 0)
        {
            errno = EINVAL;
            return -1;
        }
        ctx->value = 0;
        ctx->state = IDLE;
        return 0;
    }

    default:
        errno = ENOTSUP;
        return -1;
    }
}

static int init_socket()
{
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to open socket");
        return -1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };
    strcpy(addr.sun_path, SOCK_NAME);

    unlink(SOCK_NAME);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("Failed to bind socket");
        return -1;
    }

    return sockfd;
}

int init_timer()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0)
    {
        perror("Failed to create timerfd");
        return -1;
    }

    // Disarm timer
    if (timer_disarm(timerfd) < 0)
    {
        return -1;
    }
    
    return timerfd;
}

void handle_timer(Context *ctx)
{
    if (ctx->state == BLINKING)
    {
        ctx->value = ctx->value ? 0 : 1;
    }
    else
    {
        ctx->value = 0;
    }
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    if (led_begin() < 0)
    {
        return -1;
    }

    Context ctx = {
        .state = IDLE,
        .value = 0,
        .timerfd = -1
    };

    int sockfd = init_socket();
    if (sockfd < 0)
    {
        cleanup(sockfd);
        return -1;
    }

    ctx.timerfd = init_timer();
    if (ctx.timerfd < 0)
    {
        cleanup(sockfd);
        return -1;
    }

    Datagram dgram = {0};

    struct pollfd fds[2] = {0};
    fds[0].fd = sockfd;
    fds[1].fd = ctx.timerfd;
    fds[0].events = POLLIN;
    fds[1].events = POLLIN;

    while (!term_flag)
    {
        int nfds = poll(fds, 2, -1);
        if (nfds > 0)
        {
            if (fds[1].revents & POLLIN)
            {
                // Timer expired
                fds[1].revents = 0;

                unsigned char buf[8] = {0};
                if (read(ctx.timerfd, buf, 8) < 0)
                {
                    perror("Failed to read timer buffer");
                    break;
                }
                handle_timer(&ctx);
            }

            if (fds[0].revents & POLLIN)
            {
                // Got data from socket
                fds[0].revents = 0;
                if (recv(sockfd, dgram.buffer, DGRAM_SIZE, 0) < (int)DGRAM_SIZE)
                {
                    perror("Failed reading from socket");
                }

                (void)handle_dgram(&ctx, &dgram);
            }
            led_write(ctx.value);
        }
    }
    
    cleanup(sockfd);

    return 0;
}
