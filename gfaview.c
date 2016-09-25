#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include "gfa.h"

const char *tr_opts = "v:r:t:b:o:RTBOM1";

int main(int argc, char *argv[])
{
	int c;
	int gap_fuzz = 1000, max_ext = 4, bub_dist = 50000, M_only = 0;
	float ovlp_drop_ratio = .7f;
	gfa_t *g;

	while ((c = getopt(argc, argv, tr_opts)) >= 0);
	if (optind == argc) {
		fprintf(stderr, "Usage: gfaview [options] <in.gfa>\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "  -v INT      verbose level [%d]\n", gfa_verbose);
		fprintf(stderr, "  -R          transitive reduction\n");
		fprintf(stderr, "  -r INT      fuzzy length for -R [%d]\n", gap_fuzz);
		fprintf(stderr, "  -T          trim tips\n");
		fprintf(stderr, "  -t INT      tip length for -T [%d]\n", max_ext);
		fprintf(stderr, "  -B          pop bubbles\n");
		fprintf(stderr, "  -b INT      max bubble dist for -B [%d]\n", bub_dist);
		fprintf(stderr, "  -O          drop shorter overlaps\n");
		fprintf(stderr, "  -o FLOAT    dropped/longest<FLOAT, for -O [%g]\n", ovlp_drop_ratio);
		fprintf(stderr, "  -M          misc trimming\n");
		fprintf(stderr, "  -1          only output CIGAR-M operators\n");
		return 1;
	}

	g = gfa_read(argv[optind]);
	if (g == 0) {
		fprintf(stderr, "ERROR: failed to read the graph\n");
		return 2;
	}
	if (optind > 1) gfa_symm(g);
	
	optind = 1;
	while ((c = getopt(argc, argv, tr_opts)) >= 0) {
		if (c == 'v') gfa_verbose = atoi(optarg);
		else if (c == '1') M_only = 1;
		else if (c == 'r') gap_fuzz = atoi(optarg);
		else if (c == 'R') gfa_arc_del_trans(g, gap_fuzz);
		else if (c == 't') max_ext = atoi(optarg);
		else if (c == 'T') gfa_cut_tip(g, max_ext);
		else if (c == 'b') bub_dist = atoi(optarg);
		else if (c == 'B') gfa_pop_bubble(g, bub_dist);
		else if (c == 'o') ovlp_drop_ratio = atof(optarg);
		else if (c == 'O') {
			if (gfa_arc_del_short(g, ovlp_drop_ratio) != 0) {
				gfa_cut_tip(g, max_ext);
				gfa_pop_bubble(g, bub_dist);
			}
		} else if (c == 'M') {
			gfa_cut_internal(g, 1);
			gfa_cut_biloop(g, max_ext);
			gfa_cut_tip(g, max_ext);
			gfa_pop_bubble(g, bub_dist);
		}
	}

	gfa_print(g, stdout, M_only);
	return 0;
}
