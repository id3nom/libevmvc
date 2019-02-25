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

// #include <string>
// #include <algorithm> 
// #include <cctype>
// #include <locale>
// #include <string>
// #include <numeric>
// #include <type_traits>
// #include <functional>
// #include <chrono>


#define EVMVC_ENUM_FLAGS(t) \
inline constexpr t operator&(t x, t y) \
{ \
    return static_cast<t>(static_cast<int>(x) & static_cast<int>(y)); \
} \
inline constexpr t operator|(t x, t y) \
{ \
    return static_cast<t>(static_cast<int>(x) | static_cast<int>(y)); \
} \
inline constexpr t operator^(t x, t y) \
{ \
    return static_cast<t>(static_cast<int>(x) ^ static_cast<int>(y)); \
} \
inline constexpr t operator~(t x) \
{ \
    return static_cast<t>(~static_cast<int>(x)); \
} \
inline t& operator&=(t& x, t y) \
{ \
    x = x & y; \
    return x; \
} \
inline t& operator|=(t& x, t y) \
{ \
    x = x | y; \
    return x; \
} \
inline t& operator^=(t& x, t y) \
{ \
    x = x ^ y; \
    return x; \
}

#define EVMVC_SET_BIT(_v, _f) _v |= _f
#define EVMVC_UNSET_BIT(_v, _f) _v &= ~_f
#define EVMVC_TEST_FLAG(_v, _f) ((_v & _f) == _f)

// #define mm__alloc_(type, ...)
//     (type*)evmvc::_internal::mm__dup_((type[]) {__VA_ARGS__ }, sizeof(type))

#define mm__alloc_(dst, type, ...) \
{ \
    type _tmp_types_[] = {__VA_ARGS__ }; \
    dst = (type*)evmvc::_internal::mm__dup_(_tmp_types_, sizeof(type)); \
}

namespace evmvc {
namespace _internal {
    static void* mm__dup_(const void* src, size_t size)
    {
        void* mem = malloc(size);
        return mem ? memcpy(mem, src, size) : NULL;
    }
   
}//ns evmvc::_internal


class cb_error
{
    friend std::ostream& operator<<(std::ostream& s, const cb_error& v);
public:
    static cb_error no_err;
    
    cb_error(): _has_err(false), _msg(""), _has_stack(false)
    {}
    
    cb_error(nullptr_t /*np*/): _has_err(false), _msg(""), _has_stack(false)
    {}
    
    cb_error(const std::exception& err):
        _err(err), _has_err(true)
    {
        _msg = std::string(err.what());
        
        try{
            auto se = dynamic_cast<const evmvc::stacked_error&>(err);
            _stack = se.stack();
            _file = se.file().data();
            _line = se.line();
            _func = se.func().data();
            _has_stack = true;
            
        }catch(...){
            _has_stack = false;
        }
    }
    
    // cb_error(const std::exception& err): _err(err), _has_err(true)
    // {
    //     _msg = std::string(err.what());
    // }
    
    virtual ~cb_error()
    {
    }
    
    const std::exception& error() const { return _err;}
    
    explicit operator bool() const
    {
        return _has_err;
    }
    
    explicit operator const char*() const
    {
        if(!_has_err)
            return "No error assigned!";
        return _msg.c_str();
    }
    
    const char* c_str() const
    {
        if(!_has_err)
            return "No error assigned!";
        return _msg.c_str();
    }
    
    bool has_stack() const { return _has_stack;}
    std::string stack() const { return _stack;}
    std::string file() const { return _file;}
    int line() const { return _line;}
    std::string func() const { return _func;}
    
private:
    std::exception _err;
    bool _has_err;
    std::string _msg;
    
