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
//#include "router.h"
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

template<bool READ_ONLY>
class http_headers;
using request_headers = http_headers<true>;
using response_headers = http_headers<false>;

typedef std::shared_ptr<request_headers> sp_request_headers;
typedef std::shared_ptr<response_headers> sp_response_headers;

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

template<bool READ_ONLY>
class http_headers
{
    friend class response;
    
public:
    http_headers()
        : _hdrs(std::make_shared<header_map>())
    {
    }

    http_headers(sp_header_map hdrs)
        : _hdrs(hdrs)
    {
    }
    
    bool exists(evmvc::field header_name) const
    {
        return exists(to_string(header_name));
    }

    bool exists(evmvc::string_view header_name) const
    {
        auto it = _hdrs->find(header_name.to_string());
        return it != _hdrs->end();
    }
    
    evmvc::sp_header get(evmvc::field header_name) const
    {
        return get(to_string(header_name));
    }
    
    evmvc::sp_header get(evmvc::string_view header_name) const
    {
        auto it = _hdrs->find(header_name.to_string());
        if(it == _hdrs->end())
            return nullptr;
        return std::make_shared<evmvc::header>(
            header_name,
            it->second[0]
        );
    }

    bool compare_value(
        evmvc::field header_name,
        evmvc::string_view val, bool case_sensitive = false) const
    {
        return compare_value(to_string(header_name), val, case_sensitive);
    }
    
    bool compare_value(
        evmvc::string_view header_name,
        evmvc::string_view val, bool case_sensitive = false) const
    {
        sp_header hdr = get(header_name);
        if(!hdr)
            return false;
        
        if(case_sensitive)
            return strcmp(hdr->value(), val.data()) == 0;
        return strcasecmp(hdr->value(), val.data()) == 0;
    }
    
    std::vector<evmvc::sp_header> list(evmvc::field header_name) const
    {
        return list(to_string(header_name));
    }
    
    std::vector<evmvc::sp_header> list(evmvc::string_view header_name) const
    {
        std::vector<evmvc::sp_header> vals;
        
        auto it = _hdrs->find(header_name.to_string());
        if(it == _hdrs->end())
            return vals;
        
        for(auto& el : it->second)
            vals.emplace_back(
                std::make_shared<evmvc::header>(
                    header_name,
                    el
                )
            );
        
        return vals;
    }
    
    template<class K, class V,
        bool CAN_WRITE = !READ_ONLY,
        typename std::enable_if<
            (std::is_same<K, std::string>::value ||
                std::is_same<K, evmvc::field>::value) &&
            std::is_same<V, std::string>::value
        , int32_t>::type = -1
    >
    typename std::enable_if<
        CAN_WRITE, http_headers<READ_ONLY>
    >::type& set(
        const std::map<K, V>& m,
        bool clear_existing = true)
    {
        for(const auto& it = m.begin(); it < m.end(); ++it)
            this->set(it->first, it->second, clear_existing);
        return *this;
    }
    
    
    template<
        typename T, 
        bool CAN_WRITE = !READ_ONLY,
        typename std::enable_if<
            std::is_same<T, evmvc::json>::value,
            int32_t
        >::type = -1
    >
    typename std::enable_if<CAN_WRITE, http_headers<READ_ONLY>>::type& set(
        const T& key_val,
        bool clear_existing = true)
    {
        for(const auto& el : key_val.items())
            this->set(el.key(), el.value(), clear_existing);
        return *this;
    }
    
    template<bool CAN_WRITE = !READ_ONLY>
    typename std::enable_if<CAN_WRITE, http_headers<READ_ONLY>>::type& set(
        evmvc::field header_name, evmvc::string_view header_val,
        bool clear_existing = true)
    {
        // static_assert(
        //     std::integral_constant<bool, !CAN_WRITE>::value,
        //     "Unable to edit READ_ONLY headers"
        // );
        
        return set(to_string(header_name), header_val, clear_existing);
    }
    
    
    template<bool CAN_WRITE = !READ_ONLY>
    typename std::enable_if<CAN_WRITE, http_headers<READ_ONLY>>::type& set(
        evmvc::string_view header_name, evmvc::string_view header_val,
        bool clear_existing = true)
    {
        std::string hn = header_name.to_string();
        auto it = _hdrs->find(hn);
        
        if(clear_existing && it != _hdrs->end())
            it->second.clear();
        if(it == _hdrs->end())
            _hdrs->emplace(
                std::move(hn),
                std::vector<std::string>{
                    header_val.to_string()
                }
            );
        else
            it->second.emplace_back(header_val.to_string());
        
        return *this;
    }
    
    
    template<bool CAN_WRITE = !READ_ONLY>
    typename std::enable_if<CAN_WRITE, http_headers<READ_ONLY>>::type& remove(
        evmvc::field header_name)
    {
        return remove(to_string(header_name));
    }
    
    template<bool CAN_WRITE = !READ_ONLY>
    typename std::enable_if<CAN_WRITE, http_headers<READ_ONLY>>::type remove(
        evmvc::string_view header_name)
    {
        _hdrs->erase(header_name.to_string());
        return *this;
    }
    
private:
    sp_header_map _hdrs;
};


}//ns evmvc
#endif //_libevmvc_headers_h
