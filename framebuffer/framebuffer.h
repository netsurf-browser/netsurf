#include "desktop/plotters.h"

extern const struct plotter_table fb_plotters;

nsfb_t *framebuffer_initialise(const char *fename, int width, int height, int bpp);
void framebuffer_finalise(void);
bool framebuffer_set_cursor(struct bitmap *bm);
