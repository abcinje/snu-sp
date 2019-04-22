/*
 * mm.c - malloc package with explicit free list
 *
 * In this approach, the free blocks are organized with explicit data
 * structure called a free list. This data structure has double link to
 * the previous and next free block in each node. They reduces the allocation
 * time because we don't need to look up the allocated blocks.
 *
 * The pointers to the previous and next blocks in free blocks are located in
 * 2nd and 3rd word in each block, respectively. Of course, each block has
 * their header and footer like the implementation of 'implicit list'.
 *
 * In this implementation, the free list keep the order of the free blocks by
 * their sizes. And if mm_free make some contiguous free blocks, they are
 * coalesced immediately so that we can avoid memory fragmentation.
 */

/**************************************************
 * [2013-11706] Kang Injae,
 * Dept. of Mechanical & Aerospace Engineering
 **************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Basic constants and macros */
#define WSIZE		4
#define DSIZE		8
#define CHUNKSIZE	(1<<12)
#define INIT_CHUNKSIZE	(1<<6)    

#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define MIN(x, y) ((x) < (y) ? (x) : (y)) 

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)		(*(unsigned int *)(p))
#define PUT(p, val)	(*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)	(GET(p) & ~0x7)
#define GET_ALLOC(p)	(GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Given block ptr bp, compute address of pointers to next and previous free blocks */
#define FREE_PREV_PTR(bp) ((char *)(bp))
#define FREE_NEXT_PTR(bp) ((char *)(bp) + WSIZE)

// Given block ptr bp, compute address of next and previous free blocks */
#define FREE_PREV(bp) (*(char **)(bp))
#define FREE_NEXT(bp) (*(char **)(FREE_NEXT_PTR(bp)))

/* free list */
void *heap_listp;
void *freelist;

/* helper functions */
static void *extend_heap(size_t size);
static void *coalesce(void *bp);
static void *place(void *ptr, size_t asize);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);

#ifdef DEBUG
/* debug function */
int mm_check(void);
#endif

/******************************
 * memory management functions
 ******************************/

/*
 * mm_init - Initialize the malloc package
 */
int mm_init(void)
{
	/* Create the initial empty heap */
	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
		return -1;
	PUT(heap_listp, 0);
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
	PUT(heap_listp + (3*WSIZE), PACK(0, 1));
	heap_listp += (2*WSIZE);

	/* Initialize the free list */
	freelist = NULL;

	/* Extend the empty heap with a free block of INIT_CHUNKSIZE bytes */
	if (!extend_heap(INIT_CHUNKSIZE/WSIZE))
		return -1;
#ifdef DEBUG
	mm_check();
#endif
	return 0;
}

/*
 * mm_malloc - Allocate a block
 */
void *mm_malloc(size_t size)
{
	size_t asize;
	size_t extendsize;
	char *bp = freelist;

	/* Ignore spurious requests */
	if (!size)
		return NULL;
    
	/* Adjust block size to include overhead and alignment reqs */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

	/* Search the free list for a fit */
	while (bp && asize > GET_SIZE(HDRP(bp)))
		bp = FREE_PREV(bp);
		
	/* No fit found. Get more memory and place the block */
	if (!bp) {
		extendsize = MAX(asize, CHUNKSIZE);
		if (!(bp = extend_heap(extendsize/WSIZE)))
			return NULL;
	}

	bp = place(bp, asize);

#ifdef DEBUG
	mm_check();
#endif
	return bp;
}

/*
 * mm_free - Free an allocated block
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));

	/* Set the header and footer with appropriate values */
	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));

	/* Insert the freed block into the free list and coalesce if needed */
	insert_node(ptr, size);
	coalesce(ptr);

#ifdef DEBUG
	mm_check();
#endif
}

/*
 * mm_realloc - Reallocate an allocated block
 */
void *mm_realloc(void *ptr, size_t size)
{
	void *newptr = ptr;
	size_t asize;
	size_t extendsize;
	int remainder;

	/* Ignore spurious requests */
	if (!size)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs */
	if (size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

	if (GET_SIZE(HDRP(ptr)) < asize) {
		/* Check if next block is a free block or the epilogue block */
		if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)))) {
			remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - asize;

			/* Extend the heap if necessary */
			if (remainder < 0) {
				extendsize = MAX(-remainder, CHUNKSIZE);
				if (!extend_heap(extendsize/WSIZE))
					return NULL;
				remainder += extendsize;
			}

			delete_node(NEXT_BLKP(ptr));
			PUT(HDRP(ptr), PACK(asize + remainder, 1)); 
			PUT(FTRP(ptr), PACK(asize + remainder, 1)); 
		}

		/* Allocate a new block */
		else {
			newptr = mm_malloc(asize - DSIZE);
			memcpy(newptr, ptr, MIN(size, asize));
			mm_free(ptr);
		}
	}

#ifdef DEBUG
	mm_check();
#endif
	return newptr;
}

/********************
 * Helper Functions
 ********************/

/*
 * extend_heap - Extend the heap
 */
