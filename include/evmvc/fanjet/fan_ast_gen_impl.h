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

typedef std::function<std::string(const std::string&)> escape_fn;

std::string unique_ident(std::string prefix = "__evmvc_fanjet_ast_")
{
    static size_t n = 0;
    return prefix + "_" + md::num_to_str(++n, false);
}

std::string align_src(document doc, const node_t* n)
{
    // std::string ln;
    // for(size_t i = doc->lines; i < n->_dbg_line; ++i){
    //     ln += "\n";
    //     ++doc->lines;
    // }
    // return ln;
    return "";
}


std::string gen_code_block(
    bool dbg, std::vector<document>& docs, document doc,
    const std::vector<node>& tns
);

std::string escape_cpp_source(const std::string& source)
{
    //std::string escs = source;
    
    std::string escs = source;
    
    // backslash
    md::replace_substring(escs, "\\", "\\\\");
    // double quote
    md::replace_substring(escs, "\"", "\\\"");
    // tab
    md::replace_substring(escs, "\t", "\\t");
    // // lf
    // md::replace_substring(escs, "\n", "\\n");
    // cr
    md::replace_substring(escs, "\r", "\\r");

    std::string tmp;
    for(auto c : escs){
        if(c == '\n')
            tmp += "\\n\"\n\"";
        else
            tmp += c;
    }
    
    return "\"" + tmp + "\"";
}

std::string replace_fan_keys(document doc, const std::string& s)
{
    return replace_words(s, [&](std::string& wrd){
        doc->replace_alias(wrd);
        if(wrd == "@this"){
            wrd = doc->self_name;
            return;
        }
        if(wrd == "@req"){
            wrd = doc->self_name + "->req";
            return;
        }
        if(wrd == "@res"){
            wrd = doc->self_name + "->res";
            return;
        }
        if(wrd == "@cb"){
            wrd = doc->cb_name;
            return;
        }
        
        if(wrd == "@filename"){
            wrd = doc->filename;
            return;
        }
        
        if(wrd == "@dirname"){
            wrd = doc->dirname;
            return;
        }
    });
}

std::string write_tokens(
    document doc,
    node& tok_node, escape_fn esc = nullptr,
    const std::string& prefix = "@this->write_raw(",
    const std::string& suffix = ");"
    )
{
    std::string out;
    node tn = tok_node;
    while(tn && tn->sec_type() == section_type::token){
        tok_node = tn;
        out += tn->token_text();
        tn = tn->next();
    }
    
    if(esc)
        out = esc(out);
    
    return 
        replace_fan_keys(doc, prefix) +
        out +
        replace_fan_keys(doc, suffix);
}

std::string write_tokens_limit(
    document doc,
    node& tok_node,
    node& tok_limit,
    escape_fn esc = nullptr,
    const std::string& prefix = "@this->write_raw(",
    const std::string& suffix = ");"
    )
{
    std::string out;
    node tn = tok_node;
    while(tn && tn != tok_limit && tn->sec_type() == section_type::token){
        tok_node = tn;
        out += tn->token_text();
        tn = tn->next();
    }
    
    if(esc)
        out = esc(out);
    
    return 
        replace_fan_keys(doc, prefix) +
        out +
        replace_fan_keys(doc, suffix);
}


