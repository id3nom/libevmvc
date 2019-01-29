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

#include <unordered_map>

namespace evmvc {

class route;
//class query_param;

class query_param
{
public:
    query_param()
        : _is_valid(false)
    {
    }
    
    query_param(const evmvc::string_view& param_value)
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
inline std::string evmvc::query_param::as<std::string, -1>() const
{
    return _param_value;
}


class request
    : public std::enable_shared_from_this<request>
{
public:

   typedef
        std::unordered_map<std::string, std::shared_ptr<evmvc::query_param>>
        param_map;

    request(const evhttp_request* ev_req, const param_map& p)
        : _ev_req(ev_req), _params(p)
    {
    }
    
    std::shared_ptr<evmvc::query_param> query_param(
        evmvc::string_view pname) const noexcept
    {
        auto p = _params.find(std::string(pname));
        if(p == _params.end())
            return std::shared_ptr<evmvc::query_param>();
        return p->second;
    }
    
    template<typename ParamType>
    ParamType query_param_as(
        const evmvc::string_view& pname,
        ParamType default_val = ParamType()) const
    {
        auto p = _params.find(std::string(pname));
        if(p == _params.end())
            return default_val;
        return p->second->as<ParamType>();
    }
    
protected:
    const evhttp_request* _ev_req;
    param_map _params;
    
};



} //ns evmvc
#endif //_libevmvc_request_h
