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

#ifndef _libevmvc_view_base_h
#define _libevmvc_view_base_h

#include "stable_headers.h"
#include "response.h"

#include <stack>

#define EVMVC_VIEW_BASE_ADD_TYPE(T, w_data, wr_data, tos_data) \
void write_enc(T data) \
{ \
    _append_buffer(w_data); \
} \
void write_raw(T data) \
{ \
    _append_buffer(wr_data); \
} \
void set(md::string_view name, T data) \
{ \
    auto it = _data.find(name.to_string()); \
    if(it == _data.end()) \
        _data.emplace( \
            std::make_pair( \
                name.to_string(), \
                view_base_data( \
                    new view_base_data_t(tos_data) \
                ) \
            ) \
        ); \
    else \
        it->second = view_base_data( \
            new view_base_data_t(tos_data) \
        ); \
}


namespace evmvc {

class view_engine;
typedef std::shared_ptr<view_engine> sp_view_engine;

class view_base_data_t
{
public:
    view_base_data_t(const std::string& v)
        : _v(v)
    {
    }
    
    template<typename T,
        typename std::enable_if<
            !(std::is_same<int16_t, T>::value ||
            std::is_same<int32_t, T>::value ||
            std::is_same<int64_t, T>::value ||

            std::is_same<uint16_t, T>::value ||
            std::is_same<uint32_t, T>::value ||
            std::is_same<uint64_t, T>::value ||
            
            std::is_same<float, T>::value ||
            std::is_same<double, T>::value)
        , int32_t>::type = -1
    >
    T get() const
    {
        std::stringstream ss;
        ss << "Unsupported type:\n" << __PRETTY_FUNCTION__;
        throw MD_ERR(ss.str());
    }
    
    template<
        typename T,
        typename std::enable_if<
            std::is_same<int16_t, T>::value ||
            std::is_same<int32_t, T>::value ||
            std::is_same<int64_t, T>::value ||

            std::is_same<uint16_t, T>::value ||
            std::is_same<uint32_t, T>::value ||
            std::is_same<uint64_t, T>::value ||

            std::is_same<float, T>::value ||
            std::is_same<double, T>::value
        , int32_t>::type = -1
    >
    T get() const
    {
        return md::str_to_num<T>(_v);
    }
    
