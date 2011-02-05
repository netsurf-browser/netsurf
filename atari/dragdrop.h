/*
*	MultiTOS Drag&Drop Header file
*/

/* RÅckgabewerte von ddstry() etc. */

#define	DD_OK			0
#define DD_NAK			1
#define DD_EXT			2
#define DD_LEN			3
#define DD_TRASH		4
#define DD_PRINTER		5
#define DD_CLIPBOARD	6


/* Timeout in Millisekunden (4 sek.) */

#define DD_TIMEOUT		4000


/* Anzahl der Extensionen/Bytes der "bevorzugten Extensionen" */

#define DD_NUMEXTS		8
#define DD_EXTSIZE		32L


/* Max. LÑnge des Drag&Drop name/file */

#define DD_NAMEMAX		128


/* Max. LÑnge des Drag&Drop Header */

#define DD_HDRMAX		(8+DD_NAMEMAX+DD_NAMEMAX)


/*
*	Funktionsdeklarationen
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
