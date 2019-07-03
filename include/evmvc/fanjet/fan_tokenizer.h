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

#ifndef _libevmvc_fanjet_tokenizer_h
#define _libevmvc_fanjet_tokenizer_h

#include "../stable_headers.h"
#include <unordered_set>

namespace evmvc { namespace fanjet { namespace ast {
    
class token_t;
typedef std::shared_ptr<token_t> token;

class token_t
    : public std::enable_shared_from_this<token_t>
{
public:
    token_t(
        token pre,
        const std::string& text, size_t line, size_t col, size_t pos,
        token next)
        : _prev(pre), _text(text),
        _line(line), _col(col), _pos(pos), _next(next)
    {
    }
    
    ~token_t()
    {
    }
    
    bool is_root() const { return _prev.expired();}
    token root() const
    {
        token r = this->_prev.lock();
        if(!r)
            return nullptr;
        while(!r->is_root())
            r = r->prev();
        return r;
    }
    
    token prev() const { return _prev.lock();}
    
    bool empty(bool list = false, bool trimmed = false) const
    {
        if(!list || !_text.empty())
            return _text.empty();
        
        token t = this->_next;
        while(t){
            if(!t->_text.empty())
                return false;
            t = t->_next;
        }
        return true;
    }
    std::string text(bool list = false, bool trimmed = false) const
    {
        if(!list)
            return trimmed ? md::trim_copy(_text) : _text;
        
        std::string lst(_text);
        token t = this->_next;
        while(t){
            lst += t->_text;
            t = t->_next;
        }
        return trimmed ? md::trim_copy(lst) : lst;
    }
    size_t line() const { return _line;}
    size_t col() const { return _col;}
    size_t pos() const { return _pos;}
    token next() const { return _next;}
    
    std::string norm_text();
    
    bool norm_is_empty()
    {
        return this->_text.size() > 0 && this->norm_text().size() > 0;
    }
    
    token get_norm_non_empty_prev()
    {
        auto p = this->_prev.lock();
        if(!p)
            return p;
        if(p->norm_is_empty())
            return p->get_norm_non_empty_prev();
        return p;
    }
    
    std::string debug_string()
    {
        auto p = this->_prev.lock();
        return fmt::format(
            "TOKEN: cur '{0}', prev '{1}', next '{2}'",
            this->_text,
            p ? p->_text : "NULL",
            this->_next ? this->_next->_text : "NULL"
        );
    }
    
    std::string dump() const
    {
        std::string s;
        token r = root();
        if(!r)
            r = this->next();
        while(r){
            s += r->_text + (r->_next ? ", " : "");
            r = r->next();
        }
        return s;
    }
    
    token add_next(const std::string& text, size_t line, size_t col, size_t pos)
    {
        this->_next = std::make_shared<token_t>(
            this->shared_from_this(),
            text,
            line,
            col,
            pos,
            nullptr
        );
        
        return this->_next;
    }
    
    /**
     * detach the current token from the token list.
     * after that call, _prev node is nullptr.
     */
    token snip()
    {
        token r = root();
        
        if(!_prev.expired())
            _prev.lock()->_next.reset();
        
        _prev.reset();
        return r;
    }
    
    
    bool is_whitespace() const
    {
        return
            _text == " " ||
            _text == "\t" ||
            _text == "\n" ||
            _text == "\v" ||
            _text == "\f" ||
            _text == "\r" ||
            text(false, true).empty();
    }
    
    bool is_space_or_tab() const { return is_space() || is_tab();}
    bool is_space() const { return _text == " ";}
    bool is_tab() const { return _text == "\t";}

    bool is_cpp_line_comment() const { return _text == "//";}
    bool is_cpp_blk_comment_open() const { return _text == "/*";}
    bool is_cpp_blk_comment_close() const { return _text == "*/";}
    
    bool is_cpp_pointer() const { return _text == "->";}
    
    bool is_parenthesis_open() const { return _text == "(";}
    bool is_parenthesis_close() const { return _text == ")";}
    bool is_curly_brace_open() const { return _text == "{";}
    bool is_curly_brace_close() const { return _text == "}";}
    
    bool is_htm_ltc() const { return _text == "</";}
    bool is_htm_gtc() const { return _text == "/>";}
    bool is_lt() const { return _text == "<";}
    bool is_gt() const { return _text == ">";}
    
    bool is_fan_keyword() const
    {
        return
            is_fan_filename() ||
            is_fan_dirname() ||
            is_fan_body() ||
            is_fan_scripts() ||
            is_fan_styles()
            ;
    }
    
    bool is_fan_body() const { return _text == "@body";}
    bool is_fan_scripts() const { return _text == "@scripts";}
    bool is_fan_styles() const { return _text == "@styles";}
    bool is_fan_filename() const { return _text == "@filename";}
    bool is_fan_dirname() const { return _text == "@dirname";}
    
    bool is_fan_fn() const
    {
        return
            is_fan_render() ||
            
            is_fan_set() ||
            
            is_fan_get() ||
            is_fan_fmt() ||
            is_fan_get_raw() ||
            is_fan_fmt_raw() ||
            
            is_fan_import() ||
            
            is_fan_script() ||
            is_fan_style()
            ;
    }
    
    bool is_fan_render() const { return _text == "@>";}
    bool is_fan_set() const { return _text == "@set";}
    bool is_fan_get() const { return _text == "@get";}
    bool is_fan_fmt() const { return _text == "@fmt";}
    bool is_fan_get_raw() const { return _text == "@get-raw";}
    bool is_fan_fmt_raw() const { return _text == "@fmt-raw";}
    
    bool is_fan_import() const { return _text == "@import";}
    
    bool is_fan_script() const { return _text == "@script";}
    bool is_fan_style() const { return _text == "@style";}
    
    bool is_fan_key() const
    {
        return
            is_fan_escape() ||
            is_fan_comment() ||
            is_fan_region() ||
            is_fan_directive() ||
            is_fan_output() ||
            is_fan_code_block() ||
            is_fan_code() ||
            
            
            is_fan_markup_open();
    }
    
    bool is_fan_start_key() const
    {
        return
            is_fan_line_comment() ||
            is_fan_blk_comment_open() ||
            is_fan_region_open() ||
            is_fan_directive() ||
            is_fan_output() ||
            
            is_fan_code_block() ||

            is_fan_if() ||
            is_fan_switch() ||
            is_fan_while() ||
            is_fan_for() ||
            is_fan_do() ||

            is_fan_try() ||

            is_fan_funi() ||
            is_fan_func() ||

            is_fan_funa() ||
            is_fan_await() ||
            
            is_fan_markup_open();
    }
    
    bool is_fan_escape() const { return _text == "@@";}
    
    bool is_fan_code_block() const { return _text == "@{";}

    bool is_fan_comment() const
    {
        return
            is_fan_line_comment() ||
            is_fan_blk_comment_open() ||
            is_fan_blk_comment_close();
    }
    bool is_fan_line_comment() const { return _text == "@**";}
    bool is_fan_blk_comment_open() const { return _text == "@*";}
    bool is_fan_blk_comment_close() const { return _text == "*@";}
    
    bool is_fan_directive() const
    {
        return
            is_fan_path() ||
            is_fan_name() ||
            is_fan_namespace() ||
            is_fan_layout() ||
            is_fan_header() ||
            is_fan_inherits() ||
            is_fan_include();
    }
    bool is_fan_namespace() const
    {
        return _text == "@namespace" || _text == "@ns";
    }
    bool is_fan_path() const { return _text == "@path";}
    bool is_fan_name() const { return _text == "@name";}
    bool is_fan_layout() const { return _text == "@layout";}
    bool is_fan_header() const { return _text == "@header";}
    bool is_fan_inherits() const { return _text == "@inherits";}
    bool is_fan_include() const { return _text == "@include";}
    
    bool is_fan_region() const
    {
        return
            is_fan_region_open() || is_fan_region_close();
    }
    bool is_fan_region_open() const { return _text == "@region";}
    bool is_fan_region_close() const { return _text == "@endregion";}
    
    bool is_fan_output() const 
    {
        return is_fan_output_raw() || is_fan_output_enc();
    }
    bool is_fan_output_raw() const { return _text == "@::";}
    bool is_fan_output_enc() const { return _text == "@:";}
    
    bool is_fan_code() const
    {
        return
            is_fan_if() ||
            is_fan_switch() ||
            is_fan_while() ||
            is_fan_for() ||
            is_fan_do() ||
            is_fan_try() ||
            is_fan_funi() ||
            is_fan_func() ||
            is_fan_funa() ||
            
            is_fan_await();
    }
    
    bool is_fan_control() const
    {
        return
            is_fan_if() ||
            is_fan_switch() ||
            is_fan_while() ||
            is_fan_for() ||
            is_fan_do();
    }
    
    
    bool is_fan_if() const { return _text == "@if";}
    bool is_cpp_else() const { return _text == "else";}
    bool is_cpp_if() const { return _text == "if";}
    
    bool is_fan_switch() const { return _text == "@switch";}
    
    bool is_fan_while() const { return _text == "@while";}
    bool is_fan_for() const { return _text == "@for";}
    bool is_fan_do() const { return _text == "@do";}
    bool is_cpp_while() const { return _text == "while";}
    
    bool is_fan_try() const { return _text == "@try";}
    bool is_cpp_catch() const { return _text == "catch";}
    bool is_cpp_try() const { return _text == "try";}
    
    bool is_fan_funi() const { return _text == "@funi";}
    bool is_fan_func() const { return _text == "@func";}
    
    bool is_fan_funa() const { return _text == "@<";}
    bool is_fan_await() const { return _text == "@await";}
    
    bool is_fan_markup_open() const { return _text == "@(";}
    
    bool is_double_quote() const { return _text == "\"";}
    bool is_single_quote() const { return _text == "'";}
    bool is_backtick() const { return _text == "`";}
    
    bool is_eol() const { return _text == "\n";}
    
    bool is_semicolon() const { return _text == ";";}
    bool is_colon() const { return _text == ":";}
    bool is_comma() const { return _text == ",";}
    bool is_dot() const { return _text == ".";}
    
    bool is_cpp_return() const { return _text == "return";}
    bool is_cpp_throw() const { return _text == "throw";}
    bool is_cpp_switch() const { return _text == "switch";}
    bool is_cpp_for() const { return _text == "for";}
    bool is_cpp_do() const { return _text == "do";}
    
    bool is_cpp_op() const
    {
        return
            is_comma() ||
            is_dot() ||
            _text == "::" ||
            _text == "==" ||
            _text == "!=" ||
            _text == "!" ||
            _text == "&&" ||
            _text == "||" ||
            _text == "=" ||
            _text == "++" ||
            _text == "--" ||
            _text == "+" ||
            _text == "-" ||
            _text == "*" ||
            _text == "/" ||
            _text == "~" ||
            _text == "&" ||
            _text == "%" ||
            _text == "|" ||
            _text == "^"
            ;
    }
    
    bool is_scope_resolution() const { return _text == "::";}
    // "==",
    bool is_equal_to() const { return _text == "==";}
    // "!=",
    bool is_not_equal_to() const { return _text == "!=";}
    // "!",
    bool is_log_not() const { return _text == "!";}
    // "&&",
    bool is_log_and() const { return _text == "&&";}
    // "||",
    bool is_log_or() const { return _text == "||";}
    // "=",
    bool is_assign() const { return _text == "=";}
    // "++",
    bool is_increment() const { return _text == "++";}
    // "--",
    bool is_decrement() const { return _text == "--";}
    // "+",
    bool is_add() const { return _text == "+";}
    bool is_plus() const { return _text == "+";}
    // "-",
    bool is_sub() const { return _text == "-";}
    bool is_minus() const { return _text == "-";}
    // "*",
    bool is_deference() const { return _text == "*";}
    bool is_star() const { return _text == "*";}
    // "/",
    bool is_div() const { return _text == "/";}
    // "~",
    bool is_bit_not() const { return _text == "~";}
    bool is_tilde() const { return _text == "~";}
    // "&",
    bool is_bit_and() const { return _text == "&";}
    bool is_address_of() const { return _text == "&";}
    bool is_amp() const { return _text == "&";}
    // "%",
    bool is_mod() const { return _text == "%";}
    // "|",
    bool is_bit_or() const { return _text == "|";}
    bool is_pipe() const { return _text == "|";}
    // "^",
    bool is_bit_xor() const { return _text == "^";}
    
    //bool is_markdown_code_backticks() const { return _text == "\n```";}
    //bool is_markdown_code_tildes() const { return _text == "\n~~~";}
    
    bool is_tag_lt() const { return _text == "<";}
    bool is_tag_gt() const { return _text == ">";}
    bool is_tag_slash_gt() const { return _text == "/>";}
    bool is_tag_lt_slash() const { return _text == "</";}
    
    bool is_html_void_tag() const
    {
        static std::unordered_set<std::string> _s {
            "area",
            "base",
            "basefont",
            "bgsound",
            "br",
            "col",
            "command",
            "embed",
            "frame",
            "hr",
            "image",
            "img",
            "input",
            "isindex",
            "keygen",
            "link",
            "menuitem",
            "meta",
            "nextid",
            "param",
            "source",
            "track",
            "wbr"
        };
        if(_text.empty())
            return false;
        
        auto it = _s.find(_text);
        return it != _s.end();
    }
    
    bool is_html_tag() const
    {
        static std::unordered_set<std::string> _s {
            "a",
            "abbr",
            "acronym",
            "address",
            "applet",
            "area",
            "article",
            "aside",
            "audio",
            "b",
            "base",
            "basefont",
            "bdi",
            "bdo",
            "bgsound",
            "big",
            "blink",
            "blockquote",
            "body",
            "br",
            "button",
            "canvas",
            "caption",
            "center",
            "cite",
            "code",
            "col",
            "colgroup",
            "command",
            "content",
            "data",
            "datalist",
            "dd",
            "del",
            "details",
            "dfn",
            "dialog",
            "dir",
            "div",
            "dl",
            "dt",
            "element",
            "em",
            "embed",
            "fieldset",
            "figcaption",
            "figure",
            "font",
            "footer",
            "form",
            "frame",
            "frameset",
            "h1",
            "h2",
            "h3",
            "h4",
            "h5",
            "h6",
            "head",
            "header",
            "hgroup",
            "hr",
            "html",
            "i",
            "iframe",
            "image",
            "img",
            "input",
            "ins",
            "isindex",
            "kbd",
            "keygen",
            "label",
            "legend",
            "li",
            "link",
            "listing",
            "main",
            "map",
            "mark",
            "marquee",
            "math",
            "menu",
            "menuitem",
            "meta",
            "meter",
            "multicol",
            "nav",
            "nextid",
            "nobr",
            "noembed",
            "noframes",
            "noscript",
            "object",
            "ol",
            "optgroup",
            "option",
            "output",
            "p",
            "param",
            "picture",
            "plaintext",
            "pre",
            "progress",
            "q",
            "rb",
            "rbc",
            "rp",
            "rt",
            "rtc",
            "ruby",
            "s",
            "samp",
            "script",
            "section",
            "select",
            "shadow",
            "slot",
            "small",
            "source",
            "spacer",
            "span",
            "strike",
            "strong",
            "style",
            "sub",
            "summary",
            "sup",
            "svg",
            "table",
            "tbody",
            "td",
            "template",
            "textarea",
            "tfoot",
            "th",
            "thead",
            "time",
            "title",
            "tr",
            "track",
            "tt",
            "u",
            "ul",
            "var",
            "video",
            "wbr",
            "xmp"
        };
        
        if(_text.empty())
            return false;
        
        auto it = _s.find(_text);
        return it != _s.end();
    }
    
    bool is_mathml_tag() const
    {
        static std::unordered_set<std::string> _s {
            "abs",
            "and",
            "annotation",
            "annotation-xml",
            "apply",
            "approx",
            "arccos",
            "arccosh",
            "arccot",
            "arccoth",
            "arccsc",
            "arccsch",
            "arcsec",
            "arcsech",
            "arcsin",
            "arcsinh",
            "arctan",
            "arctanh",
            "arg",
            "bind",
            "bvar",
            "card",
            "cartesianproduct",
            "cbytes",
            "ceiling",
            "cerror",
            "ci",
            "cn",
            "codomain",
            "complexes",
            "compose",
            "condition",
            "conjugate",
            "cos",
            "cosh",
            "cot",
            "coth",
            "cs",
            "csc",
            "csch",
            "csymbol",
            "curl",
            "declare",
            "degree",
            "determinant",
            "diff",
            "divergence",
            "divide",
            "domain",
            "domainofapplication",
            "emptyset",
            "encoding",
            "eq",
            "equivalent",
            "eulergamma",
            "exists",
            "exp",
            "exponentiale",
            "factorial",
            "factorof",
            "false",
            "floor",
            "fn",
            "forall",
            "function",
            "gcd",
            "geq",
            "grad",
            "gt",
            "ident",
            "image",
            "imaginary",
            "imaginaryi",
            "implies",
            "in",
            "infinity",
            "int",
            "integers",
            "intersect",
            "interval",
            "inverse",
            "lambda",
            "laplacian",
            "lcm",
            "leq",
            "limit",
            "list",
            "ln",
            "log",
            "logbase",
            "lowlimit",
            "lt",
            "maction",
            "malign",
            "maligngroup",
            "malignmark",
            "malignscope",
            "math",
            "matrix",
            "matrixrow",
            "max",
            "mean",
            "median",
            "menclose",
            "merror",
            "mfenced",
            "mfrac",
            "mfraction",
            "mglyph",
            "mi",
            "min",
            "minus",
            "mlabeledtr",
            "mlongdiv",
            "mmultiscripts",
            "mn",
            "mo",
            "mode",
            "moment",
            "momentabout",
            "mover",
            "mpadded",
            "mphantom",
            "mprescripts",
            "mroot",
            "mrow",
            "ms",
            "mscarries",
            "mscarry",
            "msgroup",
            "msline",
            "mspace",
            "msqrt",
            "msrow",
            "mstack",
            "mstyle",
            "msub",
            "msubsup",
            "msup",
            "mtable",
            "mtd",
            "mtext",
            "mtr",
            "munder",
            "munderover",
            "naturalnumbers",
            "neq",
            "none",
            "not",
            "notanumber",
            "notin",
            "notprsubset",
            "notsubset",
            "or",
            "otherwise",
            "outerproduct",
            "partialdiff",
            "pi",
            "piece",
            "piecewice",
            "piecewise",
            "plus",
            "power",
            "primes",
            "product",
            "prsubset",
            "quotient",
            "rationals",
            "real",
            "reals",
            "reln",
            "rem",
            "root",
            "scalarproduct",
            "sdev",
            "sec",
            "sech",
            "select",
            "selector",
            "semantics",
            "sep",
            "set",
            "setdiff",
            "share",
            "sin",
            "sinh",
            "span",
            "subset",
            "sum",
            "tan",
            "tanh",
            "tendsto",
            "times",
            "transpose",
            "true",
            "union",
            "uplimit",
            "var",
            "variance",
            "vector",
            "vectorproduct",
            "xor"
        };
        if(_text.empty())
            return false;
        
        auto it = _s.find(_text);
        return it != _s.end();
    }
    
    bool is_svg_tag() const
    {
        static std::unordered_set<std::string> _s {
            "a",
            "altGlyph",
            "altGlyphDef",
            "altGlyphItem",
            "animate",
            "animateColor",
            "animateMotion",
            "animateTransform",
            "animation",
            "audio",
            "canvas",
            "circle",
            "clipPath",
            "color-profile",
            "cursor",
            "defs",
            "desc",
            "discard",
            "ellipse",
            "feBlend",
            "feColorMatrix",
            "feComponentTransfer",
            "feComposite",
            "feConvolveMatrix",
            "feDiffuseLighting",
            "feDisplacementMap",
            "feDistantLight",
            "feDropShadow",
            "feFlood",
            "feFuncA",
            "feFuncB",
            "feFuncG",
            "feFuncR",
            "feGaussianBlur",
            "feImage",
            "feMerge",
            "feMergeNode",
            "feMorphology",
            "feOffset",
            "fePointLight",
            "feSpecularLighting",
            "feSpotLight",
            "feTile",
            "feTurbulence",
            "filter",
            "font",
            "font-face",
            "font-face-format",
            "font-face-name",
            "font-face-src",
            "font-face-uri",
            "foreignObject",
            "g",
            "glyph",
            "glyphRef",
            "handler",
            "hatch",
            "hatchpath",
            "hkern",
            "iframe",
            "image",
            "line",
            "linearGradient",
            "listener",
            "marker",
            "mask",
            "mesh",
            "meshgradient",
            "meshpatch",
            "meshrow",
            "metadata",
            "missing-glyph",
            "mpath",
            "path",
            "pattern",
            "polygon",
            "polyline",
            "prefetch",
            "radialGradient",
            "rect",
            "script",
            "set",
            "solidColor",
            "solidcolor",
            "stop",
            "style",
            "svg",
            "switch",
            "symbol",
            "tbreak",
            "text",
            "textArea",
            "textPath",
            "title",
            "tref",
            "tspan",
            "unknown",
            "use",
            "video",
            "view",
            "vkern"
        };
        if(_text.empty())
            return false;
        
        auto it = _s.find(_text);
        return it != _s.end();
    }
    
    bool is_backslash() const { return _text == "\\";}
    
private:
    std::weak_ptr<token_t> _prev;
    
    std::string _text;
    size_t _line;
    size_t _col;
    size_t _pos;
    
    token _next;
};

class tokenizer
{
    tokenizer()
    {
    }
    
public:
    class tmp_token
    {
    public:
        size_t tl = 0;
        size_t tc = 0;
        size_t ti = 0;
        std::string text;
    };
    
