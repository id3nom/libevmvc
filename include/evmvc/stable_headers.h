/*
    This file is part of libpq-async++
    Copyright (C) 2011-2018 Michel Denommee (and other contributing authors)
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "evmvc_config.h"

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <deque>
#include <vector>

#include "fmt/format.h"

extern "C" {
#ifndef EVHTP_DISABLE_REGEX
    #define EVHTP_DISABLE_REGEX
#endif
#include <event2/http.h>
#include <evhtp/evhtp.h>
}

#include <boost/logic/tribool.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#ifdef EVENT_MVC_USE_STD_STRING_VIEW
    #include <string_view>
#else
    #include <boost/utility/string_view.hpp>
#endif

#ifndef _libevmvc_stable_headers_h
#define _libevmvc_stable_headers_h

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    #include "nlohmann/json.hpp"
#pragma GCC diagnostic pop

namespace evmvc {

#ifdef EVENT_MVC_USE_STD_STRING_VIEW
    /// The type of string view used by the library
    using string_view = std::string_view;
    
    /// The type of basic string view used by the library
    template<class CharT, class Traits>
    using basic_string_view = std::basic_string_view<CharT, Traits>;

#else
    /// The type of string view used by the library
    using string_view = boost::string_view;
    
    /// The type of basic string view used by the library
    template<class CharT, class Traits>
    using basic_string_view = boost::basic_string_view<CharT, Traits>;

#endif

typedef nlohmann::json json;


class app;
typedef std::shared_ptr<app> sp_app;

class http_param;
typedef std::shared_ptr<http_param> sp_http_param;

typedef std::vector<sp_http_param> http_params;

namespace _internal{
    evhtp_res on_headers(
        evhtp_request_t* req, evhtp_headers_t* hdr, void* arg);
    void on_app_request(evhtp_request_t* req, void* arg);
}}//ns evmvc::_internal

#include "stack_debug.h"

#define EVMVC_ERR(msg, ...) \
    evmvc::stacked_error( \
        fmt::format(msg, ##__VA_ARGS__), \
        __FILE__, __LINE__, __PRETTY_FUNCTION__ \
    )

namespace evmvc {
    
class stacked_error : public std::runtime_error 
{
public:
    stacked_error(evmvc::string_view msg)
        : std::runtime_error(msg.data()),
        _stack(evmvc::_internal::get_stacktrace()),
        _file(), _line(), _func()
    {
    }
    
    stacked_error(
        evmvc::string_view msg,
        evmvc::string_view filename,
        int line,
        evmvc::string_view func)
        : std::runtime_error(msg.data()),
        _stack(evmvc::_internal::get_stacktrace()),
        _file(filename), _line(line), _func(func)
    {
    }
    
    ~stacked_error(){}
    
    std::string stack() const { return _stack;}
    evmvc::string_view file(){ return _file;}
    int line(){ return _line;}
    evmvc::string_view func(){ return _func;}
    
private:
    std::string _stack;
    evmvc::string_view _file;
    int _line;
    evmvc::string_view _func;
};
} //ns evmvc

#endif //_libevmvc_stable_headers_h
