#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include "gfa.h"

const char *tr_opts = "t:";

int main(int argc, char *argv[])
{
	int c;
	int gap_fuzz = 1000;
	gfa_t *g;

	while ((c = getopt(argc, argv, tr_opts)) >= 0);
	if (optind == argc) {
		fprintf(stderr, "Usage: gfaview [options] <in.gfa>\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "  -t INT      transitive reduction; fuzzy length [%d]\n", gap_fuzz);
		return 1;
	}

	g = gfa_read(argv[optind]);
	if (g == 0) {
		fprintf(stderr, "ERROR: failed to read the graph\n");
		return 2;
	}
	
	optind = 1;
	while ((c = getopt(argc, argv, tr_opts)) >= 0) {
		if (c == 't') {
			if (isdigit(*optarg)) gap_fuzz = atoi(optarg);
			gfa_arc_del_trans(g, gap_fuzz);
		}
	}

	gfa_print(g, stdout);
	return 0;
}
