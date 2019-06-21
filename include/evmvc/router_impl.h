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
#include "router.h"

namespace evmvc {


inline md::log::logger route_result::log()
{
    if(_route)
        return _route->log();
    return md::log::default_logger();
}

inline void route_result::execute(
    sp_route_result rr, evmvc::response res, md::callback::async_cb cb)
{
    rr->_route->get_router()->run_pre_handlers(res->req(), res,
    [rr, res, cb](const md::callback::cb_error& err){
        if(err)
            return cb(err);
        
        if(res->started())
            return cb(nullptr);
        
        rr->_route->_exec(res->req(), res, 0, 
        [rr, res, cb](const md::callback::cb_error& err){
            if(err)
                res->set_error(err);
                //return cb(err);
            
            // if(res->ended())
            //     return cb(nullptr);
            
            rr->_route->get_router()->run_post_handlers(res->req(), res, cb);
        });
    });
}


inline md::log::logger route::log() const
{
    if(!_log)
        _log = _rtr.lock()->log()->add_child(this->_rp);
    return _log;
}

inline router_t::router_t(evmvc::wp_app app_t)
    : _app(app_t), 
    _path(_norm_path("")),
    _log(_app.lock()->log()->add_child(_path)),
    _parent(),
    _router_index("/index.html")
    /*,
    _match_first(boost::indeterminate),
    _match_strict(boost::indeterminate),
    _match_case(boost::indeterminate)
    */
{
    EVMVC_DEF_TRACE("router_t {:p} created", (void*)this);
}

inline router_t::router_t(evmvc::wp_app app_t, const md::string_view& path)
    : _app(app_t),
    _path(_norm_path(path)),
    _log(_app.lock()->log()->add_child(_path)),
    _parent(),
    _router_index("/index.html")
    /*,
    _match_first(boost::indeterminate),
    _match_strict(boost::indeterminate),
    _match_case(boost::indeterminate)
    */
{
    EVMVC_DEF_TRACE("router_t {:p} created", (void*)this);
}

inline sp_route route::null(wp_app a)
{
    sp_route rt = std::make_shared<route>(
        router_t::null(a), "null"
    );
    
    return rt;
}

inline router router_t::null(wp_app a)
{
    static router rtr = std::make_shared<router_t>(
        a.lock(), "null"
    );
    auto sa = a.lock();
    auto root_rtr = sa->find_router("/");
    rtr->_policies.clear();
    // rtr->_pre_handlers.clear();
    // rtr->_post_handlers.clear();
    
    for(auto p : root_rtr->_policies)
        rtr->_policies.emplace_back(p);
    // for(auto h : root_rtr->_pre_handlers)
    //     rtr->_pre_handlers.emplace_back(h);
    // for(auto h : root_rtr->_post_handlers)
    //     rtr->_post_handlers.emplace_back(h);
    return rtr;
}


}//::evmvc
