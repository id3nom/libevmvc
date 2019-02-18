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
#include "logging.h"
#include "router.h"
#include "stack_debug.h"
#include "fields.h"
#include "http_param.h"
#include "response.h"

// max content buffer size of 10KiB
#define EVMVC_MAX_CONTENT_BUF_LEN 10240
#define EVMVC_DEFAULT_MIME_TYPE "text/plain"
namespace evmvc { namespace _internal {

void on_multipart_request(evhtp_request_t* req, void* arg);

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

struct multipart_content_t
    : public std::enable_shared_from_this<multipart_content_t>
{
    multipart_content_t()
        :
        parent(), type(multipart_content_type::unknown),
        //headers(std::make_unique<http_params_t>()),
        headers(),
        name(""), mime_type(EVMVC_DEFAULT_MIME_TYPE)
    {
    }

    multipart_content_t(multipart_content_type ct)
        : parent(), type(ct),
        //headers(std::make_unique<http_params_t>()),
        headers(),
        name(""), mime_type(EVMVC_DEFAULT_MIME_TYPE)
    {
    }
    
    multipart_content_t(
        const std::weak_ptr<multipart_subcontent>& p,
        multipart_content_type ct)
        : parent(p), type(ct),
        //headers(std::make_unique<http_params_t>()),
        headers(),
        name(""), mime_type(EVMVC_DEFAULT_MIME_TYPE)
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
    //evmvc::http_params headers;
    std::vector<sp_http_param> headers;
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
        //boost::system::error_code ec;
        if(!temp_path.empty() && bfs::exists(temp_path))
            bfs::remove(temp_path);
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

struct multipart_parser_t
{
    multipart_parser_t()
        : //app(nullptr),
        ra(nullptr),
        state(multipart_parser_state::init),
        total_size(0), uploaded_size(0),
        buf(evbuffer_new()),
        root(),
        current(),
        temp_dir(""),
        completed(false)
    {
    }
    
    ~multipart_parser_t()
    {
        evbuffer_free(buf);
    }
    
    evmvc::_internal::request_args* ra;
    //evmvc::app* app;
    evmvc::sp_logger log;
    
    multipart_parser_state state;
    
    uint64_t total_size;
    uint64_t uploaded_size;
    
    evbuf_t* buf;
    
    std::shared_ptr<multipart_subcontent> root;
    std::shared_ptr<multipart_content> current;
    bfs::path temp_dir;
    bool completed;
    
};


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

static std::string get_boundary(
    evmvc::sp_logger log, evhtp_headers_t* hdr)
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
    
    log->error(
        "Invalid multipart/form-data query, boundary wasn't found!"
        );
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
    std::string hdr_line(hdr);
    size_t col_idx = hdr_line.find_first_of(":");
    if(col_idx == std::string::npos){
        mp->log->error("Invalid boundary header format!");
        return false;
    }
    
    std::string hdr_name = hdr_line.substr(0, col_idx);
    
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
            mp->current->mime_type = "";
            ct.reset();
        }
        
    }else{
        ct = std::make_shared<http_param>(
            evmvc::to_string(evmvc::field::content_type),
            EVMVC_DEFAULT_MIME_TYPE
        );
    }
    
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
            cc->temp_path = mp->temp_dir / bfs::unique_path();
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

static evmvc::cb_error/*evhtp_res*/ parse_end_of_section(
    evmvc::_internal::multipart_parser* mp,
    bool& ended,
    char* line)
{
    EVMVC_TRACE(mp->log, "parse_end_of_section");

    if(mp->current->get_parent()->start_boundary == line){
        ended = true;
        EVMVC_TRACE(mp->log, "start boundary detected");

        mp->state = evmvc::_internal::multipart_parser_state::headers;
        if(auto sp = mp->current->parent.lock()){
            auto nc = std::make_shared<multipart_content>(
                sp,
                multipart_content_type::unset
            );
            mp->current = nc;
            
        }else{
            return EVMVC_ERR(
                "Unable to lock parent weak reference"
            );
        }
    }
    
    if(mp->current->get_parent()->end_boundary == line){
        ended = true;
        EVMVC_TRACE(mp->log, "end boundary detected");
        
        mp->state = evmvc::_internal::multipart_parser_state::headers;
        if(std::shared_ptr<multipart_subcontent> spa = 
            mp->current->parent.lock()
        ){
            if(spa == mp->root)
                mp->current = mp->root;
            else if(std::shared_ptr<multipart_subcontent> spb =
                spa->parent.lock()
            ){
                auto nc = std::make_shared<multipart_content>(
                    spb,
                    multipart_content_type::unset
                );
                mp->current = nc;
            }else
                return EVMVC_ERR(
                    "Unable to lock parent weak reference"
                );
        }else
            return EVMVC_ERR(
                "Unable to lock parent weak reference"
            );
    }
    
    if(ended && mp->current == mp->root){
        EVMVC_TRACE(mp->log, "on_read_file_data transmission is completed!");
        mp->completed = true;
    }
    
    return nullptr;
}

static evmvc::cb_error/*evhtp_res*/ on_read_form_data(
    evmvc::_internal::multipart_parser* mp,
    bool& has_works)
{
    EVMVC_TRACE(mp->log, "on_read_form_data");
    
    size_t len;
    char* line = evbuffer_readln(mp->buf, &len, EVBUFFER_EOL_CRLF_STRICT);
    
    if(line == nullptr){
        has_works = false;
        return nullptr;
    }
    
    EVMVC_TRACE(mp->log, "recv: '{}'\n", line);
    bool ended = false;
    evmvc::cb_error cberr = parse_end_of_section(mp, ended, line);
    if(cberr || ended){
        free(line);
        return cberr;
    }
    
    auto mf = std::static_pointer_cast<multipart_content_form>(mp->current);
    
    mf->value += line;

    free(line);
    return nullptr;
}

static evmvc::cb_error/*evhtp_res*/ on_read_file_data(
    evmvc::_internal::multipart_parser* mp,
    bool& has_works)
{
    EVMVC_TRACE(mp->log, "on_read_file_data");
    size_t len;
    char* line = evbuffer_readln(mp->buf, &len, EVBUFFER_EOL_CRLF_STRICT);
    
    auto mf = std::static_pointer_cast<multipart_content_file>(mp->current);
    
    if(line != nullptr){
        EVMVC_TRACE(mp->log, "recv: '{}'\n", line);
        bool ended = false;
        evmvc::cb_error cberr = parse_end_of_section(mp, ended, line);
        if(cberr || ended){
            if(mf->fd != -1){
                if(close(mf->fd) < 0){
                    cberr = EVMVC_ERR(
                        "Failed to close temp file '{}'\nErr: {}",
                        mf->temp_path.c_str(), strerror(errno)
                    );
                }
            }
            
            free(line);
            return cberr;
        }
    }
    
    if(line == nullptr){
        size_t buf_size = evbuffer_get_length(mp->buf);
        if(buf_size >= EVMVC_MAX_CONTENT_BUF_LEN){
            if(mf->append_crlf && writen(mf->fd, "\r\n", 2) < 0){
                return EVMVC_ERR(
                    "Failed to write to temp file '{}'\nErr: {}",
                    mf->temp_path.c_str(), strerror(errno)
                );
            }
            mf->append_crlf = false;
            
            char buf[buf_size];
            evbuffer_remove(mp->buf, buf, buf_size);
            EVMVC_TRACE(mp->log,
                "extracted: '{}'",
                std::string(buf, buf+buf_size)
            );
            if(writen(mf->fd, buf, buf_size) < 0){
                return EVMVC_ERR(
                    "Failed to write to temp file '{}'\nErr: {}",
                    mf->temp_path.c_str(), strerror(errno)
                );
            }
        }
        has_works = false;
        return nullptr;
    }
    
    if(mf->append_crlf && writen(mf->fd, "\r\n", 2) < 0){
        free(line);
        return EVMVC_ERR(
            "Failed to write to temp file '{}'\nErr: {}",
            mf->temp_path.c_str(), strerror(errno)
        );
    }
    
    if(writen(mf->fd, line, len) < 0){
        free(line);
        return EVMVC_ERR(
            "Failed to write to temp file '{}'\nErr: {}",
            mf->temp_path.c_str(), strerror(errno)
        );
    }
    
    free(line);
    mf->append_crlf = true;
    
    return nullptr;
}

static evhtp_res on_read_multipart_data(
    evhtp_request_t *req, evbuf_t *buf, void *arg)
{
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    if(mp->state == multipart_parser_state::failed)
        return EVHTP_RES_OK;

    size_t blen = evbuffer_get_length(buf);
    
    EVMVC_TRACE(mp->log,
        "on_read_multipart_data received '{}' bytes",
        blen
    );

    mp->uploaded_size += blen;
    evbuffer_add_buffer(mp->buf, buf);
    evbuffer_drain(buf, blen);
    
    int res = EVHTP_RES_OK;
    evmvc::cb_error cberr(nullptr);
    
    bool has_works = true;
    while(has_works && !mp->completed){
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
                
                EVMVC_TRACE(mp->log, "recv: '{}'", line);
                if(mp->current->get_parent()->start_boundary != line){
                    free(line);
                    cberr = EVMVC_ERR(
                        "must start with the boundary\n'{}'\n"
                        "But started with '{}'\n",
                        mp->current->get_parent()->start_boundary,
                        line
                    );
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
                    cberr = EVMVC_ERR(
                        "Unable to lock parent weak reference"
                    );
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
                
                EVMVC_TRACE(mp->log, "recv: '{}'", line);
                if(len == 0){
                    // end of header part
                    free(line);
                    if(!assign_content_type(mp)){
                        cberr = EVMVC_ERR(
                            "Unable to assign content type!"
                        );
                        res = EVHTP_RES_BADREQ;
                        goto cleanup;
                    }
                    break;
                }
                
                if(!parse_boundary_header(mp, line)){
                    free(line);
                    
                    cberr = EVMVC_ERR(
                        "Unable to parse the boundary header!"
                    );
                    res = EVHTP_RES_BADREQ;
                    goto cleanup;
                }
                
                free(line);
                break;
            }
            
            case multipart_parser_state::content:{
                switch(mp->current->type){
                    case multipart_content_type::form:
                        cberr = on_read_form_data(mp, has_works);
                        break;
                    case multipart_content_type::file:
                        cberr = on_read_file_data(mp, has_works);
                        break;
                        
                    default:
                        if(res == EVHTP_RES_OK){
                            cberr = EVMVC_ERR(
                                "Invalid multipart content type!"
                            );
                            res = EVHTP_RES_SERVERR;
                        }
                        goto cleanup;
                }
                
                if(res != EVHTP_RES_OK || cberr)
                    goto cleanup;
                
                break;
            }
            
            case multipart_parser_state::failed:{
                if(res == EVHTP_RES_OK){
                    cberr = EVMVC_ERR(
                        "Multipart parser has failed!"
                    );
                    res = EVHTP_RES_SERVERR;
                }
                goto cleanup;
            }
        }
    }
    
cleanup:

