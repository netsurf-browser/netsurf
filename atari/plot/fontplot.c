#include "atari/plot/fontplot.h"

const struct s_font_driver_table_entry font_driver_table[] =
{
#ifdef WITH_VDI_FONT_DRIVER
	{"vdi", ctor_font_plotter_vdi, 0},
#endif
#ifdef WITH_FREETYPE_FONT_DRIVER
	{"freetype", ctor_font_plotter_freetype, 0},
#endif
#ifdef WITH_INTERNAL_FONT_DRIVER
	{"internal", ctor_font_plotter_internal, 0},
#endif
	{(char*)NULL, NULL, 0}
};

void dump_font_drivers(void)
{
	int i = 0;
	while( font_driver_table[i].name != NULL ) {
		printf("%s -> flags: %d\n",
			font_driver_table[i].name,
			font_driver_table[i].flags
		);
		i++;
	}
}


/*
	Create an new text plotter object
*/
FONT_PLOTTER new_font_plotter( int vdihandle, char * name, unsigned long flags,
		int * error)
{
	int i=0;
	int res = 0-ERR_PLOTTER_NOT_AVAILABLE;
	FONT_PLOTTER fplotter = (FONT_PLOTTER)malloc( sizeof(struct s_font_plotter) );
	if( fplotter == NULL ) {
		*error = 0-ERR_NO_MEM;
		return( NULL );
	}
	memset( fplotter, 0, sizeof(FONT_PLOTTER));
	fplotter->vdi_handle = vdihandle;
	fplotter->name = name;
	fplotter->flags = 0;
	fplotter->flags |= flags;
	for( i = 0; ; i++) {
		if( font_driver_table[i].name == NULL ) {
			res = 0-ERR_PLOTTER_NOT_AVAILABLE;
			break;
		} else {
			if( strcmp(name, font_driver_table[i].name) == 0 ) {
				if( font_driver_table[i].ctor  ) {
					res = font_driver_table[i].ctor( fplotter );
					*error = 0;
				} else {
					res = 0-ERR_PLOTTER_NOT_AVAILABLE;
					*error = res;
					return (NULL);
				}
				break;
			}
		}
	}
	if( res < 0 ) {
		free( fplotter );
		*error = res;
		return( NULL );
   	}
	return( fplotter );
}

/*
	Free an font plotter
*/
int delete_font_plotter(FONT_PLOTTER p)
{
	if( p ) {
		p->dtor(p);
		free( p );
		p = NULL;
	}
	else
		return( -1 );
	return( 0 );
}