    bool _has_stack;
    std::string _stack;
    std::string _file;
    int _line;
    std::string _func;
};

std::ostream& operator<<(std::ostream& s, const cb_error& v)
{
    s << std::string(v.c_str());
    return s;
}

typedef std::function<void()> void_cb;
typedef std::function<void(const cb_error& err)> async_cb;


inline bool to_bool(evmvc::string_view val)
{
    return
        val.size() > 0 &&
        strcasecmp(val.data(), "false") != 0 &&
        strcasecmp(val.data(), "0") != 0;
}

inline evmvc::string_view to_string(bool val)
{
    return val ? "true" : "false";
}


struct string_view_cmp {
    bool operator()(
        const evmvc::string_view& a,
        const evmvc::string_view& b) const
    {
        return strncmp(a.data(), b.data(), std::min(a.size(), b.size())) < 0;
    }
};


inline void lower_case(std::string& str)
{
    std::transform(
        str.begin(), str.end(), str.begin(),
        [](unsigned char c){ return std::tolower(c); }
    );
}

inline std::string lower_case_copy(evmvc::string_view str)
{
    std::string s = str.to_string();
    lower_case(s);
    return s;
}


inline bool iequals(const std::string& a, const std::string& b)
{
    return (
        (a.size() == b.size()) && 
        std::equal(
            a.begin(), a.end(),
            b.begin(), 
            [](const char& ca, const char& cb)-> bool {
                return ca == cb || std::tolower(ca) == std::tolower(cb);
            }
        )
    );
}

class no_grouping : public std::numpunct_byname<char> {
    std::string do_grouping() const { return ""; }
public:
    no_grouping() : numpunct_byname("") {}
};

template <typename T>
inline std::string num_to_str(T number, bool grouping = true)
{
    std::stringstream ss;
    if(!grouping)
        ss.imbue(std::locale(std::locale(""), new no_grouping));
    
    ss << number;
    return ss.str();
}

template <typename T>
inline T str_to_num(const std::string& text)
{
    std::stringstream ss(text);
    T result;
    return ss >> result ? result : 0;
}

// trim from start (in place)
inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
inline std::string ltrim_copy(std::string s)
{
    ltrim(s);
    return s;
}

// trim from end (copying)
inline std::string rtrim_copy(std::string s)
{
    rtrim(s);
    return s;
}

// trim from both ends (copying)
inline std::string trim_copy(std::string s)
{
    trim(s);
    return s;
}

template< typename T >
inline std::string join(const T& lst, const std::string& separator)
{
    static_assert(
        std::is_same<typename T::value_type, std::string>::value,
        "Type T must be a container of std::string type"
    );
    return
        std::accumulate(lst.begin(), lst.end(), std::string(),
            [&separator](const std::string& a, const std::string& b) -> std::string { 
                return a + (a.length() > 0 ? separator : "") + b;
            }
        );
}

inline void replace_substring(
    std::string& s, const std::string& f, const std::string& t)
{
    for (auto pos = s.find(f);            // find first occurrence of f
        pos != std::string::npos;         // make sure f was found
        s.replace(pos, f.size(), t),      // replace with t, and
        pos = s.find(f, pos + t.size()))  // find next occurrence of f
    {}
}

inline std::string replace_substring_copy(
    std::string s, const std::string& f, const std::string& t)
{
    replace_substring(s, f, t);
    return s;
}

inline std::string html_version()
{
    static std::string ver =
        evmvc::replace_substring_copy(
            evmvc::replace_substring_copy(version(), "\n", "<br/>"),
            " ", "&nbsp;"
        );
    return ver;
}


//std::string hex_to_str(const uint8_t* data, int len);
constexpr char hexmap[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

inline std::string hex_to_str(const uint8_t* data, int len)
{
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i) {
        s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
}




static const std::string base64_chars(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"
);

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(evmvc::string_view value)
{
    const unsigned char* bytes_to_encode = 
        (const unsigned char*)value.data();
    size_t in_len = value.size();
    
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while(in_len--){
        char_array_3[i++] = *(bytes_to_encode++);
        if(i == 3){
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] =
                ((char_array_3[0] & 0x03) << 4) +
                ((char_array_3[1] & 0xf0) >> 4);
            
            char_array_4[2] =
                ((char_array_3[1] & 0x0f) << 2) +
                ((char_array_3[2] & 0xc0) >> 6);
            
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            
            i = 0;
        }
    }
    
    if(i){
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] =
            ((char_array_3[0] & 0x03) << 4) +
            ((char_array_3[1] & 0xf0) >> 4);
        
        char_array_4[2] =
            ((char_array_3[1] & 0x0f) << 2) +
            ((char_array_3[2] & 0xc0) >> 6);
        
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
    }
    
    return ret;
}

