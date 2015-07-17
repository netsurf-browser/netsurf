#!/bin/make
#
# Create the WebIDL for core DOM and HTML spec directly from
#  downloaded specifications. The resulting IDL is pretty close but
#  carries the example text etc. and should be manually cleaned up and
#  tagged with the retrival date and source URL
#
# needs debin packages: curl w3m tidy html-xml-utils
#
# Copyright 2012 Vincent Sanders 
# MIT licenced

.PHONY:all clean

all: dom.idl html.idl

.INTERMEDIATE:dom-spec.html dom-spec.xml dom-idl.html html-spec.html html-spec.xml html-idl.html


dom-spec.html:
	curl -s https://dom.spec.whatwg.org/ -o $@

html-spec.html:
	curl -s https://html.spec.whatwg.org/ -o $@

%-spec.xml: %-spec.html
	-tidy -q -f $@.errors --new-blocklevel-tags header,hgroup,figure,time,main,nav,svg,rect,text,image,mark,figcaption -o $@ -asxml $<

%-idl.html: %-spec.xml
	hxselect 'pre[class="idl"]' < $< > $@

%.idl: %-idl.html
	@echo "// Retrived from " >$@
	@echo "// $$(date)" >>$@
	@echo "" >>$@
	@echo "" >>$@
	cat $< | w3m -dump -T text/html >>$@



clean:
	${RM} dom.idl html.idl dom-spec.html dom-spec.xml dom-idl.html html-spec.html html-spec.xml html-idl.html
