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

#ifndef _libevmvc_request_h
#define _libevmvc_request_h

#include "stable_headers.h"
#include "utils.h"
#include "fields.h"
#include "methods.h"

#include <unordered_map>

extern "C" {
#define EVHTP_DISABLE_REGEX
#include <event2/http.h>
#include <evhtp/evhtp.h>
}


namespace evmvc {

class route;

class http_param
{
public:
    http_param()
        : _is_valid(false)
    {
    }
    
    http_param(const evmvc::string_view& param_value)
        : _is_valid(true), _param_value(param_value)
    {
    }
    
    bool is_valid() const noexcept { return _is_valid;}
    
    template<typename ParamType,
        typename std::enable_if<
            !(std::is_same<int16_t, ParamType>::value ||
            std::is_same<int32_t, ParamType>::value ||
            std::is_same<int64_t, ParamType>::value ||

            std::is_same<float, ParamType>::value ||
            std::is_same<double, ParamType>::value)
        , int32_t>::type = -1
    >
    ParamType as() const
    {
        std::stringstream ss;
        ss << "Parsing from url_value to " << typeid(ParamType).name()
            << " is not supported";
        throw std::runtime_error(ss.str().c_str());
    }
    
    template<
        typename ParamType,
        typename std::enable_if<
            std::is_same<int16_t, ParamType>::value ||
            std::is_same<int32_t, ParamType>::value ||
            std::is_same<int64_t, ParamType>::value ||

            std::is_same<float, ParamType>::value ||
            std::is_same<double, ParamType>::value
        , int32_t>::type = -1
    >
    ParamType as() const
    {
        return str_to_num<ParamType>(_param_value);
    }
    
    
    
private:
    bool _is_valid;
    std::string _param_value;
};

template<>
inline std::string evmvc::http_param::as<std::string, -1>() const
{
    return _param_value;
}


class request
    : public std::enable_shared_from_this<request>
{
public:

   typedef
        std::unordered_map<std::string, std::shared_ptr<evmvc::http_param>>
        param_map;

    request(evhtp_request_t* ev_req, const param_map& p)
        : _ev_req(ev_req), _rt_params(p)
    {
    }
    
    std::shared_ptr<evmvc::http_param> route_param(
        evmvc::string_view pname) const noexcept
    {
        auto p = _rt_params.find(std::string(pname));
        if(p == _rt_params.end())
            return std::shared_ptr<evmvc::http_param>();
        return p->second;
    }
    
    template<typename ParamType>
    ParamType route_param_as(
        const evmvc::string_view& pname,
        ParamType default_val = ParamType()) const
    {
        auto p = _rt_params.find(std::string(pname));
        if(p == _rt_params.end())
            return default_val;
        return p->second->as<ParamType>();
    }
    
    
    evmvc::string_view get(evmvc::field header_name)
    {
        return get(to_string(header_name));
    }
    
    evmvc::string_view get(evmvc::string_view header_name) const
    {
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_in, header_name.data()
        )) != nullptr)
            return header->val;
        return nullptr;
    }
    
    evmvc::method method()
    {
        return (evmvc::method)_ev_req->method;
    }
    
    
    
protected:
    evhtp_request_t* _ev_req;
    param_map _rt_params;
    
};



} //ns evmvc
#endif //_libevmvc_request_h
