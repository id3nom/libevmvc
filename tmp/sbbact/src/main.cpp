
#include "stable_headers.h"
#include "worker.h"
#include "unix_sockets.h"


void close_workers();
void start_listener();
void exiting();
void sig_received(int sig);

int main(int argc, char** argv)
{
    signal(SIGINT, sig_received);
    
    std::clog << OPENSSL_VERSION_TEXT << "\n";
    
    int worker_count = 4;
    //int worker_count = 1;
    
    for(int i = 0; i < worker_count; ++i){
        worker w = std::make_shared<worker_t>();
        w->wid = workers().size();
        
        char buf[FILENAME_MAX];
        getcwd(buf, FILENAME_MAX);
        std::string cur_dir(buf);
        
        if(cur_dir[cur_dir.size()-1] != '/')
            cur_dir += "/";
        std::string run_dir = cur_dir + ".run";
        __mode_t mod = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
        if(mkdir(run_dir.c_str(), mod) == -1 && errno != EEXIST)
            error_exit(
                "mkdir failed with: " + std::to_string(errno)
            );
        if(run_dir[run_dir.size()-1] != '/')
            run_dir += "/";
        
        w->chan.usock_path = run_dir + "bbact." + std::to_string(w->wid);
        
        pipe(w->chan.ptoc);
        pipe(w->chan.ctop);
        
        w->pid = fork();
        if(w->pid == -1){// error
            std::cerr << "unable to create worker\n";
            close_workers();
            return -1;
            
        }else if(w->pid == 0){// is worker proc
            workers().clear();
            
            SSL_load_error_strings();
            SSL_library_init();
            
            w->pid = getpid();
            ssl_config_t* ssl_cfg = new ssl_config_t();
            ssl_cfg->pemfile = cur_dir + "srv-crt.pem";
            ssl_cfg->privfile = cur_dir + "srv-key.pem";
            ssl_cfg->dhparams = cur_dir + "dhparam.pem";
            ssl_cfg->ciphers = "HIGH:!aNULL:!MD5;";
            
            w->init_ssl(ssl_cfg);
            
            // remove old socket
            if(remove(w->chan.usock_path.c_str()) == -1 && errno != ENOENT)
                error_exit("unable to remove: " + w->chan.usock_path);
            
            // create the client listener
            int lfd = unix_bind(w->chan.usock_path.c_str(), SOCK_STREAM);
            if(lfd == -1)
                error_exit("unix_bind: " + std::to_string(errno));
            if(listen(lfd, 5) == -1)
                error_exit("listen: " + std::to_string(errno));
            do{
                w->chan.usock = accept(lfd, nullptr, nullptr);
                if(w->chan.usock == -1)
                    std::cerr << "accept err: " << errno << std::endl;
            }while(w->chan.usock == -1);
            unix_set_sock_opts(w->chan.usock);
            // now close the listener
            close(lfd);
            
            fcntl(w->chan.ptoc[PIPE_READ_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ptoc[PIPE_WRITE_FD]);
            fcntl(w->chan.ctop[PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ctop[PIPE_READ_FD]);
            
            int wres = start_worker(w);
            w.reset();
            return wres;
            
        }else{// is main proc
            fcntl(w->chan.ptoc[PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ptoc[PIPE_READ_FD]);
            fcntl(w->chan.ctop[PIPE_READ_FD], F_SETFL, O_NONBLOCK);
            close(w->chan.ctop[PIPE_WRITE_FD]);
            
            workers().emplace_back(w);
        }
    }
    
    // connect unix socket to workers.
    for(auto w : workers()){
        int tryout = 0;
        do{
            w->chan.usock = unix_connect(
                w->chan.usock_path.c_str(), SOCK_STREAM
            );
            if(w->chan.usock == -1){
                std::cerr << "unix connect err: " << errno << std::endl;
                sleep(1);
            }else
                break;
            
            if(++tryout >= 5)
                error_exit(
                    "unable to connect to client: " +
                    std::to_string(w->wid)
                );
        }while(w->chan.usock == -1);
        unix_set_sock_opts(w->chan.usock);
    }
    
    std::atexit(exiting);
    std::at_quick_exit(exiting);
    start_listener();
    close_workers();
    
    return 0;
}

void close_workers()
{
    for(auto el : workers())
        kill(el->pid, SIGINT);
    workers().clear();
}

void exiting()
{
    std::clog << "exiting\n";
    close_workers();
}

void sig_received(int sig)
{
    if(sig == SIGINT){
        //_run = false;
        event_base_loopbreak(ev_base());
    }
}



void start_listener()
{
    std::cout << "starting listener\n";
    ev_base();
    
    int i = 0;
    struct event* tev = event_new(ev_base(), -1, EV_PERSIST,
    [](int,short,void* args)->void{
        int* i = (int*)args;
        
        if(workers().size() == 0)
            return;
        
        int widx = ++(*i) % workers().size();
        
        worker w = workers()[widx];
        std::string msg("Hello worker, idx: ");
        msg += std::to_string(widx) + ", pid: " + std::to_string(w->pid);
        writen(w->chan.ptoc[PIPE_WRITE_FD], msg.c_str(), msg.size());
        
    }, &i);
    struct timeval tv = {30, 0};
    event_add(tev, &tv);
    
    // read events for each worker
    for(auto w : workers()){
        w->chan.rcmd_ev = event_new(
            ev_base(), w->chan.ctop[PIPE_READ_FD], EV_READ | EV_PERSIST,
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
        event_add(w->chan.rcmd_ev, nullptr);
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
    int wid = -1;
    struct evconnlistener* lev = evconnlistener_new(
        ev_base(),
        [](struct evconnlistener*, int sock,
            sockaddr* saddr, int socklen, void* args
        )->void{
            int* wid = (int*)args;
            
            if(workers().size() == 0){
                close(sock);
                return;
            }
            
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
            
            //int res = sendmsg(
            //    workers()[0]->chan.acm[PIPE_WRITE_FD], &msgh, 0);
            if(++(*wid) >= workers().size())
                *wid = 0;
            int res = sendmsg(workers()[*wid]->chan.usock, &msgh, 0);
            if(res == -1)
                std::exit(-1);
        },
        &wid, //args
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        backlog,
        sock
    );
    
    event_base_loop(ev_base(), 0);
    
    evconnlistener_free(lev);
    
    event_del(tev);
    event_free(tev);
    
    for(auto w : workers()){
        event_del(w->chan.rcmd_ev);
        event_free(w->chan.rcmd_ev);
    }
    event_base_free(ev_base());
    
    std::cout << "listener closing\n";
}

