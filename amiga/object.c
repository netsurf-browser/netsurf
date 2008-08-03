/*
 * Copyright 2005,2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include "amiga/object.h"

struct MinList *NewObjList(void)
{

	struct MinList *objlist;

	objlist = (struct MinList *)AllocVec(sizeof(struct MinList),MEMF_CLEAR);

	NewMinList(objlist);

	return(objlist);

}

struct nsObject *AddObject(struct MinList *objlist,ULONG otype)
{
	struct nsObject *dtzo;

	dtzo = (struct nsObject *)AllocVec(sizeof(struct nsObject),MEMF_CLEAR);

	AddTail((struct List *)objlist,(struct Node *)dtzo);

	dtzo->Type = otype;
//	dtzo->SubType = ostype;

	return(dtzo);
}

void DelObject(struct nsObject *dtzo)
{
	Remove((struct Node *)dtzo);
	if(dtzo->objstruct) FreeVec(dtzo->objstruct);
	FreeVec(dtzo);
	dtzo = NULL;
}

void FreeObjList(struct MinList *objlist)
{
	struct nsObject *node;
	struct nsObject *nnode;

	node = (struct nsObject *)objlist->mlh_Head;

//	for(node = objlist->mlh_Head ; node->dtz_Node.mln_Succ ; node = node->dtz_Node.mln_Succ)

	while(nnode=(struct nsObject *)(node->dtz_Node.mln_Succ))
	{
		printf("%lx next %lx node objtype %ld\n",node,nnode,node->Type);
		DelObject(node);
		node=nnode;
	}

	FreeVec(objlist);
}

/*
int main(int argc, char **argv)
{
	struct MinList *ol;
	struct DTZObject *dtzo;
	struct DTZObject *tobj;

	ol = NewObjList();


	tobj = AddObject(ol);

	tobj->Data = 6;

	dtzo = AddObject(ol);

	dtzo->Data = 7;

//	tobj = (struct DTZObject *)ol->mlh_Head;

//	tobj = ol->dtz_Node.mln_Succ;

//	printf("%ld\n",tobj->Data);

	FreeObjList(ol);

	return(0);
}

*/
