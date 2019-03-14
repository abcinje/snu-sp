#include <stdlib.h>
#include <string.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

int get_callinfo(char *fname, size_t fnlen, unsigned long long *ofs)
{
	unw_context_t ctxt;
	unw_cursor_t c;

	if (unw_getcontext(&ctxt) || unw_init_local(&c, &ctxt))
		return -1;

	while (1) {
		unw_step(&c);
		unw_get_proc_name(&c, fname, fnlen, (unw_word_t *)ofs);

		if (!strlen(fname))
			return -1;

		if (!strcmp(fname, "malloc")	||
		    !strcmp(fname, "free")	||
		    !strcmp(fname, "calloc")	||
		    !strcmp(fname, "realloc"))
			break;
	}

	unw_step(&c);
	unw_get_proc_name(&c, fname, fnlen, (unw_word_t *)ofs);
	*ofs -= 5;

	return 0;
}
