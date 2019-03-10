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


#ifndef _libevmvc_cmd_h
#define _libevmvc_cmd_h

#include "stable_headers.h"
#include "utils.h"
#include "configuration.h"

namespace evmvc {

constexpr int CMD_SYS_ID = 0;
constexpr int CMD_USER_ID = 100;

constexpr int CMD_PING = evmvc::CMD_SYS_ID + 0;
constexpr int CMD_PONG = evmvc::CMD_SYS_ID + 1;
constexpr int CMD_LOG = evmvc::CMD_SYS_ID + 2;
constexpr int CMD_CLOSE = evmvc::CMD_SYS_ID + 3;
constexpr int CMD_CLOSE_APP = evmvc::CMD_SYS_ID + 4;

class command;
typedef std::shared_ptr<command> sp_command;

class command
    : public std::enable_shared_from_this<command>
{
public:
    command(int id)
        : _id(id),
        _buf(evbuffer_new())
    {
    }
    
    command(int id, const char* payload, size_t payload_len)
        : _id(id), _buf(evbuffer_new())
    {
        evbuffer_add(_buf, payload, payload_len);
    }
    
    ~command()
    {
        evbuffer_free(_buf);
        _buf = nullptr;
    }
    
    int id() const
    {
        return _id;
    }
    
    const char* data() const
    {
        if(size() == 0)
            return nullptr;
        
        return (const char*)evbuffer_pullup(_buf, size());
    }
    size_t size() const
    {
        return evbuffer_get_length(_buf);
    }
    
    template<typename T>
    command& write(const T& v)
    {
        size_t l = sizeof(T);
        _write((const char*)&v, l);
        
        return *this;
    }
    
    template<typename T>
    command& write_list(const std::vector<T>& v)
    {
        write(v.size());
        for(auto it = v.begin(); it != v.end(); ++it)
            write(*it);
        return *this;
    }
    
    template<typename T>
    T read()
    {
        size_t l = sizeof(T);
        T v;
        _read((char*)&v, l);
        return v;
    }
    
    template<typename T>
    void read_list(std::vector<T>& v)
    {
        size_t s = read<size_t>();
        for(size_t i = 0; i < s; ++i)
            v.emplace_back(read<T>());
    }
    
    template<typename T>
    T peak(size_t offset = 0) const
    {
        size_t l = sizeof(T);
        T v;
        _peak((char*)&v, l);
        return v;
    }
    
    template<typename T>
    void peak_list(std::vector<T>& v, size_t offset = 0) const
    {
        size_t s = peak<size_t>();
        offset += sizeof(size_t);
        for(size_t i = 0; i < s; ++i){
            v.emplace_back(peak<T>(offset));
            offset += sizeof(T);
        }
    }
    
    void drain(size_t n)
    {
        evbuffer_drain(_buf, n);
    }
    
private:
    void _write(const char* d, size_t l)
    {
        if(evbuffer_add(_buf, d, l))
            throw MD_ERR("Unable to write to buffer");
    }
    
    void _peak(char* d, size_t l, size_t offset = 0) const
    {
        size_t os = l + offset;
        unsigned char* s = evbuffer_pullup(_buf, os);
        if(s == nullptr)
            throw MD_ERR("Invalid buffer length");
        memcpy(d, s + offset, l);
    }
    
    void _read(char* d, size_t l)
    {
        unsigned char* s = evbuffer_pullup(_buf, l);
        if(s == nullptr)
            throw MD_ERR("Invalid buffer length");
        memcpy(d, s, l);
        evbuffer_drain(_buf, l);
    }
    
    int _id;
    evbuffer* _buf;
};


template<>
inline command& command::write<md::string_view>(const md::string_view& v)
{
    write(v.size());
    _write(v.data(), v.size());
    return *this;
}

template<>
inline command& command::write<evmvc::json>(const evmvc::json& v)
{
    return write(v.dump());
}


template<>
inline std::string command::read<std::string>()
{
    size_t l = read<size_t>();
    char b[l];
    _read(b, l);
    return std::string(b, l);
}

template<>
inline evmvc::json command::read<evmvc::json>()
{
    std::string s = read<std::string>();
    return evmvc::json::parse(s);
}

template<>
inline std::string command::peak(size_t offset) const
{
    size_t l = peak<size_t>(offset);
    offset += sizeof(size_t);
    char b[l];
    _peak(b, l, offset);
    return std::string(b, l);
}

template<>
inline evmvc::json command::peak<evmvc::json>(size_t offset) const
{
    std::string s = peak<std::string>(offset);
    return evmvc::json::parse(s);
}


template<>
inline void command::peak_list(std::vector<std::string>& v, size_t offset) const
{
    size_t s = peak<size_t>();
    offset += sizeof(size_t);
    for(size_t i = 0; i < s; ++i){
        std::string s = peak<std::string>(offset);
        v.emplace_back(s);
        offset += sizeof(size_t) + s.size();
    }
}


}//::evmvc
#endif //_libevmvc_cmd_h