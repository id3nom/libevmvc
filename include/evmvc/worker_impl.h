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

#include "app.h"
#include "stable_headers.h"

namespace evmvc {

#define EVMVC_CMD_HEADER_SIZE (sizeof(int) + sizeof(size_t))
inline void channel_cmd_read(int fd, short events, void* arg)
{
    channel* chan = (channel*)arg;
    while(true){
        ssize_t l = evbuffer_read(chan->rcmd_buf, fd, 4096);
        if(l == 0){
            event_free(chan->rcmd_ev);
            chan->rcmd_ev = nullptr;
            return;
        }
        if(l == -1){
            int err = errno;
            if(err == EAGAIN || err == EWOULDBLOCK)
                return;
            chan->worker_t()->log()->error(MD_ERR(
                "Unable to read from pipe: {}, err: {}",
                fd, err
            ));
            return;
        }
        
        if(l > 0){
            while(true){
                size_t bl = evbuffer_get_length(chan->rcmd_buf);
                if(bl < EVMVC_CMD_HEADER_SIZE)
                    break;
                    
                char* cmd_head = (char*)evbuffer_pullup(
                    chan->rcmd_buf, EVMVC_CMD_HEADER_SIZE
                );
                
                int t = *(int*)cmd_head;
                cmd_head += sizeof(int);
                size_t ps = *(size_t*)cmd_head;
                
                if(bl < EVMVC_CMD_HEADER_SIZE + ps)
                    break;
                
                evbuffer_drain(chan->rcmd_buf, EVMVC_CMD_HEADER_SIZE);
                if(ps == 0){
                    chan->worker_t()->parse_cmd(t, nullptr, ps);
                    continue;
                }
                
                char* cmd_payload = (char*)evbuffer_pullup(
                    chan->rcmd_buf, ps
                );
                chan->worker_t()->parse_cmd(t, cmd_payload, ps);
                evbuffer_drain(chan->rcmd_buf, ps);
            }
        }
    }
}

inline void channel::_init_master_channels()
{
    usock_path = (
        _worker->get_app()->options().run_dir /
        ("evmvc." + std::to_string(_worker->id()))
    ).string();
    
    fcntl(ptoc[EVMVC_PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
    close(ptoc[EVMVC_PIPE_READ_FD]);
    ptoc[EVMVC_PIPE_READ_FD] = -1;
    fcntl(ctop[EVMVC_PIPE_READ_FD], F_SETFL, O_NONBLOCK);
    close(ctop[EVMVC_PIPE_WRITE_FD]);
    ctop[EVMVC_PIPE_WRITE_FD] = -1;
    
    // init the command listener
    rcmd_buf = evbuffer_new();
    this->rcmd_ev = event_new(
        global::ev_base(),
        ctop[EVMVC_PIPE_READ_FD], EV_READ | EV_PERSIST,
        channel_cmd_read,
        this
    );
    event_add(this->rcmd_ev, nullptr);
}


inline void channel::_init_child_channels()
{
    usock_path = (
        _worker->get_app()->options().run_dir /
        ("evmvc." + std::to_string(_worker->id()))
    ).string();
    // usock_path = _worker->get_app()->abs_path(
    //     "~/.run/evmvc." + std::to_string(_worker->id())
    // ).string();
    
    if(remove(usock_path.c_str()) == -1){
        int err = errno;
        if(err != ENOENT)
            return _worker->log()->fatal(MD_ERR(
                "unable to remove unix socket '{}', err: {}", usock_path, err
            ));
    }
    
    // create the client listener.
    int lfd = _internal::unix_bind(usock_path.c_str(), SOCK_STREAM);
    if(lfd == -1)
        return _worker->log()->fatal(MD_ERR(
            "unix_bind '{}', err: {}", usock_path, errno
        ));
    
    if(listen(lfd, 5) == -1)
        return _worker->log()->fatal(MD_ERR(
            "listen: {}", std::to_string(errno)
        ));
    
    do{
        // this will block until the master process connect.
        usock = accept(lfd, nullptr, nullptr);
        if(usock == -1)
            _worker->log()->warn(
                MD_ERR("accept err: {}", errno)
            );
    }while(usock == -1);
    _internal::unix_set_sock_opts(usock);
    
    // close the listener
    close(lfd);
    
    fcntl(ptoc[EVMVC_PIPE_READ_FD], F_SETFL, O_NONBLOCK);
    close(ptoc[EVMVC_PIPE_WRITE_FD]);
    ptoc[EVMVC_PIPE_WRITE_FD] = -1;
    
    fcntl(ctop[EVMVC_PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
    close(ctop[EVMVC_PIPE_READ_FD]);
    ctop[EVMVC_PIPE_READ_FD] = -1;
    
    // init the command listener
    rcmd_buf = evbuffer_new();
    this->rcmd_ev = event_new(
        global::ev_base(),
        ptoc[EVMVC_PIPE_READ_FD], EV_READ | EV_PERSIST,
        channel_cmd_read,
        this
    );
    event_add(this->rcmd_ev, nullptr);
}

inline void worker_t::close_service()
{
    if(this->is_child())
        _channel->sendcmd(command(evmvc::CMD_CLOSE_APP));
    else if(auto a = _app.lock()){
        a->stop();
    }
}

inline void worker_t::sig_received(int sig)
{
    if(sig == SIGINT){
        auto w = evmvc::active_worker();
        if(w && w->is_child() && w->running()){
            w->close_service();
            //w->send_cmd(command(evmvc::CMD_CLOSE));
        }
    }
}

inline void worker_t::parse_cmd(int cmd_id, const char* p, size_t plen)
{
    shared_command c = std::make_shared<command>(
        cmd_id, p, plen
    );
    if(cmd_id < evmvc::CMD_USER_ID){
        switch(cmd_id){
            case evmvc::CMD_PING:{
                EVMVC_DBG(_log, "CMD PING recv");
                _channel->sendcmd(command(evmvc::CMD_PONG));
                break;
            }
            case evmvc::CMD_PONG:{
                EVMVC_DBG(_log, "CMD PONG recv");
                break;
            }
            case evmvc::CMD_CLOSE:{
                if(this->is_child()){
                    if(this->running())
                        this->stop();
                }else{
                    event_active(_channel->rcmd_ev, EV_READ, 1);
                    this->_status = running_state::stopped;
                }
                break;
            }
            case evmvc::CMD_CLOSE_APP:{
                if(this->is_child())
                    return;
                if(auto a = _app.lock())
                    event_base_loopbreak(global::ev_base());
                break;
            }
            case evmvc::CMD_LOG:{
                md::log::log_level lvl = (md::log::log_level)c->read<int>();
                std::string log_path = c->read<std::string>();
                std::string log_msg = c->read<std::string>();
                _log->log(
                    log_path,
                    lvl,
                    log_msg
                );
                
                break;
            }
            default:
                if(plen)
                    _log->warn(
                        "unknown cmd_id: '{}', payload len: '{}'\n"
                        "payload: '{}'",
                        cmd_id, plen,
                        md::base64_encode(md::string_view(p, plen))
                    );
                else
                    _log->warn(
                        "unknown cmd_id: '{}', payload len: '{}'",
                        cmd_id, plen
                    );
                break;
        }
        
    }else{
        for(auto pfn : _cmd_parsers){
            if(pfn(c))
                return;
        }
        
        if(auto a = _app.lock()){
            if(!a->running())
                return;
        }
        
        if(plen)
            _log->warn(
                "unknown cmd_id: '{}', payload len: '{}'\n"
                "payload: '{}'",
                cmd_id, plen,
                md::base64_encode(md::string_view(p, plen))
            );
        else
            _log->warn(
                "unknown cmd_id: '{}', payload len: '{}'",
                cmd_id, plen
            );
    }
        
}


inline void http_worker_t::on_http_worker_accept(int fd, short events, void* arg)
{
    http_worker_t* w = (http_worker_t*)arg;
    
    while(true){
        //int data;
        _internal::ctrl_msg_data data;
        struct msghdr msgh;
        struct iovec iov;
        
        union
        {
            char buf[CMSG_SPACE(sizeof(_internal::ctrl_msg_data))];
            struct cmsghdr align;
        } ctrl_msg;
        struct cmsghdr* cmsgp;
        
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        
        msgh.msg_iov = &iov;
        msgh.msg_iovlen = 1;
        iov.iov_base = &data;
        iov.iov_len = sizeof(_internal::ctrl_msg_data);
        
        msgh.msg_control = ctrl_msg.buf;
        msgh.msg_controllen = sizeof(ctrl_msg.buf);
        
        int nr = recvmsg(fd, &msgh, 0);
        if(nr == 0)
            return;
        if(nr == -1){
            int terrno = errno;
            if(terrno == EAGAIN || terrno == EWOULDBLOCK)
                return;
            
            w->_log->fatal(MD_ERR(
                "recvmsg: " + std::to_string(terrno)
            ));
        }
        
        if(nr > 0)
            w->_log->debug(
                "received data, srv_id: {}, iproto: {}",
                data.srv_id, data.iproto
            );
            //fprintf(stderr, "Received data = %d\n", data);
        
        cmsgp = CMSG_FIRSTHDR(&msgh);
        /* Check the validity of the 'cmsghdr' */
        if (cmsgp == NULL || cmsgp->cmsg_len != CMSG_LEN(sizeof(int)))
            w->_log->fatal(MD_ERR("bad cmsg header / message length"));
        if (cmsgp->cmsg_level != SOL_SOCKET)
            w->_log->fatal(MD_ERR("cmsg_level != SOL_SOCKET"));
        if (cmsgp->cmsg_type != SCM_RIGHTS)
            w->_log->fatal(MD_ERR("cmsg_type != SCM_RIGHTS"));
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstrict-aliasing"
        int sock = *((int*)CMSG_DATA(cmsgp));
        #pragma GCC diagnostic pop
        w->_revc_sock(
            data.srv_id, data.iproto, sock
        );
    }
}

inline ssize_t channel::_sendcmd(
    int cmd_id, const char* payload, size_t payload_len)
{
    #ifdef EVMVC_DEBUG_SENDCMD
    std::clog << "Sending command: '" << cmd_id << 
        "', from: '" << (_type == channel_type::child ? "Child" : "Master") <<
        "', to: '" << (_type == channel_type::child ? "Master" : "Child") <<
        "'" << std::endl;
    #endif
    int fd = _type == channel_type::child ?
        ctop[EVMVC_PIPE_WRITE_FD] : ptoc[EVMVC_PIPE_WRITE_FD];
    
    ssize_t tn = 0;
    ssize_t n = md::files::writen(
        fd,
        &cmd_id,
        sizeof(int)
    );
    if(n == -1){
        std::cerr << fmt::format(
            "Unable to send cmd id, err: '{}'\n", errno
        );
        return -1;
    }
    tn += n;
    n = md::files::writen(
        fd,
        &payload_len,
        sizeof(size_t)
    );
    if(n == -1){
        std::cerr << fmt::format(
            "Unable to send cmd payload length, err: '{}'\n", errno
        );
        return -1;
    }
    tn += n;
    if(payload_len > 0){
        n = md::files::writen(
            fd,
            payload,
            payload_len
        );
        if(n == -1){
            std::cerr << fmt::format(
                "Unable to send cmd payload, err: '{}'\n", errno
            );
            return -1;
        }
        tn += n;
    }
    
    // send the command
    fsync(fd);
    
    return tn;
}


}//::evmvc
