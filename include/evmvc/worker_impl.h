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
void channel_cmd_read(int fd, short events, void* arg)
{
    channel* chan = (channel*)arg;
    while(true){
        ssize_t l = evbuffer_read(chan->rcmd_buf, fd, 4096);
        if(l == -1){
            int err = errno;
            if(err == EAGAIN || err == EWOULDBLOCK)
                return;
            chan->worker()->log()->error(EVMVC_ERR(
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
                    chan->worker()->parse_cmd(t, nullptr, ps);
                    continue;
                }
                
                char* cmd_payload = (char*)evbuffer_pullup(
                    chan->rcmd_buf, ps
                );
                chan->worker()->parse_cmd(t, cmd_payload, ps);
                evbuffer_drain(chan->rcmd_buf, ps);
            }
        }
    }
}

void channel::_init_master_channels()
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


void channel::_init_child_channels()
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
            return _worker->log()->fatal(EVMVC_ERR(
                "unable to remove unix socket '{}', err: {}", usock_path, err
            ));
    }
    
    // create the client listener.
    int lfd = _internal::unix_bind(usock_path.c_str(), SOCK_STREAM);
    if(lfd == -1)
        return _worker->log()->fatal(EVMVC_ERR(
            "unix_bind '{}', err: {}", usock_path, errno
        ));
    
    if(listen(lfd, 5) == -1)
        return _worker->log()->fatal(EVMVC_ERR(
            "listen: {}", std::to_string(errno)
        ));
    
    do{
        // this will block until the master process connect.
        usock = accept(lfd, nullptr, nullptr);
        if(usock == -1)
            _worker->log()->warn(
                EVMVC_ERR("accept err: {}", errno)
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

void worker::close_service()
{
    if(this->is_child())
        _channel->sendcmd(EVMVC_CMD_CLOSE_APP, nullptr, 0);
    else if(auto a = _app.lock()){
        a->stop();
    }
}

void worker::sig_received(int sig)
{
    if(sig == SIGINT){
        auto w = active_worker();
        if(w){
            if(w->is_child()){
                if(w->running())
                    w->stop();
            }
        }
    }
}

void worker::parse_cmd(int cmd_id, const char* p, size_t plen)
{
    switch(cmd_id){
        case EVMVC_CMD_PING:{
            EVMVC_DBG(_log, "CMD PING recv");
            _channel->sendcmd(EVMVC_CMD_PONG, nullptr, 0);
            break;
        }
        case EVMVC_CMD_PONG:{
            EVMVC_DBG(_log, "CMD PONG recv");
            
            break;
        }
        case EVMVC_CMD_CLOSE:{
            if(this->is_child()){
                if(this->running())
                    this->stop();
                //raise(SIGINT);
            }else{
                event_active(_channel->rcmd_ev, EV_READ, 1);
                this->_status = running_state::stopped;
            }
            break;
        }
        case EVMVC_CMD_CLOSE_APP:{
            if(this->is_child())
                return;
            if(auto a = _app.lock())
                a->stop([self = this->shared_from_this()](auto err){
                    if(err)
                        self->_log->error(err);
                    event_base_loopbreak(global::ev_base());
                });
            break;
        }
        case EVMVC_CMD_LOG:{
            //EVMVC_DBG(_log, "CMD LOG recv");
            log_level lvl = (log_level)(*(int*)p);
            p += sizeof(int);
            
            size_t log_path_size = *(size_t*)p;
            p += sizeof(size_t);
            const char* log_path = p;
            p += log_path_size;
            
            size_t log_msg_size = *(size_t*)p;
            p += sizeof(size_t);
            const char* log_msg = p;
            p += log_msg_size;
            
            _log->log(
                evmvc::string_view(log_path, log_path_size),
                lvl,
                evmvc::string_view(log_msg, log_msg_size)
            );
            
            break;
        }
        default:
            if(plen)
                _log->warn(
                    "unknown cmd_id: '{}', payload len: '{}'\n"
                    "payload: '{}'",
                    cmd_id, plen,
                    evmvc::base64_encode(evmvc::string_view(p, plen))
                );
            else
                _log->warn(
                    "unknown cmd_id: '{}', payload len: '{}'",
                    cmd_id, plen
                );
            break;
    }
}


void http_worker::on_http_worker_accept(int fd, short events, void* arg)
{
    http_worker* w = (http_worker*)arg;
    
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
            
            w->_log->fatal(EVMVC_ERR(
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
            w->_log->fatal(EVMVC_ERR("bad cmsg header / message length"));
        if (cmsgp->cmsg_level != SOL_SOCKET)
            w->_log->fatal(EVMVC_ERR("cmsg_level != SOL_SOCKET"));
        if (cmsgp->cmsg_type != SCM_RIGHTS)
            w->_log->fatal(EVMVC_ERR("cmsg_type != SCM_RIGHTS"));
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstrict-aliasing"
        int sock = *((int*)CMSG_DATA(cmsgp));
        #pragma GCC diagnostic pop
        w->_revc_sock(
            data.srv_id, data.iproto, sock
        );
    }
}


}//::evmvc