    if(cberr || res != EVHTP_RES_OK){
        if(!cberr)
            cberr = EVMVC_ERR(
                "Unknown error has occured!"
            );
        if(res == EVHTP_RES_OK)
            res = EVHTP_RES_SERVERR;
    }

    if(res != EVHTP_RES_OK){
        evhtp_request_set_keepalive(req, 0);
        mp->state = multipart_parser_state::failed;
        send_error(mp->ra->res, res, cberr);
        return EVHTP_RES_OK;
    }
    
    return EVHTP_RES_OK;
}

static evhtp_res on_request_fini_multipart_data(
    evhtp_request_t *req, void *arg)
{
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    EVMVC_TRACE(mp->log, "on_request_fini_multipart_data");
    delete mp;
    
    return EVHTP_RES_OK;
}

evhtp_res parse_multipart_data(
    evmvc::sp_logger log,
    evhtp_request_t* req, evhtp_headers_t* hdr,
    evmvc::_internal::request_args* ra,
    const bfs::path& temp_dir)
{
    std::string boundary = get_boundary(log, hdr);
    if(boundary.size() == 0)
        return EVHTP_RES_BADREQ;
    
    evmvc::_internal::multipart_parser* mp = 
        new evmvc::_internal::multipart_parser();
    
    mp->ra = ra;
    //mp->app = arg;
    mp->log = log;
    mp->total_size = evmvc::_internal::get_content_length(hdr);
    mp->temp_dir = temp_dir;
    
    auto cc = std::make_shared<multipart_subcontent>();
    cc->sub_type = multipart_subcontent_type::root;
    cc->parent = std::static_pointer_cast<multipart_subcontent>(
        cc->shared_from_this()
    );
    cc->set_boundary(boundary);
    mp->current = mp->root = cc;
    
    req->hooks->on_read = on_read_multipart_data;
    req->hooks->on_read_arg = mp;

    req->hooks->on_request_fini = on_request_fini_multipart_data;
    req->hooks->on_request_fini_arg = mp;
    
    req->cb = on_multipart_request;
    req->cbarg = mp;
    
    return EVHTP_RES_OK;
}





}}//ns evmvc::_internal
#endif //_libevmvc_multipart_parser_h