std::string base64_decode(evmvc::string_view encoded_string)
{
    size_t in_len = encoded_string.size();
    size_t i = 0;
    size_t j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    
    while(in_len-- && 
        (encoded_string[in_] != '=') &&
        is_base64(encoded_string[in_])
    ){
        char_array_4[i++] = encoded_string[in_]; in_++;
        if(i ==4){
            for(i = 0; i <4; i++)
                char_array_4[i] =
                    static_cast<unsigned char>(
                        base64_chars.find(char_array_4[i])
                    );
            
            char_array_3[0] =
                (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            
            char_array_3[1] =
                ((char_array_4[1] & 0xf) << 4) +
                ((char_array_4[2] & 0x3c) >> 2);
            
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for(i = 0; (i < 3); i++)
                ret += char_array_3[i];
            
            i = 0;
        }
    }
    
    if(i){
        for(j = i; j <4; j++)
            char_array_4[j] = 0;
        
        for(j = 0; j <4; j++)
            char_array_4[j] =
                static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
        
        char_array_3[0] =
            (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] =
            ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for(j = 0; (j < i - 1); j++)
            ret += char_array_3[j];
    }

    return ret;
}


namespace _internal{
    struct gzip_file{
        event* ev;
        int fd_in;
        int fd_out;
        z_stream* zs;
        uLong zs_size;
        evmvc::async_cb cb;
    };
    
    struct gzip_file* new_gzip_file()
    {
        struct evmvc::_internal::gzip_file* gzf = nullptr;
        mm__alloc_(gzf, struct evmvc::_internal::gzip_file, {
            nullptr, -1, -1, nullptr, 0, nullptr
        });
        
        gzf->zs = (z_stream*)malloc(sizeof(*gzf->zs));
        memset(gzf->zs, 0, sizeof(*gzf->zs));
        return gzf;
    }
    
    void free_gzip_file(struct evmvc::_internal::gzip_file* gzf)
    {
        deflateEnd(gzf->zs);
        event_del(gzf->ev);
        event_free(gzf->ev);
        close(gzf->fd_in);
        close(gzf->fd_out);
        free(gzf->zs);
        free(gzf);
    }
    
    void gzip_file_on_read(int fd, short events, void* arg)
    {
        if((events & IN_IGNORED) == IN_IGNORED)
            return;
        
        gzip_file* gzf = (gzip_file*)arg;
        
        char buf[EVMVC_READ_BUF_SIZE];
        /* try to read EVMVC_READ_BUF_SIZE bytes from the file pointer */
        do{
            ssize_t bytes_read = read(gzf->fd_in, buf, sizeof(buf));
            
            if(bytes_read == -1){
                int terr = errno;
                if(terr == EAGAIN || terr == EWOULDBLOCK)
                    return;
                auto cb = gzf->cb;
                free_gzip_file(gzf);
                cb(EVMVC_ERR(
                    "read function returned: '{}'",
                    terr
                ));
            }
            
            if(bytes_read > 0){
                gzf->zs->next_in = (Bytef*)buf;
                gzf->zs->avail_in = bytes_read;
                
                // retrieve the compressed bytes blockwise.
                int ret;
                char zbuf[EVMVC_READ_BUF_SIZE];
                bytes_read = 0;
                
                gzf->zs->next_out = reinterpret_cast<Bytef*>(zbuf);
                gzf->zs->avail_out = sizeof(zbuf);
                
                ret = deflate(gzf->zs, Z_SYNC_FLUSH);
                if(ret != Z_OK && ret != Z_STREAM_END){
                    auto cb = gzf->cb;
                    free_gzip_file(gzf);
                    cb(EVMVC_ERR(
                        "zlib deflate function returned: '{}'",
                        ret
                    ));
                    return;
                }
                
                if(gzf->zs_size < gzf->zs->total_out){
                    bytes_read += gzf->zs->total_out - gzf->zs_size;
                    writen(
                        gzf->fd_out,
                        zbuf,
                        gzf->zs->total_out - gzf->zs_size
                    );
                    gzf->zs_size = gzf->zs->total_out;
                }
                
            }else{
                int ret = deflate(gzf->zs, Z_FINISH);
                if(ret != Z_OK && ret != Z_STREAM_END){
                    auto cb = gzf->cb;
                    free_gzip_file(gzf);
                    cb(EVMVC_ERR(
                        "zlib deflate function returned: '{}'",
                        ret
                    ));
                    return;
                }
                
                if(gzf->zs_size < gzf->zs->total_out){
                    bytes_read += gzf->zs->total_out - gzf->zs_size;
                    writen(
                        gzf->fd_out,
                        gzf->zs->next_out,
                        gzf->zs->total_out - gzf->zs_size
                    );
                    gzf->zs_size = gzf->zs->total_out;
                }

                auto cb = gzf->cb;
                free_gzip_file(gzf);
                cb(nullptr);
                return;
            }
        }while(true);
    }
    
}// _internal
void gzip_file(
    event_base* ev_base,
    evmvc::string_view src,
    evmvc::string_view dest,
    evmvc::async_cb cb)
{
    cb(EVMVC_ERR(
        "file async IO Not working with EPOLL!\n"
        "Should be reimplement with aio or thread pool!"
    ));
    struct evmvc::_internal::gzip_file* gzf = _internal::new_gzip_file();
    if(deflateInit2(
        gzf->zs,
        Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        EVMVC_ZLIB_GZIP_WSIZE,
        EVMVC_ZLIB_MEM_LEVEL,
        EVMVC_ZLIB_STRATEGY
        ) != Z_OK
    ){
        free(gzf->zs);
        free(gzf);
        cb(EVMVC_ERR("deflateInit2 failed!"));
        return;
    }
    
    gzf->fd_in = open(src.data(), O_NONBLOCK | O_RDONLY);
    gzf->fd_out = open(
        dest.data(),
        O_CREAT | O_WRONLY, 
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP//ug+wr
    );
    gzf->cb = cb;
    
    gzf->ev = event_new(
        ev_base, gzf->fd_in, EV_READ | EV_PERSIST,
        _internal::gzip_file_on_read, gzf
    );
    if(event_add(gzf->ev, nullptr) == -1){
        _internal::free_gzip_file(gzf);
        cb(EVMVC_ERR("evmvc::gzip_file::event_add failed!"));
    }
}

void gzip_file(
    evmvc::string_view src, evmvc::string_view dest)
{
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if(deflateInit2(
        &zs,
        Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        EVMVC_ZLIB_GZIP_WSIZE,
        EVMVC_ZLIB_MEM_LEVEL,
        EVMVC_ZLIB_STRATEGY
        ) != Z_OK
    ){
        throw EVMVC_ERR(
            "deflateInit2 failed!"
        );
    }
    std::ifstream fin(
        src.data(), std::ios::binary
    );
    std::ostringstream ostrm;
    ostrm << fin.rdbuf();
    std::string inf_data = ostrm.str();
    fin.close();
    
    std::string def_data;
    zs.next_in = (Bytef*)inf_data.data();
    zs.avail_in = inf_data.size();
    int ret;
    char outbuffer[32768];
    
    do{
        zs.next_out =
            reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        if (def_data.size() < zs.total_out) {
            // append the block to the output string
            def_data.append(
                outbuffer,
                zs.total_out - def_data.size()
            );
        }
    }while(ret == Z_OK);
    
    deflateEnd(&zs);
    if(ret != Z_STREAM_END){
        throw EVMVC_ERR(
            "Error while log compression: ({}) {}",
            ret, zs.msg
        );
    }
    
    std::ofstream fout(
        dest.data(), std::ios::binary
    );
    fout.write(def_data.c_str(), def_data.size());
    fout.flush();
    fout.close();
}

template<typename PARAM_T>
PARAM_T json_default(
    const evmvc::json& jobj,
    evmvc::string_view key,
    PARAM_T def_val)
{
    auto it = jobj.find(key.to_string());
    if(it == jobj.end())
        return def_val;
    return *it;
}


evmvc::json parse_jsonc_string(const std::string jsonwc)
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
                throw EVMVC_ERR("JSONC invalid format!");
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
                tmp += res.substr(last_comma_loc+1);
                res = tmp;
            }
            last_comma_loc = -1;
        }
        if(cur_chr == ','){
            last_comma_loc = i;
        }else if(last_comma_loc > -1 && !isspace(cur_chr)){
            last_comma_loc = -1;
        }
        
        res += cur_chr;
    }
    
    return res;
}
evmvc::json parse_jsonc_file(
    const evmvc::string_view jsonc_filename)
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


} //ns evmvc

namespace fmt {
    template <>
    struct formatter<evmvc::cb_error> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(const evmvc::cb_error& e, FormatContext& ctx) {
            return format_to(ctx.out(), "{}", e.c_str());
        }
    };
}

#endif //_boost_beast_mvc_utils_h