inline std::string gen_code_block(
    bool dbg, std::vector<document>& docs, document doc,
    const std::vector<node>& tns)
{
    // gen source code
    std::string s;
    for(auto tn : tns){
        if(tn->sec_type() == section_type::token){
            s += tn->token_text();
        }else
            s += tn->gen_header_code(
                dbg, docs, doc
            );
    }
    
    return replace_fan_keys(doc, s);
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
        if(ns_v.empty())
            continue;
        ns_open += "namespace " + ns_v + "{ ";
        ns_close += "}";
    }
    if(dbg)
        ns_close += " // " + ns_open;
    
    std::string inherit_val;
    std::string inherit_cstr;
    bool inherit_pub = false;
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
        if(ii->access == ast::inherits_access_type::pub)
            inherit_pub = true;
        if(!inherit_val.empty())
            inherit_val += " ";
        inherit_cstr +=
            (inherit_cstr.empty() ? "" : ", ") + ii->nscls_name +
            "(engine, _res)";
    }
    if(inherit_val.empty()){
        inherit_val = " : public ::evmvc::fanjet::view_base";
        inherit_cstr = " ::evmvc::fanjet::view_base(engine, _res)";
    }else if(!inherit_pub){
        inherit_val += ", public ::evmvc::fanjet::view_base";
        inherit_cstr += ", ::evmvc::fanjet::view_base(engine, _res)";
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
    
    doc->reset_lines();
    std::string cls_body;
    
    /*
    layout = 0,
    partial = 1,
    helper = 2,
    body = 3    
    */
    
    std::string exec_fn = unique_ident("__exec_" + doc->cls_name);
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
        "    evmvc::view_type type() const {{ return {7};}}\n"
        "\n"
        "    void render(std::shared_ptr<evmvc::view_base> self,\n"
        "        md::callback::async_cb cb)\n"
        "    {{\n"
        "        try{{\n"
        "            this->{8}(std::static_pointer_cast<{9}>(self), cb);\n"
        "        }}catch(const std::exception& err){{\n"
        "            cb(err);\n"
        "        }}\n"
        "    }}\n"
        "\n"
        "}};\n"
        "{10}\n",
        doc->cls_name,
        inherit_cstr,
        doc->ns,
        doc->path == "/" ? "" : doc->path,
        doc->name,
        doc->abs_path,
        doc->layout,
        
        doc->type == doc_type::layout ?
            "evmvc::view_type::layout" :
        doc->type == doc_type::partial ?
            "evmvc::view_type::partial" :
        doc->type == doc_type::helper ?
            "evmvc::view_type::helper" :
            "evmvc::view_type::body",
        
        exec_fn,
        doc->cls_name,
        ns_close
    );
    
    cls_body += fmt::format(
        "void {}("
        "std::shared_ptr<{}> {}, md::callback::async_cb {}"
        "){{ try{{ {}->begin_write(\"html\");",
        exec_fn,
        doc->cls_name,
        doc->self_name,
        doc->cb_name,
        doc->self_name
    );
    std::string next_fn = unique_ident("__exec_" + doc->cls_name);
    
    ast::literal_node ln = 
        std::static_pointer_cast<ast::literal_node_t>(this->child(0));
    ast::node n = ln->first_child();
    while(n){
        if(n->sec_type() == section_type::token)
            cls_body += write_tokens(doc, n, escape_cpp_source);
        
        else if(n->node_type() == ast::node_type::code_fun){
            cls_body += fmt::format(
                "{}->{}({}, {});"
                "}}catch(const std::exception& __err){{ {}(__err);return;}}",
                doc->self_name, next_fn, doc->self_name,
                doc->cb_name, doc->cb_name
            );
            for(size_t i = 0; i < doc->scope_level; ++i)
                cls_body += "});";
            cls_body += "}";
            
            cls_body += n->gen_header_code(dbg, docs, doc);
            
            doc->scope_level = 0;
            cls_body += fmt::format(
                "private: void {}("
                "std::shared_ptr<{}> {}, md::callback::async_cb {}"
                "){{ try{{",
                next_fn,
                doc->cls_name,
                doc->self_name,
                doc->cb_name
            );
            
            next_fn = unique_ident("__exec_" + doc->cls_name);
            
        }else if(n->node_type() != ast::node_type::directive)
            cls_body += n->gen_header_code(dbg, docs, doc);
        
        if(!n->next()){
            cls_body += fmt::format(
                "{}->commit_write(\"html\");"
                "{}(nullptr);",
                doc->self_name, doc->cb_name
            );
            
            for(size_t i = 0; i < doc->scope_level; ++i)
                cls_body += fmt::format(
                    "}}catch(const std::exception& __err){{ {}(__err);return;}}"
                    "}});",
                    doc->cb_name
                );

            cls_body += fmt::format(
                "}}catch(const std::exception& __err){{ {}(__err);return;}}"
                "}}",
                doc->cb_name
            );
            
            cls_body +=
                "\n\n// ===========================\n"
                "// === end of user section ===\n"
                "// ===========================\n";
            
        }
        
        n = n->next();
    }
    
    md::replace_substring(cls_body, "@}", "}");
    md::replace_substring(cls_body, "@@", "@");
    
    return cls_head + cls_body + cls_foot;
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
        auto cns = this->child(1)->childs(1, -1);
        return gen_code_block(dbg, docs, doc, cns);
    }
    
    throw MD_ERR("Not implemented!");
}

