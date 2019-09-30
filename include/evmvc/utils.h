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

#ifndef _libevmvc_utils_h
#define _libevmvc_utils_h

#include "stable_headers.h"




namespace evmvc {

inline std::string html_version()
{
    static std::string ver =
        md::replace_substring_copy(
            md::replace_substring_copy(version(), "\n", "<br/>"),
            " ", "&nbsp;"
        );
    return ver;
}



template<typename PARAM_T>
PARAM_T json_default(
    const evmvc::json& jobj,
    md::string_view key,
    PARAM_T def_val)
{
    auto it = jobj.find(key.to_string());
    if(it == jobj.end())
        return def_val;
    if(it->is_null())
        return def_val;
    return *it;
}


inline evmvc::json parse_jsonc_string(const std::string jsonwc)
{
    bool in_str = false;
    // single line comments
    bool in_scom = false;
    // multi line comments
    bool in_mcom = false;
    
    ssize_t last_comma_loc = -1;
    
    std::string res;
    // removes comments
    for(size_t i = 0; i < jsonwc.size(); ++i){
        //char pprev_chr = i > 1 ? jsonwc[i-2] : 0;
        char prev_chr = i > 0 ? jsonwc[i-1] : 0;
        char cur_chr = jsonwc[i];
        char nxt_chr = i < jsonwc.size()-1 ? jsonwc[i+1] : 0;
        
        if(in_scom){
            if(cur_chr == '\n'){
                in_scom = false;
                if(prev_chr == '\r')
                    res += '\r';
                res += '\n';
            }
            continue;
        }
        
        if(in_mcom){
            if(cur_chr == '/' && prev_chr == '*'){
                in_mcom = false;
                
            }else if(cur_chr == '\n'){
                if(prev_chr == '\r')
                    res += '\r';
                res += '\n';
            }
            
            continue;
        }
        
        if(in_str){
            if(cur_chr == '\\'){
                if(nxt_chr != 0){
                    res += cur_chr;
                    res += nxt_chr;
                    ++i;
                    continue;
                }
                throw MD_ERR("JSONC invalid format!");
            }
            
            if(cur_chr == '"')
                in_str = false;
            res += cur_chr;
            continue;
        }
        
        if(cur_chr == '/' && nxt_chr == '/'){
            in_scom = true;
            continue;
        }
        
        if(cur_chr == '/' && nxt_chr == '*'){
            in_mcom = true;
            continue;
        }
        
        if(cur_chr == '"'){
            last_comma_loc = -1;
            in_str = true;
            res += cur_chr;
            continue;
        }
        
        //verify if it's a bad comma
        if(cur_chr == '}' || cur_chr == ']'){
            if(last_comma_loc > -1){
                std::string tmp = res.substr(0, last_comma_loc);
                if((size_t)(last_comma_loc+1) < res.size())
                    tmp += res.substr(last_comma_loc+1);
                res = tmp;
            }
            last_comma_loc = -1;
        }
        if(cur_chr == ','){
            last_comma_loc = res.size();
        }else if(last_comma_loc > -1 && !isspace(cur_chr) &&
            cur_chr != '\t' && cur_chr != '\n' && cur_chr != '\r'
        ){
            last_comma_loc = -1;
        }
        
        res += cur_chr;
    }
    
    return evmvc::json::parse(res);
}
inline evmvc::json parse_jsonc_file(
    const md::string_view jsonc_filename)
{
    std::ifstream fin(
        jsonc_filename.data(), std::ios::binary
    );
    std::ostringstream ostrm;
    ostrm << fin.rdbuf();
    std::string jsonwc = ostrm.str();
    fin.close();
    return parse_jsonc_string(jsonwc);
}


inline std::string data_substring(
    const char* data, size_t offset, size_t end_idx)
{
    return std::string(data+offset, end_idx-offset);
}

inline std::string data_substr(
    const char* data, size_t offset, size_t len)
{
    return std::string(data+offset, len);
}

inline ssize_t find_ch(
    const char* data, size_t len, char ch, size_t start_pos)
{
    for(size_t i = start_pos; i < len; ++i)
        if(data[i] == ch)
            return i;
    return -1;
}

inline ssize_t find_eol(const char* data, size_t len, size_t start_pos)
{
    for(size_t i = start_pos; i < len -1; ++i)
        if(data[i] == '\r' && data[i+1] == '\n')
            return i;
    return -1;
}

inline std::string escape(md::string_view s)
{
    char* r = evhttp_encode_uri(s.data());
    std::string tmp(r);
    free(r);
    return tmp;
}
inline std::string unescape(md::string_view s)
{
    char* r = evhttp_decode_uri(s.data());
    std::string tmp(r);
    free(r);
    return tmp;
}
inline std::string html_escape(md::string_view s)
{
    char* r = evhttp_htmlescape(s.data());
    std::string tmp(r);
    free(r);
    return tmp;
}

inline std::string encode_uri(md::string_view s)
{
    std::ostringstream os;
    
    for(auto c : s){
        if(
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == ';' || c == ',' || c == '/' || c == '?' || c == ':' ||
            c == '@' || c == '&' || c == '=' || c == '+' || c == '$' ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')' || c == '#'
        ){
            os << c;
            continue;
        }
        
        os << "%";
        os << std::hex << std::setw(2) << (int)c;
    }
    
    return os.str();
}
inline std::string decode_uri(md::string_view s)
{
    std::ostringstream os;
    
    for(size_t i = 0; i < s.size(); ++i){
        if(s[i] == '%' && i < s.size()-2){
            os << (unsigned char)strtol(s.data() +i+1, nullptr, 16);
            i += 2;
            continue;
        }
        
        os << s[i];
    }
    
    return os.str();
}
inline std::string encode_uri_component(md::string_view s)
{
    std::ostringstream os;
    
    for(auto c : s){
        if(
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')'
        ){
            os << c;
            continue;
        }
        
        os << "%";
        os << std::hex << std::setw(2) << (int)c;
    }
    
    return os.str();
}
inline std::string decode_uri_component(md::string_view s)
{
    return decode_uri(s);
}



/* Generate the human-readable hex representation of an apr_uint64_t
* (basically a faster version of 'sprintf("%llx")')
*/
#define HEX_DIGITS "0123456789abcdef"
static char *etag_uint64_to_hex(char *next, uint64_t u)
{
    int printing = 0;
    int shift = sizeof(uint64_t) * 8 - 4;
    do {
        unsigned short next_digit =
            (unsigned short)((u >> shift) & (uint64_t)0xf);
        
        if (next_digit) {
            *next++ = HEX_DIGITS[next_digit];
            printing = 1;
        
        } else if (printing) {
            *next++ = HEX_DIGITS[next_digit];
        }
        
        shift -= 4;
        
    } while (shift);
    
    *next++ = HEX_DIGITS[u & (uint64_t)0xf];
    
    return next;
}

void get_etag(const struct stat& file_stat, std::string& str_etag);
inline void get_etag(const char* filename, std::string& str_etag)
{
    struct stat file_stat;
    stat(filename, &file_stat);
    get_etag(file_stat, str_etag);
}

constexpr size_t CHARS_PER_UINT64 = (sizeof(uint64_t) * 2);
constexpr size_t CHARS_PER_ETAG = (3 * CHARS_PER_UINT64 + 1 + sizeof("--"));

inline void get_etag(const struct stat& file_stat, std::string& str_etag)
{
    //char *etag = new char[3 * CHARS_PER_UINT64 + 1 + sizeof("\"--\"")];
    //char *etag = new char[3 * CHARS_PER_UINT64 + 1 + sizeof("--")];
    char *etag = new char[CHARS_PER_ETAG];
    char *next = etag;

    //*next++ = '"';
    next = etag_uint64_to_hex( next, file_stat.st_ino);
    *next++ = '-';
    next = etag_uint64_to_hex( next, file_stat.st_size);
    *next++ = '-';
    next = etag_uint64_to_hex( next, file_stat.st_mtime);
    //*next++ = '"';
    *next = '\0';
    
    str_etag.append(etag);
    
    delete[] etag;
}



} //ns evmvc

#endif //_boost_beast_mvc_utils_h
