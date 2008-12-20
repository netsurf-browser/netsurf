/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_COMPAT_H
#define AMIGA_COMPAT_H
//#include <sys/unistd.h>
#include <sys/utsname.h>

/* for termios.h compatiblity */
typedef unsigned int	tcflag_t;
typedef unsigned char	cc_t;
typedef unsigned int	speed_t;

#define NCCS	16

struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	cc_t	c_cc[NCCS];
	/* Private */
	speed_t	c_ispeed;
	speed_t	c_ospeed;
	int type;
	unsigned int flags;
};
/**/

extern gid_t getegid(void);
extern uid_t geteuid(void);
extern uid_t getuid(void);
extern gid_t getgid(void);
extern int tcsetattr(int fildes, int optional_actions, const struct termios *termios_p);
extern int tcgetattr(int fildes, struct termios *termios_p); 

//char *strndup(const char *,size_t);
extern int	strcasecmp(const char *, const char *);
extern int uname(struct utsname *);
#endif