    ~tokenizer()
    {
    }
    
    static bool find_token(const std::string& text)
    {
        const char* ptr = s_tokens[0];
        while(ptr){
            if(text.compare(ptr) == 0)
                return true;
            ++ptr;
        }
        return false;
    }
    
    static token tokenize(const std::string& text)
    {
        token root = std::make_shared<token_t>(nullptr, "", 0, 0, 0, nullptr);
        token t = root;
        
        std::string tmp_text;
        
        size_t l = 1;
        size_t c = 1;
        
        size_t tl = 0;
        size_t tc = 0;
        size_t ti = 0;
        
        for(size_t i = 0; i < text.size(); ++i){
            auto cur_chr = text[i];
            bool is_token = false;
            
            size_t ib = i;
            size_t lb = l;
            size_t cb = c;
            const char** tp = s_tokens;
            
            std::string tval;
            size_t il = 0;
            size_t ll = 0;
            size_t cl = 0;
            
            while(*tp){
                std::string tok(*tp);
                if(tok.size() > tval.size()){
                    if(tokenizer::is_token(tok, text, i, l, c)){
                        il = i;
                        ll = l;
                        cl = c;
                        
                        i = ib;
                        l = lb;
                        c = cb;
                        
                        is_token = true;
                        tval = tok;
                    }
                }
                // if(tokenizer::is_token(tok, text, i, l, c)){
                //     is_token = true;
                //     if(tmp_text.size() > 0)
                //         t = t->add_next(tmp_text, tl, tc, ti);
                //     tmp_text = "";
                //     ti = i;
                //     tl = l;
                //     t = t->add_next(tok, lb, cb, ib);
                //     break;
                // }
                ++tp;
            }
            
            if(is_token){
                if(tmp_text.size() > 0)
                    t = t->add_next(tmp_text, tl, tc, ti);
                tmp_text = "";
                
                i = il;
                l = ll;
                c = cl;
                
                ti = i;
                tl = l;
                t = t->add_next(tval, lb, cb, ib);
            }
            
            if(!is_token){
                tmp_text += cur_chr;
                c += 1;
            }
        }
        
        if(tmp_text.size() > 0)
            t = t->add_next(tmp_text, tl, tc, ti);
        
        EVMVC_DEF_DBG(
            md::replace_substring_copy(
                md::replace_substring_copy(root->dump(), "{", "{{"),
                "}", "}}"
            )
        );
        return root;
    }
    
