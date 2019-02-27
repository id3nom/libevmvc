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

#ifndef _libevmvc_http_param_h
#define _libevmvc_http_param_h

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

            std::is_same<uint16_t, ParamType>::value ||
            std::is_same<uint32_t, ParamType>::value ||
            std::is_same<uint64_t, ParamType>::value ||
            
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

            std::is_same<uint16_t, ParamType>::value ||
            std::is_same<uint32_t, ParamType>::value ||
            std::is_same<uint64_t, ParamType>::value ||

            std::is_same<float, ParamType>::value ||
            std::is_same<double, ParamType>::value
        , int32_t>::type = -1
    >
    ParamType get() const
    {
        return str_to_num<ParamType>(_param_value);
    }
    
    template<typename PARAM_T>
    operator PARAM_T() const
    {
        return get<PARAM_T>();
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
template<>
inline const char* evmvc::http_param::get<const char*, -1>() const
{
    return _param_value.c_str();
}
template<>
inline evmvc::string_view evmvc::http_param::get<evmvc::string_view, -1>() const
{
    return _param_value.c_str();
}


class http_params_t
{
    friend class multip::multipart_content_form_t;
    friend class multip::multipart_content_file_t;
    friend class multip::multipart_subcontent_t;
    
public:
    http_params_t()
    {
    }
    
    http_params_t(std::initializer_list<sp_http_param> lst)
        : _params(lst)
    {
    }

    http_params_t(const std::vector<std::shared_ptr<evmvc::http_param>>& p)
        : _params(p)
    {
    }

    http_params_t(const http_params_t& o)
    {
        _params = o._params;
    }
    
    void emplace_back(sp_http_param p)
    {
        _params.emplace_back(p);
    }
    
    sp_http_param get(evmvc::string_view name) const
    {
        for(auto it : _params)
            if(strcasecmp(it->name(), name.data()) == 0)
                return it;
        return nullptr;
    }
    
    template<typename PARAM_T>
    PARAM_T get(evmvc::string_view name, PARAM_T def_val = PARAM_T()) const
    {
        for(auto it : _params)
            if(strcasecmp(it->name(), name.data()) == 0)
                return it->get<PARAM_T>();
        return def_val;
    }
    
    size_t size() const
    {
        return _params.size();
    }
    
    template<typename PARAM_T>
    PARAM_T operator[](size_t idx) const
    {
        return _params[idx]->get<PARAM_T>();
    }

    template<typename PARAM_T>
    PARAM_T operator[](evmvc::string_view name) const
    {
        for(auto it : _params)
            if(strcasecmp(it->name(), name.data()) == 0)
                return it->get<PARAM_T>();
        return PARAM_T();
    }
    
    template<typename PARAM_T>
    PARAM_T operator()(
        evmvc::string_view name, PARAM_T def_val = PARAM_T()) const
    {
        for(auto it : _params)
            if(strcasecmp(it->name(), name.data()) == 0)
                return it->get<PARAM_T>();
        return def_val;
    }

    
private:
    std::vector<sp_http_param> _params;
};

}//ns evmvc
#endif //_libevmvc_http_param_h
