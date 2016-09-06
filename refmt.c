#include <stdio.h>
#include <unistd.h>
#include "gfa.h"

int main(int argc, char *argv[])
{
	gfa_t *g;
	int c;

	while ((c = getopt(argc, argv, "")) >= 0) {
	}
	if (argc == optind) {
		fprintf(stderr, "Usage: gfa2 refmt [options] <in.gfa>\n");
		return 1;
	}
	g = gfa_read(argv[optind]);
	gfa_print(g, stdout);
	gfa_destroy(g);
	return 0;
}
