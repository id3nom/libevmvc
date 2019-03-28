@ns app_ns
@name test_01

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
    // include file defs
    
    void test(const std::string& text_arg, const char* arg_b);
    
    void testb()
    {
        int32_t xyz;
        //do something...
    }
    
private:
    void testc(const std::string& text_arg, const char* arg_b);
    
}

@func{
    void test(const std::string& text_arg, const char* arg_b)
    {
        int32_t t = 8;
        for(auto i = 0; i < 10; ++i){
            
            int32_t x = 0;
            
        }
    }
}


@for (auto i = 0; i < abc.size(); ++i){
    int32_t j = i;
}

@if(test){
    int32_t j = i;
}


@{
    int32_t t = 9;
    
}

@(md){
    
    # head1
    
    ## head2
    
    * bullet a
    * bullet b
    
}

@region testing

@{
    std::string s("blablabla");
    int32_t test;
    if(1 == 3){
        int32_t test;
        
        @<html>{
            html output!
            
            <div>
                <input type="text" />
            </div>
            
            @{ // back to cpp
                char* o = nullptr;
                std::vector<std::string> texts;
                for(auto v : texts){
                    char* b = nullptr;
                    std::cout << v << std::endl;
                }
            }
            
        }
        
        int32_t x = 1;
        int32_t y = 2;
        
    }else{
        
        char* c = nullptr;
        
    }
}

@endregion testing

@if(1 == 1){
    
}else{
    
}



<!DOCTYPE html>
<html lang="en">
@::test;


<body>
@if(1 == 2){
    std::cout << s << std::endl;
    
}


@** single line comment
<br/> this must not be a comment!<br/>
<div>
    
</div>

@* * @@
****    multiline comment
*@

<br/>


</body>
</html>