inline std::string literal_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::string s = align_src(doc, this);
    
    node n = 
        sec_type() == section_type::literal ? child(0) :
        sec_type() == section_type::markup_html ? child(1) :
        sec_type() == section_type::markup_other ? child(1) :
            child(0);
    
    node ln = 
        sec_type() == section_type::literal ? nullptr :
        sec_type() == section_type::markup_html ? last_child() :
        sec_type() == section_type::markup_other ? last_child() :
            nullptr;
    
    switch(sec_type()){
        case section_type::literal:
        case section_type::markup_html:
            s += fmt::format(
                "{}->begin_write(\"{}\");",
                doc->self_name, "html"
            );
            break;
        case section_type::markup_other:
            s += fmt::format(
                "{}->begin_write(\"{}\");",
                doc->self_name, this->markup_language
            );
            break;
        default:
            break;
    }
    
    while(n && n != ln){
        if(n->sec_type() == section_type::token)
            s += write_tokens_limit(doc, n, ln, escape_cpp_source);
        
        else if(n->node_type() != ast::node_type::directive)
            s += n->gen_header_code(dbg, docs, doc);
        
        n = n->next();
    }
    
    switch(sec_type()){
        case section_type::literal:
        case section_type::markup_html:
            s += fmt::format(
                "{}->commit_write(\"{}\");",
                doc->self_name, "html"
            );
            break;
        case section_type::markup_other:
            s += fmt::format(
                "{}->commit_write(\"{}\");",
                doc->self_name, this->markup_language
            );
            break;
        default:
            break;
    }
    
    return s;
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
    std::string s = align_src(doc, this);
    node n = child(0);
    while(n){
        if(this->is_literal_ctx()){
            if(n->sec_type() == section_type::token)
                s += write_tokens(doc, n, escape_cpp_source);
            
            else if(n->node_type() != ast::node_type::directive)
                s += n->gen_header_code(dbg, docs, doc);
            
        }else if(this->is_cpp_ctx()){
            if(n->sec_type() == section_type::token)
                s += n->token_text();
            
            else if(n->node_type() != ast::node_type::directive)
                s += n->gen_header_code(dbg, docs, doc);
        }
        
        n = n->next();
    }
    
    return s;
}

inline std::string string_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::string s = align_src(doc, this);
    
    if(this->is_literal_ctx()){
        node n = child(0);
        while(n){
            if(n->sec_type() == section_type::token)
                s += write_tokens(doc, n, escape_cpp_source);
            
            else if(n->node_type() != ast::node_type::directive)
                s += n->gen_header_code(dbg, docs, doc);
            
            n = n->next();
        }
        
    }else if(this->is_cpp_ctx()){
        std::string ch = child(0)->token_text();
        s += ch;// "\"";
        auto ns = childs(1, -1);
        for(auto n : ns){
            if(n->sec_type() == section_type::token)
                s += n->token_text();
            
            else if(n->node_type() != ast::node_type::directive)
                s += n->gen_header_code(dbg, docs, doc);
        }
        s += ch;//"\"";
    }
    
    return s;
}

inline std::string tag_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::string s = align_src(doc, this);
    node n = child(0);
    while(n){
        if(n->sec_type() == section_type::token){
            s += write_tokens(doc, n, escape_cpp_source);
        }
        
        else if(n->node_type() != ast::node_type::directive)
            s += n->gen_header_code(dbg, docs, doc);
        
        n = n->next();
    }
    
    return s;
}

inline std::string comment_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    // source output
    std::string s = align_src(doc, this);
    s += text(0);
    
    md::replace_substring(s, "@**", "//");
    md::replace_substring(s, "@*", "/*");
    md::replace_substring(s, "*@", "*/");
    
    return s;
}

inline std::string output_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::string s = align_src(doc, this);
    
    if(this->sec_type() == section_type::output_enc)
        s += doc->self_name + "->write_enc(";
    else
        s += doc->self_name + "->write_raw(";
    
    // render (expr child content)
    auto ns = child(1)->childs(0, -1);
    s += gen_code_block(dbg, docs, doc, ns);
    s += ");";
    
    return s;
}

inline std::string code_block_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    // source output
    std::string s = align_src(doc, this);

    node fn = this->child(0);
    
    std::string tt = fn->token_text();
    if(tt != "{" && tt != "@{")
        throw MD_ERR("Unknown code block start token: '{}'", fn->token_text());
    
    bool fanjet_block = tt == "@{";
    if(!fanjet_block){
        s += "{";
    }
    
    auto cns = this->childs(1, -1);
    return s + gen_code_block(dbg, docs, doc, cns) + 
        (fanjet_block ? "" : "}");
}

inline std::string code_control_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    // source output
    std::string s = align_src(doc, this);
    node fn = this->child(0);
    
    std::string tt = fn->token_text();
    if(
        tt != "@if" &&
        tt != "@switch" &&

        tt != "@while" &&
        tt != "@for" &&
        tt != "@do" &&
        
        tt != "if" &&
        tt != "switch" &&

        tt != "while" &&
        tt != "for" &&
        tt != "do"
    )
        throw MD_ERR("Unknown code block start token: '{}'", fn->token_text());
    
    bool fanjet_block = tt[0] == '@';
    if(fanjet_block)
        s += tt.substr(1);
    else
        s += tt;
    
    auto cns = this->childs(1);
    return s + gen_code_block(dbg, docs, doc, cns);
}

inline std::string code_err_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::string s = align_src(doc, this);
    
    return text(0);
}

