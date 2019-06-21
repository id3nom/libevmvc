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
#include "utils.h"
#include "master_server.h"

namespace evmvc {

inline master_server listener::get_server() const { return _server.lock();}
inline app master_server_t::get_app() const { return _app.lock();}


inline void listener::master_listen_cb(
    struct evconnlistener*, int sock,
    sockaddr* saddr, int socklen, void* args)
{
    static size_t rridx = 100000;
    evmvc::listener* l = (evmvc::listener*)args;
    
    master_server s = l->get_server();
    if(!s){
        close(sock);
        return;
    }
    app a = s->get_app();
    if(!a){
        close(sock);
        return;
    }
    if(a->workers().empty()){
        a->log()->warn("No worker avaiable to handle http connection");
        close(sock);
        return;
    }
    
    // send the sock via cmsg
    _internal::ctrl_msg_data data{
        s->id(),
        (int)(l->ssl() ? url_scheme::https : url_scheme::http)
    };
    
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
    //data = 12345;
    
    msgh.msg_control = ctrl_msg.buf;
    msgh.msg_controllen = sizeof(ctrl_msg.buf);
    
    memset(ctrl_msg.buf, 0, sizeof(ctrl_msg.buf));
    
    cmsgp = CMSG_FIRSTHDR(&msgh);
    cmsgp->cmsg_len = CMSG_LEN(sizeof(int));
    cmsgp->cmsg_level = SOL_SOCKET;
    cmsgp->cmsg_type = SCM_RIGHTS;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    *((int*)CMSG_DATA(cmsgp)) = sock;
    #pragma GCC diagnostic pop
    
    // select prefered worker;
    if(++rridx >= a->workers().size())
        rridx = 0;
    size_t cur_idx = rridx;
    
    worker pw = nullptr;
    while(!pw){
        if(a->workers()[rridx]->work_type() == worker_type::http &&
            a->workers()[rridx]->is_valid()
        )
            pw = a->workers()[rridx];
        else{
            ++rridx;
            if(rridx >= a->workers().size())
                rridx = 0;
            if(rridx == cur_idx &&
                a->workers()[rridx]->work_type() != worker_type::http
            ){
                break;
            }
        }
    }
    // worker pw;
    // for(auto& w : a->workers())
    //     if(w->work_type() == worker_type::http){
    //         if(!pw){
    //             pw = w;
    //             continue;
    //         }
            
    //         if(w->workload() < pw->workload())
    //             pw = w;
    //     }
    
    if(!pw)
        return a->log()->fail(MD_ERR(
            "Unable to find an available http_worker!"
        ));
    try{
        while(true){
            if(pw->channel().usock <= 1){
                a->log()->warn(MD_ERR(
                    "invalid socket file number: {}",
                    pw->channel().usock
                ));
                close(sock);
                return;
            }
            
            int res = pw->channel().sendmsg(&msgh, MSG_NOSIGNAL);
            if(res == -1){
                int err_no = errno;
                if(err_no == EAGAIN || err_no == EWOULDBLOCK){
                    usleep(500);
                    continue;
                }
                a->log()->warn(MD_ERR(
                    "send msg to http_worker failed err: {}",
                    err_no
                ));
                if(err_no == EPIPE){
                    pw->channel().usock = _internal::unix_connect(
                        pw->channel().usock_path.c_str(), SOCK_STREAM
                    );
                    if(pw->channel().usock == -1){
                        err_no = errno;
                        a->log()->error(MD_ERR(
                            "Unable to reconnect the unix socket err: {}",
                            err_no
                        ));
                    }else{
                        continue;
                    }
                }
                
                close(sock);
                return a->log()->error(MD_ERR(
                    "sendmsg to http_worker failed: {}", err_no
                ));
            }
            close(sock);
            break;
        }
    }catch(const std::exception& err){
        close(sock);
        a->log()->error(MD_ERR(
            "sendmsg to http_worker failed\n{}",
            err.what()
        ));
    }
}

}//::evmvc
