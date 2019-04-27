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
    
    static ast::root_node parse(
        const std::vector<std::string>& markup_langs,
        const std::string& text)
    {
        std::unordered_set<std::string> allowed_langs = {
            "html", "htm",
            "markdown", "md",
            "tex",
            "latex",
            "wiki"
        };
        for(auto l : markup_langs)
            if(allowed_langs.find(l) == allowed_langs.end())
                throw MD_ERR(
                    "Unsupported language '{}'\n"
                    "Fanjet parser support the following markup languages:\n{}",
                    l, md::join(allowed_langs, ", ")
                );
        
        ast::token root_token = ast::tokenizer::tokenize(text);
        ast::root_node r = ast::parse(markup_langs, root_token);
        
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
        bfs::path file_path,
        const std::string& ns,
        const std::string& view_path,
        const std::vector<std::string>& markup_langs,
        const std::string& fan_src,
        bool dbg)
    {
        ast::document doc = ast::document(new ast::document_t());
        doc->skip_gen = false;
        
        file_path = bfs::absolute(file_path);
        doc->filename = file_path.string();
        doc->dirname = file_path.parent_path().string();
        if(*doc->dirname.rbegin() != '/')
            doc->dirname += "/";
        
        doc->rn = parse(markup_langs, fan_src);
        
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

            else if(n->sec_type() == ast::section_type::dir_header){
                auto dn = std::static_pointer_cast<ast::directive_node_t>(n);
                
                if(!doc->pre_inc_header)
                    doc->pre_inc_header = dn;
                else if(!doc->post_inc_header)
                    doc->post_inc_header = dn;
                else
                    throw MD_ERR(
                        "@header directive can only be specified twice.\n"
                        "First time for view pre include definition and "
                        "second time for view post include definition.\n"
                        "line: {}, col: {}",
                        n->line(), n->col()
                    );
            }
            
            n = n->next();
        }
        
        doc->scope_level = 0;
        doc->lines = 1;
        
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
        if(doc->path != "/" && !ast::validate_vname(err, doc->path, false, "/"))
            throw MD_ERR("Invalid path value '{}'\n{}", doc->path, err);
        if(!ast::validate_vname(err, doc->name, false, ""))
            throw MD_ERR("Invalid name value '{}'\n{}", doc->name, err);
        
        doc->abs_path = doc->ns + "::" +
            (doc->path == "/" ? "" : doc->path) +
            doc->name;
        
        doc->cls_name = ast::norm_vname(doc->path + doc->name, "_");
        doc->nscls_name = doc->ns + "::" + doc->cls_name;
        doc->self_name = "_" + doc->cls_name + "__self_";
        doc->cb_name = "_" + doc->cls_name + "__cb_";
        
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
        const std::string& ns,
        std::vector<ast::document>& docs,
        bool dbg)
    {
        std::string inc_src_incs;
        std::string inc_src_gens = fmt::format(
            "void register_engine()\n{{\n"
            "    std::shared_ptr<evmvc::fanjet::view_engine> fjv =\n"
            "        std::shared_ptr<evmvc::fanjet::view_engine>(\n"
            "            new evmvc::fanjet::view_engine(\"{}\")\n"
            "        );\n",
            ns
        );
        
        std::vector<std::string> ns_vals;
        std::string inc_ns_open, inc_ns_close("\n");
        boost::split(ns_vals, ns, boost::is_any_of(":"));
        for(auto ns_v : ns_vals){
            if(ns_v.empty())
                continue;
            inc_ns_open += "namespace " + ns_v + "{ ";
            inc_ns_close += "}";
        }
        if(dbg)
            inc_ns_close += " // " + inc_ns_open;
        inc_ns_open += "\n";
        inc_ns_close += "\n";
        
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
            
            for(auto ii : doc->inherits_items){
                auto id = ast::find(docs, doc, ii->path);
                ii->nscls_name = id->nscls_name;
                
                includes += fmt::format(
                    "#include \"{0}\"\n",
                    id->i_filename
                );
            }
            
            
            ns_vals.empty();
            std::string ns_open, ns_close("\n");
            boost::split(ns_vals, doc->ns, boost::is_any_of(":"));
            for(auto ns_v : ns_vals){
                if(ns_v.empty())
                    continue;
                ns_open += "namespace " + ns_v + "{ ";
                ns_close += "}";
            }
            if(dbg)
                ns_close += " // " + ns_open;
            ns_open += "\n";
            ns_close += "\n";

            if(!doc->skip_gen){
            
                std::string pre_inc = doc->pre_inc_header ? 
                    doc->pre_inc_header->gen_header_code(dbg, docs, doc) : "";
                std::string post_inc = doc->post_inc_header ? 
                    doc->post_inc_header->gen_header_code(dbg, docs, doc) : "";
                
                if(!pre_inc.empty())
                    pre_inc = ns_open + pre_inc + ns_close;
                if(!post_inc.empty())
                    post_inc = ns_open + post_inc + ns_close;
                
                doc->i_src = fmt::format(
                    "/*\n"
                    // generation notice
                    "  {0}"
                    "*/\n"
                    "\n"
                    "#ifndef {1}\n"
                    "#define {1}\n"
                    // include the fanjet base class
                    "#include \"evmvc/fanjet/fan_view_base.h\"\n"
                    // includes
                    "{2} \n"
                    // pre inc
                    "{3} \n"
                    // include view header
                    "#include \"{4}\"\n"
                    // post inc
                    "{5} \n"
                    
                    "#endif // {1}\n",
                    gen_notice,
                    inc_guards,
                    includes,
                    pre_inc,
                    doc->h_filename,
                    post_inc
                );
                
                
                doc->h_src =
                    doc->rn->gen_header_code(dbg, docs, doc) +
                    "\n/*\n" + gen_notice + " */\n";
            }
            
            inc_src_incs += fmt::format(
                "#include \"{}\"\n",
                doc->i_filename
            );
            
            std::string doc_path = doc->path;
            if(doc_path == "/")
                doc_path = "";
            
            if(doc->type != ast::doc_type::helper)
                inc_src_gens += fmt::format(
                    "    fjv->register_view_generator( \"{}\", \n"
                    "    [](evmvc::sp_view_engine engine, "
                    "const evmvc::sp_response& res\n"
                    "    )->std::shared_ptr<evmvc::fanjet::view_base>{{\n"
                    "        return std::shared_ptr<{}>(\n"
                    "            new {}(engine, res)\n"
                    "        );\n"
                    "    }});\n",
                    doc_path + doc->name,
                    doc->cls_name,
                    doc->cls_name
                );
            
            /*
            doc->c_src = doc->rn->gen_source_code(
                dbg, docs, doc
            );
            */
        }
        
        inc_src_gens += fmt::format(
            "    evmvc::view_engine::register_engine(\n"
            "        \"{}\", fjv\n"
            "    );\n}}\n",
            ns
        );
        
        std::string inc_guards = "__" + ns + "_views_h";
        md::replace_substring(inc_guards, "-", "_");
        md::replace_substring(inc_guards, "/", "_");
        md::replace_substring(inc_guards, ".", "_");
        md::replace_substring(inc_guards, ":", "_");
        
        include_src +=
            "#ifndef " + inc_guards + "\n"
            "#define " + inc_guards + "\n"
            "#include \"evmvc/fanjet/fan_view_engine.h\"\n" +
            inc_src_incs +
            inc_ns_open +
            inc_src_gens +
            inc_ns_close +
            "#endif //" + inc_guards + "\n"
            ;
    }
    
private:
    
    
    
    
};

}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_parser_h
