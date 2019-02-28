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


evmvc::sp_logger route_result::log()
{
    if(_route)
        return _route->log();
    return evmvc::_internal::default_logger();
}

void route_result::execute(evmvc::sp_response res, async_cb cb)
{
    _route->_exec(res->req(), res, 0, cb);
}


evmvc::sp_logger route::log() const
{
    if(!_log)
        _log = _rtr.lock()->log()->add_child(this->_rp);
    return _log;
}

router::router(evmvc::wp_app app)
    : _app(app), 
    _path(_norm_path("")),
    _log(_app.lock()->log()->add_child(_path)),
    _parent(nullptr)
    /*,
    _match_first(boost::indeterminate),
    _match_strict(boost::indeterminate),
    _match_case(boost::indeterminate)
    */
{
}

router::router(evmvc::wp_app app, const evmvc::string_view& path)
    : _app(app),
    _path(_norm_path(path)),
    _log(_app.lock()->log()->add_child(_path)),
    _parent(nullptr)
    /*,
    _match_first(boost::indeterminate),
    _match_strict(boost::indeterminate),
    _match_case(boost::indeterminate)
    */
{
}

sp_route& route::null(wp_app a)
{
    static sp_route rt = std::make_shared<route>(
        router::null(a), "null"
    );
    
    return rt;
}

sp_router& router::null(wp_app a)
{
    static sp_router rtr = std::make_shared<router>(
        a.lock(), "null"
    );
    
    return rtr;
}


}//::evmvc
