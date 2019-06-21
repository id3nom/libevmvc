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

#define EVMVC_VIEW_BASE_ADD_TYPE(T, w_data, wr_data) \
    void write_enc(T data) \
    { \
        _append_buffer(w_data); \
    } \
    void write_raw(T data) \
    { \
        _append_buffer(wr_data); \
    } \


namespace evmvc {

class view_engine;
typedef std::shared_ptr<view_engine> sp_view_engine;


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
    
public:
    view_base(
        sp_view_engine engine,
        const evmvc::response& _res)
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
    
    EVMVC_VIEW_BASE_ADD_TYPE(
        const char*,
        evmvc::html_escape(data),
        data
    )
    EVMVC_VIEW_BASE_ADD_TYPE(
        const std::string&,
        evmvc::html_escape(data),
        data
    )
    
    // ===============
    // == view data ==
    // ===============
    
    template<typename T>
    void set(md::string_view name, T data)
    {
        //res->set_data(name, data);
        res->set_data(name, data);
    }
    
    template<typename T>
    //view_data get(md::string_view name) const
    T get(md::string_view name) const
    {
        //return res->get_data(name);
        return res->get_data<T>(name);
    }
    
    template<typename T>
    T operator()(
        md::string_view name, T def_val = T()) const
    {
        //return res->get_data(name, def_val);
        return res->get_data<T>(name, def_val);
    }
    
    template<typename T>
    T get(md::string_view name, T def_val) const
    {
        //return res->get_data(name, def_val);
        return res->get_data<T>(name, def_val);
    }
    
    std::string fmt(md::string_view f)
    {
        return f.to_string();
    }
    
    template <typename... Args>
    std::string fmt(md::string_view f, const Args&... args)
    {
        return fmt::format(f.data(), args...);
    }
    
    // ==============================
    // == body, scripts and styles ==
    // ==============================
    
    void set_body(std::shared_ptr<view_base> body)
    {
        _body = body;
    }
    
    void write_body()
    {
        if(!_body)
            return;
        
        this->begin_write("html");
        this->write_raw(_body->buffer());
        this->commit_write("html");
    }
    void render_view(md::string_view path, md::callback::async_cb cb);
    
    void add_script(const std::string& src)
    {
        res->scripts().emplace_back(src);
    }
    void add_style(const std::string& src)
    {
        res->styles().emplace_back(src);
    }
    
    void write_scripts()
    {
        this->begin_write("html");
        std::string scripts = "\n";
        for(auto& s : res->scripts()){
            scripts += fmt::format(
                "<script type=\"text/javascript\" src=\"{}\"></script>\n",
                s
            );
        }
        this->write_raw(scripts);
        this->commit_write("html");
    }
    
    void write_styles()
    {
        this->begin_write("html");
        std::string styles = "\n";
        for(auto& s : res->styles()){
            styles += fmt::format(
                "<link rel=\"stylesheet\" type=\"text/css\" href=\"{}\">\n",
                s
            );
        }
        this->write_raw(styles);
        this->commit_write("html");
    }
    
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
    
    
    evmvc::response res;
    evmvc::request req;
    
private:
    
};




}//::evmvc
#endif //_libevmvc_view_base_h