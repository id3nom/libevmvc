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

// https://stackoverflow.com/questions/25710599/content-transfer-encoding-7bit-or-8-bit
// https://www.w3.org/Protocols/rfc1341/5_Content-Transfer-Encoding.html


#ifndef _libevmvc_multipart_parser_h
#define _libevmvc_multipart_parser_h

#include "stable_headers.h"
#include "router.h"
#include "stack_debug.h"
#include "fields.h"
#include "http_param.h"
#include "response.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// max content buffer size of 10KiB
#define EVMVC_MAX_CONTENT_BUF_LEN 10240

namespace evmvc { namespace _internal {

/* Write "n" bytes to a descriptor. */
ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char* ptr;
    
    ptr = (const char*)vptr;
    nleft = n;
    while(nleft > 0){
        if((nwritten = write(fd, ptr, nleft)) <= 0){
            if (errno == EINTR)
                nwritten = 0;/* and call write() again */
            else
                return(-1);/* error */
        }
        
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}
/* end writen */

// multipart/form-data; boundary=---------------------------334625884604427441415954120

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

struct multipart_subcontent_t;
typedef struct multipart_subcontent_t multipart_subcontent;

typedef struct multipart_content_t
    : public std::enable_shared_from_this<multipart_content_t>
{
    multipart_content_t()
        :
        parent(), type(multipart_content_type::unknown),
        headers(), name(""), mime_type("text/plain")
    {
    }
    
    multipart_content_t(
        const std::weak_ptr<multipart_subcontent>& p,
        multipart_content_type ct)
        : parent(p), type(ct),
        headers(), name(""), mime_type("text/plain")
    {
    }
    
    evmvc::sp_http_param get(const evmvc::string_view& pname)
    {
        for(auto& ele : headers)
            if(strcmp(ele->name(), pname.data()) == 0)
                return ele;
        return nullptr;
    }
    
    std::shared_ptr<multipart_subcontent> get_parent(){
        return parent.lock();
    }
    
    std::weak_ptr<multipart_subcontent> parent;
    
    multipart_content_type type;
    evmvc::http_params headers;
    std::string name;
    std::string mime_type;
    
} multipart_content;

typedef struct multipart_content_form_t
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
    
} multipart_content_form;

typedef struct multipart_content_file_t
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
    
    std::string filename;
    boost::filesystem::path temp_path;
    
    size_t size;
    int fd;
    bool append_crlf;
    
} multipart_content_file;


