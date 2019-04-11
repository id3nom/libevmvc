"use strict";

$(() => {
    parse_markdown();
    parse_tex();
});

function parse_markdown(){
    var defaults = {
        html:         true,        // Enable HTML tags in source
        xhtmlOut:     true,        // Use '/' to close single tags (<br />)
        breaks:       true,        // Convert '\n' in paragraphs into <br>
        langPrefix:   'language-',  // CSS language prefix for fenced blocks
        linkify:      true,         // autoconvert URL-like texts to links
        typographer:  true, // Enable smartypants and other sweet transforms
        
        // options below are for demo only
        _highlight: true,
        _strict: false,
        _view: 'html'               // html / src / debug
    };
    
    
    var mdHtml = window.markdownit(defaults);
    
    var mds = document.querySelectorAll("[data-markup=\"md\"]");
    mds.forEach((v, i) => {
        //v.innerHTML = markdown.toHTML(unescape(v.innerHTML));
        v.innerHTML = mdHtml.render(unescape(v.innerHTML));
    });
}

function parse_tex_old(){
    var texs = document.querySelectorAll("[data-markup=\"tex\"]");
    texs.forEach((v, i) => {
        try{
            var texgen = new latexjs.HtmlGenerator({hyphenate: false});
            texgen = latexjs.parse(
                unescape(v.innerHTML),
                {generator: texgen}
            );
            
            document.body.appendChild(
                texgen.stylesAndScripts(
                    "https://cdn.jsdelivr.net/npm/latex.js@0.11.1/dist/"
                )
            );
            v.innerHTML = "";
            v.appendChild(texgen.domFragment());
            
        }catch(err){
            v.innerHTML = err.message;
        }
    });
}

function parse_tex(){
    var texs = document.querySelectorAll("div[data-markup=\"tex\"]");
    
    texs.forEach((v, i) => {
        try{
            var ifraEle = v.nextSibling;
            if(ifraEle.tagName.toLowerCase() != "iframe")
                throw new Error(
                    "next sibling of div[data-markup=\"tex\"] must be an iframe"
                );
            
            var ifraDoc = ifraEle.contentDocument;
            //var ifraBody = ifraDoc.getElementsByTagName('body')[0];
            
            var texgen = new latexjs.HtmlGenerator({hyphenate: false});
            texgen = latexjs.parse(
                unescape(v.innerHTML),
                {generator: texgen}
            );
            
            ifraDoc.body.appendChild(
                texgen.stylesAndScripts(
                    "https://cdn.jsdelivr.net/npm/latex.js@0.11.1/dist/"
                )
            );
            v.remove();
            //v.innerHTML = "";
            ifraDoc.body.appendChild(texgen.domFragment());
            
            setTimeout(() => {
                resizeIframe(ifraEle);
                $(ifraEle).resize(() => {
                    setTimeout(() => {
                        resizeIframe(ifraEle);
                    }, 50);
                });
            }, 10);
            
        }catch(err){
            v.innerHTML = err.message;
        }
    });
}


function resizeIframe(obj){
    obj.style.width = "100%";
    obj.style.height = obj.contentWindow.document.body.scrollHeight + 'px';
}