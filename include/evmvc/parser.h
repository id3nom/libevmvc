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
#include "logging.h"

#include "headers.h"

#define EVMVC_EOL_SIZE 2
namespace evmvc {


class parser
{
public:
    parser(wp_connection conn, const sp_logger log)
        : _conn(conn), _log(log)
    {
    }
    
    sp_connection get_connection() const { return _conn.lock();}
    sp_logger log() const { return _log;}
    
    virtual void reset() = 0;
    virtual size_t parse(const char* buf, size_t len, cb_error& ec) = 0;
    
private:
    wp_connection _conn;
    sp_logger _log;
};

class http_parser
    : public parser
{
    enum class error
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
    
    enum class flag
    {
        none                    = 0,
        chunked                 = (1 << 0),
        connection_keep_alive   = (1 << 1),
        connection_close        = (1 << 2),
        trailing                = (1 << 3),
    };
    
    enum class state
    {
        parse_req_line = 0,
        parse_header_section,
        parse_body_section,
    };
    
    
public:
    http_parser(wp_connection conn, const sp_logger& log)
        : parser(conn, log->add_child("parser"))
    {
    }
    
    void reset()
    {
        _flags = http_parser::flag::none;
        _status = http_parser::state::parse_req_line;
        
        _body_size = 0;
        _total_bytes_read = 0;
        _bytes_read = 0;
        
        _hdrs.reset();
        _hdrs_eol_count = 0;
    }
    
    size_t parse(const char* in_data, size_t in_len, cb_error& ec)
    {
        if(in_len == 0)
            return 0;
        
        _bytes_read = 0;
        
        size_t sol_idx = 0;
        ssize_t eol_idx = _find_eol(in_data, in_len);
        if(eol_idx == -1)
            return 0;
        
        const char* line = in_data + sol_idx;
        size_t line_len = eol_idx - sol_idx;
        
        while(eol_idx > -1){
            switch(_status){
                case http_parser::state::parse_req_line:{
                    if(line_len == 0){
                        ec = EVMVC_ERR("Bad request line format");
                        return _bytes_read;
                    }
                    
                    ssize_t em_idx = _find_ch(line, line_len, ' ');
                    if(em_idx == -1){
                        ec = EVMVC_ERR("Bad request line format");
                        return _bytes_read;
                    }
                    ssize_t eu_idx = _find_ch(
                        line + em_idx +1, line_len - em_idx -1, ' '
                    );
                    if(eu_idx == -1){
                        ec = EVMVC_ERR("Bad request line format");
                        return _bytes_read;
                    }
                    
                    _method = std::string(line, em_idx);
                    _uri = std::string(line + em_idx +1, eu_idx - em_idx);
                    _http_ver = std::string(
                        line + eu_idx +1, line_len - eu_idx
                    );
                    
                    _bytes_read += eol_idx + EVMVC_EOL_SIZE;
                    _status = http_parser::state::parse_header_section;
                    _hdrs = std::make_shared<header_map>();
                    break;
                }
                case http_parser::state::parse_header_section:{
                    if(line_len == 0){
                        _status = http_parser::state::parse_body_section;
                        _bytes_read += EVMVC_EOL_SIZE;
                        break;
                    }
                    
                    ssize_t sep = _find_ch(line, line_len, ':');
                    if(sep == -1){
                        ec = EVMVC_ERR("Bad header line format");
                        return _bytes_read;
                    }
                    
                    std::string hn(line, sep);
                    auto it = _hdrs->find(hn);
                    if(it != _hdrs->end())
                        it->second.emplace_back(
                            std::string(line + sep +1, line_len - sep)
                        );
                    else
                        _hdrs->emplace(std::make_pair(
                            std::move(hn),
                            std::vector<std::string>{
                                std::string(line + sep +1, line_len - sep)
                            }
                        ));
                    _bytes_read += line_len + EVMVC_EOL_SIZE;
                }
                case http_parser::state::parse_body_section:{
                    _bytes_read += line_len + EVMVC_EOL_SIZE;
                    break;
                }
                default:
                    break;
            }
            
            sol_idx = eol_idx + EVMVC_EOL_SIZE;
            eol_idx = _find_eol(in_data + sol_idx, in_len - sol_idx);
            if(eol_idx == -1)
                break;
            
            line = in_data + sol_idx;
            line_len = eol_idx - sol_idx;
        }
        
        return _bytes_read;
    }
    
private:
    
    ssize_t _find_ch(const char* data, size_t len, char ch)
    {
        for(size_t i = 0; i < len; ++i)
            if(data[i] == ch)
                return i;
        return -1;
    }
    
    ssize_t _find_eol(const char* data, size_t len)
    {
        for(size_t i = 0; i < len -1; ++i)
            if(data[i] == '\r' && data[i+1] == '\n')
                return i;
        return -1;
    }
    
    void _parse_request_line(const char* data, size_t len)
    {
        
    }
    
    http_parser::error _err = http_parser::error::none;
    http_parser::flag _flags = http_parser::flag::none;
    http_parser::state _status = http_parser::state::parse_req_line;
    
    size_t _body_size = 0;
    uint64_t _total_bytes_read = 0;
    uint64_t _bytes_read = 0;
    
    
    std::string _method;
    std::string _uri;
    std::string _http_ver;
    
    std::string _scheme;
    
    std::shared_ptr<header_map> _hdrs;
    size_t _hdrs_eol_count = 0;
    
    size_t _buf_size = 0;
    char* _buf[8192];
    
};


}//::evmvc

#endif//_libevmvc_http_parser_h