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

#ifndef _libevmvc_events_h
#define _libevmvc_events_h

#include "stable_headers.h"

namespace evmvc { 

enum class event_type
    : short
{
    none = 0,
    read = EV_READ,
    write = EV_WRITE,
    signal = EV_SIGNAL,
    persist = EV_PERSIST,
    edege_trigger = EV_ET,
    timed_out = EV_TIMEOUT
};
MD_ENUM_FLAGS(evmvc::event_type)

// namespace
namespace _internal {

class event_wrapper_base
{
public:
    event_wrapper_base(md::string_view name)
        : _name(name.to_string())
    {
    }
    
    const std::string& name() const { return _name;}
    virtual void stop() = 0;
    
protected:
    std::string _name; 
};
typedef std::shared_ptr<event_wrapper_base> shared_evw;

inline std::unordered_map<std::string, shared_evw>& named_events()
{
    static std::unordered_map<std::string, shared_evw> _events;
    return _events;
}

//inline std::mutex& events_mutex()
inline std::recursive_mutex& events_mutex()
{
//    static std::mutex _m;
    static std::recursive_mutex _m;
    return _m;
}


template<typename T>
class event_wrapper
    : public std::enable_shared_from_this<event_wrapper<T>>,
    public event_wrapper_base
{
    struct void_event_wrapper
    {
        std::shared_ptr<event_wrapper<T>> ew;
    };
public:
    event_wrapper(
        md::string_view name,
        event_base* _ev_base, int fd, event_type events,
        evmvc::response _res, md::callback::async_cb _nxt,
        std::function<void(
            std::shared_ptr<event_wrapper<T>>, int, event_type, T)
            > _cb
    )
        : event_wrapper_base(name), ev_base(_ev_base), ev(nullptr),
        res(_res), nxt(_nxt),
        arg(T()), cb(_cb), _fd(fd), _events(events), _ewt(nullptr)
    {
    }

    event_wrapper(
        event_base* _ev_base, int fd, event_type events,
        evmvc::response _res, md::callback::async_cb _nxt,
        T _arg,
        std::function<void(
            std::shared_ptr<event_wrapper<T>>, int, event_type, T)
            > _cb
    )
        : ev_base(_ev_base), ev(nullptr),
        res(_res), nxt(_nxt),
        arg(_arg), cb(_cb), _fd(fd), _events(events), _ewt(nullptr)
    {
    }
    
    ~event_wrapper()
    {
        this->stop();
        EVMVC_DEF_TRACE("event_wrapper {} released", _name);
    }
    
    void wait()
    {
        this->_create_event();
        if(event_add(this->ev, nullptr) == -1)
            throw MD_ERR("event_add failed!");
    }
    
    void wait(const struct timeval& tv)
    {
        this->_create_event();
        if(event_add(this->ev, &tv) == -1)
            throw MD_ERR("event_add failed!");
    }
    
    void stop()
    {
        std::unique_lock<std::recursive_mutex> lock(events_mutex());
        if(_ewt){
            delete this->_ewt;
            this->_ewt = nullptr;
        }
        
        event_del(this->ev);
        event_free(this->ev);
        this->ev = nullptr;
        
        auto it = named_events().find(_name);
        if(it == named_events().end())
            return;
        auto self = it->second;
        named_events().erase(it);
        lock.unlock();
        res.reset();
        self.reset();
    }
    
    event_base* ev_base;
    event* ev;
    evmvc::response res;
    md::callback::async_cb nxt;
    T arg;
    std::function<void(
        std::shared_ptr<event_wrapper<T>>, int, event_type, T)
        > cb;
    
private:
    void _create_event()
    {
        _ewt = new void_event_wrapper();
        _ewt->ew = this->shared_from_this();
        
        this->ev = event_new(ev_base, _fd, (short)_events, 
            [](int fd, short opts, void* self_arg)->void{
                void_event_wrapper* ewt = (void_event_wrapper*)self_arg;
                std::shared_ptr<event_wrapper> self = ewt->ew;
                
                self->cb(self, fd, (event_type)opts, self->arg);
                if((self->_events & event_type::persist) != event_type::persist)
                    self->stop();
            },
            (void*)_ewt
        );
    }
    
    int _fd;
    event_type _events;
    void_event_wrapper* _ewt;
};


template<>
class event_wrapper<void>
    : public std::enable_shared_from_this<event_wrapper<void>>,
    public event_wrapper_base
{
    struct void_event_wrapper
    {
        std::shared_ptr<event_wrapper<void>> ew;
    };
public:
    event_wrapper(
        md::string_view name,
        event_base* _ev_base, int fd, event_type events,
        evmvc::response _res, md::callback::async_cb _nxt,
        std::function<void(
            std::shared_ptr<event_wrapper<void>>, int, event_type)
            > _cb
    )
        : event_wrapper_base(name), ev_base(_ev_base), ev(nullptr),
        res(_res), nxt(_nxt),
        cb(_cb), _fd(fd), _events(events), _ewt(nullptr)
    {
    }
    
    ~event_wrapper()
    {
        this->stop();
        EVMVC_DEF_TRACE("event_wrapper {} released", _name);
    }
    
    void wait()
    {
        this->_create_event();
        if(event_add(this->ev, nullptr) == -1)
            throw MD_ERR("event_add failed!");
    }
    
    void wait(const struct timeval& tv)
    {
        this->_create_event();
        if(event_add(this->ev, &tv) == -1)
            throw MD_ERR("event_add failed!");
    }
    
    void stop()
    {
        std::unique_lock<std::recursive_mutex> lock(events_mutex());
        
        if(_ewt){
            delete this->_ewt;
            this->_ewt = nullptr;
        }
        
        if(this->ev){
            event_del(this->ev);
            event_free(this->ev);
            this->ev = nullptr;
        }
        
        auto it = named_events().find(_name);
        if(it == named_events().end())
            return;
        auto self = it->second;
        named_events().erase(it);
        lock.unlock();
        self.reset();
    }
    
    event_base* ev_base;
    event* ev;
    evmvc::response res;
    md::callback::async_cb nxt;
    std::function<void(
        std::shared_ptr<event_wrapper<void>>, int, event_type)
        > cb;
    
private:
    void _create_event()
    {
        _ewt = new void_event_wrapper();
        _ewt->ew = this->shared_from_this();
        
        this->ev = event_new(ev_base, _fd, (short)_events, 
            [](int fd, short opts, void* self_arg)->void{
                void_event_wrapper* ewt = (void_event_wrapper*)self_arg;
                std::shared_ptr<event_wrapper> self = ewt->ew;
                
                self->cb(self, fd, (event_type)opts);
                if((self->_events & event_type::persist) != event_type::persist)
                    self->stop();
            },
            (void*)_ewt
        );
    }
    
    int _fd;
    event_type _events;
    void_event_wrapper* _ewt;
};


// template<typename T>
// std::shared_ptr<event_wrapper<T>> new_event(
//     event_base* _ev_base, int fd, event_type events,
//     evmvc::response _res, md::callback::async_cb _nxt,
//     T _arg,
//     std::function<void(
//         std::shared_ptr<event_wrapper<T>>, int, event_type, T)
//         > _cb
// )
// {
//     return std::shared_ptr<event_wrapper<T>>(
//         new event_wrapper<T>(_ev_base, fd, events, _arg, _res, _nxt, _cb)
//     );
// }

// //template<>
// std::shared_ptr<event_wrapper<void>> new_event(
//     event_base* _ev_base, int fd, event_type events,
//     evmvc::response _res, md::callback::async_cb _nxt,
//     std::function<void(
//         std::shared_ptr<event_wrapper<void>>, int, event_type)
//         > _cb
// )
// {
//     return std::shared_ptr<event_wrapper<void>>(
//         new event_wrapper<void>(_ev_base, fd, events, _res, _nxt, _cb)
//     );
// }

// std::shared_ptr<event_wrapper<void>> new_event(
//     int fd, event_type events,
//     evmvc::response _res, md::callback::async_cb _nxt,
//     std::function<void(
//         std::shared_ptr<event_wrapper<void>>, int, event_type)
//         > _cb
// )
// {
//     return std::shared_ptr<event_wrapper<void>>(
//         new event_wrapper<void>(
//             evmvc::global::ev_base(), fd, events, _res, _nxt, _cb
//         )
//     );
// }

inline std::string next_event_name()
{
    std::unique_lock<std::recursive_mutex> lock(events_mutex());
    static size_t uid = 0;
    return "da724ca0-308c-11e9-9071-5b3166957f05_" + md::num_to_str(++uid);
}

inline void register_event(shared_evw ev)
{
    EVMVC_DEF_TRACE("Registering event: '{}'", ev->name());
    std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());
    auto it = named_events().find(ev->name());
    if(it != named_events().end())
        throw MD_ERR(
            "Event: '{}' is already registered!", ev->name()
        );
    named_events().emplace(std::make_pair(ev->name(), ev));
}

inline bool event_exists(const std::string& name)
{
    std::unique_lock<std::recursive_mutex> lock(events_mutex());
    
    auto it = named_events().find(name);
    return it != named_events().end();
}

}//ns: _internal

