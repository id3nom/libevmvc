/*
MIT License

Copyright (c) 2019 Michel Dénommée

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _libevmvc_unix_socket_utils_h
#define _libevmvc_unix_socket_utils_h

#include "stable_headers.h"

#include <sys/socket.h>
#include <sys/un.h>

namespace evmvc { namespace _internal {

int unix_build_address(const char *path, struct sockaddr_un *addr);
int unix_connect(const char *path, int type);
int unix_bind(const char *path, int type);

int unix_build_address(const char *path, struct sockaddr_un *addr)
{
    if(addr == NULL || path == NULL){
        errno = EINVAL;
        return -1;
    }
    
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    if(strlen(path) < sizeof(addr->sun_path)){
        strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
        return 0;
        
    }else{
        errno = ENAMETOOLONG;
        return -1;
    }
}

int unix_connect(const char *path, int type)
{
    int sd, savedErrno;
    struct sockaddr_un addr;

    if(unix_build_address(path, &addr) == -1)
        return -1;

    sd = socket(AF_UNIX, type, 0);
    if(sd == -1)
        return -1;

    if(connect(sd, (struct sockaddr *) &addr,
                sizeof(struct sockaddr_un)) == -1){
        savedErrno = errno;
        close(sd);                      /* Might change 'errno' */
        errno = savedErrno;
        return -1;
    }

    return sd;
}

int unix_bind(const char *path, int type)
{
    int sd, savedErrno;
    struct sockaddr_un addr;

    if(unix_build_address(path, &addr) == -1)
        return -1;

    sd = socket(AF_UNIX, type, 0);
    if(sd == -1)
        return -1;

    if(bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1){
        savedErrno = errno;
        close(sd);                      /* Might change 'errno' */
        errno = savedErrno;
        return -1;
    }

    return sd;
}

void unix_set_sock_opts(int sock)
{
    evutil_make_socket_closeonexec(sock);
    evutil_make_socket_nonblocking(sock);
    
    int on = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on)) == -1)
        return std::exit(-1);
    // if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on)) == -1)
    //     return std::exit(-1);
    // if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
    //     (void*)&on, sizeof(on)) == -1
    // ){
    //     if(errno != EOPNOTSUPP)
    //         return std::exit(-1);
    //     std::clog << "SO_REUSEPORT NOT SUPPORTED\n";
    // }
    // if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
    //     (void*)&on, sizeof(on)) == -1
    // ){
    //     if(errno != EOPNOTSUPP)
    //         return std::exit(-1);
    //     std::clog << "TCP_NODELAY NOT SUPPORTED\n";
    // }
    // if(setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, 
    //     (void*)&on, sizeof(on)) == -1
    // ){
    //     if(errno != EOPNOTSUPP)
    //         return std::exit(-1);
    //     std::clog << "TCP_DEFER_ACCEPT NOT SUPPORTED\n";
    // }
}


}}//::evmvc::unix
#endif//_libevmvc_unix_socket_utils_h