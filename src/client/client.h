#ifndef _CLIENT_H
#define _CLIENT_H

#include <linux/inet.h>

#include "../shared/protocol.h"

typedef struct ServerInfo
{
    char *ip;
    uint16_t port;
} ServerInfo;

int call_method(ServerInfo *info, MethodRequest *req, MethodResponse *resp);

#endif