inline bool timeout_exists(md::string_view name)
{
    //std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());

    std::string n = "to:" + name.to_string();
    return _internal::event_exists(n);
}

inline bool interval_exists(md::string_view name)
{
    //std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());

    std::string n = "iv:" + name.to_string();
    return _internal::event_exists(n);
}

inline void clear_events()
{
    EVMVC_DEF_TRACE("Clearing all events");
    
    std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());
    std::vector<_internal::shared_evw> evs;
    for(auto it = _internal::named_events().begin();
        it != _internal::named_events().end(); ++it)
        evs.emplace_back(it->second);
    _internal::named_events().clear();
    
    lock.unlock();
    for(auto& ev : evs)
        ev->stop();
}

inline void clear_timeouts()
{
    EVMVC_DEF_TRACE("Clearing all timeouts");
    
    std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());
    std::vector<_internal::shared_evw> evs;
    auto it = _internal::named_events().begin();
    while(it != _internal::named_events().end()){
        if(strncmp(it->second->name().c_str(), "to:", 3) == 0){
            evs.emplace_back(it->second);
            it = _internal::named_events().erase(it);
            continue;
        }
        ++it;
    }
    
    lock.unlock();
    for(auto& ev : evs)
        ev->stop();
}
inline void clear_intervals()
{
    EVMVC_DEF_TRACE("Clearing all intervals");
    
    std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());
    std::vector<_internal::shared_evw> evs;
    auto it = _internal::named_events().begin();
    while(it != _internal::named_events().end()){
        if(strncmp(it->second->name().c_str(), "iv:", 3) == 0){
            evs.emplace_back(it->second);
            it = _internal::named_events().erase(it);
            continue;
        }
        ++it;
    }
    
    lock.unlock();
    for(auto& ev : evs)
        ev->stop();
}


