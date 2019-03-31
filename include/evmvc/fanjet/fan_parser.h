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

class document_t
{
public:
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
        
        auto ln = r->first_child();
        if(!ln || ln->node_type() != ast::node_type::literal)
            throw MD_ERR(
                "Missing literal node!"
            );
        
        auto n = ln->first_child();
        
        bool ns_exists = false;
        bool name_exists = false;
        bool layout_exists = false;
        bool error = false;
        while(n){
            if(
                n->node_type() != ast::node_type::directive &&
                n->node_type() != ast::node_type::comment
            ){
                if(n->node_type() == ast::node_type::token){
                    if(md::trim_copy(n->token_section_text()).size() > 0){
                        error = true;
                        break;
                    }
                }else{
                    error = true;
                    break;
                }
            }
            
            else if(n->sec_type() == ast::section_type::dir_ns){
                if(ns_exists)
                    throw MD_ERR(
                        "@namespace directive can only be specified once."
                    );
                ns_exists = true;
            }
            
            else if(n->sec_type() == ast::section_type::dir_name){
                if(name_exists)
                    throw MD_ERR(
                        "@name directive can only be specified once."
                    );
                
                name_exists = true;
            }
            
            else if(n->sec_type() == ast::section_type::dir_layout){
                if(layout_exists)
                    throw MD_ERR(
                        "@layout directive can only be specified once."
                    );
                layout_exists = true;
            }
            
            else
                throw MD_ERR(
                    "@ns, @name and @layout directive are all required "
                    "and must be defined before any other directive\n"
                    "Current directive: '{}'",
                    ast::to_string(n->sec_type())
                );
            
            if(ns_exists && name_exists && layout_exists)
                break;
            n = n->next();
        }
        
        if(error || !ns_exists || !name_exists || !layout_exists)
            throw MD_ERR(
                "@ns, @name and @layout directive are all required "
                "and must be defined before any other expression"
            );
        
        return r;
    }
    
    // /**
    //  * for more information see: Fanjet engine directory structure
    //  */
    // static void generate(evmvc::sp_app a, bfs::path fan_filename)
    // {
    //     bfs::path views_path = fan_filename.parent_path();
    //     while(views_path.empty() || views_path.filename() != "views")
    //         views_path = views_path.parent_path();
    //     
    //     if(views_path.empty())
    //         throw MD_ERR(
    //             "All fanjet views must be under a "
    //             "'views' directory structure, filename: '{}'",
    //             fan_filename.string()
    //         );
    //     
    //     
    //     
    //     bfs::ifstream fin(fan_filename);
    //     std::ostringstream ostrm;
    //     ostrm << fin.rdbuf();
    //     std::string fan_src = ostrm.str();
    //     fin.close();
    //     
    //     
    // }
    
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
        
        
        return doc;
    }
    
private:

};

}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_parser_h
