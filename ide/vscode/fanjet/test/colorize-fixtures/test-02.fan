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
@* Fanjet code format.
    @ns, @name and @layout directive are all required
    and must be defined first
*@

 @namespace app_ns::ns2::ns4

@name app_name

@layout path/to/layout_name

@header{
    #include <string>
    #include <iostream> 
    #include <vector>
    
    int j = 0;
    
}

@inherits{
    public v1 = "path/to/view_1",
    protected v2 = "~/path/to/view_2"
}

@*
    multiline
    comment
*@
@** single line comment

@funi{
public:
    
    void function_in_include_file(int i,
        std::stack<std::string> s)
    {
        if(test){
            
        }else{
            
        }
        
        for(int i = 0; i < 5; ++i){
            
        }
    }
    
    void function_def(bool i);
    
    
    @<std::vector<std::string>> async_function(long i)
    {
        for(int i = 0; i < 5; ++i){
            
        }
    }
    
    @<std::vector<std::string>> async_function3(long i);
    
    @<void> async_function4()
    {
        if(a){
            return;
        }else if(b){
            return;
        }
    }
}

@func{
    
    std::vector<int> test()
    {
    }
    
    // function definition in cpp file.
    void function_def(bool j)
    {
        for(int i = 0; i < 5; ++i){
            std::cout << i << std::endl;
        }
    }
    
    @<std::vector<std::string>> async_function2(long i)
    {
        for(int i = 0; i < 5; ++i){
            
        }
    }
    
    @<std::vector<std::string>> async_function3(long i)
    {
        for(int i = 0; i < 5; ++i){
            
        }
        
        return std::vector<std::string>;
    }
    
}

<!DOCTYPE html>
<html>
<head>
    <title>fastfan-test-02</title>
    <link rel="stylesheet" type="text/css" href="main.css" />
    <script src="main.js"></script>
</head>
<body>



@region start md
@(markdown){

# head1

## head2

* bullet a
* bullet b

_test_
~~~ js

    var i = 0;
        }

    function still_in_mardown(){
        
    }

~~~

### head3

    @(htm){
        <div>
        
        </div>
        @{
            
        }
    }
    
}@* end of @(md) *@
@endregion md

@(html){
    
    tesing
    <br/>
    <table>
    
    bad tag
    <any></any>
    
@(md){
# heading 1

## heading 2

    @{
        int32_t i = 0;
        for(i = 0; i < 10; ++i){
            std::cout << i << std::endl;
        }
    }

    @(html){
        <div>
@(md){
# markdown is left padding sensitive

markdown text
}@** md end
    
        </div>
    }
}
    
    </table>
}

@*--------------
-- code block --
--------------*@
@{
    try{
        @await const std::vector<std::string>> r async_function2(int t);
        @await auto v get_double_val();
        
        
        
        @(htm){ switch to html inside cpp code block }
        
    }catch(const auto& e){
        @(htm){<div>Error: @:err.what(); </div>}
    }
}

@*-----------
-- outputs --
-----------*@
@** encode
@:"this will be encoded: <br/>";
@:this->_non_safe_html_val.to_string();
@** html
@::"this will not be encoded: <br/>";
@::this->_safe_html_val.to_string();

@*----------------------
-- Control statements --
----------------------*@

@if(test_fn_a() && test_fn_b()){
    
} else if(1 != false){
    auto i = test_fn_c();
    
}else{
    
}

@switch(val){
    case 0:
        break;
    case 1:
        break;
    default:
        break;
}

@while(true){
    
    @(htm){
        <a href="#">test</a>
    }
}

@for(int = i; i < 10; ++i){
    
}

<div>do loop</div>

@do{
    
} while(true);

@** error handling
@try{
    int i = abc;

}catch(const std::exception& err){
    for(auto i : list){
        try{
            throw i;
        }catch(int i){
            @(html){ <div> @:i; </div> }
        }
    }
    throw err;
}

</body>
</html>
