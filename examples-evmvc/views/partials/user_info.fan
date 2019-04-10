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
@ns     
@name   user_info @** comment ....

@** include common.h relative to source view directory
@include @* comment *@ "@dirname/../../common.h" @** line comment

@header{
    
    class abc
    {
    public:
        char a;
        char b;
        char c;
    };
    
}<br/>

@(md){
# header AAA
_italic text_

``` js
var test = 0;
```
@get("test", "blablbla")

``` sh

$ echo("some bash code!!!")
`` $ echo("some bash code!!!") ``

```

some ```$ echo("inline `` code")``` ...

~~~ c++
// this is c++ code
int i = j;
class test
{
    test()
    {
    }
};
~~~

# header BBB
*_bold italic text_*
@} this must be inside markdown block
}
for(){
    test
}

@::"\\ \" \r \n \t \v " + 
    @this->name().to_string() + 
    @this->get<std::string>("\\ \" \r \n \t \v ")
;


@{
    @this->write_enc("\\ \" \r \n \t \v");
    
    std::string ts("tst");
    @this->write_enc("<div></div>" + ts + "<div></div>");
    int i = 1;
    for(; i < 10; ++i){
        @this->write_enc(i);
    }
    
    if(i == 1){
        @this->write_enc("i equals " + md::num_to_str(i, false));
    }else if(i > 1){
        @this->write_enc("i is gt than \"1\"");
    }else{
        @this->write_enc("than i must be lower than \"1\"");
    }
    
    @(htm){<div id="@:"\"a\"";">@(md){_italic_ *bold*}</div>}
}<br/>

<div>
    username: @get("username", "abc")<br/>
    fullname: @get-raw("fullname", "def 123")<br/>
</div>

@header{
}

<br/>


@(tex){
    The quadratic formula is $-b \pm \sqrt{b^2 - 4ac} \over 2a$
    \bye
}

@(latex){
\documentclass{article}
\usepackage{amsmath}
\title{\LaTeX}

\begin{document}
    \maketitle
    \LaTeX{} is a document preparation system for
    the \TeX{} typesetting program. It offers
    programmable desktop publishing features and
    extensive facilities for automating most
    aspects of typesetting and desktop publishing,
    including numbering and  cross-referencing,
    tables and figures, page layout,
    bibliographies, and much more. \LaTeX{} was
    originally written in 1984 by Leslie Lamport
    and has become the  dominant method for using
    \TeX; few people write in plain \TeX{} anymore.
    The current version is \LaTeXe.

    % This is a comment, not shown in final output.
    % The following shows typesetting  power of LaTeX:
    \begin{align}
    E_0 &= mc^2 \\
    E &= \frac{mc^2}{\sqrt{1-\frac{v^2}{c^2}}}
    \end{align}  
\end{document}
}