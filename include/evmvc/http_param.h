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

#ifndef _libevmvc_multipart_parser_h
#define _libevmvc_multipart_parser_h

#include "stable_headers.h"
#include "router.h"
#include "stack_debug.h"
#include "fields.h"

namespace evmvc {

class http_param
{
public:
    http_param(
        const evmvc::string_view& param_name,
        const evmvc::string_view& param_value)
        : _param_name(param_name), _param_value(param_value)
    {
    }
    
    const char* name(){ return _param_name.c_str();}
    
    template<typename ParamType,
        typename std::enable_if<
            !(std::is_same<int16_t, ParamType>::value ||
            std::is_same<int32_t, ParamType>::value ||
            std::is_same<int64_t, ParamType>::value ||

            std::is_same<float, ParamType>::value ||
            std::is_same<double, ParamType>::value)
        , int32_t>::type = -1
    >
    ParamType get() const
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
    ParamType get() const
    {
        return str_to_num<ParamType>(_param_value);
    }
    
private:
    std::string _param_name;
    std::string _param_value;
};

template<>
inline std::string evmvc::http_param::get<std::string, -1>() const
{
    return _param_value;
}


}//ns evmvc
#endif //_libevmvc_multipart_parser_h
