#ifndef BBACT_SOCKETS_util_H
#define BBACT_SOCKETS_util_H

#include "stable_headers.h"

void socket_set_opts(int sock)
{
    evutil_make_socket_closeonexec(sock);
    evutil_make_socket_nonblocking(sock);
    
    int on = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on)) == -1)
        return std::exit(-1);
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on)) == -1)
        return std::exit(-1);
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
        (void*)&on, sizeof(on)) == -1
    ){
        if(errno != EOPNOTSUPP)
            return std::exit(-1);
        std::clog << "SO_REUSEPORT NOT SUPPORTED\n";
    }
    if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
        (void*)&on, sizeof(on)) == -1
    ){
        if(errno != EOPNOTSUPP)
            return std::exit(-1);
        std::clog << "TCP_NODELAY NOT SUPPORTED\n";
    }
    if(setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, 
        (void*)&on, sizeof(on)) == -1
    ){
        if(errno != EOPNOTSUPP)
            return std::exit(-1);
        std::clog << "TCP_DEFER_ACCEPT NOT SUPPORTED\n";
    }

}


#endif //BBACT_SOCKETS_util_H
