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

#ifndef _libevmvc_fanjet_common_h
#define _libevmvc_fanjet_common_h

#include "../stable_headers.h"

namespace evmvc { namespace fanjet { 
class parser;
}}

namespace evmvc { namespace fanjet { namespace ast {

class node_t;
typedef std::shared_ptr<node_t> node;
class root_node_t;
typedef std::shared_ptr<root_node_t> root_node;
class token_node_t;
typedef std::shared_ptr<token_node_t> token_node;
class expr_node_t;
typedef std::shared_ptr<expr_node_t> expr_node;
class string_node_t;
typedef std::shared_ptr<string_node_t> string_node;

class directive_node_t;
typedef std::shared_ptr<directive_node_t> directive_node;
class literal_node_t;
typedef std::shared_ptr<literal_node_t> literal_node;
class tag_node_t;
typedef std::shared_ptr<tag_node_t> tag_node;
class comment_node_t;
typedef std::shared_ptr<comment_node_t> comment_node;
class output_node_t;
typedef std::shared_ptr<output_node_t> output_node;
class code_block_node_t;
typedef std::shared_ptr<code_block_node_t> code_block_node;
class code_control_node_t;
typedef std::shared_ptr<code_control_node_t> code_control_node;
class code_err_node_t;
typedef std::shared_ptr<code_err_node_t> code_err_node;
class code_fun_node_t;
typedef std::shared_ptr<code_fun_node_t> code_fun_node;
class code_async_node_t;
typedef std::shared_ptr<code_async_node_t> code_async_node;

class fan_key_node_t;
typedef std::shared_ptr<fan_key_node_t> fan_key_node;
class fan_fn_node_t;
typedef std::shared_ptr<fan_fn_node_t> fan_fn_node;


enum class inherits_access_type
{
    // public
    pub     = 0,
    // protected
    pro     = 1,
    // private
    pri     = 2,
};
class inherits_item_t
{
public:
    inherits_access_type access;
    std::string alias;
    std::string path;
    
    std::string nscls_name;
};
typedef std::shared_ptr<inherits_item_t> inherits_item;

void replace_words(std::string& s, const std::string& f, const std::string& r)
{
    std::string d;
    std::string wrd;
    for(auto c : s){
        if(isalnum(c) || c == '_' || c == '@'){
            wrd += c;
            continue;
        }
        
        if(wrd.empty()){
            d += c;
            continue;
        }
        
        if(wrd == f){
            wrd = r;
        }
        d += wrd + c;
        wrd.clear();
    }
    s = d;
}

typedef std::function<void(std::string& wrd)> replace_fn;
void replace_words(std::string& s, replace_fn r)
{
    std::string d;
    std::string wrd;
    for(auto c : s){
        if(isalnum(c) || c == '_' || c == '@'){
            wrd += c;
            continue;
        }
        
        if(wrd.empty()){
            d += c;
            continue;
        }
        
        r(wrd);
        
        d += wrd + c;
        wrd.clear();
    }
    s = d;
}


enum class doc_type
{
    layout = 0,
    partial = 1,
    helper = 2,
    body = 3
};

class document_t
{
    friend class ::evmvc::fanjet::parser;
    document_t()
    {
    }
    
public:
    std::string dirname;
    std::string filename;
    
    root_node rn;
    
    doc_type type;
    
    std::string ns;
    std::string path;
    std::string name;
    std::string abs_path;
    
    std::string cls_name;
    std::string nscls_name;
    std::string self_name;
    
    std::string layout;
    
    std::string h_filename;
    std::string h_src;
    std::string i_filename;
    std::string i_src;
    std::string c_filename;
    std::string c_src;
    
    std::vector<directive_node> includes;
    std::vector<inherits_item> inherits_items;
    
    directive_node pre_inc_header;
    directive_node post_inc_header;
    
    
    
    void replace_alias(std::string& source) const
    {
        replace_words(source, [&](std::string& wrd){
            if(wrd == "@this"){
                wrd = self_name;
                return;
            }
            
            for(auto ii : inherits_items)
                if(ii->alias == wrd){
                    wrd = ii->nscls_name;
                    return;
                }
        });
    }
    
    void replace_paths(std::string& source) const
    {
        replace_words(source, "@dirname", this->dirname);
        replace_words(source, "@filename", this->filename);
    }
    
