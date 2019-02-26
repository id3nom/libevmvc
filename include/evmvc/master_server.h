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

#ifndef _libevmvc_master_server_h
#define _libevmvc_master_server_h

#include "stable_headers.h"
#include "utils.h"
#include "logging.h"
#include "configuration.h"

namespace evmvc {

namespace _internal {
    void _master_listen_cb(
        struct evconnlistener* lev, int sock,
        sockaddr* saddr, int socklen, void* args
    );
}//::_internal

class listener;
typedef std::unique_ptr<listener> up_listener;
typedef std::weak_ptr<listener> wp_listener;

class master_server;
typedef std::shared_ptr<master_server> sp_master_server;
typedef std::weak_ptr<master_server> wp_master_server;


enum class address_type
{
    none,
    ipv4,
    ipv6,
    un_path
};


class listener
    : public std::enable_shared_from_this<listener>
{
    friend void _internal::_master_listen_cb(
        struct evconnlistener*, int sock,
        sockaddr* saddr, int socklen, void* args
    );
    
public:
    listener(wp_master_server server,
        const listen_options& config,
        const sp_logger& log)
        : _server(server), _config(config),
        _log(
            log->add_child(
                fmt::format(
                    "{}:{}{}",
                    _config.address,
                    _config.port,
                    _config.ssl ? " ssl" : ""
                )
            )
        ),
        
        _type(address_type::none),
        _csa(nullptr), _sin_len(0), _sa(nullptr),
        _lsock(-1),
        _lev(nullptr)
    {
    }
    
    ~listener()
    {
        if(_csa){
            switch(_type){
                case address_type::ipv4:{
                    struct sockaddr_in* sin = (struct sockaddr_in*)_csa;
                    delete sin;
                    break;
                }
                case address_type::ipv6:{
                    struct sockaddr_in6* sin6 = (struct sockaddr_in6*)_csa;
                    delete sin6;
                    break;
                }
                case address_type::un_path:{
                    struct sockaddr_un* sockun = (struct sockaddr_un*)_csa;
                    delete sockun;
                    break;
                }
                default:
                    break;
            }
            
            _csa = nullptr;
            _sa = nullptr;
        }
        
        if(_lev)
            evconnlistener_free(_lev);
        _lev = nullptr;
    }
    
    sp_master_server get_server() const;
    
    address_type type() const { return _type;}
    std::string address() const { return _config.address;}
    int port() const { return _config.port;}
    bool ssl() const { return _config.ssl;}
    int backlog() const { return _config.backlog;}
    
    void start()
    {
        // try to parse the address param
        bool ipv6_only = true;
        _parse(ipv6_only);
        
        if((_lsock = socket(_sa->sa_family, SOCK_STREAM, 0)) == -1)
            return _log->fatal(EVMVC_ERR("couldn't create socket"));
        
        _set_sock_options(ipv6_only);
        
        if(bind(_lsock, _sa, _sin_len) == -1)
            return _log->fatal(EVMVC_ERR("couldn't bind the socket"));
        
        _lev = evconnlistener_new(
            evmvc::global::ev_base(),
            evmvc::_internal::_master_listen_cb,
            this,
            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
            _config.backlog,
            _lsock
        );
        
        _log->info(
            "Listening at: {}:{} ssl: {}, backlog: {}",
            _config.address, _config.port,
            _config.ssl ? "on" : "off", _config.backlog
        );
    }
    
private:
    void _parse(bool& ipv6_only)
    {
        std::string tmp_addr = _config.address;
        if(_config.address == "*"){
            ipv6_only = false;
            tmp_addr = "[::]";
            
        }
        char* baddr = (char*)tmp_addr.c_str();
        if(!strncasecmp(baddr, "unix:", 5)){
            baddr += 5;
            _type = address_type::un_path;
            
            struct sockaddr_un* sockun = new struct sockaddr_un();
            _csa = sockun;
            
            if(strlen(baddr) >= sizeof(sockun->sun_path))
                throw EVMVC_ERR(
                    "UNIX socket path '{}' is out of bound",
                    baddr
                );
            
            _sin_len = sizeof(struct sockaddr_un);
            sockun->sun_family = AF_UNIX;
            
            strncpy(sockun->sun_path, baddr, strlen(baddr));
            _sa = (struct sockaddr*)sockun;
            
        }else if(!strncasecmp(baddr, "[", 1)){
            std::string ipv6_addr =
                tmp_addr.substr(1, tmp_addr.size() -2);
            
            _type = address_type::ipv6;
            
            struct sockaddr_in6* sin6 = new struct sockaddr_in6();
            _csa = sin6;
            
            _sin_len = sizeof(struct sockaddr_in6);
            sin6->sin6_port = htons(_config.port);
            sin6->sin6_family = AF_INET6;
            
            evutil_inet_pton(AF_INET6, ipv6_addr.c_str(), &sin6->sin6_addr);
            _sa = (struct sockaddr*)sin6;
            
        }else{
            if(!strcasecmp(tmp_addr.c_str(), "localhost"))
                tmp_addr = "127.0.0.1";
            _type = address_type::ipv4;
            
            struct sockaddr_in* sin = new struct sockaddr_in();
            _csa = sin;
            
            _sin_len = sizeof(struct sockaddr_in);
            sin->sin_family = AF_INET;
            sin->sin_port = htons(_config.port);
            sin->sin_addr.s_addr = inet_addr(baddr);
            
            _sa = (struct sockaddr*)sin;
            
        }
    }
    
    void _set_sock_options(bool ipv6_only)
    {
        evutil_make_socket_closeonexec(_lsock);
        evutil_make_socket_nonblocking(_lsock);
        
        int on = 1;
        int off = 0;
        if(setsockopt(
            _lsock, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on)) == -1)
            return _log->fatal(EVMVC_ERR("setsockopt: SO_KEEPALIVE"));
        if(setsockopt(
            _lsock, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on)) == -1)
            return _log->fatal(EVMVC_ERR("setsockopt: SO_REUSEADDR"));
        if(setsockopt(
            _lsock, SOL_SOCKET, SO_REUSEPORT, (void*)&on, sizeof(on)) == -1
        ){
            if(errno != EOPNOTSUPP)
                return _log->fatal(EVMVC_ERR("setsockopt: SO_REUSEPORT"));
            _log->warn("SO_REUSEPORT NOT SUPPORTED");
        }
        if(setsockopt(_lsock, IPPROTO_TCP, TCP_NODELAY, 
            (void*)&on, sizeof(on)) == -1
        ){
            if(errno != EOPNOTSUPP)
                return _log->fatal(EVMVC_ERR("setsockopt: TCP_NODELAY"));
            _log->warn("TCP_NODELAY NOT SUPPORTED");
        }
        if(setsockopt(_lsock, IPPROTO_TCP, TCP_DEFER_ACCEPT, 
            (void*)&on, sizeof(on)) == -1
        ){
            if(errno != EOPNOTSUPP)
                return _log->fatal(EVMVC_ERR("setsockopt: TCP_DEFER_ACCEPT"));
            std::clog << "TCP_DEFER_ACCEPT NOT SUPPORTED";
        }
        