inline void clear_timeout(md::string_view name)
{
    EVMVC_DEF_TRACE("Clearing timeout '{}'", name);
    
    std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());
    auto ns = "to:" + name.to_string();
    auto it = _internal::named_events().find(ns);
    if(it == _internal::named_events().end())
        return;
    lock.unlock();
    it->second->stop();
}

inline void clear_interval(md::string_view name)
{
    EVMVC_DEF_TRACE("Clearing interval '{}'", name);
    
    std::unique_lock<std::recursive_mutex> lock(_internal::events_mutex());
    auto ns = "iv:" + name.to_string();
    auto it = _internal::named_events().find(ns);
    if(it == _internal::named_events().end())
        return;
    lock.unlock();
    it->second->stop();
}
inline void clear_timeout(std::shared_ptr<_internal::event_wrapper_base> ev)
{
    ev->stop();
}
inline void clear_interval(std::shared_ptr<_internal::event_wrapper_base> ev)
{
    ev->stop();
}

typedef std::shared_ptr<_internal::event_wrapper<void>> timer_ev_wrapper;
inline timer_ev_wrapper set_timeout(
    md::string_view name,
    int fd, event_type et,
    std::function<void(
        timer_ev_wrapper, int, event_type)
        > _cb,
    size_t ms
)
{
    if(evmvc::global::ev_base() == nullptr)
        return nullptr;
    
    auto ev = timer_ev_wrapper(
        new _internal::event_wrapper<void>(
            "to:" + name.to_string(),
            evmvc::global::ev_base(), fd, et, nullptr, nullptr, _cb
        )
    );
    if(ms == 0){
        struct timeval tv = {0,0};
        ev->wait(tv);
    }else{
        long s = std::floor(ms / 1000);
        long us = (ms - (s * 1000)) * 1000;
        struct timeval tv = {s, us};
        ev->wait(tv);
    }
    _internal::register_event(ev);
    return ev;
}
inline timer_ev_wrapper set_timeout(
    int fd, event_type et,
    std::function<void(
        timer_ev_wrapper, int, event_type)
        > _cb,
    size_t ms
)
{
    return set_timeout(_internal::next_event_name(), fd, et, _cb, ms);
}


