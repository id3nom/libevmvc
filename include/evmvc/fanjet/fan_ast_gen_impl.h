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

#include "fan_ast.h"


namespace evmvc { namespace fanjet { namespace ast {

std::string gen_code_block(
    bool dbg, std::vector<document>& docs, document doc,
    std::vector<token_node>& tns
);


inline std::string gen_code_block(
    bool dbg,
    std::vector<document>& docs,
    document doc,
    std::vector<token_node>& tns)
{
    std::string _code;
    for(auto tn : tns){
        if(tn->sec_type() == section_type::token){
            _code += tn->token_text();
        }else
            _code += tn->gen_header_code(
                dbg, docs, doc
            );
    }
    
    return _code;
}





inline std::string root_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::vector<std::string> ns_vals;
    std::string ns_open, ns_close;
    boost::split(ns_vals, doc->ns, boost::is_any_of(":"));
    for(auto ns_v : ns_vals){
        ns_open += "namespace " + ns_v + "{ ";
        ns_close += "}";
    }
    if(dbg)
        ns_close += " // " + ns_open;
    
    std::string inherit_val;
    std::string inherit_cstr;
    for(auto ii : doc->inherits_items){
        auto id = ast::find(docs, doc, ii->path);
        ii->nscls_name = id->nscls_name;
        
        inherit_val += fmt::format(
            "{}{} {}",
            inherit_val.empty() ? " : " : ", ",
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
            inherit_val += " ";
        inherit_cstr +=
            (inherit_cstr.empty() ? "" : ", ") + ii->nscls_name +
            "(engine, _res)";
    }
    if(inherit_val.empty()){
        inherit_val = " : public ::evmvc::fanjet::view_base";
        inherit_cstr = " ::evmvc::fanjet::view_base(engine, _res)";
    }
    inherit_val += " ";
    
    std::string cls_head = fmt::format(
        // -inc.h include
        //"#include \"{0}\"\n"
        // namespace
        "{0}"
        // class name
        "class {1} "
        // inheriance
        "{2}"
        "{{ private: ",
        //doc->i_filename,
        ns_open,
        doc->cls_name,
        inherit_val
    );
    
    std::string cls_body;
    
    std::string cls_foot = fmt::format(
        "\n\npublic:\n"
        // constructor
        "    {0}(\n"
        "        ::evmvc::sp_view_engine engine,\n"
        "        const ::evmvc::sp_response& _res)\n"
        "        : {1}\n"
        "    {{\n"
        "    }}\n"
        "\n"
        "    md::string_view ns() const {{ return \"{2}\";}}\n"
        "    md::string_view path() const {{ return \"{3}\";}}\n"
        "    md::string_view name() const {{ return \"{4}\";}}\n"
        "    md::string_view abs_path() const {{ return \"{5}\";}}\n"
        "    md::string_view layout() const {{ return \"{6}\";}}\n"
        "\n"
        "    void render(std::shared_ptr<view_base> self,\n"
        "        md::callback::async_cb cb\n"
        "    );\n"
        "\n"
        "}};\n"
        "{7}\n",
        doc->cls_name,
        inherit_cstr,
        doc->ns,
        doc->path,
        doc->name,
        doc->abs_path,
        doc->layout,
        ns_close
    );
    
    //cls_body = this->child(0)->gen_header_code(dbg, docs, doc);
    
    return cls_head + cls_body + cls_foot;
}

inline std::string token_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string expr_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string string_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}


inline std::string directive_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    if(this->sec_type() == section_type::dir_include){
        auto nl = this->childs(1, SSIZE_MAX, {section_type::token});
        std::string inc_val;
        for(auto n : nl)
            inc_val += n->token_text();
        
        doc->replace_paths(inc_val);
        return "#include " + inc_val;
    }
    
    if(this->sec_type() == section_type::dir_header){
        return gen_code_block(
            dbg, docs, doc,
            this->child(1)->childs(1, -1)
        );
    }
    
    throw MD_ERR("Not implemented!");
}

inline std::string literal_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string tag_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string comment_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string output_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_block_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_control_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_err_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_fun_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_async_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}


inline std::string fan_key_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string fan_fn_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}











inline std::string root_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string token_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string expr_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string string_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}


inline std::string directive_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string literal_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string tag_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string comment_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string output_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_block_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_control_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_err_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_fun_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string code_async_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}


inline std::string fan_key_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}

inline std::string fan_fn_node_t::gen_source_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    throw MD_ERR("Not implemented!");
}



}}}//::evmvc::fanjet::ast