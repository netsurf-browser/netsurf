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

/* IDL http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#exception-domexception
exception DOMException {
  const unsigned short INDEX_SIZE_ERR = 1;
  const unsigned short DOMSTRING_SIZE_ERR = 2; // historical
  const unsigned short HIERARCHY_REQUEST_ERR = 3;
  const unsigned short WRONG_DOCUMENT_ERR = 4;
  const unsigned short INVALID_CHARACTER_ERR = 5;
  const unsigned short NO_DATA_ALLOWED_ERR = 6; // historical
  const unsigned short NO_MODIFICATION_ALLOWED_ERR = 7;
  const unsigned short NOT_FOUND_ERR = 8;
  const unsigned short NOT_SUPPORTED_ERR = 9;
  const unsigned short INUSE_ATTRIBUTE_ERR = 10; // historical
  const unsigned short INVALID_STATE_ERR = 11;
  const unsigned short SYNTAX_ERR = 12;
  const unsigned short INVALID_MODIFICATION_ERR = 13;
  const unsigned short NAMESPACE_ERR = 14;
  const unsigned short INVALID_ACCESS_ERR = 15;
  const unsigned short VALIDATION_ERR = 16; // historical
  const unsigned short TYPE_MISMATCH_ERR = 17;
  const unsigned short SECURITY_ERR = 18;
  const unsigned short NETWORK_ERR = 19;
  const unsigned short ABORT_ERR = 20;
  const unsigned short URL_MISMATCH_ERR = 21;
  const unsigned short QUOTA_EXCEEDED_ERR = 22;
  const unsigned short TIMEOUT_ERR = 23;
  const unsigned short INVALID_NODE_TYPE_ERR = 24;
  const unsigned short DATA_CLONE_ERR = 25;
  unsigned short code;
};

*/

static JSClass jsclass_domexception =
{
        "DOMException", 
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



JSObject *
jsapi_new_domexception(JSContext *cx, 
		  JSObject *parent,
		  int code)
{
	/* create element object and return it*/
	JSObject *jsdomexception;

	jssomexception = JS_InitClass(cx, 
			   parent, 
			   NULL, 
			   &jsclass_domexception, 
			   NULL, 
			   0, 
			   NULL, 
			   NULL, 
			   NULL, 
			   NULL);
	if (jsdomexecption == NULL) {
		return NULL;
	}

	LOG(("setting element private to %d", code));
	/* private pointer to browsing context */
	if (JS_SetPrivate(cx, jsdomexception, code) != JS_TRUE) {
		LOG(("failed to set content"));
		return NULL;
	}

	return jsdomexception;
}
