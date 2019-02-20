

#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
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

struct event_base* _ev_base = nullptr;
//bool _run = true;

#define PIPE_WRITE_FD 1
#define PIPE_READ_FD 0

class channel
{
public:
    int ptoc[2] = {0,0};
    int ctop[2] = {0,0};
    // access control messages (ancillary data) pipe
    int acm[2] = {0,0};
    struct event* ev = nullptr;
};

class worker_t
{
public:
    int wid = -1;
    int pid = -1;
    channel chan;
};

typedef std::shared_ptr<worker_t> worker;
std::vector<worker> workers;

void close_children();
void start_listener();
int start_worker(worker w);
void exiting();
void sig_received(int sig);

int main(int argc, char** argv)
{
    signal(SIGINT, sig_received);
    signal(SIGKILL, sig_received);
    
    int worker_count = 4;
    //int worker_count = 1;
    
    for(int i = 0; i < worker_count; ++i){
        worker w = std::make_shared<worker_t>();
        w->wid = workers.size();
        
        pipe(w->chan.acm);
        pipe(w->chan.ptoc);
        pipe(w->chan.ctop);
        
        w->pid = fork();
        if(w->pid == -1){// error
            std::cerr << "unable to create child\n";
            close_children();
            return -1;
            
        }else if(w->pid == 0){// is child proc
            workers.clear();
            
            //TODO: must be converted to unix socket...
            fcntl(w->chan.acm[PIPE_READ_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.acm[PIPE_WRITE_FD]);
            
            fcntl(w->chan.ptoc[PIPE_READ_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ptoc[PIPE_WRITE_FD]);
            fcntl(w->chan.ctop[PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ctop[PIPE_READ_FD]);
            
            int wres = start_worker(w);
            return wres;
            
        }else{// is main proc
            //TODO: must be converted to unix socket...
            fcntl(w->chan.acm[PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.acm[PIPE_READ_FD]);
            
            fcntl(w->chan.ptoc[PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ptoc[PIPE_READ_FD]);
            fcntl(w->chan.ctop[PIPE_READ_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ctop[PIPE_WRITE_FD]);
            
            workers.emplace_back(w);
        }
    }
    
    std::atexit(exiting);
    std::at_quick_exit(exiting);
    start_listener();
    close_children();
    
    return 0;
}

void close_children()
{
    for(auto el : workers)
        kill(el->pid, SIGINT);
    workers.clear();
}

void exiting()
{
    std::clog << "exiting\n";
    close_children();
}

void sig_received(int sig)
{
    if(sig == SIGINT)
        //_run = false;
        event_base_loopbreak(_ev_base);
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


void on_listen()
{
    
}

void start_listener()
{
    std::cout << "starting listener\n";
    
    event_enable_debug_mode();
    _ev_base = event_base_new();
    int i = 0;
    struct event* tev = event_new(_ev_base, -1, EV_PERSIST,
    [](int,short,void* args)->void{
        int* i = (int*)args;
        int widx = ++(*i) % workers.size();
        
        worker w = workers[widx];
        std::string msg("Hello worker, idx: ");
        msg += std::to_string(widx) + ", pid: " + std::to_string(w->pid);
        writen(w->chan.ptoc[PIPE_WRITE_FD], msg.c_str(), msg.size());
        
    }, &i);
    struct timeval tv = {5, 0};
    event_add(tev, &tv);
    
    // read events for each worker
    for(auto w : workers){
        w->chan.ev = event_new(
            _ev_base, w->chan.ctop[PIPE_READ_FD], EV_READ | EV_PERSIST,
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
                    std::cout << "recv from worker '" << w->wid << "': '" << 
                        std::string(buf, len)
                        << "'\n";
                }
                if(len == 0)
                    break;
            }
        }, w.get());
        event_add(w->chan.ev, nullptr);
    }
    
    struct sockaddr* sa;
    
    char saddr[] = "ipv4:0.0.0.0";
    int16_t port = 8080;
    
    char* baddr = saddr;
    struct sockaddr_un sockun = {0};
    struct sockaddr_in6 sin6 = {0};
    struct sockaddr_in sin = {0};
    size_t sin_len;
    
    
    if(!strncasecmp(baddr, "ipv6:", 5)){
        baddr += 5;
        
        sin_len = sizeof(struct sockaddr_in6);
        sin6.sin6_port = htons(port);
        sin6.sin6_family = AF_INET6;
        
        evutil_inet_pton(AF_INET6, baddr, &sin6.sin6_addr);
        sa = (struct sockaddr*)&sin6;
        
    }else if(!strncasecmp(baddr, "unix:", 5)){
        baddr += 5;
        
        if(strlen(baddr) >= sizeof(sockun.sun_path))
            return std::exit(-1);
        
        sin_len = sizeof(struct sockaddr_un);
        sockun.sun_family = AF_UNIX;
        
        strncpy(sockun.sun_path, baddr, strlen(baddr));
        sa = (struct sockaddr*)&sockun;
        
    }else{
        if(!strncasecmp(baddr, "ipv4:", 5))
            baddr += 5;
            
        sin_len = sizeof(struct sockaddr_in);
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = inet_addr(baddr);
        
        sa = (struct sockaddr*)&sin;
    }
    
    evutil_socket_t sock = -1;

    if((sock = socket(sa->sa_family, SOCK_STREAM, 0)) == -1) {
        std::cerr << "couldn't create socket\n";
        return std::exit(-1);
    }
    
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
        std::clog << "SO_REUSEPORT NOT SUPPORTED";
    }
    if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
        (void*)&on, sizeof(on)) == -1
    ){
        if(errno != EOPNOTSUPP)
            return std::exit(-1);
        std::clog << "TCP_NODELAY NOT SUPPORTED";
    }
    if(setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, 
        (void*)&on, sizeof(on)) == -1
    ){
        if(errno != EOPNOTSUPP)
            return std::exit(-1);
        std::clog << "TCP_DEFER_ACCEPT NOT SUPPORTED";
    }
    if(sa->sa_family == AF_INET6){
        if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, 
            (void*)&on, sizeof(on)) == -1
        )
            return std::exit(-1);
    }
    
    if(bind(sock, sa, sin_len) == -1)
        return std::exit(-1);
    
    int backlog = -1;
    struct evconnlistener* lev = evconnlistener_new(
        _ev_base,
        [](struct evconnlistener*, int sock,
            sockaddr* saddr, int socklen, void* args
        )->void{
            // send the sock via cmsg
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
            data = 12345;
            
            msgh.msg_control = ctrl_msg.buf;
            msgh.msg_controllen = sizeof(ctrl_msg.buf);
            
            memset(ctrl_msg.buf, 0, sizeof(ctrl_msg.buf));
            
            cmsgp = CMSG_FIRSTHDR(&msgh);
            cmsgp->cmsg_len = CMSG_LEN(sizeof(int));
            cmsgp->cmsg_level = SOL_SOCKET;
            cmsgp->cmsg_type = SCM_RIGHTS;
            *((int*)CMSG_DATA(cmsgp)) = sock;
            
            int res = sendmsg(workers[0]->chan.acm[PIPE_WRITE_FD], &msgh, 0);
            if(res == -1)
                std::exit(-1);
        },
        nullptr, //args
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        backlog,
        sock
    );
    
    event_base_loop(_ev_base, 0);

