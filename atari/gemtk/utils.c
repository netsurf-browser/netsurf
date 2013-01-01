#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <gem.h>
#include "gemtk.h"

/* -------------------------------------------------------------------------- */
/* GEM Utillity functions:                                                    */
/* -------------------------------------------------------------------------- */

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
	return _systype_v;
}

bool rc_intersect_ro(GRECT *a, GRECT *b)
{
	GRECT r1, r2;

	r1 = *a;
	r2 = *b;

	return((bool)rc_intersect(&r1, &r2));
}
