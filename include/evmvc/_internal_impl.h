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

namespace evmvc { namespace _internal{

void send_error(
    evmvc::sp_response res, int status_code,
    evmvc::string_view msg)
{
    res->error(
        (evmvc::status)status_code,
        EVMVC_ERR(msg.data())
    );
}

void send_error(
    evmvc::sp_response res, int status_code,
    evmvc::cb_error err)
{
    res->error((evmvc::status)status_code, err);
}

/*
void on_app_request(evhtp_request_t* req, void* arg)
{
    evmvc::request_args* ra = (evmvc::request_args*)arg;
    
    if(!ra->ready){
        ra->res->pause();
        ra->ready = true;
        return;
    }
    
    sp_app a = ra->res->get_app();
    sp_route_result rr = ra->rr;
    sp_response res = ra->res;
    delete ra;
    
    try{
        rr->execute(res, [res](auto error
        ){
            if(error){
                res->error(
                    evmvc::status::internal_server_error, error
                );
                return;
            }
        });
    }catch(const std::exception& err){
        res->error(
            evmvc::status::internal_server_error,
            EVMVC_ERR(err.what())
        );
    }
}
*/

void _master_listen_cb(
    struct evconnlistener*, int sock,
    sockaddr* saddr, int socklen, void* args)
{
    static size_t rridx = 100000;
    evmvc::listener* l = (evmvc::listener*)args;
    
    sp_master_server s = l->get_server();
    if(!s){
        close(sock);
        return;
    }
    sp_app a = s->get_app();
    if(!a){
        close(sock);
        return;
    }
    if(a->workers().empty()){
        close(sock);
        return;
    }
    
    // send the sock via cmsg
    ctrl_msg_data data{
        s->id(),
        (int)(l->ssl() ? url_scheme::https : url_scheme::http),
        0,
        port
    };
    
    if(saddr->sa_family == AF_UNIX){
        memcpy(data.addr, this->address().c_str(), this->address().size());
        
    }else if(saddr->sa_family == AF_INET){
        auto addr_in = (sockaddr_in*)saddr;
        inet_ntop(
            AF_INET, &addr_in->sin_addr, data.addr, INET_ADDRSTRLEN
        );
        data.port = ntohs(addr_in->sin_port);
        
    }else if(saddr->sa_family == AF_INET6){
        auto addr_in6 = (sockaddr_in6*)saddr;
        inet_ntop(
            AF_INET6, &addr_in6->sin6_addr, data.addr, INET6_ADDRSTRLEN
        );
        data.port = ntohs(addr_in6->sin6_port);
        
    }
    
    struct msghdr msgh;
    struct iovec iov;
    
    union
    {
        char buf[CMSG_SPACE(sizeof(ctrl_msg_data))];
        struct cmsghdr align;
    } ctrl_msg;
    struct cmsghdr* cmsgp;
    
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    
    msgh.msg_iovlen = 1;
    iov.iov_base = &data;
    iov.iov_len = sizeof(ctrl_msg_data);
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
    
    sp_worker pw = nullptr;
    while(!pw){
        if(a->workers()[rridx]->work_type() == worker_type::http)
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
    // sp_worker pw;
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
        return a->log()->fail(EVMVC_ERR(
            "Unable to find an available http_worker!"
        ));
    
    int res = pw->channel().sendmsg(&msgh, 0);
    if(res == -1)
        return a->log()->fatal(EVMVC_ERR(
            "sendmsg to http_worker failed: {}", errno
        ));
}




evmvc::sp_response create_http_response(
    wp_connection conn,
    http_version ver,
    url uri,
    sp_header_map hdrs,
    sp_route_result rr)
{
    static uint64_t cur_id = 0;
    uint64_t rid = ++cur_id;
    
    evmvc::sp_http_cookies cks = std::make_shared<evmvc::http_cookies>(
        rid, rr->log(), rr->_route, uri, hdrs
    );
    evmvc::sp_request req = std::make_shared<evmvc::request>(
        rid, ver, conn, rr->log(), rr->_route, uri, hdrs, cks, rr->params
    );
    evmvc::sp_response res = std::make_shared<evmvc::response>(
        rid, req, conn, rr->log(), rr->_route, uri, cks
    );
    
    return res;
}
    
    
}}//::evmvc::internal
