#include "client.h"

int call_method(ServerInfo *info, MethodRequest *req, MethodResponse *resp)
{
    struct socket *sock;

    if (sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock) < 0)
        return -1;
    
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr = { .s_addr = in_aton(info->ip) }, .sin_port = htons(info->port) };
    printk(KERN_INFO "addr: %s:%d", info->ip, info->port);

    if (kernel_connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in), 0) < 0)
    {
        printk(KERN_ERR "can't connect to server\n");
        sock_release(sock);
        return -1;
    }

    struct msghdr hdr;
    memset(&hdr, 0, sizeof(struct msghdr));

    struct kvec vec;
    vec.iov_base = req;
    vec.iov_len = sizeof(MethodRequest);

    if (kernel_sendmsg(sock, &hdr, &vec, 1, vec.iov_len) < 0)
    {
        printk(KERN_ERR "sendmsg error\n");
        kernel_sock_shutdown(sock, SHUT_RDWR);
        sock_release(sock);
        return -1;
    }

    memset(resp, 0, sizeof(MethodResponse));
    memset(&hdr, 0, sizeof(struct msghdr));

    vec.iov_base = resp;
    vec.iov_len = sizeof(MethodResponse);

    while (vec.iov_len > 0)
    {
        int recv = kernel_recvmsg(sock, &hdr, &vec, 1, vec.iov_len, 0);
        if (recv == 0)
            break;
        if (recv < 0)
        {
            printk(KERN_ERR "recvmsg err\n");
            kernel_sock_shutdown(sock, SHUT_RDWR);
            sock_release(sock);
            return -1;
        }
        vec.iov_base += recv;
        vec.iov_len -= recv;
    }

    kernel_sock_shutdown(sock, SHUT_RDWR);
    sock_release(sock);

    return 0;
}