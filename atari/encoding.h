#ifndef NS_ATARI_ENCODING_H
#define NS_ATARI_ENCODING_H

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <windom.h>

#include "css/css.h"
#include "render/font.h"
#include "utils/utf8.h"

int atari_to_ucs4( unsigned char atarichar);

#endif
