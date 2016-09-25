#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <zlib.h>
#include "kseq.h"
KSTREAM_INIT(gzFile, gzread, 65536)

int main(int argc, char *argv[])
{
	int c, dret;
	gzFile fp;
	kstream_t *ks;
	kstring_t str = {0,0,0};

	while ((c = getopt(argc, argv, "")) >= 0);

	if (optind == argc) {
		fprintf(stderr, "Usage: falcon2gfa [options] <sg_edges_list>\n");
		return 1;
	}

	fp = gzopen(argv[optind], "r");
	assert(fp);
	ks = ks_init(fp);

	while (ks_getuntil(ks, KS_SEP_LINE, &str, &dret) >= 0) {
		char *q = str.s, *p, *s[2], *type = 0;
		int i, o[2], x[3], is_err = 0;
		float identity = -1.;
		for (i = 0, p = q;; ++p) {
			if (*p == 0 || isspace(*p)) {
				c = *p, *p = 0;
				//fprintf(stderr, "%s\n", q);
				if (i == 0 || i == 1) {
					if (p - q < 3 || (*(p-1) != 'B' && *(p-1) != 'E') || *(p-2) != ':') {
						is_err = 1;
						break;
					}
					*(p-2) = 0;
					o[i] = *(p-1) == 'E'? 0 : 1;
					s[i] = q;
				} else if (i == 2) {
					if (strcmp(q, s[1]) != 0) break;
				} else if (i >= 3 && i <= 5) x[i-3] = atoi(q);
				else if (i == 6) identity = atof(q);
				else if (i == 7) type = q;
				q = p + 1, ++i;
				if (c == 0) break;
			}
		}
		if (x[0] < x[1] && o[1]) is_err = 1;
		if (x[0] > x[1] && !o[1]) is_err = 1;
		if (is_err || type == 0) continue;
		printf("L\t%s\t%c\t%s\t%c\t", s[0], "+-"[o[0]], s[1], "+-"[o[1]]);
		if (x[0] < x[1]) printf(":%d\tL2:i:%d\n", x[0], x[1] - x[0]);
		else printf(":%d\tL2:i:%d\n", x[2], x[0]);
	}

	ks_destroy(ks);
	gzclose(fp);
	return 0;
}
