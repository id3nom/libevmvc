

#ifndef BBACT_STABLE_HEADERS_H
#define BBACT_STABLE_HEADERS_H

// #include <stdio.h>      /* Standard I/O functions */
// #include <stdlib.h>     /* Prototypes of commonly used library functions,
//                            plus EXIT_SUCCESS and EXIT_FAILURE constants */
#include <errno.h>      /* Declares errno and defines error constants */

#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/prctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <signal.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include <memory>
#include <vector>
#include <functional>

#include <strings.h>



void error_exit(std::string msg, int code = -1)
{
    std::cerr << msg << std::endl;
    std::exit(code);
}

ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char* ptr;
    
    ptr = (const char*)vptr;
    nleft = n;
    while(nleft > 0){
        if((nwritten = write(fd, ptr, nleft)) <= 0){
            if (errno == EINTR)
                nwritten = 0;/* and call write() again */
            else
                return(-1);/* error */
        }
        
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}


#endif//BBACT_STABLE_HEADERS_H