    template<typename T>
    operator T() const
    {
        return get<T>();
    }
    
private:
    std::string _v;
};
template<>
inline std::string evmvc::view_base_data_t::get<std::string, -1>() const
{
    return _v;
}
template<>
inline const char* evmvc::view_base_data_t::get<const char*, -1>() const
{
    return _v.c_str();
}
template<>
inline md::string_view evmvc::view_base_data_t::get<md::string_view, -1>() const
{
    return _v.c_str();
}
typedef std::shared_ptr<view_base_data_t> view_base_data;

enum class view_type
{
    layout = 0,
    partial = 1,
    helper = 2,
    body = 3
};

class view_base
    : public std::enable_shared_from_this<view_base>
{
    typedef std::unordered_map<std::string, view_base_data> data_map;
    
public:
    view_base(
        sp_view_engine engine,
        const evmvc::sp_response& _res)
        : _engine(engine),
        res(_res),
        req(_res->req())
    {
    }
    
    sp_view_engine engine() const { return _engine.lock();}
    
    virtual evmvc::view_type type() const = 0;
    virtual md::string_view ns() const = 0;
    virtual md::string_view path() const = 0;
    virtual md::string_view name() const = 0;
    virtual md::string_view abs_path() const = 0;
    virtual md::string_view layout() const = 0;
    
    virtual void render(
        std::shared_ptr<view_base> self,
        md::callback::async_cb cb
    ) = 0;
    
    
    void begin_write(md::string_view lng)
    {
        _push_buffer(lng);
    }
    void commit_write(md::string_view lng)
    {
        _pop_buffer(lng);
    }
    
    // ==================
    // == INVALID TYPE ==
    // ==================
    
    template<typename T,
        typename std::enable_if<
            !(
                std::is_same<int16_t, T>::value ||
                std::is_same<int32_t, T>::value ||
                std::is_same<int64_t, T>::value ||

                std::is_same<uint16_t, T>::value ||
                std::is_same<uint32_t, T>::value ||
                std::is_same<uint64_t, T>::value ||
                
                std::is_same<float, T>::value ||
                std::is_same<double, T>::value
            )
        , int32_t>::type = -1
    >
    void write_enc(T data)
    {
        std::stringstream ss;
        ss << "Writing to " << typeid(T).name() << " is not supported";
        throw MD_ERR(ss.str());
    }

    template<typename T,
        typename std::enable_if<
            !(
                std::is_same<int16_t, T>::value ||
                std::is_same<int32_t, T>::value ||
                std::is_same<int64_t, T>::value ||

                std::is_same<uint16_t, T>::value ||
                std::is_same<uint32_t, T>::value ||
                std::is_same<uint64_t, T>::value ||
                
                std::is_same<float, T>::value ||
                std::is_same<double, T>::value
            )
        , int32_t>::type = -1
    >
    void write_raw(T data)
    {
        std::stringstream ss;
        ss << "Unsupported type:\n" << __PRETTY_FUNCTION__;
        throw MD_ERR(ss.str());
    }
    
    template<typename T,
        typename std::enable_if<
            !(std::is_same<int16_t, T>::value ||
            std::is_same<int32_t, T>::value ||
            std::is_same<int64_t, T>::value ||

            std::is_same<uint16_t, T>::value ||
            std::is_same<uint32_t, T>::value ||
            std::is_same<uint64_t, T>::value ||
            
            std::is_same<float, T>::value ||
            std::is_same<double, T>::value)
        , int32_t>::type = -1
    >
    void set(md::string_view name, T data)
    {
        std::stringstream ss;
        ss << "Unsupported type:\n" << __PRETTY_FUNCTION__;
        throw MD_ERR(ss.str());
    }
    
    // ================
    // == VALID TYPE ==
    // ================
    
    template<
        typename T,
        typename std::enable_if<
            std::is_same<int16_t, T>::value ||
            std::is_same<int32_t, T>::value ||
            std::is_same<int64_t, T>::value ||

            std::is_same<uint16_t, T>::value ||
            std::is_same<uint32_t, T>::value ||
            std::is_same<uint64_t, T>::value ||

            std::is_same<float, T>::value ||
            std::is_same<double, T>::value
        , int32_t>::type = -1
    >
    void write_enc(T data)
    {
        _append_buffer(html_escape(md::num_to_str(data)));
    }
    
    template<
        typename T,
        typename std::enable_if<
            std::is_same<int16_t, T>::value ||
            std::is_same<int32_t, T>::value ||
            std::is_same<int64_t, T>::value ||

            std::is_same<uint16_t, T>::value ||
            std::is_same<uint32_t, T>::value ||
            std::is_same<uint64_t, T>::value ||

            std::is_same<float, T>::value ||
            std::is_same<double, T>::value
        , int32_t>::type = -1
    >
    void write_raw(T data)
    {
        _append_buffer(md::num_to_str(data));
    }
    
    template<
        typename T,
        typename std::enable_if<
            std::is_same<int16_t, T>::value ||
            std::is_same<int32_t, T>::value ||
            std::is_same<int64_t, T>::value ||

            std::is_same<uint16_t, T>::value ||
            std::is_same<uint32_t, T>::value ||
            std::is_same<uint64_t, T>::value ||

            std::is_same<float, T>::value ||
            std::is_same<double, T>::value
        , int32_t>::type = -1
    >
    void set(md::string_view name, T data)
    {
        std::string sdata = md::num_to_str(data);
        auto it = _data.find(name.to_string());
        if(it == _data.end())
            _data.emplace(
                std::make_pair(
                    name.to_string(), 
                    view_base_data(
                        new view_base_data_t(sdata)
                    )
                )
            );
        else
            it->second = view_base_data(
                new view_base_data_t(sdata)
            );
    }
    
    EVMVC_VIEW_BASE_ADD_TYPE(
        const char*,
        html_escape(data),
        data,
        std::string(data)
    )
    EVMVC_VIEW_BASE_ADD_TYPE(
        const std::string&,
        html_escape(data),
        data,
        data
    )
    
    // ================
    // ==   ==
    // ================
    
    view_base_data get(md::string_view name) const
    {
        auto it = _data.find(name.to_string());
        if(it == _data.end())
            return nullptr;
        return *it->second;
    }
    
    template<typename T>
    T operator()(
        md::string_view name, T def_val = T()) const
    {
        auto it = _data.find(name.to_string());
        if(it == _data.end())
            return def_val;
        return it->second->get<T>();
    }
    
    template<typename T>
    T get(md::string_view name, T def_val = T()) const
    {
        auto it = _data.find(name.to_string());
        if(it == _data.end())
            return def_val;
        return it->second->get<T>();
    }
    
    
    
    // ================
    // ==  ==
    // ================
    
    
    void set_body(std::shared_ptr<view_base> body)
    {
        _body = body;
    }
    
    void write_body()
    {
        this->begin_write("html");
        this->write_raw(_body->buffer());
        this->commit_write("html");
    }
    void render_view(md::string_view path, md::callback::async_cb cb);
    
    const std::string& buffer() const
    {
        return _out_buffer;
    }
    
private:
    std::weak_ptr<view_engine> _engine;
    std::shared_ptr<view_base> _body;
    std::string _out_buffer;
    
    std::stack<std::string> _buffers;
    std::stack<std::string> _buffer_lngs;
    
    void _append_buffer(md::string_view d)
    {
        _buffers.top() += d.to_string();
    }
    
    void _push_buffer(md::string_view lng)
    {
        _buffers.push("");
        _buffer_lngs.push(lng.to_string());
    }
    
    void _pop_buffer(md::string_view lng);
    
protected:
    
    static std::string uri_encode(md::string_view s)
    {
        char* r = evhttp_encode_uri(s.data());
        std::string tmp(r);
        free(r);
        return tmp;
    }
    static std::string uri_decode(md::string_view s)
    {
        char* r = evhttp_decode_uri(s.data());
        std::string tmp(r);
        free(r);
        return tmp;
    }
    static std::string html_escape(md::string_view s)
    {
        char* r = evhttp_htmlescape(s.data());
        std::string tmp(r);
        free(r);
        return tmp;
    }
    
    evmvc::sp_response res;
    evmvc::sp_request req;
    
private:
    
    data_map _data;
};





/*
template<>
inline void view_base::write_enc<md::string_view>(md::string_view data)
{
    _append_buffer(html_escape(data));
}
template<>
inline void view_base::write_enc<std::string>(std::string data)
{
    _append_buffer(html_escape(data));
}
template<>
inline void view_base::write_enc<const char*>(const char* data)
{
    _append_buffer(html_escape(data));
}


template<>
inline void view_base::write_raw<md::string_view>(md::string_view data)
{
    _append_buffer(data);
}
template<>
inline void view_base::write_raw<std::string>(std::string data)
{
    _append_buffer(data);
}
template<>
inline void view_base::write_raw<const char*>(const char* data)
{
    _append_buffer(data);
}

template<>
inline void view_base::set<md::string_view>(
    md::string_view name, md::string_view data)
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        _data.emplace(std::make_pair(name.to_string(), data.to_string()));
    else
        it->second = data.to_string();
}

template<>
inline std::string view_base::get<std::string>(
    md::string_view name, const std::string& def_data
    ) const
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        return def_data;
    return it->second;
}

template<>
inline void view_base::set<bool>(
    md::string_view name, bool data)
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        _data.emplace(std::make_pair(name.to_string(), data ? "true" : ""));
    else
        it->second = data ? "true" : "";
}

template<>
inline bool view_base::get<bool>(
    md::string_view name, const bool& def_data
    ) const
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        return def_data;
    return !it->second.empty();
}
*/







}//::evmvc
#endif //_libevmvc_view_base_h