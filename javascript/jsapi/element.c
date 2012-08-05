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

static void jsfinalize_element(JSContext *cx, JSObject *obj);

typedef struct {
	struct html_content *htmlc;
	struct dom_element *dom_element;
} elementp;

static JSClass jsclass_element =
{
        "element", 
	JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_StrictPropertyStub,
        JS_EnumerateStub, 
	JS_ResolveStub, 
	JS_ConvertStub, 
	jsfinalize_element, 
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static void jsfinalize_element(JSContext *cx, JSObject *obj)
{
	elementp *element;
	element = JS_GetInstancePrivate(cx, obj, &jsclass_element, NULL);
	if (element != NULL) {
		free(element);
	}
}



static JSFunctionSpec jsfunctions_element[] = {
	JSAPI_FS_END
};


JSObject *
jsapi_new_element(JSContext *cx, 
		  JSObject *parent, 
		  struct html_content *htmlc, 
		  struct dom_element *domelement)
{
	/* create element object and return it*/
	JSObject *jselement;
	elementp *element;

	element = malloc(sizeof(element));
	if (element == NULL) {
		return NULL;
	}
	element->htmlc = htmlc;
	element->dom_element = domelement;

	jselement = JS_InitClass(cx, 
			   parent, 
			   NULL, 
			   &jsclass_element, 
			   NULL, 
			   0, 
			   NULL, 
			   jsfunctions_element, 
			   NULL, 
			   NULL);
	if (jselement == NULL) {
		free(element);
		return NULL;
	}

	LOG(("setting element private to %p", element));
	/* private pointer to browsing context */
	if (JS_SetPrivate(cx, jselement, element) != JS_TRUE) {
		LOG(("failed to set content"));
		free(element);
		return NULL;
	}

	return jselement;
}
