/* DO NOT USE, DODGY BIT FOR VINCE */

#ifndef DUKKY_H
#define DUKKY_H

duk_ret_t dukky_create_object(duk_context *ctx, const char *name, int args);
duk_bool_t dukky_push_node_stacked(duk_context *ctx);
duk_bool_t dukky_push_node(duk_context *ctx, struct dom_node *node);


#endif
