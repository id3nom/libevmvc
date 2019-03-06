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
#include "headers.h"

// max content buffer size of 10KiB
#define EVMVC_MAX_CONTENT_BUF_LEN 10240
#define EVMVC_DEFAULT_MIME_TYPE "text/plain"


namespace evmvc { namespace multip {

enum class multipart_parser_state
{
    init,
    headers,
    content,
    failed,
};

enum class multipart_content_type
{
    unknown,
    unset,
    form,
    file,
    subcontent,
};

enum class multipart_subcontent_type
{
    unknown,
    root,
    mixed,
    alternative,
    digest
};

struct multipart_content_t
    : public std::enable_shared_from_this<multipart_content_t>
{
    multipart_content_t()
        :
        parent(), type(multipart_content_type::unknown),
        headers(std::make_shared<header_map>()),
        name(""), mime_type(EVMVC_DEFAULT_MIME_TYPE)
    {
    }

    multipart_content_t(multipart_content_type ct)
        : parent(), type(ct),
        headers(std::make_shared<header_map>()),
        name(""), mime_type(EVMVC_DEFAULT_MIME_TYPE)
    {
    }
    
    multipart_content_t(
        const std::weak_ptr<multipart_subcontent>& p,
        multipart_content_type ct)
        : parent(p), type(ct),
        //headers(std::make_unique<http_params_t>()),
        headers(std::make_shared<header_map>()),
        name(""), mime_type(EVMVC_DEFAULT_MIME_TYPE)
    {
    }
    
    std::string get(md::string_view pname)
    {
        auto it = headers->find(pname.data());
        if(it == headers->end())
            return "";
        return it->second.front();
    }
    
    std::shared_ptr<multipart_subcontent> get_parent(){
        return parent.lock();
    }
    
    std::weak_ptr<multipart_subcontent> parent;
    
    multipart_content_type type;
    //std::vector<sp_http_param> headers;
    std::shared_ptr<header_map> headers;
    std::string name;
    std::string mime_type;
};

struct multipart_content_form_t
    : public multipart_content
{
    multipart_content_form_t(const std::weak_ptr<multipart_subcontent>& p)
        : multipart_content(p, multipart_content_type::form),
        value("")
    {
    }
    
    multipart_content_form_t(const std::shared_ptr<multipart_content>& copy)
        : multipart_content(copy->parent, multipart_content_type::form),
        value("")
    {
        this->headers = copy->headers;
        this->name = copy->name;
        this->mime_type = copy->mime_type;
    }
    
    std::string value;
    
};

struct multipart_content_file_t
    : public multipart_content
{
    multipart_content_file_t(const std::weak_ptr<multipart_subcontent>& p)
        : multipart_content(p, multipart_content_type::file),
        filename(""), temp_path(), size(0), fd(-1), append_crlf(false)
    {
    }

    multipart_content_file_t(const std::shared_ptr<multipart_content>& copy)
        : multipart_content(copy->parent, multipart_content_type::file),
        filename(""), temp_path(), size(0), fd(-1), append_crlf(false)
    {
        this->headers = copy->headers;
        this->name = copy->name;
        this->mime_type = copy->mime_type;
    }
    
    ~multipart_content_file_t()
    {
        boost::system::error_code ec;
        if(!temp_path.empty() && bfs::exists(temp_path, ec)){
            if(ec){
                md::log::default_logger()->warn(MD_ERR(
                    "Unable to verify file '{}' existence!\n{}",
                    temp_path.string(), ec.message()
                ));
                return;
            }
            bfs::remove(temp_path, ec);
            if(ec)
                md::log::default_logger()->warn(MD_ERR(
                    "Unable to remove file '{}'!\n{}",
                    temp_path.string(), ec.message()
                ));
        }
    }
    
    std::string filename;
    bfs::path temp_path;
    
    size_t size;
    int fd;
    bool append_crlf;
    
};

struct multipart_subcontent_t
    : public multipart_content
{
    multipart_subcontent_t()
        : multipart_content(multipart_content_type::subcontent),
        start_boundary(""), end_boundary("")//, contents()
    {
    }
    
    multipart_subcontent_t(const std::weak_ptr<multipart_subcontent>& p)
        : multipart_content(p, multipart_content_type::subcontent),
        start_boundary(""), end_boundary("")//, contents()
    {
    }
    
    multipart_subcontent_t(const std::shared_ptr<multipart_content>& copy)
        : multipart_content(copy->parent, multipart_content_type::subcontent),
        start_boundary(""), end_boundary("")//, contents()
    {
        this->headers = copy->headers;
        this->name = copy->name;
        this->mime_type = copy->mime_type;
    }
    
    void set_boundary(const std::string& b)
    {
        start_boundary = "--" + b;
        end_boundary = "--" + b + "--";
    }
    
    void set_boundary(const std::shared_ptr<struct multipart_subcontent_t>& p)
    {
        start_boundary = p->start_boundary;
        end_boundary = p->end_boundary;
    }
    
    multipart_subcontent_type sub_type;
    
    std::string start_boundary;
    std::string end_boundary;
    
    std::vector<std::shared_ptr<multipart_content>> contents;
};




inline std::string get_boundary(const std::string& hdr_val)
{
    std::vector<std::string> kvs;
    boost::split(kvs, hdr_val, boost::is_any_of(";"));
    for(auto& kv_val : kvs){
        std::vector<std::string> kv;
        auto tkv_val = md::trim_copy(kv_val);
        boost::split(kv, tkv_val, boost::is_any_of("="));
        
        if(kv.size() != 2)
            continue;
        
        if(md::trim_copy(kv[0]) == "boundary")
            return md::trim_copy(kv[1]);
    }
    return "";
}

inline std::string get_boundary(
    md::log::sp_logger log, const std::shared_ptr<header_map>& hdrs)
{
    auto it = hdrs->find(evmvc::to_string(evmvc::field::content_type).data());
    if(it != hdrs->end()){
        std::string val = get_boundary(it->second.front());
        if(!val.empty())
            return val;
    }
    
    log->error(MD_ERR(
        "Invalid multipart/form-data query, boundary not found!"
    ));
    return "";
}


inline std::string get_header_attribute(
    const std::string& hdr_val, const std::string& attr_name)
{
    std::vector<std::string> kvs;
    boost::split(kvs, hdr_val, boost::is_any_of(";"));
    for(auto& kv_val : kvs){
        std::vector<std::string> kv;
        auto tkv_val = md::trim_copy(kv_val);
        boost::split(kv, tkv_val, boost::is_any_of("="));
        
        if(kv.size() != 2)
            continue;
        
        if(md::trim_copy(kv[0]) == attr_name)
            return md::trim_copy(kv[1]);
    }
    return "";
}

inline uint64_t get_content_length(const std::shared_ptr<header_map>& hdrs)
{
    auto it = hdrs->find(evmvc::to_string(evmvc::field::content_length).data());
    if(it != hdrs->end())
        return md::str_to_num<uint64_t>(it->second.front());
    return 0;
}



}}//::evmvc::_internal
#endif //_libevmvc_multipart_utils_h
