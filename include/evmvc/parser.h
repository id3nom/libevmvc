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

#ifndef _libevmvc_http_parser_h
#define _libevmvc_http_parser_h

#include "stable_headers.h"
#include "utils.h"
#include "logging.h"

#include "statuses.h"
#include "headers.h"
#include "response.h"

#include "multipart_utils.h"

#define EVMVC_EOL_SIZE 2
#define EVVMC_ENCTYPE_FRM_URLENC "application/x-www-form-urlencoded"
#define EVVMC_ENCTYPE_FRM_MULTIP "multipart/form-data"
#define EVVMC_ENCTYPE_FRM_TXTPLN "text/plain"

namespace evmvc {


class parser
{
public:
    parser(wp_connection conn, const sp_logger& log)
        : _conn(conn), _log(log)
    {
    }
    
    sp_connection get_connection() const { return _conn.lock();}
    sp_logger log() const { return _log;}
    
    virtual void reset() = 0;
    virtual size_t parse(const char* buf, size_t len, cb_error& ec) = 0;
    
protected:
    wp_connection _conn;
    sp_logger _log;
};


enum class parser_state
{
    parse_req_line          = 0,
    parse_header            = 1,
    parse_body              = 2,
    parse_form_multipart    = 3,
    parse_form_urlencoded   = 4,
    parse_form_text         = 5,
    end                     = 6,
    error                   = 7,
};

enum class parser_error
{
    none = 0,
    too_big,
    inval_method,
    inval_reqline,
    inval_schema,
    inval_proto,
    inval_ver,
    inval_hdr,
    inval_chunk_sz,
    inval_chunk,
    inval_state,
    user,
    status,
    generic
};

enum class parser_flag
{
    none                    = 0,
    chunked                 = (1 << 0),
    connection_keep_alive   = (1 << 1),
    connection_close        = (1 << 2),
    trailing                = (1 << 3),
};


// max content buffer size of 10KiB
#define EVMVC_MAX_CONTENT_BUF_LEN 10240
#define EVMVC_DEFAULT_MIME_TYPE "text/plain"
namespace _internal {
    
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
    
