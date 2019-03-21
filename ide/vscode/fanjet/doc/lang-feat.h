#ifndef _app_ns_lang_feat_h //@ns app_ns
#define _app_ns_lang_feat_h //@name lang_feat
//
//@* Fanjet code format.
//    multiline comment
//    @ns and @name are required and must be defined first
//*@
//@** single line comment
//
#include "./lang-feat_inc.h"//@header
//    // view header cpp code block
//    #include <string>
//    
//;
//
//@** view can inherit other views:
//@*
//@inherits
//    public "path/to/view_1",
//    protected "~/path/to/view_2"
//;
//*@
//
//@*
//    include will be automatically added in the header and 
//    view real name replaced.
//    
//    absolute path start point is the configured "root_dir" view parameter.
//    
//    if you ever whant to ref a view in the code i.e.: explicitly call a
//    function on it, you must have to define an alias first using the following
//    syntax
//*@
namespace app_ns { class view_lang_feat : public view_base, //@inherits
public view_1,//    public v1 = "path/to/view_1",
protected view_2 {//    protected v2 = "~/path/to/view_2"
//;
//@** than you can use 'v1' and 'v2' in the view.
private: void __exec_0(async_cb cb){//@{
view_1::some_avaiable_method();//    v1::some_avaiable_method();
//}
//@*
//    when alias are define inherited views will not be execute by default
//    it the programmers responsibilities to execute it.
//*@
//@{
//    //it must be execute with the async token '<@' see below for more info.
view_1::exec([&](const cb_error& err){ if(err){cb(err);return;}//    <@ v1::execute();
//    //
view_1::exec([&](const cb_error& err){ if(err){cb(err);return;}//    <@ v2::execute();
//}
//
/*@>*/ //raw cpp code that wont be interpreted /*<@*/
//
//@{ // cpp code block
    auto a = 10;//    auto a = 10;
    {//    {
        auto b = 11;//        auto b = 11;
    }//    }
//}
//
//@{ // cpp code block
this->write_encode("var a is visible from here, a == '%i'", a);//    this->write_encode("var a is visible from here, a == '%i'", a);
this->write_encode("but b is not");//    this->write_encode("but b is not");
//}
//
this->parse("md", " <!-- switch language parser 'md' parser is the markdown default parser "//@<md>{ <!-- switch language parser 'md' parser is the markdown default parser //-->
"    "//    
);//} end of language parser section
//
this->write_html("view member functions definitions");//view member functions definitions
this->_exec_1(cb);}}} private: //@funi{ // header member function declarations
//
    void get_double(async_value<double> cb)//    void get_double(async_value<double> cb)
    {//    {
        cb(nullptr, 1.2);//        cb(nullptr, 1.2);
    }//    }
public://public:
    void do_something();//    void do_something();
//}
//
//@func{ // cpp member function declarations
//    void do_something()
//    {
//        // ...
//        <div>some html text</div>
//    }
//    // do_something will be redefined as:
//    // void view_name::do_something()
//    // {
//    //     // ...
//    //     this->write_html("<div>some html text</div>");
//    // }
//}
//
private: void __exec_1(async_cb cb){ this->write_html("Async features"//Async features
"@> and <@ keyword in code block"//@> and <@ keyword in code block
"@> is used to declare an async function"//@> is used to declare an async function
"<@ is used to call an async function");//<@ is used to call an async function
//
this->__exec_2(cb);} private://@fini{
    void get_int_val(async_value<int32_t> cb)//    @> int32_t get_int_val()
    {try{//    {
        cb(nullptr, 3);return;//        return 3;
    }catch(const std::exception& err){cb(err)}}//    }
//    // get_int_val will be redefined as:
//    // void get_int_val(async_value<int32_t> cb)
//    // {
//    //     cb(nullptr, 3);
//    //     return;
//    // }
//    
//    // async member function can also 
//}
//
//@{
private: void __err_handler_0(const auto& e){this->write_html("<div>Error: ");this->write_encode(e.what());this->write_html("</div>");}//    try{
        private: void __sub_exec_0(async_cb cb){get_int_val([cb](const cb_error& err, auto r){if(err){cb(err);return;}//        auto r <@ get_int_val();
        get_double_val([cb, r](const cb_error& err, auto v){if(err){cb(err);return;}//        auto v <@ get_double_val();
//        
        this->write_html(" switch to html inside cpp code block ");//        @<htm>{ switch to html inside cpp code block }
//        
    cb(nullptr);});});}//    }catch(const auto& e){
    private: void __exec_3(async_cb cb){this->__sub_exec_0([cb](const auto& e){if(e){this->__err_handler_0(e);return;}this->__exec_4(cb);};}//        <div>Error: @:err.what(); </div>
//    }
//}
//
private: void __exec_4(async_cb cb){try{//@try{
    //    // cpp code
}catch(const auto& e){//}catch(const auto& e){
    //    // cpp code
}//}
//
switch(val){//@switch(val){
    //    //...
}//}
//
if(cpp_expr){//@if(cpp_expr){
    //    // cpp code
}else if(cpp_expr){//}else if(cpp_expr){
    //    // cpp code
}else{//}else{
    //    // cpp code
}//}
//
this->write_encode("encode output");//@:"encode output";
this->write_html("<br/><div>raw output</div>");cb(nullptr);}//@::"<br/><div>raw output</div>";
//
public: 
    void exec(async_cb cb)
    {
        try{
            this->__exec_0(cb);
        }catch(const std::exception& err){
            cb(err);
        }
    }
};} //app_ns::class view_lang_feat
#endif //_app_ns_lang_feat_h
