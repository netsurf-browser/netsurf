Javascript bindings
===================

In order for javascript programs to to interact with the page contents
it must use the Document Object Model (DOM) and Cascading Style Sheet
Object Model (CSSOM) API.

These interfaces are described using web Interface Description
Language (IDL) within the relevant specifications
(e.g. https://dom.spec.whatwg.org/).

Each interface described by the webIDL must be bound (connected) to
the browsers internal representation for the DOM or CSS, etc. These
bindings desciptions are processed together with the WebIDL by the
nsgenbind tool to generate source code.

A list of [DOM and CSSOM methods](unimplemented.html) is available
outlining the remaining unimplemented API bindings.

WebIDL
------

The WebIDL specification defines the interface description language used.

These descriptions should be periodicaly updated to keep the browser
interfaces current.

There is a content/handlers/javascript/WebIDL/Makefile which attempts
to automaticaly update the IDL files by scraping the web specs.

This tool needs a great deal of hand holding, not least because many of the
source documents list the IDL fragments multiple times, some even have
appendicies with the entrire IDL repeated.

Interface binding introduction
------------------------------

The binding files are processed by the nsgenbind tool to generate c
source code which implements the interfaces within the javascript
engine.

The bindings are specific to a javascript engine, the DOM library, the
CSS library and the browser. In this case that is the tuple of
duktape, libdom, libcss and NetSurf.

In principle other engines or libraries could be substituted
(historicaly NetSurf unsucessfully tried to use spidermonkey) but the
engineering to do so is formidable.

The bindings are kept the sorce rpository within the duktape
javascript handler content/handlers/javascript/duktape/

The root binding which contains all the interfaces initroduced into
the javascript programs initial execution context is nesurf.bnd this
references all the WebIDL to be bound and includes all additional
binding definitions to implement the interfaces.

The bindings are a Domain Specific Language (DSL) which allows
implementations to be added to each WebIDL method.

Javascript implementation
-------------------------

NetSurf consumes the Duktape JS engine in order to run the JS code which
is used within the browser.  Duktape is exceedingly well documented and
its API docs at https://duktape.org/api.html are incredibly useful.

It'll be worthwhile learning about how duktape stacks work in order to
work on bindings in NetSurf

Dukky
-----

Wrappering around and layering between duktape and the browser is a set of
functionality we call `dukky`.  This defines a variety of conventions and
capabilities which are common to almost all bindings.  The header `dukky.h`
provides the interface to these functions.  Normally these functions are
subsequently used by automatically generated content, but if your bindings
need to add DOM nodes back into the JavaScript environment (for example when
returning them from a method implementation) you will want `dukky_push_node()`
or when calling a function in a JS context you'll likely want `dukky_pcall()`

Dukky automatically terminates any JS call which lasts for more than 10
seconds.  If you are calling a JS function from outside any of the "normal"
means by which dukky might call code on your behalf (`js_exec()`, events, etc)
then you should be sure to use `dukky_pcall()` and pass in `true` in
`reset_timeout` otherwise your code may unexpectedly terminate early.