cleanup:
    event_del(tev);
    event_free(tev);

    for(auto w : workers){
        event_del(w->chan.ev);
        event_free(w->chan.ev);
    }
    event_base_free(_ev_base);
    
    std::cout << "listener closing\n";
}

void worker_recv_sock(int sock)
{
    struct bufferevent* bev = bufferevent_socket_new(
        _ev_base, sock,
        BEV_OPT_CLOSE_ON_FREE
    );
    bufferevent_setcb(bev,
        // readcb
        [](struct bufferevent* bev, void* ctx)->void{
            if(bev == nullptr)
                return;
            
            size_t avail =
                evbuffer_get_length(bufferevent_get_input(bev));
            if(avail == 0)
                return;
            
            void* buf = evbuffer_pullup(
                bufferevent_get_input(bev), avail
            );
            
            std::clog << "Received: " << (const char*)buf <<
                "\n";
            
            evbuffer_drain(bufferevent_get_input(bev), avail);
            
            std::string msg("HTTP/1.1 200 OK\r\n\r\n");
            bufferevent_write(bev, msg.c_str(), msg.size());
            
        },
        
        // writecb
        [](struct bufferevent* bev, void* ctx)->void{
            bufferevent_free(bev);
        },
        
        // eventcb
        [](struct bufferevent* bev, short what, void* ctx)->void{
            
        },
        
        nullptr
    );
    bufferevent_enable(bev, EV_READ);
}

int start_worker(worker w)
{
    // will send a SIGINT when the parent dies
    prctl(PR_SET_PDEATHSIG, SIGINT);
    int ppid = getppid();
    std::cout << "starting worker\n parent pid: " << ppid << "\n";
    
    event_enable_debug_mode();
    _ev_base = event_base_new();
    
    // on read
    w->chan.ev = event_new(
        _ev_base, w->chan.ptoc[PIPE_READ_FD],
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
    event_add(w->chan.ev, nullptr);
    
    event_base_loop(_ev_base, 0);
    // event_del(tev);
    // event_free(tev);
    event_del(w->chan.ev);
    event_free(w->chan.ev);
    event_base_free(_ev_base);

    std::cout << "worker closing\n";
    
    return 0;
}