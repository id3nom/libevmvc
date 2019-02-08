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

#ifndef _libevmvc_headers_h
#define _libevmvc_headers_h

#include "stable_headers.h"
#include "router.h"
#include "stack_debug.h"
#include "fields.h"
#include "utils.h"

namespace evmvc {

enum class encoding_type
{
    unsupported,
    gzip,
    deflate,
    star
};

struct accept_encoding
{
    encoding_type type;
    float weight;
};

class header;
typedef std::shared_ptr<header> sp_header;

class header
{
public:
    constexpr header(
        const evmvc::string_view& hdr_name,
        const evmvc::string_view& hdr_value)
        : _hdr_name(hdr_name), _hdr_value(hdr_value)
    {
    }
    
    const char* name() const { return _hdr_name.data();}
    const char* value() const { return _hdr_value.data();}
    
    bool compare_value(
        evmvc::string_view val, bool case_sensitive = false) const
    {
        if(case_sensitive)
            return strcmp(_hdr_value.data(), val.data()) == 0;
        return strcasecmp(_hdr_value.data(), val.data()) == 0;
    }
    
    std::string attr(
        evmvc::string_view name,
        evmvc::string_view default_val = "",
        const char& attr_sep = ';',
        const char& val_sep = '=') const
    {
        size_t attr_start = 0;
        for(size_t i = 0; i < _hdr_value.size(); ++i){
            if(_hdr_value.data()[i] == attr_sep || i == _hdr_value.size() -1){
                // remove spaces
                while(attr_start < i && isspace(_hdr_value.data()[attr_start]))
                    ++attr_start;
                // look for val_sep
                size_t attr_val_start = i;
                bool has_val = false;
                for(size_t j = attr_start; j <= i; ++j)
                    if(_hdr_value[j] == val_sep){
                        has_val = true;
                        attr_val_start = j+1;
                        break;
                    }
                
                size_t attr_name_end = attr_val_start -2;
                while(attr_name_end > attr_start &&
                    isspace(_hdr_value.data()[attr_name_end]))
                    --attr_name_end;
                
                size_t len = attr_name_end - attr_start;
                if(len != name.size() ||
                    strncasecmp(
                        _hdr_value.data() + attr_start,
                        name.data(),
                        name.size()
                    ) != 0
                ){
                    attr_start = i+1;
                    continue;
                }
                
                if(has_val){
                    return std::string(
                        _hdr_value.data() + attr_val_start,
                        _hdr_value.data() + i
                    );
                }else
                    return default_val.to_string();
            }
        }
        
        return default_val.to_string();
    }
    
    bool flag(evmvc::string_view name,
        const char& attr_sep = ';',
        const char& val_sep = '=') const
    {
        size_t attr_start = 0;
        for(size_t i = 0; i < _hdr_value.size(); ++i){
            if(_hdr_value.data()[i] == attr_sep || i == _hdr_value.size() -1){
                // remove spaces
                while(attr_start < i && isspace(_hdr_value.data()[attr_start]))
                    ++attr_start;
                // look for val_sep
                size_t attr_val_start = i;
                for(size_t j = attr_start; j <= i; ++j)
                    if(_hdr_value[j] == val_sep){
                        attr_val_start = j+1;
                        break;
                    }
                
                size_t attr_name_end = attr_val_start -2;
                while(attr_name_end > attr_start &&
                    isspace(_hdr_value.data()[attr_name_end]))
                    --attr_name_end;
                
                size_t len = attr_name_end - attr_start;
                if(len != name.size() ||
                    strncasecmp(
                        _hdr_value.data() + attr_start,
                        name.data(),
                        name.size()
                    ) != 0
                ){
                    attr_start = i+1;
                    continue;
                }
                
                return true;
            }
        }
        
        return false;
    }
    
    std::vector<accept_encoding> accept_encodings() const
    {
        //Accept-Encoding: deflate, gzip;q=1.0, *;q=0.5
        //Accept-Encoding: gzip
        //Accept-Encoding: gzip, compress, br
        //Accept-Encoding: br;q=1.0, gzip;q=0.8, *;q=0.1
        
        std::vector<accept_encoding> encs;
        
        std::vector<std::string> eles;
        std::string tele;
        for(size_t i = 0; i < _hdr_value.size(); ++i)
            if(_hdr_value[i] == ','){
                if(!tele.empty())
                    eles.emplace_back(tele);
                tele.clear();
            }else
                tele += _hdr_value[i];
        if(!tele.empty())
            eles.emplace_back(tele);
        
        // boost::split(
        //     eles,
        //     evmvc::lower_case_copy(
        //         _hdr_value
        //     ),
        //     boost::is_any_of(",")
        // );

        for(size_t i = 0; i < eles.size(); ++i){
            auto& ele = eles[i];
            
            auto qloc = ele.find(";");
            if(qloc == std::string::npos){
                std::string enc_name = evmvc::trim_copy(ele);
                encs.emplace_back(
                    accept_encoding() = {
                        enc_name == "gzip" ?
                            encoding_type::gzip :
                        enc_name == "deflate" ?
                            encoding_type::deflate :
                        enc_name == "*" ?
                            encoding_type::star :
                            encoding_type::unsupported,
                        100.0f - (float)i
                    }
                );
            } else {
                std::string enc_name = 
                    evmvc::trim_copy(ele.substr(0, qloc));
                std::string enc_q = 
                    evmvc::trim_copy(ele.substr(qloc+1));
                auto qvalloc = enc_q.find("=");
                float q = 100.0f - (float)i;
                if(qvalloc != std::string::npos)
                    q = evmvc::str_to_num<float>(enc_q.substr(qvalloc+1));
                
                encs.emplace_back(
                    accept_encoding() = {
                        enc_name == "gzip" ?
                            encoding_type::gzip :
                        enc_name == "deflate" ?
                            encoding_type::deflate :
                        enc_name == "*" ?
                            encoding_type::star :
                            encoding_type::unsupported,
                        q
                    }
                );
            }
        }
        
        std::sort(encs.begin(), encs.end(), 
        [](const accept_encoding& a, const accept_encoding& b){
            return a.weight >= b.weight;
        });
        
        return encs;
    }
    
private:
    const evmvc::string_view _hdr_name;
    const evmvc::string_view _hdr_value;
};

}//ns evmvc
#endif //_libevmvc_headers_h
