#ifndef ATARI_REDRAW_SLOTS_H
#define ATARI_REDRAW_SLOTS_H

/*
	MAX_REDRW_SLOTS
	This is the number of redraw requests that the slotlist can store.
	If a redraw is scheduled and all slots are used, the rectangle will
	be merged to one of the existing slots.
 */
#define MAX_REDRW_SLOTS	32

/*
	This struct holds scheduled redraw requests.
*/
struct rect;
struct s_redrw_slots
{
	struct rect areas[MAX_REDRW_SLOTS];
	short size;
	short areas_used;
};

void redraw_slots_init(struct s_redrw_slots * slots, short size);
void redraw_slot_schedule(struct s_redrw_slots * slots, short x0, short y0, short x1, short y1);


#endif