static void *extend_heap(size_t words)
{
	char *bp;                   
	size_t asize;

	/* Allocate an even number of words to maintain alignment */
	asize = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(asize)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(asize, 0));
	PUT(FTRP(bp), PACK(asize, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	insert_node(bp, asize);

	/* Coalesce if the previous block was free */
	return coalesce(bp);
}

/*
 * coalesce - Coalesce contiguous blocks after extending the heap or freeing a block
 */
static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	/* Case 1: previous and next blocks are allocated */
	if (prev_alloc && next_alloc)
		return bp;

	/* Case 2: previous block is allocated and next block is free */
	else if (prev_alloc && !next_alloc) {
		delete_node(bp);
		delete_node(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	
	/* Case 3: previous block is free and next block is allocated */
	else if (!prev_alloc && next_alloc) {
		delete_node(bp);
		delete_node(PREV_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	
	/* Case 4: previous and next blocks are free */
	else {
		delete_node(bp);
		delete_node(PREV_BLKP(bp));
		delete_node(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	/* Insert the newly-formed block to the free list */
	insert_node(bp, size);
	
	return bp;
}

/*
 * place - Split a block into apropriate size
 */
static void *place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));

	/* Remove a node with starting address bp from the free list */
	delete_node(bp);

	/* Don't split and just use the block */
	if ((csize - asize) <= (2*DSIZE)) {
		PUT(HDRP(bp), PACK(csize, 1)); 
		PUT(FTRP(bp), PACK(csize, 1)); 
	}

	/* Allocate large block from the back of the free block */
	else if (asize >= 100) {
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
		insert_node(bp, csize-asize);
		return NEXT_BLKP(bp);
	}
	
	/* Allocate small block from the front of the free block */
	else {
		PUT(HDRP(bp), PACK(asize, 1)); 
		PUT(FTRP(bp), PACK(asize, 1)); 
		PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 0)); 
		PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 0)); 
		insert_node(NEXT_BLKP(bp), csize-asize);
	}

	return bp;
}

/*
 * insert_node - Insert a node into the free list 
 */
static void insert_node(void *ptr, size_t size) {
	void *prev = freelist;
	void *next = NULL;

	/* Search a location in ascending order */
	while (prev && (size > GET_SIZE(HDRP(prev)))) {
		next = prev;
		prev = FREE_PREV(prev);
	}

	if (prev) {
		/* Both of prev and next blocks exist */
		if (next) {
			PUT(FREE_PREV_PTR(ptr), (unsigned int)prev);
			PUT(FREE_NEXT_PTR(prev), (unsigned int)ptr);
			PUT(FREE_NEXT_PTR(ptr), (unsigned int)next);
			PUT(FREE_PREV_PTR(next), (unsigned int)ptr);
		}

		/* Put the node in the back of the free list */
		else {
			PUT(FREE_PREV_PTR(ptr), (unsigned int)prev);
			PUT(FREE_NEXT_PTR(prev), (unsigned int)ptr);
			PUT(FREE_NEXT_PTR(ptr), (unsigned int)NULL);
			freelist = ptr;
		}
	}

	else {
		/* Put the node in the front of the free list */
		if (next) {
			PUT(FREE_PREV_PTR(ptr), (unsigned int)NULL);
			PUT(FREE_NEXT_PTR(ptr), (unsigned int)next);
			PUT(FREE_PREV_PTR(next), (unsigned int)ptr);
		}

		/* The free list was empty */
		else {
			PUT(FREE_PREV_PTR(ptr), (unsigned int)NULL);
			PUT(FREE_NEXT_PTR(ptr), (unsigned int)NULL);
			freelist = ptr;
		}
	}
}

/*
 * delete_node - Remove a node from the free list
 */
static void delete_node(void *ptr) {
	if (FREE_PREV(ptr)) {
		/* Has both of previous and next free blocks */
		if (FREE_NEXT(ptr)) {
			PUT(FREE_NEXT_PTR(FREE_PREV(ptr)), (unsigned int)FREE_NEXT(ptr));
			PUT(FREE_PREV_PTR(FREE_NEXT(ptr)), (unsigned int)FREE_PREV(ptr));
		}

		/* Has previous free block */
		else {
			PUT(FREE_NEXT_PTR(FREE_PREV(ptr)), (unsigned int)NULL);
			freelist = FREE_PREV(ptr);
		}
	}

	else {
		/* Has next free block */
		if (FREE_NEXT(ptr))
			PUT(FREE_PREV_PTR(FREE_NEXT(ptr)), (unsigned int)NULL);
		
		/* Has no previous nor next free block */
		else
			freelist = NULL;
	}
}

#ifdef DEBUG
/*
 * mm_check - Check the heap consistency
 */
int mm_check(void)
{
	void *bp;

	/* Is every block in the free list marked as free? */
	for (bp = freelist; bp; bp = FREE_PREV(bp))
		if (GET_ALLOC(HDRP(bp)))
			goto fail;

	/* Are there any contiguous free blocks that somehow escaped coalescing? */
	for (bp = freelist; bp; bp = FREE_PREV(bp))
		if (FREE_PREV(bp) == PREV_BLKP(bp))
			goto fail;

	/* Do the pointers in a heap block point to valid heap addresses? */
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (bp < heap_listp || bp >= mem_sbrk(0))
			goto fail;
		if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp)))
			goto fail;
		if (GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp)))
			goto fail;
	}

	return 0;

fail:
	fprintf(stderr, "mm_check failed\n");
	return -1;
}
#endif
