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
#include "request.h"
#include "response.h"
#include "jwt.h"
#include "multipart_utils.h"

namespace evmvc { namespace policies {

enum class filter_type
{
    access = 0,
    multipart_form,
    multipart_file,
};

typedef std::function<void(const cb_error& err)> validation_cb;

struct filter_rule_ctx_t
{
    filter_rule_ctx_t(
        sp_response res,
        sp_request req,
        std::shared_ptr<multip::multipart_content_form> form,
        std::shared_ptr<multip::multipart_content_file> file)
        : res(res), req(req), form(form), file(file)
    {
        EVMVC_DEF_TRACE("filter_rule_ctx_t {:p} created", (void*)this);
    }
    
    ~filter_rule_ctx_t()
    {
        EVMVC_DEF_TRACE("filter_rule_ctx_t {:p} released", (void*)this);
    }
    
    sp_response res;
    sp_request req;
    std::shared_ptr<multip::multipart_content_form> form;
    std::shared_ptr<multip::multipart_content_file> file;
};
typedef std::shared_ptr<filter_rule_ctx_t> filter_rule_ctx;

filter_rule_ctx new_context(sp_response res)
{
    return std::make_shared<filter_rule_ctx_t>(
        res, res->req(),
        std::shared_ptr<multip::multipart_content_form>(),
        std::shared_ptr<multip::multipart_content_file>()
    );
}

class filter_rule_t
{
    static size_t _nxt_id()
    {
        static size_t _id = 0;
        return ++_id;
    }
    
public:
    filter_rule_t()
        : _id(_nxt_id())
    {
    }
    
