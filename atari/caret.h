#ifndef NS_ATARI_CARET_H
#define NS_ATARI_CARET_H

#include <mt_gem.h>
#include <stdbool.h>

struct s_caret {
	GRECT dimensions;
	MFDB background;
	bool visible;
};

void caret_show(struct s_caret *c, VdiHdl vh, GRECT * dimensions, GRECT *clip);
void caret_hide(struct s_caret *c, VdiHdl vh, GRECT *clip);

#endif // NS_ATARI_CARET_H

