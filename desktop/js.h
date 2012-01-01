
typedef struct jscontext jscontext;
typedef struct jsobject jsobject;

void js_initialise(void);
void js_finalise(void);

jscontext *js_newcontext(void);
void js_destroycontext(jscontext *ctx);

jsobject *js_newcompartment(jscontext *ctx);