inline std::string code_fun_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    // code source
    std::string s = align_src(doc, this);
    
    if(this->sec_type() == section_type::code_func){
        s += this->token_section_text();
        std::vector<std::string> lines;
        boost::split(lines, s, boost::is_any_of("\n"));
        s.clear();
        for(auto l : lines)
            s += "// " + l + "\n";
        //md::trim(s);
        return s;
    }
    
    return "private: " +
        gen_code_block(dbg, docs, doc, this->child(1)->childs(1, -1));
}

inline std::string code_async_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    std::string s = align_src(doc, this);
    return text(0);
}


inline std::string fan_key_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    // source output
    std::string s = align_src(doc, this);
    node fn = this->child(0);
    
    std::string tt = fn->token_text();
    if(tt == "@body"){
        s += doc->self_name + "->write_body();";
        return s;
    }

    else if(tt == "@scripts"){
        s += doc->self_name + "->write_scripts();";
        return s;
    }
    else if(tt == "@styles"){
        s += doc->self_name + "->write_styles();";
        return s;
    }
    
    // if(wrd == "@this"){
    //     wrd = doc->self_name;
    //     return;
    // }
    
    // if(wrd == "@filename"){
    //     wrd = doc->filename;
    //     return;
    // }
    
    // if(wrd == "@dirname"){
    //     wrd = doc->dirname;
    //     return;
    // }
    
    return text(0);
}

inline std::string fan_fn_node_t::gen_header_code(
    bool dbg,
    std::vector<document>& docs,
    document doc) const
{
    // source output
    std::string s = align_src(doc, this);
    
    node fn = this->child(0);
    
    std::string tt = fn->token_text();
    if(
        tt != "@>" &&
        
        tt != "@set" &&
        tt != "@get" &&
        tt != "@fmt" &&

        tt != "@get-raw" &&
        tt != "@fmt-raw" &&

        tt != "@script" &&
        tt != "@style" &&
        
        tt != "@import"
    )
        throw MD_ERR("Unknown fanjet function: '{}'", fn->token_text());
    
    if(tt == "@>"){
        // get (expr child content)
        auto nds = child(1)->childs(0, -1);
        
        // store the view name
        std::string vs;
        
        // verify if any child is not of token type
        bool is_static = true;
        for(auto n : nds)
            if(n->node_type() != ast::node_type::token){
                is_static = false;
                break;
            }else
                vs += n->token_text();
        
        if(is_static)
            vs = "\"" + md::trim_copy(replace_fan_keys(doc, vs)) + "\"";
        else
            vs = gen_code_block(dbg, docs, doc, nds);
        
        ++doc->scope_level;
        std::string err_var = unique_ident("__err_" + doc->cls_name);
        
        s += fmt::format(
            "{}->render_view({}, [{}, {}](md::callback::cb_error {})"
            "{{ if({}){{ {}({}); return; }} try{{ ",
            doc->self_name,
            vs,
            
            // lambda args
            doc->self_name,
            doc->cb_name,

            // cb err
            err_var,
            // on err
            err_var,
            doc->cb_name,
            err_var
        );
        
        return s;
    }
    
    if(tt == "@import"){
        
        auto cns = this->childs(1);
        std::string src = gen_code_block(dbg, docs, doc, cns);
        src = src.substr(2, src.size() -4);
        
        if(!bfs::exists(src)){
            throw MD_ERR(
                "Unknown file name for @import!\n"
                "line: {}, col: {}\n{}",
                this->_dbg_line, this->_dbg_col, doc->filename
            );
        }
        
        bfs::ifstream fin(src);
        
        s += doc->self_name + "->write_raw(";
        
        std::string line;
        while(std::getline(fin, line)){
            md::replace_substring(line, "\"", "\\\"");
            s += "\"" + line + "\"\n";
        }
        fin.close();
        
        s += ");";
        return s;
    }
    
    
    bool close_parens = true;
    if(tt == "@set"){
        s += doc->self_name + "->set";
        close_parens = false;
    }

    else if(tt == "@get")
        s += doc->self_name + "->write_enc(" +
            doc->self_name + "->get<std::string>";
    else if(tt == "@fmt")
        s += doc->self_name + "->write_enc(" +
            doc->self_name + "->fmt";
    else if(tt == "@get-raw")
        s += doc->self_name + "->write_raw(" +
            doc->self_name + "->get<std::string>";
    else if(tt == "@fmt-raw")
        s += doc->self_name + "->write_raw(" +
            doc->self_name + "->fmt";
    
    else if(tt == "@script"){
        s += doc->self_name + "->add_script";
        close_parens = false;
    }
    else if(tt == "@style"){
        s += doc->self_name + "->add_style";
        close_parens = false;
    }
    
    auto cns = this->childs(1);
    return s + gen_code_block(dbg, docs, doc, cns) +
        (close_parens ? ");" : ";");
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