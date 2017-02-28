/* MollenOS
*
* Copyright 2011 - 2016, Philip Meulengracht
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation ? , either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.If not, see <http://www.gnu.org/licenses/>.
*
*
* MollenOS - Generic List
* Insertion, Deletion runs at O(1) 
* Lookup by key, or search runs at O(n)
* Pop, push runs at O(1)
*/

#ifndef _GENERIC_LIST_H_
#define _GENERIC_LIST_H_

/* Includes */
#include <crtdefs.h>
#include <stdint.h>
#include <ds/ds.h>

/*******************************
 *    Data Structures          *
 *******************************/

/* The list node structure 
 * this is a generic list item
 * that holds an ident (key)
 * and data */
typedef struct _ListNode
{
	/* Key(s) */
	DataKey_t Key;
	DataKey_t SortKey;

	/* Payload */
	void *Data;

	/* Link(s) */
	struct _ListNode *Link;
	struct _ListNode *Prev;

} ListNode_t;

/* This is the list structure
 * it holds basic information
 * about the list */
typedef struct _List
{
	/* Key type */
	KeyType_t KeyType;

	/* Head and Tail */
	ListNode_t *Headp, *Tailp;

	/* Attributes */
	int Attributes;

	/* Length */
	int Length;

	/* Perhaps we use a lock */
	Spinlock_t Lock;

} List_t;

/* List Definitions */
#define LIST_NORMAL				0x0
#define LIST_SAFE				0x1

/* Sorted list? Normally 
 * the list is unsorted 
 * but supports different sorts */
#define LIST_SORT_ONINSERT		0x2
#define LIST_SORT_ONCALL		(0x4 | LIST_SORT_ONINSERT)

/* Foreach Macro(s)
 * They help keeping the code 
 * clean and readable when coding
 * loops */
#define foreach(i, List) ListNode_t *i; for (i = List->Headp; i != NULL; i = i->Link)
#define _foreach(i, List) for (i = List->Headp; i != NULL; i = i->Link)
#define _foreach_nolink(i, List) for (i = List->Headp; i != NULL; )

/*******************************
 *         Prototypes          *
 *******************************/

/* Instantiates a new list 
 * with the given attribs and keytype */
MOSAPI List_t *ListCreate(KeyType_t KeyType, int Attributes);

/* Destroys the list and 
 * frees all resources associated
 * does also free all list elements
 * and keys */
MOSAPI void ListDestroy(List_t *List);

/* Returns the length of the 
 * given list */
MOSAPI int ListLength(List_t *List);

/* Instantiates a new list node 
 * that can be appended to the list 
 * by ListAppend. If using an unsorted list
 * set the sortkey == key */
MOSAPI ListNode_t *ListCreateNode(DataKey_t Key, DataKey_t SortKey, void *Data);

/* Cleans up a list node and frees 
 * all resources it had */
MOSAPI void ListDestroyNode(List_t *List, ListNode_t *Node);

/* Insert the node into a specific position
 * in the list, if position is invalid it is
 * inserted at the back. This function is not
 * available for sorted lists, it will simply 
 * call ListInsert instead */
MOSAPI void ListInsertAt(List_t *List, ListNode_t *Node, int Position);

/* Inserts the node into the front of 
 * the list. This should be used for sorted
 * lists, but is available for unsorted lists
 * aswell */
MOSAPI void ListInsert(List_t *List, ListNode_t *Node);

/* Inserts the node into the the back
 * of the list. This function is not
 * available for sorted lists, it will
 * simply redirect to ListInsert */
MOSAPI void ListAppend(List_t *List, ListNode_t *Node);

/* List pop functions, the either 
 * remove an element from the back or 
 * the front of the given list and return
 * the node */
MOSAPI ListNode_t *ListPopFront(List_t *List);
MOSAPI ListNode_t *ListPopBack(List_t *List);

/* These are the index-retriever functions 
 * they return the given index by either 
 * Key, data or node, return -1 if not found */
MOSAPI int ListGetIndexByData(List_t *List, void *Data);
MOSAPI int ListGetIndexByKey(List_t *List, DataKey_t Key);
MOSAPI int ListGetIndexByNode(List_t *List, ListNode_t *Node);

/* These are the node-retriever functions 
 * they return the list-node by either key
 * data or index */
MOSAPI ListNode_t *ListGetNodeByKey(List_t *List, DataKey_t Key, int n);

/* These are the data-retriever functions 
 * they return the list-node by either key
 * node or index */
MOSAPI void *ListGetDataByKey(List_t *List, DataKey_t Key, int n);

/* These functions execute a given function
 * on all relevant nodes (see names) */
MOSAPI void ListExecuteOnKey(List_t *List, void(*Function)(void*, int, void*), DataKey_t Key, void *UserData);
MOSAPI void ListExecuteAll(List_t *List, void(*Function)(void*, int, void*), void *UserData);

/* This functions unlinks a node
 * and returns the next node for
 * usage */
MOSAPI ListNode_t *ListUnlinkNode(List_t *List, ListNode_t *Node);

/* These are the deletion functions 
 * and remove based on either node 
 * index or key */
MOSAPI void ListRemoveByNode(List_t *List, ListNode_t* Node);
MOSAPI void ListRemoveByIndex(List_t *List, int Index);
MOSAPI int ListRemoveByKey(List_t *List, DataKey_t Key);

#endif //!_GENERIC_LIST_H_
