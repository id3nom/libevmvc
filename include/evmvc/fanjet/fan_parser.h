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

class parser
{
    parser() = delete;
public:
    
    static ast::root_node parse(const std::string& text)
    {
        ast::token root_token = ast::tokenizer::tokenize(text);
        ast::root_node r = ast::parse(root_token);
        
        md::log::debug(
            "debug ast output:\n{}",
            r->dump()
        );
        
        return r;
    }
    
    /**
     * for more information see: Fanjet engine directory structure
     */
    static ast::document generate_doc(
        const std::string& ns,
        const std::string& view_path,
        const std::string& fan_src,
        bool dbg)
    {
        ast::document doc = ast::document(new ast::document_t());
        
        doc->rn = parse(fan_src);
        
        auto ln = doc->rn->first_child();
        if(!ln || ln->node_type() != ast::node_type::literal)
            throw MD_ERR(
                "Missing literal node!"
            );
        
        auto n = ln->first_child();
        
        bool ns_exists = false;
        bool path_exists = false;
        bool name_exists = false;
        bool layout_exists = false;
        bool inherits_exists = false;
        
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

            else if(n->sec_type() == ast::section_type::dir_path){
                if(path_exists)
                    throw MD_ERR(
                        "@path directive can only be specified once.\n"
                        "line: {}, col: {}",
                        n->line(), n->col()
                    );
                
                path_exists = true;
                doc->path = md::trim_copy(n->child(1)->token_section_text());
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
            
            else if(n->sec_type() == ast::section_type::dir_inherits){
                if(inherits_exists)
                    throw MD_ERR(
                        "@inherits directive can only be specified once.\n"
                        "line: {}, col: {}",
                        n->line(), n->col()
                    );
                inherits_exists = true;
                auto dn = std::static_pointer_cast<ast::directive_node_t>(n);
                dn->get_inherits_items(doc->inherits_items);
            }
            
            else if(n->sec_type() == ast::section_type::dir_include){
                auto dn = std::static_pointer_cast<ast::directive_node_t>(n);
                doc->includes.emplace_back(dn);
            }
            
            // else if(
            //     (!ns_exists || !name_exists) &&
            //     n->node_type() != ast::node_type::directive &&
            //     n->node_type() != ast::node_type::comment){
            //     
            //     size_t toksize = 1;
            //     if(n->node_type() == ast::node_type::token)
            //         toksize = md::trim_copy(n->token_section_text()).size();
            //     
            //     if(toksize > 0)
            //         throw MD_ERR(
            //             "@ns and @name directive are required "
            //             "and must be defined before any other directive\n"
            //             "Current directive: '{}', line: {}, col: {}",
            //             ast::to_string(n->sec_type()), n->line(), n->col()
            //         );
            // }
            
            n = n->next();
        }
        
        if(doc->ns.empty())
            doc->ns = ns;

        if(doc->path.empty())
            doc->path = view_path.substr(0, view_path.rfind("/") +1);
        if(*doc->path.crbegin() != '/')
            doc->path += "/";
        
        if(doc->name.empty())
            doc->name = view_path.substr(view_path.rfind("/") +1);
        
        std::string err;
        if(!ast::validate_vname(err, doc->ns, false, ":"))
            throw MD_ERR("Invalid namespace value '{}'\n{}", doc->ns, err);
        if(!ast::validate_vname(err, doc->path, false, "/"))
            throw MD_ERR("Invalid path value '{}'\n{}", doc->path, err);
        if(!ast::validate_vname(err, doc->name, false, ""))
            throw MD_ERR("Invalid name value '{}'\n{}", doc->name, err);
        
        doc->abs_path = doc->ns + "::" + doc->path + doc->name;
        
        doc->cls_name = ast::norm_vname(doc->path + doc->name, "_");
        doc->nscls_name = doc->ns + "::" + doc->cls_name;
        
        std::string nvname = ast::norm_vname(doc->abs_path, "-");
        doc->h_filename = nvname + ".h";
        doc->i_filename = nvname + "-inc.h";
        doc->c_filename = nvname + ".cpp";
        
        doc->type = ast::doc_type::body;
        if(boost::starts_with(doc->path, "layouts/") ||
            boost::contains(doc->path, "/layouts/")
        )
            doc->type = ast::doc_type::layout;
        else if(boost::starts_with(doc->path, "partials/") ||
            boost::contains(doc->path, "/partials/")
        )
            doc->type = ast::doc_type::partial;
        else if(boost::starts_with(doc->path, "helpers/") ||
            boost::contains(doc->path, "/helpers/")
        )
            doc->type = ast::doc_type::helper;
        
        return doc;
    }

