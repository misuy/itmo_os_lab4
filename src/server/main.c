#include "../shared/protocol.h"
#define _GNU_SOURCE
#include "fs.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_CONNECTIONS 1

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("usage: server {root-path} {port}\n");
        return -1;
    }

    FS fs;
    if (fs_init(argv[1], &fs) < 0)
    {
        printf("can't init fs\n");
        return -1;
    }

    uint16_t port = atoi(argv[2]);

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("can't create socket\n");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        printf("can't bind socket\n");
        return -1;
    }

    if (listen(sockfd, MAX_CONNECTIONS) < 0)
    {
        printf("socket can't listen\n");
        return -1;
    }

    printf("server starting...\n");
    while (1)
    {
        accept_new_conn:
        int connfd = accept(sockfd, 0, 0);
        if (connfd < 0)
        {
            printf("accpet error\n");
            goto accept_new_conn;
        }
        printf("got connection\n");
        MethodRequest req;
        MethodResponse resp;
        while (1)
        {
            memset(&req, 0, sizeof(MethodRequest));
            memset(&resp, 0, sizeof(MethodResponse));
            uint32_t to_read = sizeof(MethodRequest);
            while (to_read > 0)
            {
                int len = read(connfd, &req + (sizeof(MethodRequest) - to_read), to_read);
                if (len < 0)
                {
                    printf("reading err\n");
                    goto accept_new_conn;
                }
                to_read -= len;
            }
            printf("got request\n");
            fs_handle(&fs, &req, &resp);
            uint32_t to_write = sizeof(MethodResponse);
            while (to_write > 0)
            {
                int len = write(connfd, &resp + (sizeof(MethodResponse) - to_write), to_write);
                if (len < 0)
                {
                    printf("writing err\n");
                    goto accept_new_conn;
                }
                to_write -= len;
            }
            printf("sent response\n");
        }
    }
}