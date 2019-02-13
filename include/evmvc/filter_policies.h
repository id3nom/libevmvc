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

#ifndef _libevmvc_filter_policies_h
#define _libevmvc_filter_policies_h

#include "stable_headers.h"
#include "logging.h"
#include "utils.h"
#include "headers.h"
#include "fields.h"
#include "methods.h"
#include "cookies.h"

namespace evmvc {

typedef std::function<void(const cb_error& err, bool is_valid)> validation_cb;

struct filter_rule_ctx_t
{
    sp_request_headers headers;
    sp_http_cookies cookies;
    std::shared_ptr<evmvc::_internal::multipart_content_file> file;
};

typedef std::shared_ptr<filter_rule_ctx_t> filter_rule_ctx;

class filter_rule_t
{
public:
    filter_rule_t()
    {
    }
    
    virtual void validate(filter_rule_ctx ctx, validation_cb cb) = 0;
};
typedef std::shared_ptr<filter_rule_t> filter_rule;


typedef std::function<
    void(filter_rule_ctx ctx, validation_cb cb)
    > custom_validation_cb;

class custom_filter_rule_t
    : public filter_rule_t
{
public:
    custom_filter_rule_t(custom_validation_cb vcb)
        : cb(vcb)
    {
    }
    custom_validation_cb cb;
};
typedef std::shared_ptr<custom_filter_rule_t> custom_filter_rule;



class filter_policy_t;
typedef std::shared_ptr<filter_policy_t> filter_policy;

typedef
    std::function<void(
        evmvc::filter_policy,
        async_cb
    )> filter_policy_handler_cb;

class filter_policy_t
{
    friend class app;
    
    static size_t _nxt_id()
    {
        static size_t _id = 0;
        return ++_id;
    }
    
    filter_policy_t()
        : _id(filter_policy_t::_nxt_id())
    {
    }
    
public:
    
    size_t id() const { return _id;}
    
    void add_rule(filter_rule rule)
    {
        _rules.emplace_back(rule);
    }
    
    const std::vector<filter_rule>& rules() const { return _rules;}
    
protected:
    
    size_t _id;
    std::vector<filter_rule> _rules;
    
};



class file_filter_rule_t
    : public filter_rule_t
{
public:
    file_filter_rule_t(
        std::initializer_list<std::string> v_names,
        std::initializer_list<std::string> v_mime_types,
        size_t v_max_size)
        : names(v_names),
        mime_types(v_mime_types),
        max_size(v_max_size)
    {
    }
    
    std::vector<std::string> names;
    std::vector<std::string> mime_types;
    size_t max_size;
    
    void validate(filter_rule_ctx ctx, validation_cb cb)
    {
        if(!ctx->file)
            return cb(nullptr, true);
        
        if(std::find(names.begin(), names.end(), ctx->file->name) ==
            names.end()
        )
            return cb(
                EVMVC_ERR(
                    "file name: '{}' is not allowed!", ctx->file->name
                ),
                false
            );
        
        cb(nullptr, true);
    }
};
typedef std::shared_ptr<file_filter_rule_t> file_filter_rule;
    
} //ns evmvc
#endif //_libevmvc_filter_policies_h
