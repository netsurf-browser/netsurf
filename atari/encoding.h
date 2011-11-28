#ifndef NS_ATARI_ENCODING_H
#define NS_ATARI_ENCODING_H

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <windom.h>

#include "css/css.h"
#include "render/font.h"
#include "utils/utf8.h"

utf8_convert_ret local_encoding_to_utf8(const char *string,
				       size_t len,
				       char **result);

int atari_to_ucs4( unsigned char atarichar);

#endif
