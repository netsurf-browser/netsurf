JavaScript bindings
===================

In order for JavaScript programs to to interact with the page contents
it must use the Document Object Model (DOM) and Cascading Style Sheet
Object Model (CSSOM) API.

These interfaces are described using Web Interface Description
Language (WebIDL) within the relevant specifications
(e.g. https://dom.spec.whatwg.org/).

Each interface described by the WebIDL must be bound (connected) to
the browsers internal representation for the DOM or CSS, etc. These
bindings descriptions are processed together with the WebIDL by the
nsgenbind tool to generate source code.

A list of [DOM and CSSOM methods](unimplemented.html) is available
outlining the remaining unimplemented API bindings.

WebIDL
------

The [WebIDL specification](http://www.w3.org/TR/WebIDL/) defines the
interface description language used. The WebIDL is being updated and
an [editors draft](https://heycam.github.io/webidl/) is available but
use is inconsistent.

These descriptions should be periodically updated to keep the browser
interfaces current.

There is a content/handlers/javascript/WebIDL/Makefile which attempts
to automatically update the IDL files by scraping the web specs.

This tool needs a great deal of hand holding, not least because many of the
source documents list the IDL fragments multiple times, some even have
appendices with the entire IDL repeated.

The IDL uses some slightly different terms than other object orientated
 systems.

  WebIDL | JavaScript | Common OOP | Note
 ------- | ---------- | ---------- | ----
 interface | prototype | class     | The data definition of the object
 constants | read-only value property on the prototype | class variable | Belong to class, one copy
 operation | method    | method    | functions that can be called
 attribute | property  | property  | Variables set per instance

JavaScript implementation
-------------------------

NetSurf consumes the Duktape JS engine in order to run the JS code which
is used within the browser.  Duktape is exceedingly well documented and
its API docs at https://duktape.org/api.html are incredibly useful.

It'll be worthwhile learning about how duktape stacks work in order to
work on bindings in NetSurf

Dukky
-----

Wrappering around and layering between duktape and the browser is a
set of functionality we call `dukky`.  This defines a variety of
conventions and capabilities which are common to almost all bindings.
The header `dukky.h` provides the interface to these functions.

Normally these functions are used by automatically generated content,
but if a binding needs to add DOM nodes back into the JavaScript
environment (for example when returning them from a method
implementation) `dukky_push_node()` should be used or when calling a
function in a JS context `dukky_pcall()`

Dukky automatically terminates any JS call which lasts for more than
10 seconds.  If you are calling a JS function from outside any of the
"normal" means by which dukky might call code on your behalf
(`js_exec()`, events, etc) then you should be sure to use
`dukky_pcall()` and pass in `true` in `reset_timeout` otherwise your
code may unexpectedly terminate early.

Interface binding introduction
------------------------------

The binding files are processed by the nsgenbind tool to generate c
source code which implements the interfaces within the JavaScript
engine.

The bindings are specific to a JavaScript engine, the DOM library, the
CSS library and the browser. In this case that is the tuple of
duktape, libdom, libcss and NetSurf.

In principle other engines or libraries could be substituted
(historically NetSurf unsuccessfully tried to use spidermonkey) but the
engineering to do so is formidable.

The bindings are kept the main [NetSurf source code
repository](http://git.netsurf-browser.org/netsurf.git/) within the
duktape JavaScript handler directory `content/handlers/javascript/duktape/`

The root binding which contains all the interfaces introduced into
the JavaScript programs initial execution context is nesurf.bnd this
references all the WebIDL to be bound and includes all additional
binding definitions to implement the interfaces.

The bindings are a Domain Specific Language (DSL) which allows
implementations to be added to each WebIDL method. nsgenbind
documentation contains a [full description of the
language](https://ci.netsurf-browser.org/jenkins/view/Categorized/job/docs-nsgenbind/doxygen/index.html).

The main focus on creating binding is to implement the content within
getter, setter and method stanzas. These correspond to implementations
of the WebIDL operations on an interface.

The binding implementations are in the form of C code fragments
directly pasted into the generated code with generated setup code
surrounding it.

### Simple getter and setter example

The Window interface (class) in the HTML specification has an
attribute called `name`.

The full WebIDL for the Window interface is defined in the
`content/handlers/javascript/WebIDL/html.idl` file but the
fragment for our example is just:

    interface Window : EventTarget {
        attribute DOMString name;
    };

This indicates there is an attribute called `name` which is a string
(technicaly a DOMString but the implementation does not differentiate
between string types) which means it has both a setter and a getter.

attributes can be marked readonly which would mean there is only a
getter required for them. For example the Plugin interface has a
'name' attribute defined as:

    interface Plugin {
        readonly attribute DOMString name;
    };

The getter and setter for the Window class attribute will be added to
`Window.bnd`. The entries added will be of the form:

    getter Window::name()
    %{
    %}
    
    setter Window::name()
    %{
    %}

The top level `netsurf.bnd` binding includes `Window.bnd` (using a
`#include` directive) which contains the implementation of the Window
class. This is purely to split the bindings up into logical units.

The nsgenbind tool generates code that automatically allows acess to
the classes private data structure elements through a variable called
`priv` and the duktape stack in the variable `ctx`.

The getter binding code must place the retrived value on the duktape
stack and return 1 to indicate this or 0 if it failed.

So for the name attribute case the complete getter binding is:

    getter Window::name()
    %{
            const char *name;
            browser_window_get_name(priv->win, &name);
            duk_push_string(ctx, name);
            return 1;
    %}

This uses the browser_window_get_name() interface to retrieve the name
string for the window (identified using the private context) and then
adds it to the duktape stack. The return value indicates the sucess of
the operation.

The setter must retrive the value to set from the duktap stack and
update the internal private data structure with that value and return
0 to indicate success.

So for the name attribute case the complete setter binding is:

    setter Window::name()
    %{
            const char *name;
            name = duk_to_string(ctx, -1);
            browser_window_set_name(priv->win, name);
            return 0;
    %}


### Simple method example