    static void generate_code(
        const std::string& gen_notice,
        std::string& include_src,
        std::vector<ast::document>& docs,
        bool dbg)
    {
        for(size_t i = 0; i < docs.size(); ++i){
            ast::document doc = docs[i];
            
            std::string inc_guards = "__" +
                md::replace_substring_copy(
                    md::replace_substring_copy(
                        doc->i_filename, "-", "_"
                    ),
                    ".", "_"
                );
            
            std::string includes;
            
            for(auto inc : doc->includes)
                includes += fmt::format(
                    "{0}\n",
                    inc->gen_header_code(dbg, docs, doc)
                );
            
            std::string inherit_val("    : public ::evmvc::fanjet::view_base");
            for(auto ii : doc->inherits_items){
                auto id = ast::find(docs, doc, ii->path);
                ii->nscls_name = id->nscls_name;
                
                inherit_val += fmt::format(
                    ",\n{} {}",
                    ii->access == ast::inherits_access_type::pub ?
                        "    public" :
                    ii->access == ast::inherits_access_type::pro ?
                        "    protected" :
                    ii->access == ast::inherits_access_type::pri ?
                        "    private" :
                        "    private",
                    ii->nscls_name
                );
                if(!inherit_val.empty())
                    inherit_val += "\n";
                
                includes += fmt::format(
                    "#include \"{0}\"\n",
                    id->i_filename
                );
            }
            inherit_val += "\n";
            
            std::vector<std::string> vals;
            
            std::string ns_open, ns_close;
            boost::split(vals, doc->ns, boost::is_any_of(":"));
            for(auto v : vals){
                ns_open += "namespace " + v + "{ ";
                ns_close += "}";
            }
            if(dbg)
                ns_close += " // " + ns_open;
            
            /*
                std::string ns;
                std::string path;
                std::string name;
                std::string abs_path;
                
                std::string layout;
            */
            
            std::string cls_def;
            cls_def += fmt::format(
                "\n"
                "{0}\n"
                "class {1}\n"
                "{2}"
                "{{\n"
                "public:\n"
                "    {1}(\n"
                "        sp_view_engine engine,\n"
                "        const evmvc::sp_response& _res)\n"
                "        : ::evmvc::fanjet::view_base(engine, _res)\n"
                "    {{\n"
                "    }}\n"
                "\n"
                "    md::string_view ns() const {{ return \"{3}\";}}\n"
                "    md::string_view path() const {{ return \"{4}\";}}\n"
                "    md::string_view name() const {{ return \"{5}\";}}\n"
                "    md::string_view abs_path() const {{ return \"{6}\";}}\n"
                "\n"
                "    void render(std::shared_ptr<view_base> self,\n"
                "        md::callback::async_cb cb,\n"
                "    );\n"
                "\n"
                "}};\n"
                "{7}\n",
                ns_open,
                doc->cls_name,
                inherit_val,
                doc->ns,
                doc->path,
                doc->name,
                doc->abs_path,
                ns_close
            );
            
            doc->i_src = fmt::format(
                "/*\n"
                "  {0}\n"
                "*/\n"
                "\n"
                "#ifndef {1}\n"
                "#define {1}\n"
                
                // include the fanjet base class
                "#include \"evmvc/fanjet/fan_view_base.h\"\n"
                
                "{2} \n"
                
                "{3}\n"
                
                // include view header
                "#include \"{4}\"\n"
                
                "#endif // {1}\n",
                gen_notice,
                inc_guards,
                includes,
                cls_def,
                doc->h_filename
            );
            
            /*
            doc->h_src = doc->rn->gen_header_code(
                dbg, docs, doc
            );
            doc->c_src = doc->rn->gen_source_code(
                dbg, docs, doc
            );
            */
        }
        
    }
    
private:
    
    
    
    
};

}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_parser_h
