[TOC]

# About libevmvc

## Routing

* simple route that will match url "/abc/123" and "/abc/123/"
<br/>
/abc/123

* simple route that will match url's "/abc/123", "/abc/123/def", "/abc/123/456/" but not "/abc/123/def/other_path"
<br/>
/abc/123/*

* simple route that will match url's "/abc/123", "/abc/123/def", "/abc/123/456/" and any sub path "/abc/123/def/sub/path/..."
<br/>
/abc/123/**

* optional parameter can be defined by enclosing the parameter in square brackets
<br/>
/abc/123/:p1/:[p2]

* PCRE regex can be use to validate parameter by enclosing the rules inside parentheses following the parameter name
<br/>
/abc/123/:p1(\\d+)/:[p2]

* regex parameter can be optional as well
<br/>
/abc/123/:[p1(\\d+)]

* all parameters following an optional parameter must be optional
<br/>
/abc/123/:p1(\\d+)/:[p2]/:[p3]


## Fanjet language
