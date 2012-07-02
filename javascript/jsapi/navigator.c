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

#include <assert.h>

#include "javascript/jsapi.h"

#include "desktop/netsurf.h"
#include "desktop/options.h"

#include "utils/config.h"
#include "utils/useragent.h"
#include "utils/log.h"
#include "utils/utsname.h"

/*
 * navigator properties for netsurf
 * 
 * Property    | Everyone else   | NetSurf      | Notes
 * ------------+-----------------+--------------+------------------------------
 * appCodeName | "Mozilla"       | "NetSurf"    | This is kinda a pointless 
 *             |                 |              |  constant as everyone returns 
 *             |                 |              |  "Mozilla" which is dumb
 * ------------+-----------------+--------------+------------------------------
 * appName     | "<Browsername>" | "NetSurf"    | Browsers named other than 
 *             |                 |              | "Netscape", "Mozilla", 
 *             |                 |              | "Netscape Navigator", 
 *             |                 |              | "Microsoft Internet Explorer" 
 *             |                 |              |  often other browser have 
 *             |                 |              | "(compatible with Netscape)" 
 *             |                 |              |  append.
 * ------------+-----------------+--------------+------------------------------
 * appVersion  | "<ver> (<type>)"| "<ver>"      | Actually just the version 
 *             |                 |              |  number e.g "3.0".
 * ------------+-----------------+--------------+------------------------------
 * language    | "<lang>"        | "<lang>"     | The language the frontend is 
 *             |                 |              |  configured for
 * ------------+-----------------+--------------+------------------------------
 * platform    | "<krn> <hw>"    | "<krn> <hw>" | Efectively uname -s -i, 
 *             |                 |              |   eg "Linux x86_64"
 * ------------+-----------------+--------------+------------------------------
 * userAgent   | "Mozilla/5.0 (" | "NetSurf"    | The usual useragent string  
 *             |                 |              |  with excessive lies
 * ------------+-----------------+--------------+------------------------------
 */

static JSFunctionSpec jsfunctions_navigator[] = {
	JS_FS_END
};

#define NAVIGATOR_APPNAME "NetSurf"
#define NAVIGATOR_APPCODENAME "NetSurf"

static JSBool JSAPI_PROPERTYGET(appName, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, NAVIGATOR_APPNAME)));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(appName, JSContext *cx, JSObject *obj, jsval *vp)
{
	assert(false);
	return JS_FALSE;
}

static JSBool JSAPI_PROPERTYGET(appCodeName, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, NAVIGATOR_APPCODENAME)));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(appCodeName, JSContext *cx, JSObject *obj, jsval *vp)
{
	assert(false);
	return JS_FALSE;
}

static JSBool JSAPI_PROPERTYGET(appVersion, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, netsurf_version)));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(appVersion, JSContext *cx, JSObject *obj, jsval *vp)
{
	assert(false);
	return JS_FALSE;
}

static JSBool JSAPI_PROPERTYGET(language, JSContext *cx, JSObject *obj, jsval *vp)
{
	const char *alang = nsoption_charp(accept_language);

	if (alang != NULL) {
		JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, alang)));
	} else {
		JS_SET_RVAL(cx, vp, JSVAL_VOID);
	}
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(language, JSContext *cx, JSObject *obj, jsval *vp)
{
	assert(false);
	return JS_FALSE;
}

static JSBool JSAPI_PROPERTYGET(platform, JSContext *cx, JSObject *obj, jsval *vp)
{
	struct utsname *cutsname;

	cutsname = malloc(sizeof(struct utsname));

	if ((cutsname == NULL) || uname(cutsname) < 0) {
		JS_SET_RVAL(cx, vp, JSVAL_VOID);
	} else {
		char *platstr;
		int platstrlen;
		platstrlen = strlen(cutsname->sysname) + strlen(cutsname->machine) + 2;
		platstr = malloc(platstrlen);
		if (platstr != NULL) {
			snprintf(platstr, platstrlen, "%s %s", cutsname->sysname, cutsname->machine);
			JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyN(cx, platstr, platstrlen - 1)));
			free(platstr);
		} else {
			JS_SET_RVAL(cx, vp, JSVAL_VOID);
		}
	}
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(platform, JSContext *cx, JSObject *obj, jsval *vp)
{
	assert(false);
	return JS_FALSE;
}

static JSBool JSAPI_PROPERTYGET(userAgent, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, user_agent_string())));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(userAgent, JSContext *cx, JSObject *obj, jsval *vp)
{
	assert(false);
	return JS_FALSE;
}


static JSPropertySpec jsproperties_navigator[] =
{
	JSAPI_PS(appName, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS(appCodeName, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS(appVersion, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS(language, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS(platform, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS(userAgent, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS_END
};

static JSClass jsclass_navigator =
{
	"navigator",
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


JSObject *jsapi_new_navigator(JSContext *cx, JSObject *parent)
{
	return JS_InitClass(cx,
		     parent,
		     NULL,
		     &jsclass_navigator,
		     NULL,
		     0,
		     jsproperties_navigator,
		     jsfunctions_navigator,
		     NULL,
		     NULL);
}