struct multipart_subcontent_t
    : public multipart_content
{
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
//typedef struct multipart_subcontent_t multipart_subcontent;


typedef struct multipart_parser_t
{
    multipart_parser_t()
        : app(nullptr),
        state(multipart_parser_state::init),
        // start_boundary(""), end_boundary(""),
        // contents(),
        total_size(0), uploaded_size(0),
        buf(nullptr),
        root_content(),
        current()
    {
    }
    
    evmvc::app* app;
    
    multipart_parser_state state;
    
    //std::string start_boundary;
    //std::string end_boundary;
    //std::vector<std::shared<multipart_content>> contents;
    
    uint64_t total_size;
    uint64_t uploaded_size;
    
    evbuf_t* buf;
    
    std::shared_ptr<multipart_subcontent> root_content;
    std::shared_ptr<multipart_content> current;
    boost::filesystem::path temp_dir;
    
} multipart_parser;

bool is_multipart_data(evhtp_request_t* req, evhtp_headers_t* hdr)
{
    if(req->method != htp_method_POST)
        return false;
    
    evhtp_kv_t* header = nullptr;
    if((header = evhtp_headers_find_header(
        hdr, evmvc::to_string(evmvc::field::content_type).data()
        )
    ) != nullptr){
        std::vector<std::string> vals;
        boost::split(vals, header->val, boost::is_any_of(";"));
        for(auto& val : vals)
            if(evmvc::trim_copy(val) == "multipart/form-data")
                return true;
    }
    return false;
}

static std::string get_boundary(const std::string& hdr_val)
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

static std::string get_boundary(evhtp_headers_t* hdr)
{
    evhtp_kv_t* header = nullptr;
    if((header = evhtp_headers_find_header(
        hdr, evmvc::to_string(evmvc::field::content_type).data()
        )
    ) != nullptr){
        std::string val = get_boundary(header->val);
        if(!val.empty())
            return val;
    }
    
    std::cerr <<
        "Invalid multipart/form-data query, boundary wasn't found!\n";
    return "";
}

static std::string get_header_attribute(
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

static uint64_t get_content_length(evhtp_headers_t* hdr)
{
    evhtp_kv_t* header = nullptr;
    if((header = evhtp_headers_find_header(
        hdr, evmvc::to_string(evmvc::field::content_length).data()
        )
    ) != nullptr)
        return evmvc::str_to_num<uint64_t>(header->val);
    
    return 0;
}

static bool parse_boundary_header(
    evmvc::_internal::multipart_parser* mp, char* hdr)
{
    // Host: www.w3schools.com
    // User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/60.0
    // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
    // Accept-Language: en-US,en;q=0.8,fr-CA;q=0.5,fr;q=0.3
    // Accept-Encoding: gzip, deflate, br
    // Referer: https://www.w3schools.com/TAGs/tryit.asp?filename=tryhtml_form_enctype
    // Content-Type: multipart/form-data; boundary=---------------------------1572261267569431463538505199
    // Content-Length: 279
    // Cookie: ASPSESSIONIDSSSRBABS=GFPMGCCAMPHMOPNBFBGAPCGD; ASPSESSIONIDQQQSBAAS=IGAEJFEALOACHACMNKAJAMDF; G_ENABLED_IDPS=google
    // DNT: 1
    // Connection: keep-alive
    // Upgrade-Insecure-Requests: 1
    // 
    // -----------------------------1572261267569431463538505199
    // Content-Disposition: form-data; name="fname"
    //
    //
    // -----------------------------1572261267569431463538505199
    // Content-Disposition: form-data; name="lname"
    //
    //
    // -----------------------------1572261267569431463538505199--


    // The following example illustrates "multipart/form-data" encoding. 
    // Suppose we have the following form:
    //
    //  <FORM action="http://server.com/cgi/handle"
    //        enctype="multipart/form-data"
    //        method="post">
    //    <P>
    //    What is your name? <INPUT type="text" name="submit-name"><BR>
    //    What files are you sending? <INPUT type="file" name="files"><BR>
    //    <INPUT type="submit" value="Send"> <INPUT type="reset">
    //  </FORM>
    //
    // If the user enters "Larry" in the text input, and selects the 
    // text file "file1.txt", the user agent might send back the following data:
    //
    //    Content-Type: multipart/form-data; boundary=AaB03x
    //
    //    --AaB03x
    //    Content-Disposition: form-data; name="submit-name"
    //
    //    Larry
    //    --AaB03x
    //    Content-Disposition: form-data; name="files"; filename="file1.txt"
    //    Content-Type: text/plain
    //
    //    ... contents of file1.txt ...
    //    --AaB03x--


    // If the user selected a second (image) file "file2.gif",
    // the user agent might construct the parts as follows:
    //
    //    Content-Type: multipart/form-data; boundary=AaB03x
    //
    //    --AaB03x
    //    Content-Disposition: form-data; name="submit-name"
    //
    //    Larry
    //    --AaB03x
    //    Content-Disposition: form-data; name="files"
    //    Content-Type: multipart/mixed; boundary=BbC04y
    //
    //    --BbC04y
    //    Content-Disposition: file; filename="file1.txt"
    //    Content-Type: text/plain
    //
    //    ... contents of file1.txt ...
    //    --BbC04y
    //    Content-Disposition: file; filename="file2.gif"
    //    Content-Type: image/gif
    //    Content-Transfer-Encoding: binary
    //
    //    ...contents of file2.gif...
    //    --BbC04y--
    //    --AaB03x--
    
    std::string hdr_line(hdr);
    size_t col_idx = hdr_line.find_first_of(":");
    if(col_idx == std::string::npos){
        std::cerr << "Invalid boundary header format!";
        return false;
    }
    
    std::string hdr_name = hdr_line.substr(0, col_idx -1);
    
    std::string hdr_val = hdr_line.substr(col_idx +1);
    mp->current->headers.emplace_back(
        std::make_shared<http_param>(
            hdr_name, hdr_val
        )
    );
    
    return true;
}

static bool assign_content_type(evmvc::_internal::multipart_parser* mp)
{
    auto ct = mp->current->get(
        evmvc::to_string(evmvc::field::content_type)
    );
    auto cd = mp->current->get(
        evmvc::to_string(evmvc::field::content_disposition)
    );
    
    if(ct){
        multipart_subcontent_type sub_type =
            multipart_subcontent_type::unknown;
        
        if(ct->get<std::string>().find("multipart/mixed") !=
            std::string::npos)
            sub_type = multipart_subcontent_type::mixed;
        else if(ct->get<std::string>().find("multipart/alternative") !=
            std::string::npos)
            sub_type = multipart_subcontent_type::alternative;
        else if(ct->get<std::string>().find("multipart/digest") !=
            std::string::npos)
            sub_type = multipart_subcontent_type::digest;
        
        if(sub_type != multipart_subcontent_type::unknown){
            auto cc = std::make_shared<multipart_subcontent>(
                mp->current
            );
            cc->sub_type = sub_type;
            auto boundary = get_boundary(ct->get<std::string>());
            if(boundary.empty())
                cc->set_boundary(mp->current->get_parent());
            else
                cc->set_boundary(boundary);
            
            mp->current = cc;
            ct.reset();
        }
        
    }
    
    if(!ct)
        mp->current->mime_type = "";
    
    mp->current->name = evmvc::_internal::get_header_attribute(
        cd->get<std::string>(), "name"
    );
    
    if(ct){
        mp->current->mime_type =
            evmvc::trim_copy(ct->get<std::string>());
        
        auto filename = evmvc::_internal::get_header_attribute(
            cd->get<std::string>(), "filename"
        );
        
        if(filename.empty()){
            auto cc = std::make_shared<multipart_content_form>(
                mp->current
            );
            mp->current = cc;
            ct.reset();
        }else{
            auto cc = std::make_shared<multipart_content_file>(
                mp->current
            );
            
            cc->filename = filename;
            cc->temp_path = mp->temp_dir / boost::filesystem::unique_path();
            cc->fd = open(cc->temp_path.c_str(), O_CREAT | O_WRONLY);
            if(cc->fd == -1)
                return false;
            
            mp->current = cc;
            ct.reset();
        }
    }
    
    if(auto sp = mp->current->parent.lock())
        sp->contents.emplace_back(mp->current);
    else
        return false;
    
    mp->state =
        mp->current->type == multipart_content_type::subcontent ?
            multipart_parser_state::headers :
            multipart_parser_state::content;
    
    return true;
}

static evhtp_res parse_end_of_section(
    evmvc::_internal::multipart_parser* mp,
    bool& ended,
    char* line)
{
    if(mp->current->get_parent()->start_boundary == line){
        ended = true;
        mp->state = evmvc::_internal::multipart_parser_state::headers;
        if(auto sp = mp->current->parent.lock()){
            auto nc = std::make_shared<multipart_content>(
                sp,
                multipart_content_type::unset
            );
            mp->current = nc;
            
        }else{
            return EVHTP_RES_SERVERR;
        }
    }
    
    if(mp->current->get_parent()->end_boundary == line){
        ended = true;
        mp->state = evmvc::_internal::multipart_parser_state::headers;
        if(std::shared_ptr<multipart_subcontent> spa = 
            mp->current->parent.lock()
        ){
            if(std::shared_ptr<multipart_subcontent> spb = spa->parent.lock()){
                auto nc = std::make_shared<multipart_content>(
                    spb,
                    multipart_content_type::unset
                );
                mp->current = nc;
            }else
                return EVHTP_RES_SERVERR;
        }else
            return EVHTP_RES_SERVERR;
    }
    
    return EVHTP_RES_OK;
}

static evhtp_res on_read_form_data(
    evmvc::_internal::multipart_parser* mp,
    bool& has_works)
{
    size_t len;
    char* line = evbuffer_readln(mp->buf, &len, EVBUFFER_EOL_CRLF_STRICT);
    
    if(line == nullptr){
        has_works = false;
        return EVHTP_RES_OK;
    }
    
    bool ended = false;
    evhtp_res res = parse_end_of_section(mp, ended, line);
    if(res != EVHTP_RES_OK || ended){
        free(line);
        return res;
    }
    
    auto mf = std::static_pointer_cast<multipart_content_form>(mp->current);
    
    mf->value += line;

    free(line);
    return EVHTP_RES_OK;
}

static evhtp_res on_read_file_data(
    evmvc::_internal::multipart_parser* mp,
    bool& has_works)
{
    size_t len;
    char* line = evbuffer_readln(mp->buf, &len, EVBUFFER_EOL_CRLF_STRICT);
    
    auto mf = std::static_pointer_cast<multipart_content_file>(mp->current);
    
    if(line != nullptr){
        bool ended = false;
        evhtp_res res = parse_end_of_section(mp, ended, line);
        if(res != EVHTP_RES_OK || ended){
            if(mf->fd != -1){
                if(mf->append_crlf && writen(mf->fd, "\r\n", 2) < 0){
                    std::cerr << fmt::format(
                        "Failed to write to temp file '{}'\nErr: {}",
                        mf->temp_path.c_str(), strerror(errno)
                    );
                    res = EVHTP_RES_SERVERR;
                }
                
                if(close(mf->fd) < 0){
                    std::cerr << fmt::format(
                        "Failed to close temp file '{}'\nErr: {}",
                        mf->temp_path.c_str(), strerror(errno)
                    );
                    res = EVHTP_RES_SERVERR;
                }
            }
            
            free(line);
            return res;
        }
    }
    
    if(line == nullptr){
        size_t buf_size = evbuffer_get_length(mp->buf);
        if(buf_size >= EVMVC_MAX_CONTENT_BUF_LEN){
            if(mf->append_crlf && writen(mf->fd, "\r\n", 2) < 0){
                std::cerr << fmt::format(
                    "Failed to write to temp file '{}'\nErr: {}",
                    mf->temp_path.c_str(), strerror(errno)
                );
                return EVHTP_RES_SERVERR;
            }
            mf->append_crlf = false;
            
            char buf[buf_size];
            evbuffer_remove(mp->buf, buf, buf_size);
            if(writen(mf->fd, buf, buf_size) < 0){
                std::cerr << fmt::format(
                    "Failed to write to temp file '{}'\nErr: {}",
                    mf->temp_path.c_str(), strerror(errno)
                );
                return EVHTP_RES_SERVERR;
            }
        }
        has_works = false;
        return EVHTP_RES_OK;
    }
    
    if(mf->append_crlf && writen(mf->fd, "\r\n", 2) < 0){
        std::cerr << fmt::format(
            "Failed to write to temp file '{}'\nErr: {}",
            mf->temp_path.c_str(), strerror(errno)
        );
        free(line);
        return EVHTP_RES_SERVERR;
    }
    
    if(writen(mf->fd, line, len) < 0){
        std::cerr << fmt::format(
            "Failed to write to temp file '{}'\nErr: {}",
            mf->temp_path.c_str(), strerror(errno)
        );
        free(line);
        return EVHTP_RES_SERVERR;
    }
    
    free(line);
    mf->append_crlf = true;
    
    return EVHTP_RES_OK;
}

static evhtp_res on_read_multipart_data(
    evhtp_request_t *req, evbuf_t *buf, void *arg)
{
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    if(mp->state == multipart_parser_state::failed)
        return EVHTP_RES_OK;
    
    size_t blen = evbuffer_get_length(buf);
    mp->uploaded_size += blen;
    evbuffer_add_buffer(mp->buf, buf);
    evbuffer_drain(buf, blen);
    
    int res = EVHTP_RES_OK;
    
    bool has_works = true;
    while(has_works){
        switch(mp->state){
            case multipart_parser_state::init:{
                size_t len;
                char* line = evbuffer_readln(
                    mp->buf, &len, EVBUFFER_EOL_CRLF_STRICT
                );
                if(!line){
                    has_works = false;
                    break;
                }
                
                if(mp->current->get_parent()->start_boundary != line){
                    free(line);
                    res = EVHTP_RES_BADREQ;
                    goto cleanup;
                }
                
                // header boundary found
                mp->state = multipart_parser_state::headers;
                if(auto sp = mp->current->parent.lock()){
                    auto nc = std::make_shared<multipart_content>(
                        sp,
                        multipart_content_type::unset
                    );
                    mp->current = nc;
                    free(line);
                    break;
                    
                }else{
                    free(line);
                    res = EVHTP_RES_SERVERR;
                    goto cleanup;
                }
            }
            
            case multipart_parser_state::headers:{
                size_t len;
                char* line = evbuffer_readln(
                    mp->buf, &len, EVBUFFER_EOL_CRLF_STRICT
                );
                if(line == nullptr){
                    has_works = false;
                    break;
                }
                
                if(len == 0){
                    // end of header part
                    free(line);
                    if(!assign_content_type(mp)){
                        res = EVHTP_RES_BADREQ;
                        goto cleanup;
                    }
                    break;
                }
                
                if(!parse_boundary_header(mp, line)){
                    free(line);
                    res = EVHTP_RES_BADREQ;
                    goto cleanup;
                }
                
                free(line);
                break;
            }
            
            case multipart_parser_state::content:{
                switch(mp->current->type){
                    case multipart_content_type::form:
                        res = on_read_form_data(mp, has_works);
                        break;
                    case multipart_content_type::file:
                        res = on_read_file_data(mp, has_works);
                        break;
                        
                    default:
                        if(res == EVHTP_RES_OK)
                            res = EVHTP_RES_SERVERR;
                        goto cleanup;
                }
                
                if(res != EVHTP_RES_OK)
                    goto cleanup;
                
                break;
            }
            
            case multipart_parser_state::failed:{
                if(res == EVHTP_RES_OK)
                    res = EVHTP_RES_SERVERR;
                goto cleanup;
            }
        }
    }
    
cleanup:
    if(res != EVHTP_RES_OK){
        evhtp_request_set_keepalive(req, 0);
        mp->state = multipart_parser_state::failed;
    }
    
    if(res == EVHTP_RES_BADREQ)
        send_error(mp->app, req, res);
    else if(res == EVHTP_RES_SERVERR)
        send_error(mp->app, req, res);
    
    return EVHTP_RES_OK;
}

static evhtp_res on_request_fini_multipart_data(
    evhtp_request_t *req, void *arg)
{
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    delete mp;
    
    return EVHTP_RES_OK;
}


evhtp_res parse_multipart_data(
    evhtp_request_t* req, evhtp_headers_t* hdr,
    app* arg, const boost::filesystem::path& temp_dir)
{
    std::string boundary = get_boundary(hdr);
    if(boundary.size() == 0)
        return EVHTP_RES_BADREQ;
    
    evmvc::_internal::multipart_parser* mp = 
        new evmvc::_internal::multipart_parser();
    
    mp->total_size = evmvc::_internal::get_content_length(hdr);
    mp->temp_dir = temp_dir;
    
    auto cc = std::make_shared<multipart_subcontent>(
        nullptr
    );
    cc->sub_type = multipart_subcontent_type::root;
    cc->parent = std::static_pointer_cast<multipart_subcontent>(
        cc->shared_from_this()
    );
    cc->set_boundary(boundary);
    mp->current = mp->root_content = cc;
    
    
    evhtp_callback_set_hook(
        (evhtp_callback_t*)&req->hooks, evhtp_hook_on_read, 
        (evhtp_hook)on_read_multipart_data, mp
    );
    evhtp_callback_set_hook(
        (evhtp_callback_t*)&req->hooks, evhtp_hook_on_request_fini, 
        (evhtp_hook)on_request_fini_multipart_data, mp
    );
    req->cbarg = mp;
    
    return EVHTP_RES_OK;
}


}}//ns evmvc::_internal
#endif //_libevmvc_multipart_parser_h
