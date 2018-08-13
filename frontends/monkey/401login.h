
#include <stdbool.h>

#include "utils/nsurl.h"
#include "utils/errors.h"


nserror gui_401login_open(nsurl *url, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw);
