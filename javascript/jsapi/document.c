/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dom/dom.h>


#include "javascript/jsapi.h"
#include "utils/config.h"
#include "render/html_internal.h"
#include "utils/log.h"

static JSClass jsclass_document =
{
        "document", 
	JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_StrictPropertyStub,
        JS_EnumerateStub, 
	JS_ResolveStub, 
	JS_ConvertStub, 
	JS_FinalizeStub, 
	JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool JSAPI_NATIVE(write, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;
	struct html_content *htmlc;

	htmlc = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &jsclass_document, NULL);
	if (htmlc == NULL)
		return JS_FALSE;

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;

	JSString_to_char(u16_txt, txt, length);

	LOG(("content %p parser %p writing %s",htmlc, htmlc->parser_binding, txt));
	if (htmlc->parser_binding != NULL) {
		dom_hubbub_parser_insert_chunk(htmlc->parser_binding, (uint8_t *)txt, length);
	}
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_document[] = {
	JSAPI_FS(write, 1, 0),
	JSAPI_FS_END
};




JSObject *jsapi_new_document(JSContext *cx, JSObject *parent, void *doc_priv)
{
	JSObject *doc;
	doc = JS_InitClass(cx, 
		     parent, 
		     NULL, 
		     &jsclass_document, 
		     NULL, 
		     0, 
		     NULL, 
		     jsfunctions_document, 
		     NULL, 
		     NULL);
	if (doc == NULL) {
		return NULL;
	}

	LOG(("setting content to %p",doc_priv));
	/* private pointer to browsing context */
	if (!JS_SetPrivate(cx, doc, doc_priv))
		return NULL;
	
	return doc;
}