    static bool is_alphanum(char c){
        return (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_';
    }
    
    static bool is_token(
        const std::string& token,
        const std::string& text,
        size_t& i,
        size_t& l,
        size_t& c
        )
    {
        size_t start_i = i;
        
        if(start_i + token.size() > text.size() -1)
            return false;
        
        std::string val = text.substr(start_i, token.size());
        bool is_valid = true;
        if(is_alphanum(*token.rbegin())){
            char nxt_c = val.size() + start_i < text.size() ?
                text[val.size() + start_i] : '\0';
            is_valid = !is_alphanum(nxt_c);
        }
        
        if(val == token && is_valid){
            if(token == "*@"){
                if(start_i + token.size() + 1 <= text.size() -1 &&
                    text.substr(start_i, token.size() + 1) == "*@@"
                )
                    return false;
            }
            c += token.size();
            if(token == "\n"){
                l += 1;
                c = 1;
            }
            i += token.size() -1;
            return true;
        }
        
        return false;
    }
    
    static const char* s_tokens[];
};

const char* tokenizer::s_tokens[] = {
    " ",
    "//",
    "/*",
    "*/",
    "->",
    "(",
    ")",
    "{",
    "}",
    "</", "/>",
    
    "<=", ">=",
    "<", ">",
    
    "@@",
    "@{",
    "@}", // escaping closing bracket
    "@**",
    "@*",
    "*@", // this one is special, must be verified for not to be *@@ literral
    
    "@namespace", "@ns",
    "@path",
    "@name",
    "@layout",
    "@header",
    "@inherits",
    "@include",
    
    "@region",
    "@endregion",
    
    "@::", "@:",
    
    "@if", "else", "if",
    "@switch",
    
    "@while",
    "@for",
    "@do",
    "while",
    
    "@try", "catch", "try",
    
    "@funi", "@func",
    "@<", "@await",
    
    "@body",
    "@this",
    "@filename",
    "@dirname",
    "@scripts",
    "@styles",
    
    "@(",
    "@>",
    "@set",
    "@get",
    "@fmt",
    "@get-raw",
    "@fmt-raw",
    
    "@import",
    
    "@script",
    "@style",
    
    // "\n``````````", "\n`````````", "\n````````", "\n```````", "\n``````", 
    // "\n`````", "\n````", "\n```", "\n``",
    // "\n~~~~~~~~~~", "\n~~~~~~~~~", "\n~~~~~~~~", "\n~~~~~~~", "\n~~~~~~", 
    // "\n~~~~~", "\n~~~~", "\n~~~", "\n~~",
    
    "``````````", "`````````", "````````", "```````", "``````", 
    "`````", "````", "```", "``",
    "~~~~~~~~~~", "~~~~~~~~~", "~~~~~~~~", "~~~~~~~", "~~~~~~", 
    "~~~~~", "~~~~", "~~~", "~~",
    
    "\"", "'", "`",
    "\\",
    
    "\n",
    ";",
    "::",
    ":",
    ",",
    ".",
    
    // cpp op
    "==",
    "!=",
    "!",
    "&&",
    "||",
    "=",
    "++",
    "--",
    "+",
    "-",
    "*",
    "/",
    "~",
    "&",
    "%",
    "|",
    "^",
    
    "return",
    "throw",
    
    "switch",
    "for",
    "do",
    
    // html tags
    "<!DOCTYPE html>",
    "a",
    "abbr",
    "acronym",
    "address",
    "applet",
    "area",
    "article",
    "aside",
    "audio",
    "b",
    "base",
    "basefont",
    "bdi",
    "bdo",
    "bgsound",
    "big",
    "blink",
    "blockquote",
    "body",
    "br",
    "button",
    "canvas",
    "caption",
    "center",
    "cite",
    "code",
    "col",
    "colgroup",
    "command",
    "content",
    "data",
    "datalist",
    "dd",
    "del",
    "details",
    "dfn",
    "dialog",
    "dir",
    "div",
    "dl",
    "dt",
    "element",
    "em",
    "embed",
    "fieldset",
    "figcaption",
    "figure",
    "font",
    "footer",
    "form",
    "frame",
    "frameset",
    "h1",
    "h2",
    "h3",
    "h4",
    "h5",
    "h6",
    "head",
    "header",
    "hgroup",
    "hr",
    "html",
    "i",
    "iframe",
    "image",
    "img",
    "input",
    "ins",
    "isindex",
    "kbd",
    "keygen",
    "label",
    "legend",
    "li",
    "link",
    "listing",
    "main",
    "map",
    "mark",
    "marquee",
    "math",
    "menu",
    "menuitem",
    "meta",
    "meter",
    "multicol",
    "nav",
    "nextid",
    "nobr",
    "noembed",
    "noframes",
    "noscript",
    "object",
    "ol",
    "optgroup",
    "option",
    "output",
    "p",
    "param",
    "picture",
    "plaintext",
    "pre",
    "progress",
    "q",
    "rb",
    "rbc",
    "rp",
    "rt",
    "rtc",
    "ruby",
    "s",
    "samp",
    "script",
    "section",
    "select",
    "shadow",
    "slot",
    "small",
    "source",
    "spacer",
    "span",
    "strike",
    "strong",
    "style",
    "sub",
    "summary",
    "sup",
    "svg",
    "table",
    "tbody",
    "td",
    "template",
    "textarea",
    "tfoot",
    "th",
    "thead",
    "time",
    "title",
    "tr",
    "track",
    "tt",
    "u",
    "ul",
    "var",
    "video",
    "wbr",
    "xmp",
    
    // mathml tags
    "abs",
    "and",
    "annotation",
    "annotation-xml",
    "apply",
    "approx",
    "arccos",
    "arccosh",
    "arccot",
    "arccoth",
    "arccsc",
    "arccsch",
    "arcsec",
    "arcsech",
    "arcsin",
    "arcsinh",
    "arctan",
    "arctanh",
    "arg",
    "bind",
    "bvar",
    "card",
    "cartesianproduct",
    "cbytes",
    "ceiling",
    "cerror",
    "ci",
    "cn",
    "codomain",
    "complexes",
    "compose",
    "condition",
    "conjugate",
    "cos",
    "cosh",
    "cot",
    "coth",
    "cs",
    "csc",
    "csch",
    "csymbol",
    "curl",
    "declare",
    "degree",
    "determinant",
    "diff",
    "divergence",
    "divide",
    "domain",
    "domainofapplication",
    "emptyset",
    "encoding",
    "eq",
    "equivalent",
    "eulergamma",
    "exists",
    "exp",
    "exponentiale",
    "factorial",
    "factorof",
    "false",
    "floor",
    "fn",
    "forall",
    "function",
    "gcd",
    "geq",
    "grad",
    "gt",
    "ident",
    "image",
    "imaginary",
    "imaginaryi",
    "implies",
    "in",
    "infinity",
    "int",
    "integers",
    "intersect",
    "interval",
    "inverse",
    "lambda",
    "laplacian",
    "lcm",
    "leq",
    "limit",
    "list",
    "ln",
    "log",
    "logbase",
    "lowlimit",
    "lt",
    "maction",
    "malign",
    "maligngroup",
    "malignmark",
    "malignscope",
    "math",
    "matrix",
    "matrixrow",
    "max",
    "mean",
    "median",
    "menclose",
    "merror",
    "mfenced",
    "mfrac",
    "mfraction",
    "mglyph",
    "mi",
    "min",
    "minus",
    "mlabeledtr",
    "mlongdiv",
    "mmultiscripts",
    "mn",
    "mo",
    "mode",
    "moment",
    "momentabout",
    "mover",
    "mpadded",
    "mphantom",
    "mprescripts",
    "mroot",
    "mrow",
    "ms",
    "mscarries",
    "mscarry",
    "msgroup",
    "msline",
    "mspace",
    "msqrt",
    "msrow",
    "mstack",
    "mstyle",
    "msub",
    "msubsup",
    "msup",
    "mtable",
    "mtd",
    "mtext",
    "mtr",
    "munder",
    "munderover",
    "naturalnumbers",
    "neq",
    "none",
    "not",
    "notanumber",
    "notin",
    "notprsubset",
    "notsubset",
    "or",
    "otherwise",
    "outerproduct",
    "partialdiff",
    "pi",
    "piece",
    "piecewice",
    "piecewise",
    "plus",
    "power",
    "primes",
    "product",
    "prsubset",
    "quotient",
    "rationals",
    "real",
    "reals",
    "reln",
    "rem",
    "root",
    "scalarproduct",
    "sdev",
    "sec",
    "sech",
    "select",
    "selector",
    "semantics",
    "sep",
    "set",
    "setdiff",
    "share",
    "sin",
    "sinh",
    "span",
    "subset",
    "sum",
    "tan",
    "tanh",
    "tendsto",
    "times",
    "transpose",
    "true",
    "union",
    "uplimit",
    "var",
    "variance",
    "vector",
    "vectorproduct",
    "xor",
    
    // svg tags
    "a",
    "altGlyph",
    "altGlyphDef",
    "altGlyphItem",
    "animate",
    "animateColor",
    "animateMotion",
    "animateTransform",
    "animation",
    "audio",
    "canvas",
    "circle",
    "clipPath",
    "color-profile",
    "cursor",
    "defs",
    "desc",
    "discard",
    "ellipse",
    "feBlend",
    "feColorMatrix",
    "feComponentTransfer",
    "feComposite",
    "feConvolveMatrix",
    "feDiffuseLighting",
    "feDisplacementMap",
    "feDistantLight",
    "feDropShadow",
    "feFlood",
    "feFuncA",
    "feFuncB",
    "feFuncG",
    "feFuncR",
    "feGaussianBlur",
    "feImage",
    "feMerge",
    "feMergeNode",
    "feMorphology",
    "feOffset",
    "fePointLight",
    "feSpecularLighting",
    "feSpotLight",
    "feTile",
    "feTurbulence",
    "filter",
    "font",
    "font-face",
    "font-face-format",
    "font-face-name",
    "font-face-src",
    "font-face-uri",
    "foreignObject",
    "g",
    "glyph",
    "glyphRef",
    "handler",
    "hatch",
    "hatchpath",
    "hkern",
    "iframe",
    "image",
    "line",
    "linearGradient",
    "listener",
    "marker",
    "mask",
    "mesh",
    "meshgradient",
    "meshpatch",
    "meshrow",
    "metadata",
    "missing-glyph",
    "mpath",
    "path",
    "pattern",
    "polygon",
    "polyline",
    "prefetch",
    "radialGradient",
    "rect",
    "script",
    "set",
    "solidColor",
    "solidcolor",
    "stop",
    "style",
    "svg",
    "switch",
    "symbol",
    "tbreak",
    "text",
    "textArea",
    "textPath",
    "title",
    "tref",
    "tspan",
    "unknown",
    "use",
    "video",
    "view",
    "vkern",

    
    nullptr
};

inline std::string token_t::norm_text()
{
    if(tokenizer::find_token(this->_text))
        return this->_text;
    return md::trim_copy(this->_text);
}


}}}//::evmvc::fanjet::ast
#endif //_libevmvc_fanjet_tokenizer_h
