
#define FB_SCROLL_COLOUR 0xFF888888
#define FB_FRAME_COLOUR 0xFFDDDDDD
#define FB_COLOUR_BLACK 0xFF000000
#define FB_COLOUR_WHITE 0xFFFFFFFF

typedef struct fbtk_widget_s fbtk_widget_t;

/* user widget callback */
typedef int (*fbtk_user_t)(fbtk_widget_t *widget, void *pw);

/* input callback */
typedef int (*fbtk_input_t)(fbtk_widget_t *widget, nsfb_event_t *event, void *pw);

/* mouse click callback */
typedef int (*fbtk_mouseclick_t)(fbtk_widget_t *widget, nsfb_event_t *event, int x, int y, void *pw);

/* mouse move callback */
typedef int (*fbtk_move_t)(fbtk_widget_t *widget, int x, int y, void *pw);

/* redraw function */
typedef int (*fbtk_redraw_t)(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw);

/* enter pressed on writable icon */
typedef int (*fbtk_enter_t)(void *pw, char *text);


/* Widget creation */

/** Initialise widget toolkit.
 *
 * Initialises widget toolkit and creates root window against a framebuffer.
 *
 * @param fb The underlying framebuffer.
 * @return The root widget handle.
 */
fbtk_widget_t *fbtk_init(nsfb_t *fb);

/** Create a window widget.
 *
 * @param parent The parent window or the root widget for a top level window.
 * @param x The x location relative to the parent window.
 * @param y the y location relative to the parent window.
 * @param width The width of the window. 0 indicates parents width should be
 *              used. Negative value indicates parents width less the value
 *              should be used. The width is limited to lie within the parent
 *              window.
 * @param height The height of the window limited in a similar way to the
 *               /a width.
 * @param c The background colour.
 * @return new window widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_window(fbtk_widget_t *parent, int x, int y, int width, int height);

/** Create a text widget.
 *
 * @param window The window to add the text widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_text(fbtk_widget_t *window, int x, int y, int width, int height, colour bg, colour fg, bool outline);

/** Create a bitmap widget.
 *
 * Create a widget which shows a bitmap.
 *
 * @param window The window to add the bitmap widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_bitmap(fbtk_widget_t *window, int x, int y, colour c,struct bitmap *image);

/** Create a filled rectangle
 *
 * Create a widget which is a filled rectangle, usually used for backgrounds.
 *
 * @param window The window to add the filled area widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *
fbtk_create_fill(fbtk_widget_t *window, int x, int y, int width, int height, colour c);

/** Create a horizontal scroll widget
 *
 * Create a horizontal scroll widget.
 *
 * @param window The window to add the filled area widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *
fbtk_create_hscroll(fbtk_widget_t *window, int x, int y, int width, int height, colour fg, colour bg);

/** Create a vertical scroll widget
 *
 * Create a vertical scroll widget.
 *
 * @param window The window to add the filled area widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *
fbtk_create_vscroll(fbtk_widget_t *window, int x, int y, int width, int height, colour fg, colour bg);

/** Create a user widget.
 *
 * Create a widget which is to be handled entirely by the calling application.
 *
 * @param window The window to add the user widget to.
 * @param pw The private pointer which can be read using ::fbtk_get_pw
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_user(fbtk_widget_t *window, int x, int y, int width, int height, void *pw);


/** Create a button widget.
 *
 * Helper function which creates a bitmap widget and associate a handler for
 * when it is clicked.
 *
 * @param window The window to add the button widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_button(fbtk_widget_t *window, int x, int y, colour c, struct bitmap *image, fbtk_mouseclick_t click, void *pw);

/** Create a writable text widget.
 *
 * Helper function which creates a text widget and configures an input handler
 * to create a writable text field. This call is equivalent to calling
 * ::fbtk_create_text followed by ::fbtk_writable_text
 *
 * @param window The window to add the text widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_writable_text(fbtk_widget_t *window, int x, int y, int width, int height, colour bg, colour fg, bool outline, fbtk_enter_t enter, void *pw);


/* Widget Destruction */

/** Destroy and free a widget and all its children.
 *
 * @param widget The widget to destroy.
 * @return 0 on success or -1 on error.
 */
int fbtk_destroy_widget(fbtk_widget_t *widget);

/* Widget information */

int fbtk_get_y(fbtk_widget_t *widget);
int fbtk_get_x(fbtk_widget_t *widget);
int fbtk_get_width(fbtk_widget_t *widget);
int fbtk_get_height(fbtk_widget_t *widget);
void *fbtk_get_userpw(fbtk_widget_t *widget);
nsfb_t *fbtk_get_nsfb(fbtk_widget_t *widget);

/* Set widget properties */

void fbtk_set_text(fbtk_widget_t *widget, const char *text);
void fbtk_set_bitmap(fbtk_widget_t *widget, struct bitmap *image);
void fbtk_set_scroll(fbtk_widget_t *widget, int pct);
void fbtk_set_scroll_pos(fbtk_widget_t *widget, int pos);
void fbtk_set_pos_and_size(fbtk_widget_t *widget, int x, int y, int width, int height);
void fbtk_set_handler_redraw(fbtk_widget_t *widget, fbtk_redraw_t input, void *pw);
void fbtk_set_handler_input(fbtk_widget_t *widget, fbtk_input_t input, void *pw);
void fbtk_set_handler_click(fbtk_widget_t *widget, fbtk_mouseclick_t click, void *pw);
void fbtk_set_handler_move(fbtk_widget_t *widget, fbtk_move_t move, void *pw);

/** Alter a text widget to be writable.
 */
void fbtk_writable_text(fbtk_widget_t *widget, fbtk_enter_t enter, void *pw);


/* General routines */

bool fbtk_clip_rect(const bbox_t * restrict clip, bbox_t * restrict box);

/** Pointer movement.
 *
 * Pointer has been moved.
 *
 * @param widget any tookit widget.
 * @parm x movement in horizontal plane.
 * @parm y movement in vertical plane.
 * @parm relative Wether the /a x and /a y should be considered relative to
 *                current pointer position.
 */
void fbtk_move_pointer(fbtk_widget_t *widget, int x, int y, bool relative);

/** Mouse has been clicked
 */
void fbtk_click(fbtk_widget_t *widget, nsfb_event_t *event);

/** Input has been recived
 */
void fbtk_input(fbtk_widget_t *widget, nsfb_event_t *event);

/** Indicate a widget has to be redrawn
 */
void fbtk_request_redraw(fbtk_widget_t *widget);

/** Cause a redraw to happen.
 */
int fbtk_redraw(fbtk_widget_t *widget);

int fbtk_count_children(fbtk_widget_t *widget);

bool fbtk_get_bbox(fbtk_widget_t *widget, struct nsfb_bbox_s *bbox);

bool fbtk_event(fbtk_widget_t *root, nsfb_event_t *event, int timeout);

/* keycode to ucs4 */
int fbtk_keycode_to_ucs4(int code, uint8_t mods);

/* clip a box to a widgets area */
bool fbtk_clip_to_widget(fbtk_widget_t *widget, bbox_t * restrict box);







