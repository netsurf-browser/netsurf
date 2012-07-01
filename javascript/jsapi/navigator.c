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

#include "javascript/jsapi.h"

#include "utils/log.h"

static JSFunctionSpec jsfunctions_navigator[] = {
	JS_FS_END
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
		     NULL,
		     jsfunctions_navigator,
		     NULL,
		     NULL);
}
