/*
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_OSSPEC_H
#define NS_ATARI_OSSPEC_H

typedef struct {
	long c;
	long v;
} COOKIE;

/* System type detection added by [GS]  */
#define SYS_TOS    0x0001
#define SYS_MAGIC  0x0002
#define SYS_MINT   0x0004
#define SYS_GENEVA 0x0010
#define SYS_NAES   0x0020
#define SYS_XAAES  0x0040
/* detect the system type, AES + kernel */
#define sys_type()    (_systype_v ? _systype_v : _systype())
#define sys_MAGIC()   ((sys_type() & SYS_MAGIC) != 0)
#define sys_NAES()    ((sys_type() & SYS_NAES)  != 0)
#define sys_XAAES()   ((sys_type() & SYS_XAAES) != 0)


typedef struct {
	unsigned short gemdos_version;
	unsigned short gdos_FSMC;
	unsigned short systype;
	unsigned short small_sfont_pxh;
	unsigned short medium_sfont_pxh;
	unsigned short large_sfont_pxh;
	bool sfont_monospaced;
  short aes_max_win_title_len;
} NS_ATARI_SYSINFO;

extern NS_ATARI_SYSINFO atari_sysinfo;
extern unsigned short _systype_v;

#define TOS4VER 0x03300 /* this is assumed to be the last single tasking OS */

void init_os_info(void);
int tos_getcookie( long tag, long * value );
void fix_path(char * path);
char * gemdos_realpath(const char * path, char * rpath);
unsigned short _systype (void);
#endif