inline timer_ev_wrapper set_timeout(
    md::string_view name,
    std::function<void(
        timer_ev_wrapper)
        > _cb,
    size_t ms
)
{
    if(evmvc::global::ev_base() == nullptr)
        return nullptr;
    
    auto ev = timer_ev_wrapper(
        new _internal::event_wrapper<void>(
            "to:" + name.to_string(),
            evmvc::global::ev_base(), -1, event_type::none, nullptr, nullptr, 
            [_cb](auto ew, int /*fd*/, evmvc::event_type /*et*/){
                _cb(ew);
            }
        )
    );
    if(ms == 0){
        struct timeval tv = {0,0};
        ev->wait(tv);
    }else{
        long s = std::floor(ms / 1000);
        long us = (ms - (s * 1000)) * 1000;
        struct timeval tv = {s, us};
        ev->wait(tv);
    }
    _internal::register_event(ev);
    return ev;
}
inline timer_ev_wrapper set_timeout(
    std::function<void(
        timer_ev_wrapper)
        > _cb,
    size_t ms
)
{
    return set_timeout(_internal::next_event_name(), _cb, ms);
}



inline timer_ev_wrapper set_interval(
    md::string_view name,
    int fd, event_type et,
    std::function<void(
        timer_ev_wrapper, int, event_type)
        > _cb,
    size_t ms
)
{
    if(evmvc::global::ev_base() == nullptr)
        return nullptr;
    
    auto ev = timer_ev_wrapper(
        new _internal::event_wrapper<void>(
            "iv:" + name.to_string(),
            evmvc::global::ev_base(), fd, et | event_type::persist,
            nullptr, nullptr, _cb
        )
    );
    if(ms == 0){
        struct timeval tv = {0,0};
        ev->wait(tv);
    }else{
        long s = std::floor(ms / 1000);
        long us = (ms - (s * 1000)) * 1000;
        struct timeval tv = {s, us};
        ev->wait(tv);
    }
    _internal::register_event(ev);
    return ev;
}
inline timer_ev_wrapper set_interval(
    int fd, event_type et,
    std::function<void(
        timer_ev_wrapper, int, event_type)
        > _cb,
    size_t ms
)
{
    return set_interval(_internal::next_event_name(), fd, et, _cb, ms);
}

inline timer_ev_wrapper set_interval(
    md::string_view name,
    std::function<void(
        timer_ev_wrapper)
        > _cb,
    size_t ms
)
{
    if(evmvc::global::ev_base() == nullptr)
        return nullptr;
    
    auto ev = timer_ev_wrapper(
        new _internal::event_wrapper<void>(
            "iv:" + name.to_string(),
            evmvc::global::ev_base(), -1, event_type::persist,
            nullptr, nullptr, 
            [_cb](auto ew, int /*fd*/, evmvc::event_type /*et*/){
                _cb(ew);
            }
        )
    );
    if(ms == 0){
        struct timeval tv = {0,0};
        ev->wait(tv);
    }else{
        long s = std::floor(ms / 1000);
        long us = (ms - (s * 1000)) * 1000;
        struct timeval tv = {s, us};
        ev->wait(tv);
    }
    _internal::register_event(ev);
    return ev;
}
inline timer_ev_wrapper set_interval(
    std::function<void(
        timer_ev_wrapper)
        > _cb,
    size_t ms
)
{
    return set_interval(_internal::next_event_name(), _cb, ms);
}








}// ns: evmvc
#endif//_libevmvc_events_h