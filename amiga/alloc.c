#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>

#ifdef AMIGA_NETSURF_REPLACE_ALLOC
#define nsa_malloc malloc
#define nsa_calloc calloc
#define nsa_realloc realloc
#define nsa_free free
#endif

void nsa_free(void *p) {
	if(p == NULL) return;
	UBYTE *mem = p - 4;
	FreeVec(mem);
}
void *nsa_malloc(size_t s) {
	UBYTE *mem = AllocVec(s + 4, MEMF_PRIVATE);
	*mem = s;
    return mem + 4;
}
void *nsa_calloc(size_t nelem, size_t nsize) {
	UBYTE *mem = AllocVec((nelem * nsize) + 4, MEMF_PRIVATE | MEMF_CLEAR);
	*mem = (nelem * nsize);
    return mem + 4;
}
void *nsa_realloc(void *p, size_t s) {
	void *newptr;
	ULONG old_size = *((UBYTE *)p - 4);
    newptr = nsa_malloc(s);
    memcpy(newptr, p, old_size);
    nsa_free(p);
    return newptr;
}
