#ifndef BBACT_WORKER_H
#define BBACT_WORKER_H

#include "stable_headers.h"
#include "globals.h"
#include "sockets_util.h"

#define PIPE_WRITE_FD 1
#define PIPE_READ_FD 0

class channel
{
public:
    int ptoc[2] = {0,0};
    int ctop[2] = {0,0};
    struct event* rcmd_ev = nullptr;
    // access control messages (ancillary data) pipe
    //int acm[2] = {0,0};
    int usock = -1;
    std::string usock_path = "";
    struct event* rcmsg_ev = nullptr;
};

class worker_t
{
public:
    int wid = -1;
    int pid = -1;
    channel chan;
};

typedef std::shared_ptr<worker_t> worker;

std::vector<worker>& workers()
{
    static std::vector<worker> _workers;
    return _workers;
}


void worker_recv_sock(worker_t* w, int sock)
{
    socket_set_opts(sock);
    event* sev = event_new(
        ev_base(), sock, EV_READ | EV_WRITE | EV_PERSIST,
    [](int fd, short events, void* args)->void{
        worker_t* w = (worker_t*)args;
        
        if((events & EV_READ) != EV_READ)
            return;
        
        char buf[4096];
        while(true){
            ssize_t len = read(fd, buf, 4096);
            if(len == -1){
                int err = errno;
                if(err == EAGAIN || err == EWOULDBLOCK)
                    break;
                error_exit("worker sock read: " + std::to_string(err));
            }
            if(len == 0)
                return;
            if(len > 0){
                std::clog << "Worker: " <<  w->wid
                    << " recv:\n~~~\n" << (const char*)buf
                    << "\n~~~\n\n";
            }
        }
        
        std::string msg(
            "<html><body><h1>It works! from worker: " +
            std::to_string(w->wid) +
            "</h1></body></html>"
        );
        
        std::string hdr(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: " + std::to_string(msg.size()) + "\r\n"
            "\r\n"
        );
        hdr += msg;
        
        writen(fd, hdr.c_str(), hdr.size());
    }, w);
    event_add(sev, nullptr);
    
    // struct bufferevent* bev = bufferevent_socket_new(
    //     ev_base(), sock,
    //     BEV_OPT_CLOSE_ON_FREE
    // );
    // bufferevent_setcb(bev,
    //     // readcb
    //     [](struct bufferevent* bev, void* ctx)->void{
    //         if(bev == nullptr)
    //             return;
            
    //         worker_t* w = (worker_t*)ctx;
            
    //         while(true){
    //             size_t avail =
    //                 evbuffer_get_length(bufferevent_get_input(bev));
    //             if(avail == 0)
    //                 break;
                
    //             void* buf = evbuffer_pullup(
    //                 bufferevent_get_input(bev), avail
    //             );
                
    //             std::clog << "Worker: " <<  w->wid
    //                 << " recv:\n~~~\n" << (const char*)buf
    //                 << "\n~~~\n\n";
                
    //             evbuffer_drain(bufferevent_get_input(bev), avail);
    //         }
            
    //         std::string msg(
    //             "<html><body><h1>It works! from worker: " +
    //             std::to_string(w->wid) +
    //             "</h1></body></html>"
    //         );
            
    //         std::string hdr(
    //             "HTTP/1.1 200 OK\r\n"
    //             "Content-Type: text/html; charset=UTF-8\r\n"
    //             "Content-Length: " + std::to_string(msg.size()) + "\r\n"
    //             "\r\n"
    //         );
    //         hdr += msg;
            
    //         bufferevent_write(bev, hdr.c_str(), hdr.size());
    //     },
        
    //     // writecb
    //     nullptr,
    //     // [](struct bufferevent* bev, void* ctx)->void{
    //     //     //bufferevent_free(bev);
            
    //     // },
        
    //     // eventcb
    //     [](struct bufferevent* bev, short what, void* ctx)->void{
            
    //     },
        
    //     w
    // );
    // bufferevent_enable(bev, EV_READ);
}

int start_worker(worker w)
{
    // will send a SIGINT when the parent dies
    prctl(PR_SET_PDEATHSIG, SIGINT);
    int ppid = getppid();
    std::cout << "starting worker\n parent pid: " << ppid << "\n";
    // init event_base
    ev_base();
    
    // on read command
    w->chan.rcmd_ev = event_new(
        ev_base(), w->chan.ptoc[PIPE_READ_FD],
        EV_READ | EV_PERSIST,
    [](int fd, short events, void* args)->void{
        worker_t* w = (worker_t*)args;
        
        char buf[4096];
        while(true){
            ssize_t len = read(fd, buf, 4096);
            if(len == -1){
                int err = errno;
                if(err == EAGAIN || err == EWOULDBLOCK)
                    break;
                
                std::cerr << "read failed with: " << err << "\n";
                raise(SIGINT);
                return;
            }
            if(len > 0){
                std::cout << "recv from listener: '" << 
                    std::string(buf, len)
                    << "'\n";
                std::string msg("tnks listener for the msg");
                writen(w->chan.ctop[PIPE_WRITE_FD], msg.c_str(), msg.size());
            }
            if(len == 0)
                break;
        }
    }, w.get());
    event_add(w->chan.rcmd_ev, nullptr);
    
    
    // on read cmsg
    w->chan.rcmsg_ev = event_new(
        ev_base(), w->chan.usock, EV_READ | EV_PERSIST,
    [](int fd, short events, void* args)->void{
        worker_t* w = (worker_t*)args;
        
        while(true){
            int data;
            struct msghdr msgh;
            struct iovec iov;
            
            union
            {
                char buf[CMSG_SPACE(sizeof(int))];
                struct cmsghdr align;
            } ctrl_msg;
            struct cmsghdr* cmsgp;
            
            msgh.msg_name = NULL;
            msgh.msg_namelen = 0;
            
            msgh.msg_iov = &iov;
            msgh.msg_iovlen = 1;
            iov.iov_base = &data;
            iov.iov_len = sizeof(int);
            
            msgh.msg_control = ctrl_msg.buf;
            msgh.msg_controllen = sizeof(ctrl_msg.buf);
            
            int nr = recvmsg(fd, &msgh, 0);
            if(nr == 0)
                return;
            if(nr == -1){
                int terrno = errno;
                if(terrno == EAGAIN || terrno == EWOULDBLOCK)
                    return;
                
                error_exit("recvmsg: " + std::to_string(terrno));
            }
            
            if (nr > 0)
                fprintf(stderr, "Received data = %d\n", data);
            
            cmsgp = CMSG_FIRSTHDR(&msgh);
            /* Check the validity of the 'cmsghdr' */
            if (cmsgp == NULL || cmsgp->cmsg_len != CMSG_LEN(sizeof(int)))
                error_exit("bad cmsg header / message length");
            if (cmsgp->cmsg_level != SOL_SOCKET)
                error_exit("cmsg_level != SOL_SOCKET");
            if (cmsgp->cmsg_type != SCM_RIGHTS)
                error_exit("cmsg_type != SCM_RIGHTS");
            
            int sock = *((int*)CMSG_DATA(cmsgp));
            worker_recv_sock(w, sock);
        }
        
    }, w.get());
    event_add(w->chan.rcmsg_ev, nullptr);


    
    event_base_loop(ev_base(), 0);
    // event_del(tev);
    // event_free(tev);
    event_del(w->chan.rcmd_ev);
    event_free(w->chan.rcmd_ev);
    event_base_free(ev_base());

    std::cout << "worker closing\n";
    
    return 0;
}

#endif//BBACT_WORKER_H