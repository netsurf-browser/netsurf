/*
*	MultiTOS Drag&Drop Header file
*/

/*
*	Declarations
*/

short ddcreate(short *pipe);
short ddmessage(short apid, short fd, short winid, short mx, short my, short kstate, short pipename);
short ddrexts(short fd, char *exts);
short ddstry(short fd, char *ext, char *text, char *name, long size);
void ddclose(short fd);
void ddgetsig(long *oldsig);
void ddsetsig(long oldsig);
short ddopen(short ddnam, char ddmsg);
short ddsexts(short fd, char *exts);
short ddrtry(short fd, char *name, char *file, char *whichext, long *size);
short ddreply(short fd, char ack);
