#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <mint/osbind.h>
#include <mint/cookie.h>
#include <windom.h>

#include "atari/osspec.h"

NS_ATARI_SYSINFO atari_sysinfo;
/*
short appl_xgetinfo( int16_t type, int16_t *out1, int16_t *out2, 
					 int16_t *out3, int16_t *out4 )
{
	bool has_agi;
	has_agi = ((app.aes_global[0] == 0x399 && atari_sysinfo.gdosversion == 0x1900) 
				|| (app.aes_global[0] > 0x400)  
				|| ( appl_find("?AGI") >= 0));
	if(has_agi){
		return( mt_appl_getinfo( type, out1, out2, out3, out4, app.aes_global) );
	} else {
		return( 0 );
	}
}*/

void init_os_info(void)
{
	int16_t out[4];
	atari_sysinfo.gdosversion = Sversion();
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
}

int tos_getcookie(long tag, long * value)
{
	COOKIE * cptr;
	long oldsp;

	if( atari_sysinfo.gdosversion > TOS4VER ){
		return( Getcookie(tag, value) );
	}

	cptr = (COOKIE*)Setexc(0x0168, -1L);
	if(cptr != NULL) {
		do {
			if( cptr->c == tag ){
				if(cptr->v != 0 ){
					*value = cptr->v;
					return( C_FOUND );
				}
			}
		} while( (cptr++)->c != 0L );
	}
	return( C_NOTFOUND );
}