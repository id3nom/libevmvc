@namespace app_ns::ns2::ns4
@name app_name

@* Fanjet code format.
    multiline comment
    @ns and @name are required and must be defined first
*@
@** single line comment

@header
    #include <string>
    #include <iostream> 
    #include <vector>
;

@layout path/to/layout_name

@inherits
    public v1 = "path/to/view_1",
    protected v2 = "~/path/to/view_2"
;


@funi{
public:
    
    void function_in_include_file(int i)
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
    
    
}

@func{
    
    std::vector<int> void test()
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
    
    @(htm){
        <div>
        
        </div>
    
    }
    
}@* end of @(md) *@
@endregion md

@(html){
    
    tesing
    <br/>
    <table>
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
    throw err;
    for(auto i : list){
        try{
            
        }catch(int i){
            
        }
    }
}

</body>
</html>

