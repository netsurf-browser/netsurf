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

#include "atari/osspec.h"

NS_ATARI_SYSINFO atari_sysinfo;

unsigned short _systype_v;
unsigned short _systype (void)
{
_systype_v = (_systype_v & ~0xF) | SYS_MINT | SYS_XAAES;
	return _systype_v;

	int32_t * cptr = NULL;
	_systype_v = SYS_TOS;
	
	cptr = Setexc(0x0168, -1L);
	if (cptr == NULL ) {
		return _systype_v;   /* stone old TOS without any cookie support */
	}
	while (*cptr) {
		if (*cptr == C_MgMc || *cptr == C_MgMx ) {
			_systype_v = (_systype_v & ~0xF) | SYS_MAGIC;
		} else if (*cptr == C_MiNT ) {
			_systype_v = (_systype_v & ~0xF) | SYS_MINT;
		} else if (*cptr == C_Gnva/*Gnva*/) {
			_systype_v |= SYS_GENEVA;
		} else if (*cptr == C_nAES/*nAES*/) {
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

/* convert nonsense getcwd path (returned by mintlib getcwd on plain TOS) */
void fix_path(char * path)
{
	char npath[PATH_MAX];
	/* only apply fix to paths that contain /dev/ */
	if( strlen(path) < 6 ){
		return;
	}
	if( strncmp(path, "/dev/", 5) != 0 ) {
		return;
	}
	strncpy((char*)&npath, path, PATH_MAX);
	npath[0] = path[5];
	npath[1] = ':';
	npath[2] = 0;
	strcat((char*)&npath, &path[6]);
	strcpy(path, (char*)&npath);
}

/* 
 a fixed version of realpath() which returns valid 
 paths for TOS which have no root fs. (/ , U: )
*/
char * gdos_realpath(const char * path, char * rpath)
{
	size_t l;
	size_t i;
	char old;
	char fsep = 0x5C;
	if( rpath == NULL ){
		return( NULL );
	}
	if( sys_type() & SYS_MINT ){
		return( realpath(path, rpath) );
	}

	if( fsep == '/') {
		/* replace '\' with / */
		old = 0x5C; /* / */
	} else {
		/* replace '/' with \ */
		old = '/';
	}

	if( path[0] != '/' && path[0] != 0x5c && path[1] != ':') {
		/* it is not an absolute path */
		char cwd[PATH_MAX];
		getcwd((char*)&cwd, PATH_MAX);
		fix_path((char*)&cwd);
		strcpy(rpath, (char*)&cwd);
		l = strlen(rpath);
		if(rpath[l-1] != 0x5C && rpath[l-1] != '/') {
			rpath[l] = fsep;
			rpath[l+1] = 0;
		}
		if( (path[1] == '/' || path[1] == 0x5C ) ) {
			strcat(rpath, &path[2]);
		} else {
			strcat(rpath, path);
		}
	} else {
		strcpy(rpath, path);
	}
	/* convert path seperator to configured value: */
	l = strlen(rpath);
	for( i = 0; i<l-1; i++){
		if( rpath[i] == old ){
			rpath[i] = fsep;
		}
	}
	return( rpath );	
}