    size_t id() const { return _id;}
    virtual filter_type type() const = 0;
    virtual void validate(filter_rule_ctx ctx, validation_cb cb) = 0;
    
private:
    size_t _id;
};
typedef std::shared_ptr<filter_rule_t> filter_rule;


class filter_policy_t;
typedef std::shared_ptr<filter_policy_t> filter_policy;

typedef
    std::function<void(
        evmvc::policies::filter_policy,
        evmvc::async_cb
    )> filter_policy_handler_cb;


filter_policy new_filter_policy();
class filter_policy_t
    : public std::enable_shared_from_this<filter_policy_t>
{
    friend filter_policy new_filter_policy();;
    
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
    
    void remove_rule(filter_rule rule)
    {
        for(auto it = _rules.begin(); it != _rules.end(); ++it)
            if((*it)->id() == rule->id()){
                _rules.erase(it);
                return;
            }
    }
    
    void validate(filter_type type, filter_rule_ctx ctx, validation_cb cb)
    {
        _validate(type, 0, ctx, cb);
    }
    
    const std::vector<filter_rule>& rules() const { return _rules;}
    
protected:
    
    void _validate(filter_type type, size_t idx,
        filter_rule_ctx ctx, validation_cb cb)
    {
        while(idx < _rules.size() && _rules[idx]->type() != type)
            ++idx;
        if(idx >= _rules.size())
            return cb(nullptr);
        
        _rules[idx]->validate(
            ctx,
        [self = this->shared_from_this(), type, idx, ctx, cb]
        (const cb_error& err) mutable {
            if(err)
                return cb(err);
            if(++idx >= self->_rules.size())
                return cb(nullptr);
            self->_validate(type, idx, ctx, cb);
        });
    }
    
    size_t _id;
    std::vector<filter_rule> _rules;
    
};

filter_policy new_filter_policy()
{
    return filter_policy(new filter_policy_t());
}



typedef std::function<
    void(filter_rule_ctx ctx, validation_cb cb)
    > user_validation_cb;
class user_filter_rule_t
    : public filter_rule_t
{
public:
    user_filter_rule_t(filter_type type, user_validation_cb uvcb)
        : filter_rule_t(), _type(type), _uvcb(uvcb)
    {
    }
    
    filter_type type() const { return _type;}
    
    void validate(filter_rule_ctx ctx, validation_cb cb)
    {
        _uvcb(ctx, cb);
    }
    
private:
    filter_type _type;
    user_validation_cb _uvcb;

};
typedef std::shared_ptr<user_filter_rule_t> user_filter_rule;


class form_filter_rule_t
    : public filter_rule_t
{
public:
    form_filter_rule_t(
        std::initializer_list<std::string> v_names,
        std::initializer_list<std::string> v_mime_types)
        : filter_rule_t(), names(v_names),
        mime_types(v_mime_types)
    {
    }
    
    filter_type type() const { return filter_type::multipart_form;}
    
    std::vector<std::string> names;
    std::vector<std::string> mime_types;
    
    void validate(filter_rule_ctx ctx, validation_cb cb)
    {
        if(!ctx->file)
            return cb(nullptr);
        
        if(names.size() > 0 && 
            std::find(names.begin(), names.end(), ctx->form->name) == 
            names.end()
        )
            return cb(
                EVMVC_ERR(
                    "form name: '{}' is not allowed!", ctx->form->name
                )
            );
        if(mime_types.size() > 0 && 
            std::find(
                mime_types.begin(), mime_types.end(), ctx->form->mime_type) == 
            names.end()
        )
            return cb(
                EVMVC_ERR(
                    "mime type: '{}' is not allowed!", ctx->form->mime_type
                )
            );
        
        cb(nullptr);
    }
};
typedef std::shared_ptr<form_filter_rule_t> form_filter_rule;


class file_filter_rule_t
    : public filter_rule_t
{
public:
    file_filter_rule_t(
        std::initializer_list<std::string> v_names,
        std::initializer_list<std::string> v_mime_types,
        size_t v_max_size)
        : filter_rule_t(), names(v_names),
        mime_types(v_mime_types),
        max_size(v_max_size)
    {
    }
    
    filter_type type() const { return filter_type::multipart_file;}
    std::vector<std::string> names;
    std::vector<std::string> mime_types;
    size_t max_size;
    
    void validate(filter_rule_ctx ctx, validation_cb cb)
    {
        if(!ctx->file)
            return cb(nullptr);
        
        if(names.size() > 0 && 
            std::find(names.begin(), names.end(), ctx->file->name) == 
            names.end()
        )
            return cb(
                EVMVC_ERR(
                    "file name: '{}' is not allowed!", ctx->file->name
                )
            );
        
        if(mime_types.size() > 0 && 
            std::find(
                mime_types.begin(), mime_types.end(), ctx->file->mime_type) == 
            names.end()
        )
            return cb(
                EVMVC_ERR(
                    "mime type: '{}' is not allowed!", ctx->file->mime_type
                )
            );
        
        if(max_size > 0 && ctx->file->size > max_size)
            return cb(
                EVMVC_ERR(
                    "file size: '{}' is too big!", ctx->file->size
                )
            );
        
        cb(nullptr);
    }
};
typedef std::shared_ptr<file_filter_rule_t> file_filter_rule;


typedef std::function<
    void(const jwt::decoded_jwt& tok, validation_cb cb)
    > jwt_validation_cb;
class jwt_filter_rule_t
    : public filter_rule_t
{
public:
    jwt_filter_rule_t(evmvc::jwt::verifier<evmvc::jwt::default_clock> verifier)
        : filter_rule_t(), _verifier(verifier)
    {
        // auto v = evmvc::jwt::verify()
        //     .allow_algorithm(evmvc::jwt::algorithm::hs256{"secret"})
        //     .with_issuer("auth0");
    }

    jwt_filter_rule_t(
        evmvc::jwt::verifier<evmvc::jwt::default_clock> verifier,
        jwt_validation_cb jwcb
        )
        : filter_rule_t(), _verifier(verifier), _jwcb(jwcb)
    {
    }
    
    filter_type type() const { return filter_type::access;}
    
    void validate(filter_rule_ctx ctx, validation_cb cb)
    {
        std::string tok = _get_jwt(ctx->req);
        if(tok.empty()){
            cb(
                EVMVC_ERR("Unable to find a valid JSON Web Token!")
            );
            return;
        }
        
        try{
            jwt::decoded_jwt decoded = jwt::decode(tok);
            _verifier.verify(decoded);
            
            ctx->req->_token = jwt::decoded_jwt(std::move(decoded));
            
            if(_jwcb)
                _jwcb(ctx->req->_token, cb);
            else
                cb(nullptr);
            
        }catch(const std::exception& err){
            cb(err);
        }
    }
    
private:
    std::string _get_jwt(sp_request req)
    {
        auto auth = req->headers().get(evmvc::field::authorization);
        if(auth){
            std::vector<std::string> auth_vals;
            std::string val(auth->value());
            boost::algorithm::split(
                auth_vals, val, boost::is_any_of(" ")
            );
            if(auth_vals.size() == 2 && auth_vals[0] == "bearer")
                return auth_vals[1];
        }
        
        if(req->cookies().exists("token"))
            return req->cookies().get<std::string>("token");
            
        return req->query("token", "");
    }
    
    evmvc::jwt::verifier<evmvc::jwt::default_clock> _verifier;
    jwt_validation_cb _jwcb;
};
typedef std::shared_ptr<jwt_filter_rule_t> jwt_filter_rule;


user_filter_rule new_user_filter(filter_type ft, user_validation_cb uvcb)
{
    return std::make_shared<user_filter_rule_t>(
        ft, uvcb
    );
}

form_filter_rule new_form_filter(
    std::initializer_list<std::string> names = {},
    std::initializer_list<std::string> mime_types = {}
)
{
    return std::make_shared<form_filter_rule_t>(
        names, mime_types
    );
}

file_filter_rule new_file_filter(
    std::initializer_list<std::string> names = {},
    std::initializer_list<std::string> mime_types = {},
    size_t max_size = 0
)
{
    return std::make_shared<file_filter_rule_t>(
        names, mime_types, max_size
    );
}


jwt_filter_rule new_jwt_filter(
    evmvc::jwt::verifier<evmvc::jwt::default_clock> verifier)
{
    return std::make_shared<jwt_filter_rule_t>(
        verifier
    );
}

jwt_filter_rule new_jwt_filter(
    evmvc::jwt::verifier<evmvc::jwt::default_clock> verifier,
    jwt_validation_cb jwcb)
{
    return std::make_shared<jwt_filter_rule_t>(
        verifier, jwcb
    );
}

}} //ns evmvc::policies
#endif //_libevmvc_filter_policies_h
