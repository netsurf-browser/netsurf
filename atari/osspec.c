#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <mint/osbind.h>
#include <mint/cookie.h>
#include <windom.h>

#include "utils/log.h"
#include "atari/osspec.h"

NS_ATARI_SYSINFO atari_sysinfo;

unsigned short _systype_v;
unsigned short _systype (void)
{
	int32_t * cptr = NULL;
	_systype_v = SYS_TOS;

	cptr = (int32_t *)Setexc(0x0168, -1L);
	if (cptr == NULL ) {
		return _systype_v;   /* stone old TOS without any cookie support */
	}
	while (*cptr) {
		if (*cptr == C_MgMc || *cptr == C_MgMx ) {
			_systype_v = (_systype_v & ~0xF) | SYS_MAGIC;
		} else if (*cptr == C_MiNT ) {
			_systype_v = (_systype_v & ~0xF) | SYS_MINT;
		} else if (*cptr == C_Gnva /* Gnva */ ) {
			_systype_v |= SYS_GENEVA;
		} else if (*cptr == C_nAES /* nAES */ ) {
			_systype_v |= SYS_NAES;
		}
		cptr += 2;
	}
	if (_systype_v & SYS_MINT) { /* check for XaAES */
		short out = 0, u;
		if (wind_get (0, (((short)'X') <<8)|'A', &out, &u,&u,&u) && out) {
			_systype_v |= SYS_XAAES;
		}
	}
	LOG(("Detected OS: %d\n", _systype_v ));
	return _systype_v;
}

void init_os_info(void)
{
	int16_t out[4];
   unsigned long cookie_FSMC = 0;

	atari_sysinfo.gemdos_version = Sversion();

	if( tos_getcookie (C_FSMC, &cookie_FSMC ) == C_FOUND ) {
		atari_sysinfo.gdos_FSMC = 1;
	} else {
		atari_sysinfo.gdos_FSMC = 0;
	}
	atari_sysinfo.large_sfont_pxh = 13;
	atari_sysinfo.medium_sfont_pxh = 6;
	atari_sysinfo.small_sfont_pxh = 4;
	/* todo: detect if system font is monospaced */
	atari_sysinfo.sfont_monospaced = true;
	if( appl_xgetinfo(AES_LARGEFONT, &out[0],  &out[1],  &out[2], &out[3] ) > 0 ){
		atari_sysinfo.large_sfont_pxh = out[0];
	}
	if( appl_xgetinfo(AES_SMALLFONT, &out[0],  &out[1],  &out[2], &out[3] ) > 0 ){
		atari_sysinfo.small_sfont_pxh = out[0];
	}
	atari_sysinfo.aes_max_win_title_len = 79;
	if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
		if (sys_NAES()) {
			atari_sysinfo.aes_max_win_title_len = 127;
		}
		if (sys_XAAES()) {
			atari_sysinfo.aes_max_win_title_len = 200;
		}
	}
}

int tos_getcookie(long tag, long * value)
{
	COOKIE * cptr;
	long oldsp;

	if( atari_sysinfo.gemdos_version > TOS4VER ){
		return( Getcookie(tag, value) );
	}

	cptr = (COOKIE*)Setexc(0x0168, -1L);
	if(cptr != NULL) {
		do {
			if( cptr->c == tag ){
				if(cptr->v != 0 ){
					if( value != NULL ){
						*value = cptr->v;
					}
					return( C_FOUND );
				}
			}
		} while( (cptr++)->c != 0L );
	}
	return( C_NOTFOUND );
}

/*
 a fixed version of realpath() which returns valid
 paths for TOS which have no root fs. (/ , U: )
*/

char * gemdos_realpath(const char * path, char * rpath)
{
	char work[PATH_MAX+1];
	char * work_ptr;
	size_t l;

	if( rpath == NULL ){
		return( NULL );
	}
	if( sys_type() & SYS_MINT ){
		return( realpath(path, rpath) );
	}

	LOG(("gdos rpath in: %s\n", path));
	memset( rpath, 0, PATH_MAX );

	/* first, absolutize relative path: */
	if( *path == '.' ){
		char cwd[PATH_MAX+1];
		getcwd((char*)&cwd, PATH_MAX);
		l = strlen( cwd );
		if( cwd[l-1] != 0x5C && cwd[l-1] != '/' ){
			cwd[l] = 0x5C;
			cwd[l+1] = 0;
			l++;
		}

		strncpy( (char*)&work, cwd, PATH_MAX );

		/* check for path, or maybe just a file name? */
		if( strlen(path) > 2 ) {
			int off = 0;
			if( path[1] == '/' || path[1] == 0x5C ){
				off = 2;
			}
			strncat( (char*)&work, (char*)(path+off), PATH_MAX-l );
		}
		work_ptr = (char*)&work;
	} else {
		work_ptr = (char*)path;
	}

	/* handle invalid cwd path */
	/* mintlib produces these on plain TOS systems: */
	if( strncmp( (char*)work_ptr, "/dev/", 5) == 0 ){
		work_ptr += 4;
	}

	/* make TOS compatible path, step 1: */
	l = strlen( work_ptr);
	if( l > 1 ){
		if( *work_ptr == '/' || *work_ptr == 0x5C ){
			rpath[0] = work_ptr[1];
			rpath[1] = ':';
			strncat( rpath, &work_ptr[2], PATH_MAX-2 );
		} else {
			strncpy( rpath, work_ptr, PATH_MAX );
		}

		/* step 2, perform seperator conversion: */
		l = strlen( rpath );
		rpath[PATH_MAX-1]=0;
		work_ptr = rpath;
		do{
			if( *work_ptr == '/' )
				*work_ptr = 0x5C;
			work_ptr++;
		} while( *work_ptr != 0 );

		if( rpath[l-1] == 0x5C || rpath[l-1] == '/' )
			rpath[l-1] = 0;
	} else {
		strcpy( rpath, work_ptr );
	}
	l = strlen( rpath );
	LOG(("gdos rpath out: %s\n", rpath));
	return( rpath );
}

