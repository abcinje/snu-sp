#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachelab.h"

#define PATHSIZE 256

/********************
 * data structures
 ********************/

typedef struct {
	uint64_t tag;
	int valid;
	unsigned int time;
} line_t;

typedef struct {
	line_t *lines;
	unsigned int count;
} set_t;

/* global variables */
set_t *cache;
int s, E, b;
uint64_t tag_mask, idx_mask;

/* cache access */
int access(uint64_t addr)
{
	uint64_t tag = addr & tag_mask;
	uint64_t idx = (addr & idx_mask)>>b;
	set_t *set = cache + idx;
	unsigned int max = 0;
	int i, lru = 0;
	
	/* Find the data in the cache */
	for (i = 0; i < E; i++)
		if (set->lines[i].valid && set->lines[i].tag == tag) {
			set->lines[i].time = ++set->count;
			return 0; // hit
		}

	/* Find an empty line for caching */
	for (i = 0; i < E; i++)
		if (!set->lines[i].valid) {
			set->lines[i].tag = tag;
			set->lines[i].valid = 1;
			set->lines[i].time = ++set->count;
			return 1; // miss
		}

	/* Find a line to be evicted */
	for (i = 0; i < E; i++)
		if (max < (set->count) - (set->lines[i].time)) {
			max = (set->count) - (set->lines[i].time);
			lru = i;
		}

	set->lines[lru].tag = tag;
	set->lines[lru].time = ++set->count;
	return 2; // miss & evict
}

/* main routine */
int main(int argc, char *argv[])
{
	FILE *fp;
	char *tracefile;
	uint64_t addr;
	int no_optarg, c, i, size;
	int hits = 0, misses = 0, evicts = 0;
	char op;

	/* Parse the command line arguments */
	no_optarg = 0;
	tracefile = NULL;
	while ((c = getopt(argc, argv, "s:E:b:t:")) != -1) {
		switch(c) {
		case 's':
			s = atoi(optarg);
			break;
		case 'E':
			E = atoi(optarg);
			break;
		case 'b':
			b = atoi(optarg);
			break;
		case 't':
			if (strlen(optarg) >= PATHSIZE) {
				fprintf(stderr, "error: Argument too long");
				exit(1);
			}
			tracefile = calloc(PATHSIZE, sizeof(char));
			memcpy(tracefile, optarg, strlen(optarg));
			break;
		case '?':
			no_optarg = 1;
			break;
		}
	}

	/* Print the usage for invalid arguments */
	if ((!(s && E && b && tracefile)) || no_optarg) {
		fprintf(stderr, "Usage: %s -s <s> -E <E> -b <b> -t <tracefile>\n",
				argv[0]);
		exit(1);
	}

	/* Open the tracefile */
	if (!(fp = fopen(tracefile, "rt"))) {
		fprintf(stderr, "fopen error: %s\n", strerror(errno));
		exit(1);
	}

	/* Initialize the cache */
	cache = calloc(1<<s, sizeof(set_t));
	for (i = 0; i < (1<<s); i++)
		cache[i].lines = calloc(E, sizeof(line_t));

	tag_mask = (~0)<<(s + b);
	idx_mask = (~tag_mask) & ((~0)<<b);

	/* Access the cache */
	while (fscanf(fp, " %c %lx,%d", &op, &addr, &size) == 3) {
		if (op == 'I')
			continue;

		switch (access(addr)) {
		case 0:
			hits++;
			break;
		case 1:
			misses++;
			break;
		case 2:
			misses++;
			evicts++;
		}

		if (op == 'M')
			hits++;
	}

	/* Print the results */
	printSummary(hits, misses, evicts);

	/* Deinitialize the cache */
	for (i = 0; i < (1<<s); i++)
		free(cache[i].lines);
	free(cache);

	/* Close the tracefile */
	fclose(fp);

	return 0;
}

