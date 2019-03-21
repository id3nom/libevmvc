//~* 
//    multiline comment
//*~
//~** single line comment
//
#include "language-features-inc.h"//~{header:
//    // view header cpp code block, header section is required
//    #include <string>
//    
//}

//~** view can inherit other views:
namespace default{ class view_language_features : public view_base, public view_1_full_name, public view_2_full_name{//~{>> public /abs/path/to/view_1, public ./rel/path/to/view_2, ... }
//~*
//    include will be automatically added in the header and 
//    view real name replaced.
//    
//    absolute path start point is the configured "root_dir" view parameter.
//    
//    if you ever whant to ref a view in the code i.e.: explicitly call a
//    function on it, you must have to define an alias first using the following
//    syntax
//*~
//~{>> public v1::/abs/path/to/view_1,
//    public v2::./rel/path/to/view_2, ... 
//}
//~** than you can use 'v1' and 'v2' in the view.
void __execute_0(async_cb cb){//~{
    view_1_full_name::some_avaiable_method();//    v1::some_avaiable_method();
//}
//~*
//    when alias are define inherited views will not be execute by default
//    it the programmers responsibilities to execute it.
//*~
//~{
//    //it must be execute with the async token '<~' see below for more info.
    view_1_full_name::execute([&](const cb_error& err){if(err){cb(err);return;}//    <~ v1::execute();
//    //
    view_2_full_name::execute([&](const cb_error& err){if(err){cb(err);return;}//    <~ v2::execute();
//}

/*~>*/ //raw cpp code that wont be interpreted /*<~*/

//~{ // cpp code block
    auto a = 10;//    auto a = 10;
    {//    {
        auto b = 11;//        auto b = 11;
    }//    }
//}

//~{ // cpp code block
    this->write_encode("var a is visible from here, a == '%i'", a);
    this->write_encode("but b is not");
//}

this->parse("md", " <!-- switch language parser 'md' parser is the markdown default parser -->"//~{md: <!-- switch language parser 'md' parser is the markdown default parser -->
"    "//    
);//} end of language parser section

this->write_html("view member functions definitions");//view member functions definitions
this->__execute_1(cb);}}}//~{fn: // header member function declarations
    void get_double(async_value<double> cb)
    {
        cb(nullptr, 1.2);
    }
//    
    void do_something();
//}
//~{fnc: // cpp member function declarations
//    void do_something()
//    {
//        // ...
//        <div>some html text</div>
//    }
    // do_something will be redefined as:
    // void view_name::do_something()
    // {
    //     // ...
    //     this->write_html("<div>some html text</div>");
    // }
//}

void __execute_1(async_cb cb){this->write_html("Async features "//Async features
"~> and <~ keyword in code block "//~> and <~ keyword in code block
"~> is used to declare an async function "//~> is used to declare an async function
"<~ is used to call an async function "//<~ is used to call an async function
""
);this->__execute_2(cb);}//~{fn:
    void get_int_val(async_value<int32_t> cb)//~> int32_t get_int_val()
    {//{
        cb(nullptr, 3);return;//    return 3;
    }//}
    // get_int_val will be redefined as:
    // void get_int_val(async_value<int32_t> cb)
    // {
    //     cb(nullptr, 3);
    //     return;
    // }
    
    // async member function can also 
//}

//~{
void __err_handler_0(const auto& e){this->write_html("<div>Error: ");this->write_encode(e.what());this->write_html("</div>");}//try{
        void __sub_execute_0(async_cb cb){get_int_val([cb](const cb_error& err, auto r){if(err){cb(err);return;}//auto r <~ get_int_val();
        get_double_val([cb, r](const cb_error& err, auto v){if(err){cb(err);return;}//auto v <~ get_double_val();
        
        this->write_html(" switch to html inside cpp code block ");//~{htm: switch to html inside cpp code block }
        
    cb(nullptr);});});}//}catch(const auto& e){
    void __execute_2(async_cb cb){this->__sub_execute_0([cb](const auto& e){if(e){this->__err_handler_0(e);return;}this->__execute_3(cb);};}//    <div>Error: ~:e.what(); </div>
    //}
//}



void __execute_3(async_cb cb){try{//~try{
    //    
}catch(const auto& e){//}catch(const auto& e){
    //    
}//}

switch(val){//~switch(val){
    //    //...
}//}

if(expr){//~if(expr){
    //    
}else if(expr){//}else if(expr){
    //    
}else{//}else{
    //    
}
this->write_encode("encode output");//~:"encode output";
this->write_html("<br/><div>raw output</div>");//~::"<br/><div>raw output</div>";
cb(nullptr);}//}
// execute def
void execute(async_cb cb)
{
    this->__execute_0(cb);
}
};}