        if(_sa->sa_family == AF_INET6){
            if(ipv6_only){
                if(setsockopt(
                    _lsock, IPPROTO_IPV6, IPV6_V6ONLY,
                    (void*)&on, sizeof(on)) == -1
                )
                    return _log->fatal(EVMVC_ERR("setsockopt: IPV6_V6ONLY on"));
            }else{
                if(setsockopt(
                    _lsock, IPPROTO_IPV6, IPV6_V6ONLY,
                    (void*)&off, sizeof(off)) == -1
                )
                    return _log->fatal(
                        EVMVC_ERR("setsockopt: IPV6_V6ONLY off")
                    );
            }
        }
    }
    
    wp_master_server _server;
    
    listen_options _config;
    sp_logger _log;
    
    address_type _type;
    void* _csa;
    size_t _sin_len;
    struct sockaddr* _sa;
    
    evutil_socket_t _lsock;
    struct evconnlistener* _lev;
};

class master_server
    : public std::enable_shared_from_this<master_server>
{
public:
    master_server(wp_app app, const server_options& config,
        const sp_logger& log)
        : 
        _id(std::hash<std::string>{}(config.name)),
        _status(evmvc::running_state::stopped),
        _app(app), _config(config),
        _log(log->add_child("master-server:" + std::to_string(_id)))
    {
    }
    
    ~master_server()
    {
        if(running())
            stop();
    }
    
    size_t id() const { return _id;}

    running_state status() const
    {
        return _status;
    }
    bool stopped() const { return _status == running_state::stopped;}
    bool starting() const { return _status == running_state::starting;}
    bool running() const { return _status == running_state::running;}
    bool stopping() const { return _status == running_state::stopping;}
    
    sp_app get_app() const;
    
    const std::string& name() const { return _config.name;}
    
    void start()
    {
        if(!stopped())
            throw EVMVC_ERR(
                "Server must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        for(auto& l : _config.listeners){
            auto sl = std::make_unique<listener>(
                this->shared_from_this(), l, _log
            );
            sl->start();
            _listeners.emplace_back(std::move(sl));
        }
        
        _status = running_state::running;
    }
    
    void stop()
    {
        if(stopped() || stopping())
            throw EVMVC_ERR(
                "Server must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        // release listeners instances
        _listeners.clear();
        this->_status = running_state::stopped;
    }
    
private:
    
    size_t _id;
    evmvc::running_state _status;
    wp_app _app;
    server_options _config;
    sp_logger _log;
    
    std::vector<up_listener> _listeners;
};




};//::evmvc
#endif//_libevmvc_master_server_h
