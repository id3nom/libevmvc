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

#include <gmock/gmock.h>
#include "evmvc/evmvc.h"
#include "evmvc/fanjet/fanjet.h"

namespace evmvc { namespace tests {


class fanjet_test: public testing::Test
{
public:
};


TEST_F(fanjet_test, ast_test)
{
    try{
        std::string fan_str(
            "@*\n"
            "MIT License\n"
            "\n"
            "Copyright (c) 2019 Michel Dénommée\n"
            "\n"
            "Permission is hereby granted, free of charge, "
            "to any person obtaining a copy\n"
            "of this software and associated documentation "
            "files (the \"Software\"), to deal\n"
            "in the Software without restriction, including without "
            "limitation the rights\n"
            "to use, copy, modify, merge, publish, distribute, sublicense, "
            "and/or sell\n"
            "copies of the Software, and to permit persons to whom the "
            "Software is\n"
            "furnished to do so, subject to the following conditions:\n"
            "\n"
            "The above copyright notice and this permission notice "
            "shall be included in all\n"
            "copies or substantial portions of the Software.\n"
            "\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,"
            " EXPRESS OR\n"
            "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
            "MERCHANTABILITY,\n"
            "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. "
            "IN NO EVENT SHALL THE\n"
            "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, "
            "DAMAGES OR OTHER\n"
            "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, "
            "TORT OR OTHERWISE, ARISING FROM,\n"
            "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR "
            "OTHER DEALINGS IN THE\n"
            "SOFTWARE.\n"
            "*@\n"
            "@* Fanjet code format.\n"
            "    @ns, @name and @layout directive are all required\n"
            "    and must be defined first\n"
            "*@\n"
            "\n"
            " @namespace app_ns::ns2::ns4\n"
            "\n"
            "@name app_name\n"
            "@layout path/to/layout_name\n"
            "\n"
            "@header\n"
            "    #include <string>\n"
            "    #include <iostream> \n"
            "    #include <vector>\n"
            ";\n"
            "\n"
            "@inherits\n"
            "    public v1 = \"path/to/view_1\",\n"
            "    protected v2 = \"~/path/to/view_2\"\n"
            ";\n"
            "\n"
            "@*\n"
            "    multiline comment\n"
            "*@\n"
            "@** single line comment\n"
            "\n"
            "@funi{\n"
            "public:\n"
            "    \n"
            "    void function_in_include_file(int i,\n"
            "        std::stack<std::string> s)\n"
            "    {\n"
            "        if(test){\n"
            "            \n"
            "        }else{\n"
            "            \n"
            "        }\n"
            "        \n"
            "        for(int i = 0; i < 5; ++i){\n"
            "            \n"
            "        }\n"
            "    }\n"
            "    \n"
            "    void function_def(bool i);\n"
            "    \n"
            "    \n"
            "    @<std::vector<std::string>> async_function(long i)\n"
            "    {\n"
            "        for(int i = 0; i < 5; ++i){\n"
            "            \n"
            "        }\n"
            "    }\n"
            "    \n"
            "    @<std::vector<std::string>> async_function3(long i);\n"
            "    \n"
            "    @<void> async_function4()\n"
            "    {\n"
            "        if(a){\n"
            "            return;\n"
            "        }else if(b){\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "}\n"
            "\n"
            "@func{\n"
            "    \n"
            "    std::vector<int> test()\n"
            "    {\n"
            "    }\n"
            "    \n"
            "    // function definition in cpp file.\n"
            "    void function_def(bool j)\n"
            "    {\n"
            "        for(int i = 0; i < 5; ++i){\n"
            "            std::cout << i << std::endl;\n"
            "        }\n"
            "    }\n"
            "    \n"
            "    @<std::vector<std::string>> async_function2(long i)\n"
            "    {\n"
            "        for(int i = 0; i < 5; ++i){\n"
            "            \n"
            "        }\n"
            "    }\n"
            "    \n"
            "    @<std::vector<std::string>> async_function3(long i)\n"
            "    {\n"
            "        for(int i = 0; i < 5; ++i){\n"
            "            \n"
            "        }\n"
            "        \n"
            "        return std::vector<std::string>;\n"
            "    }\n"
            "    \n"
            "}\n"
            "\n"
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <title>fastfan-test-02</title>\n"
            "    <link rel=\"stylesheet\" type=\"text/css\" href=\"main.css\" />\n"
            "    <script src=\"main.js\"></script>\n"
            "</head>\n"
            "<body>\n"
            "\n"
            "\n"
            "\n"
            "@region start md\n"
            "@(markdown){\n"
            "\n"
            "# head1\n"
            "\n"
            "## head2\n"
            "\n"
            "* bullet a\n"
            "* bullet b\n"
            "\n"
            "_test_\n"
            "~~~ js\n"
            "\n"
            "    var i = 0;\n"
            "        }\n"
            "\n"
            "    function still_in_mardown(){\n"
            "        \n"
            "    }\n"
            "\n"
            "~~~\n"
            "\n"
            "### head3\n"
            "\n"
            "    @(htm){\n"
            "        <div>\n"
            "        \n"
            "        </div>\n"
            "        @{\n"
            "            \n"
            "        }\n"
            "    }\n"
            "    \n"
            "}@* end of @(md) *@\n"
            "@endregion md\n"
            "\n"
            "@(html){\n"
            "    \n"
            "    tesing\n"
            "    <br/>\n"
            "    <table>\n"
            "    <any></any>\n"
            "    \n"
            "@(md){\n"
            "# heading 1\n"
            "\n"
            "## heading 2\n"
            "\n"
            "    @{\n"
            "        int32_t i = 0;\n"
            "        for(i = 0; i < 10; ++i){\n"
            "            std::cout << i << std::endl;\n"
            "        }\n"
            "    }\n"
            "\n"
            "    @(html){\n"
            "        <div>\n"
            "@(md){\n"
            "# markdown is left padding sensitive\n"
            "\n"
            "markdown text\n"
            "}@** md end\n"
            "    \n"
            "        </div>\n"
            "    }\n"
            "}\n"
            "    \n"
            "    </table>\n"
            "}\n"
            "\n"
            "@*--------------\n"
            "-- code block --\n"
            "--------------*@\n"
            "@{\n"
            "    try{\n"
            "        @await const std::vector<std::string>> r async_function2(int t);\n"
            "        @await auto v get_double_val();\n"
            "        \n"
            "        \n"
            "        \n"
            "        @(htm){ switch to html inside cpp code block }\n"
            "        \n"
            "    }catch(const auto& e){\n"
            "        @(htm){<div>Error: @:err.what(); </div>}\n"
            "    }\n"
            "}\n"
            "\n"
            "@*-----------\n"
            "-- outputs --\n"
            "-----------*@\n"
            "@** encode\n"
            "@:\"this will be encoded: <br/>\";\n"
            "@:this->_non_safe_html_val.to_string();\n"
            "@** html\n"
            "@::\"this will not be encoded: <br/>\";\n"
            "@::this->_safe_html_val.to_string();\n"
            "\n"
            "@*----------------------\n"
            "-- Control statements --\n"
            "----------------------*@\n"
            "\n"
            "@if(test_fn_a() && test_fn_b()){\n"
            "    \n"
            "} else if(1 != false){\n"
            "    auto i = test_fn_c();\n"
            "    \n"
            "}else{\n"
            "    \n"
            "}\n"
            "\n"
            "@switch(val){\n"
            "    case 0:\n"
            "        break;\n"
            "    case 1:\n"
            "        break;\n"
            "    default:\n"
            "        break;\n"
            "}\n"
            "\n"
            "@while(true){\n"
            "    \n"
            "    @(htm){\n"
            "        <a href=\"#\">test</a>\n"
            "    }\n"
            "}\n"
            "\n"
            "@for(int = i; i < 10; ++i){\n"
            "    \n"
            "}\n"
            "\n"
            "<div>do loop</div>\n"
            "\n"
            "@do{\n"
            "    \n"
            "} while(true);\n"
            "\n"
            "@** error handling\n"
            "@try{\n"
            "    int i = abc;\n"
            "\n"
            "}catch(const std::exception& err){\n"
            "    throw err;\n"
            "    for(auto i : list){\n"
            "        try{\n"
            "            \n"
            "        }catch(int i){\n"
            "            \n"
            "        }\n"
            "    }\n"
            "}\n"
            "\n"
            "</body>\n"
            "</html>\n"
        );
        
        std::vector<std::string> ml;
        fanjet::ast::root_node r = 
            fanjet::parser::parse(ml, fan_str);
        
        md::log::info(
            "debug ast output:\n{}",
            r->dump()
        );
        
        md::log::info(
            "ast src output:\n{}",
            r->token_section_text(true)
        );
        
        ASSERT_EQ(fan_str, r->token_section_text());
        
    }catch(const std::exception& err){
        md::log::error(err.what());
        FAIL();
    }
}


TEST_F(fanjet_test, ast_parser)
{
    try{
        // std::string fan_str(
        //     "@if(test_fn_a() && test_fn_b()){"
        //     "    std::cout << \"a\""
        //     "        << std::endl;"
        //     "} else if(1 != false){"
        //     "    auto i = test_fn_c();"
        //     "    std::cout << \"b\""
        //     "        << i"
        //     "        << std::endl;"
        //     "}else{"
        //     "    std::cout << \"cde\""
        //     "        << std::endl;"
        //     "}"
        // );
        
        // std::string fan_str(
        //     "@if(true){\n"
        //     "    return 1;\n"
        //     "}else if(std::string(\"abc\") == \"123\"){\n"
        //     "    @( md )   { in markdown literal! }\n"
        //     "    return -1;\n"
        //     "}else{\n"
        //     "    try{\n"
        //     "        return 2;\n"
        //     "    }catch(const std::exception& err){\n"
        //     "        throw err;\n"
        //     "    }\n"
        //     "}\n"
        //     "try{ inside literal }catch( n/a ){ in literal... }\n"
        // );        
        
        
        
        
    }catch(const std::exception& err){
        std::cout << "Error: " << err.what() << std::endl;
        FAIL();
    }
}


}} //ns evevmvc::tests