    std::string get(evmvc::string_view pname)
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
    
    
}//::_internal



class http_parser
    : public parser
{
public:
    http_parser(wp_connection conn, const sp_logger& log)
        : parser(conn, log->add_child("parser"))
    {
    }
    
    struct bufferevent* bev() const;
    
    inline bool ok() const { return _status != parser_state::error;}
    inline bool parsing_head() const
    {
        return ok() && (
            _status == parser_state::parse_req_line ||
            _status == parser_state::parse_header
        );
    }
    
    inline bool parsing_payload() const
    {
        return ok() && (
            parsing_form() ||
            _status == parser_state::parse_body
        );
    }
    
    inline bool parsing_form() const
    {
        return ok() && (
            _status == parser_state::parse_form_urlencoded ||
            _status == parser_state::parse_form_multipart ||
            _status == parser_state::parse_form_text
        );
    }
    
    void reset()
    {
        EVMVC_DBG(_log, "parser reset");
        
        _status = parser_state::parse_req_line;
        
        _body_size = 0;
        _total_bytes_read = 0;
        
        _hdrs.reset();
        _res.reset();
        
        reset_multip();
    }
    
    size_t parse(const char* in_data, size_t in_len, cb_error& ec)
    {
        EVMVC_DBG(_log,
            "parsing, len: {}\n{}", in_len, std::string(in_data, in_len)
        );
        
        if(in_len == 0)
            return 0;
        
        if(_status == parser_state::end ||
            _status == parser_state::error)
            reset();
        
        size_t _bytes_read = 0;
        
        switch(_status){
            case parser_state::parse_req_line:
            case parser_state::parse_header:{
                size_t sol_idx = 0;
                ssize_t eol_idx = find_eol(in_data, in_len, 0);
                if(eol_idx == -1)
                    return 0;
                
                const char* line = in_data + sol_idx;
                size_t line_len = eol_idx - sol_idx;
                
                while(eol_idx > -1){
                    switch(_status){
                        case parser_state::error:
                            return -1;
                        case parser_state::parse_req_line:
                            _bytes_read += parse_req_line(line, line_len, ec);
                            break;
                        case parser_state::parse_header:
                            _bytes_read += parse_headers(line, line_len, ec);
                            break;
                        default:
                            break;
                    }
                    
                    if(parsing_payload()){
                        _bytes_read += parse(
                            in_data + sol_idx,
                            in_len - sol_idx,
                            ec
                        );
                        break;
                    }
                    
                    sol_idx = eol_idx + EVMVC_EOL_SIZE;
                    eol_idx = find_eol(in_data, in_len, sol_idx);
                    if(eol_idx == -1)
                        break;
                    
                    line = in_data + sol_idx;
                    line_len = eol_idx - sol_idx;
                }
                break;
            }
            
            case parser_state::parse_body:
                _bytes_read += parse_body(in_data, in_len, ec);
                break;
            case parser_state::parse_form_multipart:
                _bytes_read += parse_form_multip(in_data, in_len, ec);
                break;
            case parser_state::parse_form_urlencoded:
                _bytes_read += parse_form_urlenc(in_data, in_len, ec);
                break;
            case parser_state::parse_form_text:
                _bytes_read += parse_form_txtpln(in_data, in_len, ec);
                break;
            
            default:
                break;
        }
        
        return _bytes_read;
    }
    
private:
    size_t parse_req_line(const char* line, size_t line_len, cb_error& ec)
    {
        if(line_len == 0){
            _status = parser_state::error;
            ec = EVMVC_ERR("Bad request line format");
            return 0;
        }
        
        ssize_t em_idx = find_ch(line, line_len, ' ', 0);
        if(em_idx == -1){
            _status = parser_state::error;
            ec = EVMVC_ERR("Bad request line format");
            return 0;
        }
        ssize_t eu_idx = find_ch(line, line_len, ' ', em_idx+1);
        if(eu_idx == -1){
            _status = parser_state::error;
            ec = EVMVC_ERR("Bad request line format");
            return 0;
        }
        
        _method_string = data_substr(line, 0, em_idx);
        evmvc::trim(_method_string);
        _method = evmvc::parse_method(_method_string);
        
        _uri_string = data_substring(line, em_idx+1, eu_idx);
        _uri = url(_uri_string);
        
        _http_ver_string = data_substring(line, eu_idx+1, line_len);
        evmvc::trim(_http_ver_string);
        _http_ver = http_version::unknown;
        if(!strcasecmp(_http_ver_string.c_str(), "http/1.0"))
            _http_ver = http_version::http_10;
        if(!strcasecmp(_http_ver_string.c_str(), "http/1.1"))
            _http_ver = http_version::http_11;
        //TODO: add http/2 support
        // if(!strcasecmp(_http_ver_string.c_str(), "http/2"))
        //     _http_ver = http_version::http_2;
        
        _status = parser_state::parse_header;
        _hdrs = std::make_shared<header_map>();
        
        //_bytes_read += eol_idx + EVMVC_EOL_SIZE;
        return line_len;
    }
    
    size_t parse_headers(const char* line, size_t line_len, cb_error& ec)
    {
        // if header section has ended.
        if(line_len == 0){
            validate_headers();
            post_headers_validation();
            return EVMVC_EOL_SIZE;
        }
        
        ssize_t sep = find_ch(line, line_len, ':', 0);
        if(sep == -1){
            _status = parser_state::error;
            ec = EVMVC_ERR("Bad header line format");
            return 0;
        }
        
        std::string hn(line, sep);
        EVMVC_TRACE(_log,
            "Inserting header, name: '{}', value: '{}'",
            hn, data_substring(line, sep+1, line_len)
        );
        auto it = _hdrs->find(hn);
        if(it != _hdrs->end())
            it->second.emplace_back(
                data_substring(line, sep+1, line_len)
                //std::string(line + sep +1, line_len - sep)
            );
        else
            _hdrs->emplace(std::make_pair(
                std::move(hn),
                std::vector<std::string>{
                    data_substring(line, sep+1, line_len)
                    //std::string(line + sep +1, line_len - sep)
                }
            ));
        
        return line_len + EVMVC_EOL_SIZE;
    }
    
    void validate_headers();
    
    void post_headers_validation()
    {
        if(_status != parser_state::parse_header)
            return;
        
        // look for content-length header:
        auto it = _hdrs->find("content-length");
        if(it != _hdrs->end()){
            _body_size = evmvc::str_to_num<size_t>(
                it->second.front()
            );
            _status = parser_state::parse_body;
            
            it = _hdrs->find("content-type");
            if(it == _hdrs->end())
                return;
            
            std::string ct_val = evmvc::trim_copy(
                it->second.front()
            );
            
            if(!strcasecmp(ct_val.c_str(), EVVMC_ENCTYPE_FRM_URLENC))
                _status = parser_state::parse_form_urlencoded;
            else if(!strcasecmp(ct_val.c_str(), EVVMC_ENCTYPE_FRM_MULTIP)){
                _status = parser_state::parse_form_multipart;
                init_multip();
            }else if(!strcasecmp(ct_val.c_str(), EVVMC_ENCTYPE_FRM_TXTPLN))
                _status = parser_state::parse_form_text;
            return;
        }
        
        _status = parser_state::end;
    }
    
    size_t parse_body(const char* in_data, size_t in_len, cb_error& ec)
    {
    }
    
    void reset_multip()
    {
        _mp_state = _internal::multipart_parser_state::init;
        _mp_total_size = 0;
        _mp_uploaded_size = 0;
        if(_mp_buf)
            evbuffer_free(_mp_buf);
        _mp_buf = nullptr;
        _mp_root.reset();
        _mp_current.reset();
        _mp_temp_dir = "";
        _mp_completed = false;
    }
    
    void init_multip();
    
    size_t parse_form_urlenc(const char* in_data, size_t in_len, cb_error& ec)
    {
    }
    
    size_t parse_form_txtpln(const char* in_data, size_t in_len, cb_error& ec)
    {
    }
    
    
    
    bool _mp_parse_boundary_header(char* hdr)
    {
        std::string hdr_line(hdr);
        size_t col_idx = hdr_line.find_first_of(":");
        if(col_idx == std::string::npos){
            _log->error("Invalid boundary header format!");
            return false;
        }
        
        std::string hdr_name = hdr_line.substr(0, col_idx);
        std::string hdr_val = hdr_line.substr(col_idx +1);
        
        EVMVC_TRACE(_log,
            "Inserting multipart header, name: '{}', value: '{}'",
            hdr_name, hdr_val
        );
        auto it = _mp_current->headers->find(hdr_name);
        if(it != _mp_current->headers->end())
            it->second.emplace_back(hdr_val);
        else
            _mp_current->headers->emplace(std::make_pair(
                std::move(hdr_name),
                std::vector<std::string>{hdr_val}
            ));
        
        return true;
    }
    
    bool _mp_assign_content_type()
    {
        auto ct = _mp_current->get(
            evmvc::to_string(evmvc::field::content_type)
        );
        auto cd = _mp_current->get(
            evmvc::to_string(evmvc::field::content_disposition)
        );
        
        if(!ct.empty()){
            _internal::multipart_subcontent_type sub_type =
                _internal::multipart_subcontent_type::unknown;
            
            if(ct.find("multipart/mixed") != std::string::npos)
                sub_type = _internal::multipart_subcontent_type::mixed;
            else if(ct.find("multipart/alternative") != std::string::npos)
                sub_type = _internal::multipart_subcontent_type::alternative;
            else if(ct.find("multipart/digest") != std::string::npos)
                sub_type = _internal::multipart_subcontent_type::digest;
            
            if(sub_type != _internal::multipart_subcontent_type::unknown){
                auto cc = std::make_shared<_internal::multipart_subcontent>(
                    _mp_current
                );
                cc->sub_type = sub_type;
                auto boundary = multip::get_boundary(ct);
                if(boundary.empty())
                    cc->set_boundary(_mp_current->get_parent());
                else
                    cc->set_boundary(boundary);
                
                _mp_current = cc;
                _mp_current->mime_type = "";
                ct.clear();
            }
            
        }else{
            ct = EVMVC_DEFAULT_MIME_TYPE;
        }
        
        _mp_current->name = multip::get_header_attribute(
            cd, "name"
        );
        
        if(!ct.empty()){
            _mp_current->mime_type = evmvc::trim_copy(ct);
            
            auto filename = multip::get_header_attribute(
                cd, "filename"
            );
            
            if(filename.empty()){
                auto cc = std::make_shared<_internal::multipart_content_form>(
                    _mp_current
                );
                _mp_current = cc;
                ct.clear();
            }else{
                auto cc = std::make_shared<_internal::multipart_content_file>(
                    _mp_current
                );
                
                cc->filename = filename;
                cc->temp_path = _mp_temp_dir / bfs::unique_path();
                cc->fd = open(cc->temp_path.c_str(), O_CREAT | O_WRONLY);
                if(cc->fd == -1)
                    return false;
                
                _mp_current = cc;
                ct.clear();
            }
        }
        
        if(auto sp = _mp_current->parent.lock())
            sp->contents.emplace_back(_mp_current);
        else
            return false;
        
        _mp_state =
            _mp_current->type == _internal::multipart_content_type::subcontent ?
                _internal::multipart_parser_state::headers :
                _internal::multipart_parser_state::content;
        
        return true;
    }
    
    evmvc::cb_error _mp_parse_end_of_section(bool& ended, char* line)
    {
        EVMVC_TRACE(_log, "parse_end_of_section");

        if(_mp_current->get_parent()->start_boundary == line){
            ended = true;
            EVMVC_TRACE(_log, "start boundary detected");
            
            _mp_state = evmvc::_internal::multipart_parser_state::headers;
            if(auto sp = _mp_current->parent.lock()){
                auto nc = std::make_shared<_internal::multipart_content>(
                    sp,
                    _internal::multipart_content_type::unset
                );
                _mp_current = nc;
                
            }else{
                return EVMVC_ERR(
                    "Unable to lock parent weak reference"
                );
            }
        }
        
        if(_mp_current->get_parent()->end_boundary == line){
            ended = true;
            EVMVC_TRACE(_log, "end boundary detected");
            
            _mp_state = evmvc::_internal::multipart_parser_state::headers;
            if(std::shared_ptr<_internal::multipart_subcontent> spa = 
                _mp_current->parent.lock()
            ){
                if(spa == _mp_root)
                    _mp_current = _mp_root;
                else if(std::shared_ptr<_internal::multipart_subcontent> spb =
                    spa->parent.lock()
                ){
                    auto nc = std::make_shared<_internal::multipart_content>(
                        spb,
                        _internal::multipart_content_type::unset
                    );
                    _mp_current = nc;
                }else
                    return EVMVC_ERR(
                        "Unable to lock parent weak reference"
                    );
            }else
                return EVMVC_ERR(
                    "Unable to lock parent weak reference"
                );
        }
        
        if(ended && _mp_current == _mp_root){
            EVMVC_TRACE(_log, "on_read_file_data transmission is completed!");
            _mp_completed = true;
        }
        
        return nullptr;
    }
    
    evmvc::cb_error _mp_on_read_form_data(bool& has_works)
    {
        EVMVC_TRACE(_log, "on_read_form_data");
        
        size_t len;
        char* line = evbuffer_readln(_mp_buf, &len, EVBUFFER_EOL_CRLF_STRICT);
        
        if(line == nullptr){
            has_works = false;
            return nullptr;
        }
        
        EVMVC_TRACE(_log, "recv: '{}'\n", line);
        bool ended = false;
        evmvc::cb_error cberr = _mp_parse_end_of_section(ended, line);
        if(cberr || ended){
            free(line);
            return cberr;
        }
        
        auto mf = std::static_pointer_cast<_internal::multipart_content_form>(
            _mp_current
        );
        
        mf->value += line;

        free(line);
        return nullptr;
    }
    
    evmvc::cb_error _mp_on_read_file_data(bool& has_works)
    {
        EVMVC_TRACE(_log, "on_read_file_data");
        size_t len;
        char* line = evbuffer_readln(_mp_buf, &len, EVBUFFER_EOL_CRLF_STRICT);
        
        auto mf = std::static_pointer_cast<_internal::multipart_content_file>(
            _mp_current
        );
        
        if(line != nullptr){
            EVMVC_TRACE(_log, "recv: '{}'\n", line);
            bool ended = false;
            evmvc::cb_error cberr = _mp_parse_end_of_section(ended, line);
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
            size_t buf_size = evbuffer_get_length(_mp_buf);
            if(buf_size >= EVMVC_MAX_CONTENT_BUF_LEN){
                if(mf->append_crlf && _internal::writen(mf->fd, "\r\n", 2) < 0){
                    return EVMVC_ERR(
                        "Failed to write to temp file '{}'\nErr: {}",
                        mf->temp_path.c_str(), strerror(errno)
                    );
                }
                mf->append_crlf = false;
                
                char buf[buf_size];
                evbuffer_remove(_mp_buf, buf, buf_size);
                EVMVC_TRACE(_log,
                    "extracted: '{}'",
                    std::string(buf, buf+buf_size)
                );
                if(_internal::writen(mf->fd, buf, buf_size) < 0){
                    return EVMVC_ERR(
                        "Failed to write to temp file '{}'\nErr: {}",
                        mf->temp_path.c_str(), strerror(errno)
                    );
                }
            }
            has_works = false;
            return nullptr;
        }
        
        if(mf->append_crlf && _internal::writen(mf->fd, "\r\n", 2) < 0){
            free(line);
            return EVMVC_ERR(
                "Failed to write to temp file '{}'\nErr: {}",
                mf->temp_path.c_str(), strerror(errno)
            );
        }
        
        if(_internal::writen(mf->fd, line, len) < 0){
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

    void _mp_on_request_end()
    {
        EVMVC_TRACE(_log, "on multipart request end");
        reset_multip();
        _status = parser_state::end;
    }
    
    //void _mp_on_read_multipart_data(evbuffer* buf, void *arg)
    size_t parse_form_multip(const char* in_data, size_t in_len, cb_error& ec)
    {
        //auto mp = (evmvc::_internal::multipart_parser*)arg;
        if(_mp_state == _internal::multipart_parser_state::failed)
            return 0;
        
        //size_t blen = evbuffer_get_length(buf);
        
        EVMVC_TRACE(_log,
            "on_read_multipart_data received '{}' bytes", in_len
        );
        
        _mp_uploaded_size += in_len;
        evbuffer_add(_mp_buf, in_data, in_len);
        
        evmvc::status res = evmvc::status::ok;
        evmvc::cb_error cberr(nullptr);
        
        bool has_works = true;
        while(has_works && !_mp_completed){
            switch(_mp_state){
                case _internal::multipart_parser_state::init:{
                    size_t len;
                    char* line = evbuffer_readln(
                        _mp_buf, &len, EVBUFFER_EOL_CRLF_STRICT
                    );
                    if(!line){
                        has_works = false;
                        break;
                    }
                    
                    EVMVC_TRACE(_log, "recv: '{}'", line);
                    if(_mp_current->get_parent()->start_boundary != line){
                        free(line);
                        cberr = EVMVC_ERR(
                            "must start with the boundary\n'{}'\n"
                            "But started with '{}'\n",
                            _mp_current->get_parent()->start_boundary,
                            line
                        );
                        res = evmvc::status::bad_request;
                        goto cleanup;
                    }
                    
                    // header boundary found
                    _mp_state = _internal::multipart_parser_state::headers;
                    if(auto sp = _mp_current->parent.lock()){
                        auto nc =
                            std::make_shared<_internal::multipart_content>(
                                sp,
                                _internal::multipart_content_type::unset
                            );
                        _mp_current = nc;
                        free(line);
                        break;
                        
                    }else{
                        free(line);
                        cberr = EVMVC_ERR(
                            "Unable to lock parent weak reference"
                        );
                        res = evmvc::status::internal_server_error;
                        goto cleanup;
                    }
                }
                
                case _internal::multipart_parser_state::headers:{
                    size_t len;
                    char* line = evbuffer_readln(
                        _mp_buf, &len, EVBUFFER_EOL_CRLF_STRICT
                    );
                    if(line == nullptr){
                        has_works = false;
                        break;
                    }
                    
                    EVMVC_TRACE(_log, "recv: '{}'", line);
                    if(len == 0){
                        // end of header part
                        free(line);
                        if(!_mp_assign_content_type()){
                            cberr = EVMVC_ERR(
                                "Unable to assign content type!"
                            );
                            res = evmvc::status::bad_request;
                            goto cleanup;
                        }
                        break;
                    }
                    
                    if(!_mp_parse_boundary_header(line)){
                        free(line);
                        
                        cberr = EVMVC_ERR(
                            "Unable to parse the boundary header!"
                        );
                        res = evmvc::status::bad_request;
                        goto cleanup;
                    }
                    
                    free(line);
                    break;
                }
                
                case _internal::multipart_parser_state::content:{
                    switch(_mp_current->type){
                        case _internal::multipart_content_type::form:
                            cberr = _mp_on_read_form_data(has_works);
                            break;
                        case _internal::multipart_content_type::file:
                            cberr = _mp_on_read_file_data(has_works);
                            break;
                            
                        default:
                            if(res == evmvc::status::ok){
                                cberr = EVMVC_ERR(
                                    "Invalid multipart content type!"
                                );
                                res = evmvc::status::internal_server_error;
                            }
                            goto cleanup;
                    }
                    
                    if(res != evmvc::status::ok || cberr)
                        goto cleanup;
                    
                    break;
                }
                
                case _internal::multipart_parser_state::failed:{
                    if(res == evmvc::status::ok){
                        cberr = EVMVC_ERR(
                            "Multipart parser has failed!"
                        );
                        res = evmvc::status::internal_server_error;
                    }
                    goto cleanup;
                }
            }
        }
        
        cleanup:
    
        if(cberr || res != evmvc::status::ok){
            if(!cberr)
                cberr = EVMVC_ERR(
                    "Unknown error has occured!"
                );
            if(res == evmvc::status::ok)
                res = evmvc::status::internal_server_error;
        }

        if(res != evmvc::status::ok){
            //evhtp_request_set_keepalive(req, 0);
            _mp_state = _internal::multipart_parser_state::failed;
            _res->error(res, cberr);
            return in_len;
        }
        
        if(_mp_completed){
            EVMVC_TRACE(_log, "Multipart parser task completed!");
            _res->req()->_load_multipart_params(_mp_root);
            _mp_on_request_end();
        }
        
        return in_len;
    }








    
    /// private vars
    
    parser_state _status = parser_state::parse_req_line;
    
    size_t _body_size = 0;
    uint64_t _total_bytes_read = 0;
    
    std::string _method_string;
    evmvc::method _method;
    
    std::string _scheme;
    std::string _uri_string;
    evmvc::url _uri;
    std::string _http_ver_string;
    http_version _http_ver;
    
    std::shared_ptr<header_map> _hdrs;
    sp_response _res;
    
    // multipart data
    _internal::multipart_parser_state _mp_state =
        _internal::multipart_parser_state::init;
    uint64_t _mp_total_size = 0;
    uint64_t _mp_uploaded_size = 0;
    evbuffer* _mp_buf = nullptr;
    std::shared_ptr<_internal::multipart_subcontent> _mp_root;
    std::shared_ptr<_internal::multipart_content> _mp_current;
    bfs::path _mp_temp_dir = "";
    bool _mp_completed = false;
};


}//::evmvc

#endif//_libevmvc_http_parser_h