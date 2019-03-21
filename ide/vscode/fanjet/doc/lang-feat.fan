@ns app_ns
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
    <@ v1::execute();
    //
    <@ v2::execute();
}

@> //raw cpp code that wont be interpreted <@

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

@<md>{ <!-- switch language parser 'md' parser is the markdown default parser -->
    
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
        <div>some html text</div>
    }
    // do_something will be redefined as:
    // void view_name::do_something()
    // {
    //     // ...
    //     this->write_html("<div>some html text</div>");
    // }
}

Async features
@> and <@ keyword in code block
@> is used to declare an async function
<@ is used to call an async function

@fini{
    @> int32_t get_int_val()
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
        auto r <@ get_int_val();
        auto v <@ get_double_val();
        
        @<htm>{ switch to html inside cpp code block }
        
    }catch(const auto& e){
        <div>Error: @:err.what(); </div>
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

