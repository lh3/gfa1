#include <stdio.h>
#include <unistd.h>
#include "gfa.h"

int main_fmt(int argc, char *argv[])
{
	gfa_t *g;
	int c;

	while ((c = getopt(argc, argv, "")) >= 0) {
	}
	if (argc == optind) {
		fprintf(stderr, "Usage: gfa2 fmt [options] <in.gfa>\n");
		return 1;
	}
	g = gfa_read(argv[optind]);
	gfa_print(g, stdout);
	gfa_destroy(g);
	return 0;
}
