@*
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
*@
@ns app::ns
@name lang_feat

@* Fanjet code format.
    multiline comment
    @ns and @name are required and must be defined first
*@
@** single line comment

@header
    // view header cpp code block
    #include <string>
    
;

@** view can inherit other views:
@*
@inherits
    public "path/to/view_1",
    protected "~/path/to/view_2"
;
*@

@*
    include will be automatically added in the header and 
    view real name replaced.
    
    absolute path start point is the configured "root_dir" view parameter.
    
    if you ever whant to ref a view in the code i.e.: explicitly call a
    function on it, you must have to define an alias first using the following
    syntax
*@
@inherits
    public v1 = "path/to/view_1",
    protected v2 = "~/path/to/view_2"
;
@** than you can use 'v1' and 'v2' in the view.
@{
    v1::some_avaiable_method();
}
@*
    when alias are define inherited views will not be execute by default
    it the programmers responsibilities to execute it.
*@
@{
    //it must be execute with the async token '<@' see below for more info.
    @await v1::execute();
    //
    @await v2::execute();
}

@{ std::string s("raw cpp code"); }

@{ // cpp code block
    auto a = 10;
    {
        auto b = 11;
    }
}

@{ // cpp code block
    this->write_encode("var a is visible from here, a == '%i'", a);
    this->write_encode("but b is not");
}

@(md){ <!-- switch language parser 'md' parser is the markdown default parser -->
    
} end of language parser section

view member functions definitions
@funi{ // header member function declarations

    void get_double(async_value<double> cb)
    {
        cb(nullptr, 1.2);
    }
public:
    void do_something();
}

@func{ // cpp member function declarations
    void do_something()
    {
        // ...
        @(htm){<div>some html text</div>}
    }
    // do_something will be redefined as:
    // void view_name::do_something()
    // {
    //     // ...
    //     this->write_html("<div>some html text</div>");
    // }
}

Async features
@<T>; and @await; keyword in code block
@<T>; is used to declare an async function
@await; is used to call an async function

@funi{
    @<int32_t> get_int_val(bool test = false)
    {
        return 3;
    }
    // get_int_val will be redefined as:
    // void get_int_val(async_value<int32_t> cb)
    // {
    //     cb(nullptr, 3);
    //     return;
    // }
    
    // async member function can also 
}

@{
    try{
        @await auto r = get_int_val();
        @await auto v = get_double_val();
        
        @(html){ switch to html inside cpp code block }
        
    }catch(const auto& e){
        @(html){<div>Error: @:err.what(); </div>}
    }
}

@try{
    // cpp code
}catch(const auto& e){
    // cpp code
}

@switch(val){
    //...
}

@if(cpp_expr){
    // cpp code
}else if(cpp_expr){
    // cpp code
}else{
    // cpp code
}

@:"encode output";
@::"<br/><div>raw output</div>";

