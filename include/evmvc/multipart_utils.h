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

#ifndef _libevmvc_multipart_utils_h
#define _libevmvc_multipart_utils_h

#include "stable_headers.h"
#include "logging.h"
#include "headers.h"

namespace evmvc { namespace multip {

std::string get_boundary(const std::string& hdr_val)
{
    std::vector<std::string> kvs;
    boost::split(kvs, hdr_val, boost::is_any_of(";"));
    for(auto& kv_val : kvs){
        std::vector<std::string> kv;
        auto tkv_val = evmvc::trim_copy(kv_val);
        boost::split(kv, tkv_val, boost::is_any_of("="));
        
        if(kv.size() != 2)
            continue;
        
        if(evmvc::trim_copy(kv[0]) == "boundary")
            return evmvc::trim_copy(kv[1]);
    }
    return "";
}

std::string get_boundary(
    evmvc::sp_logger log, const std::shared_ptr<header_map>& hdrs)
{
    auto it = hdrs->find(evmvc::to_string(evmvc::field::content_type).data());
    if(it != hdrs->end()){
        std::string val = get_boundary(it->second.front());
        if(!val.empty())
            return val;
    }
    
    log->error(EVMVC_ERR(
        "Invalid multipart/form-data query, boundary not found!"
    ));
    return "";
}


std::string get_header_attribute(
    const std::string& hdr_val, const std::string& attr_name)
{
    std::vector<std::string> kvs;
    boost::split(kvs, hdr_val, boost::is_any_of(";"));
    for(auto& kv_val : kvs){
        std::vector<std::string> kv;
        auto tkv_val = evmvc::trim_copy(kv_val);
        boost::split(kv, tkv_val, boost::is_any_of("="));
        
        if(kv.size() != 2)
            continue;
        
        if(evmvc::trim_copy(kv[0]) == attr_name)
            return evmvc::trim_copy(kv[1]);
    }
    return "";
}

uint64_t get_content_length(const std::shared_ptr<header_map>& hdrs)
{
    auto it = hdrs->find(evmvc::to_string(evmvc::field::content_length).data());
    if(it != hdrs->end())
        return evmvc::str_to_num<uint64_t>(it->second.front());
    return 0;
}



}}//::evmvc::_internal
#endif //_libevmvc_multipart_utils_h