    void replace_vars(std::string& source) const
    {
        replace_alias(source);
        replace_words(source, "@dirname", this->dirname);
        replace_words(source, "@filename", this->filename);
    }
};
typedef std::shared_ptr<document_t> document;
    
document find(
    std::vector<document>& docs,
    document doc,
    const std::string& path
    )
{
    if(path.empty())
        throw MD_ERR(
            "path is empty!"
        );
    
    //TODO: isolate per namespace
    // std::string ns;
    // size_t nsp = path.rfind("::");
    // if(nsp == std::string::npos)
    //     ns = doc->ns;
    
    std::vector<std::string> parts;
    std::string p = *path.rbegin() == '/' ? path.substr(1) : doc->path + path;
    boost::split(parts, p, boost::is_any_of("/"));
    
    std::string name = *parts.rbegin();
    
    std::vector<document> ds;
    for(auto d : docs)
        if(d->name == name)
            ds.emplace_back(d);
    
    if(ds.empty())
        throw MD_ERR(
            "No view matching path: '{}'", path
        );
    
    for(size_t i = parts.size() -1; i >= 0; --i){
        // look for the file in this order: 
        //  path dir,
        //  partials dir,
        //  layouts dir,
        //  helpers dir,
        
        std::string rp;
        for(size_t j = 0; j < i; ++j)
            rp += parts[j] + "/";
        
        for(auto d : ds){
            if(d->path == rp)
                return d;
            
            if(d->path == rp + "partials/")
                return d;
            
            if(d->path == rp + "layouts/")
                return d;
            
            if(d->path == rp + "helpers/")
                return d;
        }
    }
    
    throw MD_ERR(
        "No view matching path: '{}'", path
    );
}

inline std::string norm_vname(
    const std::string& vname,
    const std::string& replacing_val = "-",
    const std::string allowed_user_chars = "")
{
    std::string nvname;
    
    for(size_t i = 0; i < vname.size(); ++i){
        char c = vname[i];
        if(i == 0){
            if(
                !(c >= 'A' && c <= 'Z') &&
                !(c >= 'a' && c <= 'z') &&
                c != '_'
            ){
                nvname += replacing_val;
                continue;
            }
            nvname += c;
            continue;
        }
        
        if(
            !(c >= 'A' && c <= 'Z') &&
            !(c >= 'a' && c <= 'z') &&
            !(c >= '0' && c <= '9') &&
            c != '_'
        ){
            if(!allowed_user_chars.empty()){
                bool is_valid = false;
                for(size_t j = 0; j < allowed_user_chars.size(); ++j)
                    if(c == allowed_user_chars[j]){
                        is_valid = true;
                        break;
                    }
                if(is_valid){
                    nvname += c;
                    continue;
                }
            }
            
            nvname += replacing_val;
            continue;
        }
        
        nvname += c;
    }
    
    return nvname;
}

inline bool validate_vname(
    std::string& err,
    const std::string& vname,
    bool accept_empty = false,
    const std::string allowed_user_chars = "")
{
    if(vname.size() == 0)
        return accept_empty;
    
    for(size_t i = 0; i < vname.size(); ++i){
        char c = vname[i];
        if(i == 0){
            if(
                !(c >= 'A' && c <= 'Z') &&
                !(c >= 'a' && c <= 'z') &&
                c != '_'
            ){
                err =
                    "Variable must always start with one of 'AZ_az'";
                return false;
            }
            continue;
        }
        
        if(
            !(c >= 'A' && c <= 'Z') &&
            !(c >= 'a' && c <= 'z') &&
            !(c >= '0' && c <= '9') &&
            c != '_'
        ){
            if(!allowed_user_chars.empty()){
                bool is_valid = false;
                for(size_t j = 0; j < allowed_user_chars.size(); ++j)
                    if(c == allowed_user_chars[j]){
                        is_valid = true;
                        break;
                    }
                if(is_valid)
                    continue;
            }
            
            err =
                "Variable must only contains the following chars: "
                "'AZ_az09" + allowed_user_chars;
            return false;
        }
    }
    
    return true;
}
    
    
enum class section_type
    : int
{
    invalid             = INT_MIN,
    root                = INT_MIN +1,
    token               = INT_MIN +2,
    expr                = INT_MIN +3,
    string              = INT_MIN +4,
    any                 = INT_MIN +5,
    
    body                = INT_MIN +6,   // @body
    render              = INT_MIN +7,   // @>view partial name;
    set                 = INT_MIN +8,   // @set("name", "val")
    get                 = INT_MIN +9,   // @get("name", "val")
    fmt                 = INT_MIN +10,  // @fmt("fmtstr", ...)
    get_raw             = INT_MIN +11,  // @get-raw("name", "val")
    fmt_raw             = INT_MIN +12,  // @fmt-raw("fmtstr", ...)
    filename            = INT_MIN +13,  // @filename
    dirname             = INT_MIN +14,  // @dirname
    
    tag                 = INT_MIN +15,  // <tag/><tag></tag><void tag>
    
    dir_ns              = 1,            // @namespace ...
    dir_name            = (1 << 1),     // @name ...
    dir_layout          = (1 << 2),     // @layout ...
    dir_header          = (1 << 3),     // @header ... ;
    dir_inherits        = (1 << 4),     // @inherits ... ;
    dir_include         = (1 << 14),    // @include ...
    dir_path            = (1 << 15),    // @path ...
    
    literal             = (1 << 5),     // text <tag attr> ... </tag><tag/> 
    
    comment_line        = (1 << 6),     // @** ...
    comment_block       = (1 << 7),     // @* ... *@
    
    region_start        = (1 << 8),     // @region ...
    region_end          = (1 << 9),     // @endregion ...
    
    output_raw          = (1 << 10),    // @:: ... ;
    output_enc          = (1 << 11),    // @: ... ;
    
    code_block          = (1 << 12),    // @{ ... }
    
    code_if             = (1 << 13),    // @if( ... ){ ... }
    // code_elif           = (1 << 14),    // else if( ... ){ ... }
    // code_else           = (1 << 15),    // else{ ... }
    code_switch         = (1 << 16),    // @switch( ... ){ ... }

    code_while          = (1 << 17),    // @while( ... ){ ... }
    code_for            = (1 << 18),    // @for( ... ){ ... }
    code_do             = (1 << 19),    // @do{ ... }
    // code_dowhile        = (1 << 20),    // while( ... );
    
    code_try            = (1 << 21),    // @try{ ... }
    // code_trycatch       = (1 << 22),    // catch( ... ){ ... }
    
    code_funi           = (1 << 23),    // @funi{ ... }
    code_func           = (1 << 24),    // @func{ ... }
    
    code_funa           = (1 << 25),    // @<type> func_name(args...);
    code_await          = (1 << 26),    // @await type r = async_func_name(params...);
    
    markup_markdown     = (1 << 27),    // @(markdown){...} @(md){...}
    markup_html         = (1 << 28),    // @(html){...} @(htm){...}
    
    sub_section         = (1 << 29),    // 
};
MD_ENUM_FLAGS(section_type)

inline md::string_view to_string(section_type t)
{
    switch(t){
        case section_type::invalid:
            return "invalid";
        case section_type::root:
            return "root";
        case section_type::token:
            return "token";
        case section_type::expr:
            return "expr";
        case section_type::string:
            return "string";
        case section_type::any:
            return "any";

        case section_type::tag:
            return "tag";
            
        case section_type::body:
            return "body";
        case section_type::render:
            return "render";
        case section_type::set:
            return "set";
        case section_type::get:
            return "get";
        case section_type::fmt:
            return "fmt";
        case section_type::get_raw:
            return "get-raw";
        case section_type::fmt_raw:
            return "fmt-raw";
        case section_type::filename:
            return "filename";
        case section_type::dirname:
            return "dirname";

        case section_type::dir_ns:
            return "dir_ns";
        case section_type::dir_path:
            return "dir_path";
        case section_type::dir_name:
            return "dir_name";
        case section_type::dir_layout:
            return "dir_layout";
        case section_type::dir_header:
            return "dir_header";
        case section_type::dir_inherits:
            return "dir_inherits";
        case section_type::dir_include:
            return "dir_include";

        case section_type::literal:
            return "literal";

        case section_type::comment_line:
            return "comment_line";
        case section_type::comment_block:
            return "comment_block";
        case section_type::region_start:
            return "region_start";
        case section_type::region_end:
            return "region_end";

        case section_type::output_raw:
            return "output_raw";
        case section_type::output_enc:
            return "output_enc";

        case section_type::code_block:
            return "code_block";

        case section_type::code_if:
            return "code_if";
        // case section_type::code_elif:
        //     return "code_elif";
        // case section_type::code_else:
        //     return "code_else";
        case section_type::code_switch:
            return "code_switch";

        case section_type::code_while:
            return "code_while";
        case section_type::code_for:
            return "code_for";
        case section_type::code_do:
            return "code_do";
        // case section_type::code_dowhile:
        //     return "code_dowhile";

        case section_type::code_try:
            return "code_try";
        // case section_type::code_trycatch:
        //     return "code_trycatch";

        case section_type::code_funi:
            return "code_funi";
        case section_type::code_func:
            return "code_func";

        case section_type::code_funa:
            return "code_funa";
        case section_type::code_await:
            return "code_await";

        case section_type::markup_html:
            return "markup_html";
        case section_type::markup_markdown:
            return "markup_markdown";

        case section_type::sub_section:
            return "sub_section";
            
        
        default:
            return "UNKNOWN";
    }
}

}}}//::evmvc::fanjet::ast

#endif //_libevmvc_fanjet_common_h
