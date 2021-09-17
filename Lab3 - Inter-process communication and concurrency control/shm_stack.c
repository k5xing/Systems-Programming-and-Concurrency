/*
 * The code is derived from 
 * Copyright(c) 2018-2019 Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 License.
 */

/**
 * @brief  stack to push/pop integers.   
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shm_stack.h"

/* a stack that can hold integers */
/* Note this structure can be used by shared memory,
   since the items field points to the memory right after it.
   Hence the structure and the data items it holds are in one
   continuous chunk of memory.

   The memory layout:
   +===============+
   | size          | 4 bytes
   +---------------+
   | pos           | 4 bytes
   +---------------+
   | items         | 8 bytes
   +---------------+
   | items[0]      | 4 bytes
   +---------------+
   | items[1]      | 4 bytes
   +---------------+
   | ...           | 4 bytes
   +---------------+
   | items[size-1] | 4 bytes
   +===============+
*/
#define MAX_SIZE 1048576
#define BUF_SIZE 10240
typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

typedef struct int_stack
{
    int size;               /* the max capacity of the stack */
    int pos;                /* position of last item pushed onto the stack */
    RECV_BUF *items;             /* stack of stored integers */
} ISTACK;

/**
 * @brief calculate the total memory that the struct int_stack needs and
 *        the items[size] needs.
 * @param int size maximum number of integers the stack can hold
 * @return return the sum of ISTACK size and the size of the data that
 *         items points to.
 */

int sizeof_shm_stack(int size)
{
    return (sizeof(ISTACK) + sizeof(RECV_BUF) * size + sizeof(char)*BUF_SIZE*size);
}

/**
 * @brief initialize the ISTACK member fields.
 * @param ISTACK *p points to the starting addr. of an ISTACK struct
 * @param int stack_size max. number of items the stack can hold
 * @return 0 on success; non-zero on failure
 * NOTE:
 * The caller first calls sizeof_shm_stack() to allocate enough memory;
 * then calls the init_shm_stack to initialize the struct
 */
int init_shm_stack(ISTACK *p, int stack_size)
{
    if ( p == NULL || stack_size == 0 ) {
        return 1;
    }

    p->size = stack_size;
    p->pos  = -1;
    p->items = (RECV_BUF *) (p + sizeof(ISTACK));
    //  for(int i = 0; i < stack_size; i++){
    //         p->items[i] = (RECV_BUF *) (p + sizeof(ISTACK)+ sizeof(RECV_BUF)*i + sizeof(char) * BUF_SIZE);
    //  }
      
    //      for(int i=0;i< stack_size;i++)
    //     {
    //         (p->items)[i].buf=(char*)((p->items[i])+sizeof(RECV_BUF));
    //     }
   (p->items)->buf=(char*)(p->items+(sizeof(p->items))*stack_size);
//     for(int i=0;i< stack_size;i++)
//    {
//     (p->items)[i].buf=(char*)((p->items)+(sizeof(RECV_BUF)*i));//+sizeof(char)*BUF_SIZE);
//       }
    return 0;
}

/**
 * @brief create a stack to hold size number of integers and its associated
 *      ISTACK data structure. Put everything in one continous chunk of memory.
 * @param int size maximum number of integers the stack can hold
 * @return NULL if size is 0 or malloc fails
 */

ISTACK *create_stack(int size)
{
    int mem_size = 0;
    ISTACK *pstack = NULL;
    
    if ( size == 0 ) {
        return NULL;
    }

    mem_size = sizeof_shm_stack(size);
    pstack = malloc(mem_size);

    if ( pstack == NULL ) {
        perror("malloc");
    } else {
        char *p = (char *)pstack;
         pstack->items = (RECV_BUF *) (p + sizeof(ISTACK));
        // for(int i = 0; i < size; i++){
        //     pstack->items[i] = (RECV_BUF *) (p + sizeof(ISTACK)+ sizeof(RECV_BUF)*i + sizeof(char) * BUF_SIZE);
        // }
        // (pstack->items)->buf=(char*)(pstack->items+(sizeof(pstack->items))*size);
        // for(int i=0;i< size;i++)
        // {
        //     (pstack->items)[i].buf=(char*)((pstack->items)+sizeof(RECV_BUF)*i);
        // }
        pstack->size = size;
        pstack->pos  = -1;
    }

    return pstack;
}

/**
 * @brief release the memory
 * @param ISTACK *p the address of the ISTACK data structure
 */

void destroy_stack(ISTACK *p)
{
    if ( p != NULL ) {
        free(p);
    }
}

/**
 * @brief check if the stack is full
 * @param ISTACK *p the address of the ISTACK data structure
 * @return non-zero if the stack is full; zero otherwise
 */

int is_full(ISTACK *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == (p->size -1) );
}

/**
 * @brief check if the stack is empty 
 * @param ISTACK *p the address of the ISTACK data structure
 * @return non-zero if the stack is empty; zero otherwise
 */

int is_empty(ISTACK *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == -1 );
}

/**
 * @brief push one integer onto the stack 
 * @param ISTACK *p the address of the ISTACK data structure
 * @param int item the integer to be pushed onto the stack 
 * @return 0 on success; non-zero otherwise
 */

int push(ISTACK *p, RECV_BUF item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_full(p) ) {
        
        ++(p->pos);
        
        p->items[p->pos].max_size = item.max_size;
        p->items[p->pos].seq = item.seq;
        p->items[p->pos].size = item.size;
        memcpy(p->items->buf+(p->pos)*BUF_SIZE, item.buf, item.size);
        return 0;
    } else {
        return -1;
    }
}

/**
 * @brief push one integer onto the stack 
 * @param ISTACK *p the address of the ISTACK data structure
 * @param int *item output parameter to save the integer value 
 *        that pops off the stack 
 * @return 0 on success; non-zero otherwise
*/

int pop(ISTACK *p, RECV_BUF *p_item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_empty(p) ) {
        p_item->size = p->items[p->pos].size;
        p_item->seq = p->items[p->pos].seq;
        p_item->max_size = p->items[p->pos].max_size;

        memcpy(p_item->buf, p->items->buf+(p->pos)*BUF_SIZE, p_item->size);
        (p->pos)--;

        return 0;
    } else {
        return 1;
    }
}