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
KSEQ_INIT(gzFile, gzread)

typedef struct {
	uint64_t id;
	uint64_t k[2];
	char *seq;
} sn_seg_t;

typedef struct {
	uint64_t pid, vid;
} sn_pair_t;

#include "ksort.h"
#define pair_key(a) ((a).pid)
KRADIX_SORT_INIT(pair, sn_pair_t, pair_key, 8)

int main(int argc, char *argv[])
{
	gzFile fp;
	kseq_t *ks;
	int c;
	long i, st;
	kvec_t(sn_seg_t) a = {0,0,0};
	kvec_t(sn_pair_t) s = {0,0,0};

	while ((c = getopt(argc, argv, "")) >= 0);

	if (optind == argc) {
		fprintf(stderr, "Usage: supernova2gfa <in.snfa>\n");
		return 1;
	}

	fp = gzopen(argv[optind], "r");
	assert(fp);
	ks = kseq_init(fp);
	while (kseq_read(ks) >= 0) {
		uint64_t k[2];
		char *p, *q = ks->comment.s;
		k[0] = UINT64_MAX, k[1] = UINT64_MAX;
		for (p = q;; ++p) {
			if (*p == 0 || isspace(*p)) {
				char *r;
				if (strncmp(q, "left=", 5) == 0) {
					k[0] = strtol(q+5, &r, 10);
				} else if (strncmp(q, "right=", 6) == 0) {
					k[1] = strtol(q+6, &r, 10);
				}
				if (*p == 0) break;
				q = p + 1;
			}
		}
		if (k[0] != UINT64_MAX && k[1] != UINT64_MAX) {
			sn_seg_t *t;
			kv_pushp(sn_seg_t, a, &t);
			t->id = strtol(ks->name.s, &p, 10);
			t->k[0] = k[0], t->k[1] = k[1];
			t->seq = strdup(ks->seq.s);
		}
	}
	kseq_destroy(ks);
	gzclose(fp);

	for (i = 0; i < a.n; ++i) {
		sn_pair_t *t;
		kv_pushp(sn_pair_t, s, &t);
		t->pid = a.a[i].k[0]<<1|0, t->vid = a.a[i].id;
		kv_pushp(sn_pair_t, s, &t);
		t->pid = a.a[i].k[1]<<1|1, t->vid = a.a[i].id;
		printf("S\t%ld\t%s\tLN:i:%ld\n", (long)a.a[i].id, a.a[i].seq, (long)strlen(a.a[i].seq));
		free(a.a[i].seq);
	}
	free(a.a);
	radix_sort_pair(s.a, s.a + s.n);
	for (i = 1, st = 0; i <= s.n; ++i) {
		if (i == s.n || s.a[i-1].pid>>1 != s.a[i].pid>>1) {
			long j, k, n0 = 0, n1 = 0;
			for (j = st; j < i; ++j)
				if (s.a[j].pid&1) break;
			n0 = j - st, n1 = (i - st) - n0;
			for (j = st + n0; j < i; ++j)
				for (k = st; k < st + n0; ++k)
					printf("L\t%ld\t+\t%ld\t+\t0M\tei:i:%ld\n", (long)s.a[j].vid, (long)s.a[k].vid, (long)s.a[st].pid>>1);
			st = i;
		}
	}
	free(s.a);
	return 0;
}
