#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <zlib.h>
#include "kvec.h"
#include "kseq.h"
KSTREAM_INIT(gzFile, gzread, 65536)

#include "khash.h"
KHASH_MAP_INIT_STR(str, int)

typedef struct {
	uint32_t v, w, x[3];
} fcline_t;

typedef struct {
	char *name;
	int len;
} fcseg_t;

typedef struct {
	kvec_t(fcline_t) lines;
	kvec_t(fcseg_t) segs;
	khash_t(str) *h;
} falcon_t;

unsigned fc_add_seg(falcon_t *fc, const char *s)
{
	khint_t k;
	int absent;
	k = kh_put(str, fc->h, s, &absent);
	if (absent) {
		fcseg_t *t;
		kv_pushp(fcseg_t, fc->segs, &t);
		t->name = strdup(s);
		t->len = 0;
		kh_key(fc->h, k) = t->name;
		kh_val(fc->h, k) = kh_size(fc->h) - 1;
	}
	return kh_val(fc->h, k);
}

int main(int argc, char *argv[])
{
	int c, dret, i, no_S = 0;
	gzFile fp;
	kstream_t *ks;
	kstring_t str = {0,0,0};
	falcon_t fc;

	while ((c = getopt(argc, argv, "S")) >= 0)
		if (c == 'S') no_S = 1;

	if (optind == argc) {
		fprintf(stderr, "Usage: falcon2gfa [-S] <sg_edges_list>\n");
		return 1;
	}

	fp = strcmp(argv[optind], "-")? gzopen(argv[optind], "r") : gzdopen(fileno(stdin), "r");
	assert(fp);
	ks = ks_init(fp);
	memset(&fc, 0, sizeof(falcon_t));
	fc.h = kh_init(str);

	while (ks_getuntil(ks, KS_SEP_LINE, &str, &dret) >= 0) { // read all lines
		char *q = str.s, *p, *s[2], *type = 0;
		int i, o[2], x[3], is_err = 0;
		float identity = -1.;
		fcline_t *t;
		for (i = 0, p = q;; ++p) {
			if (*p == 0 || isspace(*p)) {
				c = *p, *p = 0;
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
		kv_pushp(fcline_t, fc.lines, &t);
		t->v = fc_add_seg(&fc, s[0]) << 1 | o[0];
		t->w = fc_add_seg(&fc, s[1]) << 1 | o[1];
		t->x[0] = x[0], t->x[1] = x[1], t->x[2] = x[2];
	}

	for (i = 0; i < fc.lines.n; ++i) { // find segment lengths (may be shorter for tips)
		fcline_t *t = &fc.lines.a[i];
		int len = t->x[0] < t->x[1]? t->x[1] : t->x[0] + t->x[2];
		if (fc.segs.a[t->w>>1].len < len)
			fc.segs.a[t->w>>1].len = len;
	}
	if (!no_S) {
		for (i = 0; i < fc.segs.n; ++i) { // print S-lines
			fcseg_t *t = &fc.segs.a[i];
			printf("S\t%s\t*\tLN:i:%d\n", t->name, t->len);
		}
	}
	for (i = 0; i < fc.lines.n; ++i) { // print L-lines
		fcline_t *t = &fc.lines.a[i];
		int o2 = t->x[0] < t->x[1]? t->x[0] : fc.segs.a[t->w>>1].len - t->x[0];
		printf("L\t%s\t%c\t%s\t%c\t:%d", fc.segs.a[t->v>>1].name, "+-"[t->v&1], fc.segs.a[t->w>>1].name, "+-"[t->w&1], o2);
		if (no_S) printf("\tL2:i:%d", fc.segs.a[t->w>>1].len - o2);
		putchar('\n');
	}
	for (i = 0; i < fc.segs.n; ++i)
		free(fc.segs.a[i].name);

	free(fc.segs.a);
	free(fc.lines.a);
	kh_destroy(str, fc.h);
	ks_destroy(ks);
	gzclose(fp);
	return 0;
}
