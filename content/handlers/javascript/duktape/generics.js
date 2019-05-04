/*
 * Generics for Duktape binding in NetSurf
 *
 * The result of this *MUST* be setting a NetSurf object only.
 *
 * That object will then be absorbed into the global object as a hidden
 * object which is used by the rest of the bindings.
 */

var NetSurf = {
    /* The make-proxy call for list-type objects */
    makeListProxy: function(inner) {
	return new Proxy(inner, {
	    has: function(target, key) {
		if (typeof key == 'number') {
		    return (key >= 0) && (key < target.length);
		} else {
		    return key in target;
		}
	    },
	    get: function(target, key) {
		if (typeof key == 'number') {
		    return target.item(key);
		} else {
		    return target[key];
		}
	    },
	});
    },
};
