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

#ifndef _libevmvc_fanjet_parser_h
#define _libevmvc_fanjet_parser_h

#include "../stable_headers.h"
#include "fan_common.h"
#include "fan_tokenizer.h"
#include "fan_ast.h"
#include "../app.h"

namespace evmvc { namespace fanjet {

enum class doc_type
{
    layout = 0,
    partial = 1,
    helper = 2,
    body = 3
};

class document_t
{
public:
    doc_type type;
    
    std::string ns;
    std::string path;
    std::string name;
    std::string abs_path;
    
    std::string layout;
    
    std::string h_filename;
    std::string h_src;
    std::string i_filename;
    std::string i_src;
    std::string c_filename;
    std::string c_src;
};
typedef std::shared_ptr<document_t> document;

class parser
{
    parser() = delete;
public:
    
    static ast::root_node parse(const std::string& text)
    {
        ast::token root_token = ast::tokenizer::tokenize(text);
        ast::root_node r = ast::parse(root_token);
        
        md::log::trace(
            "debug ast output:\n{}",
            r->dump()
        );
        
        return r;
    }
    
    /**
     * for more information see: Fanjet engine directory structure
     */
    static document generate(
        const std::string& ns,
        const std::string& view_path,
        const std::string& fan_src)
    {
        document doc = std::make_shared<document_t>(
        );
        
        ast::root_node rn = parse(fan_src);
        
        auto ln = rn->first_child();
        if(!ln || ln->node_type() != ast::node_type::literal)
            throw MD_ERR(
                "Missing literal node!"
            );
        
        auto n = ln->first_child();
        
        bool ns_exists = false;
        bool name_exists = false;
        bool layout_exists = false;
        
        while(n){
            if(n->sec_type() == ast::section_type::dir_ns){
                if(ns_exists)
                    throw MD_ERR(
                        "@namespace directive can only be specified once.\n"
                        "line: {}, col: {}",
                        n->line(), n->col()
                    );
                ns_exists = true;
                doc->ns = md::trim_copy(n->child(1)->token_section_text());
            }
            
            else if(n->sec_type() == ast::section_type::dir_name){
                if(name_exists)
                    throw MD_ERR(
                        "@name directive can only be specified once.\n"
                        "line: {}, col: {}",
                        n->line(), n->col()
                    );
                
                name_exists = true;
                doc->name = md::trim_copy(n->child(1)->token_section_text());
            }
            
            else if(n->sec_type() == ast::section_type::dir_layout){
                if(layout_exists)
                    throw MD_ERR(
                        "@layout directive can only be specified once.\n"
                        "line: {}, col: {}",
                        n->line(), n->col()
                    );
                layout_exists = true;
                doc->layout = md::trim_copy(n->child(1)->token_section_text());
            }
            
            else if(
                (!ns_exists || !name_exists) &&
                n->node_type() != ast::node_type::directive &&
                n->node_type() != ast::node_type::comment){
                
                size_t toksize = 1;
                if(n->node_type() == ast::node_type::token)
                    toksize = md::trim_copy(n->token_section_text()).size();
                
                if(toksize > 0)
                    throw MD_ERR(
                        "@ns and @name directive are required "
                        "and must be defined before any other directive\n"
                        "Current directive: '{}', line: {}, col: {}",
                        ast::to_string(n->sec_type()), n->line(), n->col()
                    );
            }
            
            n = n->next();
        }
        
        if(doc->ns.empty())
            doc->ns = ns;
        if(doc->path.empty())
            doc->path = view_path.substr(0, view_path.rfind("/") +1);
        
        if(doc->name.empty())
            doc->name = view_path.substr(view_path.rfind("/") +1);
        
        doc->abs_path = doc->ns + "::" + doc->path + doc->name;
        
        doc->type = doc_type::body;
        if(boost::starts_with(doc->path, "layouts/") ||
            boost::contains(doc->path, "/layouts/")
        )
            doc->type = doc_type::layout;
        else if(boost::starts_with(doc->path, "partials/") ||
            boost::contains(doc->path, "/partials/")
        )
            doc->type = doc_type::partial;
        else if(boost::starts_with(doc->path, "helpers/") ||
            boost::contains(doc->path, "/helpers/")
        )
            doc->type = doc_type::helper;
        
        return doc;
    }
    
private:

};

}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_parser_h
