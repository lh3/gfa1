#include <math.h>
#include <zlib.h>
#include <stdio.h>
#include <assert.h>
#include "mag.h"
#include "kvec.h"
#include "kseq.h"
KSEQ_INIT(gzFile, gzread)

int fm_verbose = 3;

unsigned char seq_nt6_table[128] = {
    0, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 1, 5, 2,  5, 5, 5, 3,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  4, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 1, 5, 2,  5, 5, 5, 3,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  4, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5
};

#include "khash.h"
KHASH_INIT2(64,, khint64_t, uint64_t, 1, kh_int64_hash_func, kh_int64_hash_equal)

typedef khash_t(64) hash64_t;

static inline int kputl(long c, kstring_t *s)
{
	char buf[32];
	long l, x;
	if (c == 0) return kputc('0', s);
	for (l = 0, x = c < 0? -c : c; x > 0; x /= 10) buf[l++] = x%10 + '0';
	if (c < 0) buf[l++] = '-';
	if (s->l + l + 1 >= s->m) {
		s->m = s->l + l + 2;
		kroundup32(s->m);
		s->s = (char*)realloc(s->s, s->m);
	}
	for (x = l - 1; x >= 0; --x) s->s[s->l++] = buf[x];
	s->s[s->l] = 0;
	return 0;
}

void mag_g_build_hash(mag_t *g)
{
	long i;
	int j, ret;
	hash64_t *h;
	h = kh_init(64);
	for (i = 0; i < g->v.n; ++i) {
		const magv_t *p = &g->v.a[i];
		for (j = 0; j < 2; ++j) {
			khint_t k = kh_put(64, h, p->k[j], &ret);
			if (ret == 0) {
				if (fm_verbose >= 2)
					fprintf(stderr, "[W::%s] terminal %ld is duplicated.\n", __func__, (long)p->k[j]);
				kh_val(h, k) = (uint64_t)-1;
			} else kh_val(h, k) = i<<1|j;
		}
	}
	g->h = h;
}

void mag_v_write(const magv_t *p, kstring_t *out)
{
	int j, k;
	if (p->len <= 0) return;
	out->l = 0;
	kputc('@', out); kputl(p->k[0], out); kputc(':', out); kputl(p->k[1], out);
	kputc('\t', out); kputw(p->nsr, out);
	for (j = 0; j < 2; ++j) {
		const ku128_v *r = &p->nei[j];
		kputc('\t', out);
		for (k = 0; k < r->n; ++k) {
			kputl(r->a[k].x, out); kputc(',', out); kputw((int32_t)r->a[k].y, out);
			kputc(';', out);
		}
		if (p->nei[j].n == 0) kputc('.', out);
	}
	kputc('\n', out);
	ks_resize(out, out->l + 2 * p->len + 5);
	for (j = 0; j < p->len; ++j)
		out->s[out->l++] = "ACGT"[(int)p->seq[j] - 1];
	out->s[out->l] = 0;
	kputsn("\n+\n", 3, out);
	kputsn(p->cov, p->len, out);
	kputc('\n', out);
}

void mag_g_print(const mag_t *g)
{
	int i;
	kstring_t out;
	out.l = out.m = 0; out.s = 0;
	for (i = 0; i < g->v.n; ++i) {
		if (g->v.a[i].len < 0) continue;
		mag_v_write(&g->v.a[i], &out);
		fwrite(out.s, 1, out.l, stdout);
	}
	free(out.s);
	fflush(stdout);
}

mag_t *mag_g_read(const char *fn)
{
	gzFile fp;
	kseq_t *seq;
	ku128_v nei;
	mag_t *g;

	fp = strcmp(fn, "-")? gzopen(fn, "r") : gzdopen(fileno(stdin), "r");
	if (fp == 0) return 0;
	kv_init(nei);
	g = calloc(1, sizeof(mag_t));
	seq = kseq_init(fp);
	while (kseq_read(seq) >= 0) {
		int i, j;
		char *q;
		magv_t *p;
		kv_pushp(magv_t, g->v, &p);
		memset(p, 0, sizeof(magv_t));
		p->len = -1;
		// parse ->k[2]
		p->k[0] = strtol(seq->name.s, &q, 10); ++q;
		p->k[1] = strtol(q, &q, 10);
		// parse ->nsr
		p->nsr = strtol(seq->comment.s, &q, 10); ++q;
		// parse ->nei[2]
		for (j = 0; j < 2; ++j) {
			int max, max2;
			max = max2 = 0; // largest and 2nd largest overlaps
			nei.n = 0;
			if (*q == '.') {
				q += 2; // skip "." and "\t" (and perhaps "\0", but does not matter)
				continue;
			}
			while (isdigit(*q) || *q == '-') { // parse the neighbors
				ku128_t *r;
				kv_pushp(ku128_t, nei, &r);
				r->x = strtol(q, &q, 10); ++q;
				r->y = strtol(q, &q, 10); ++q;
				g->min_ovlp = g->min_ovlp < r->y? g->min_ovlp : r->y;
				if (max < r->y) max2 = max, max = r->y;
				else if (max2 < r->y) max2 = r->y;
			}
			++q; // skip the tailing blank
			kv_copy(ku128_t, p->nei[j], nei);
		}
		// test if to cut a tip
		p->len = seq->seq.l;
		// set ->{seq,cov,max_len}
		p->max_len = p->len + 1;
		kroundup32(p->max_len);
		p->seq = malloc(p->max_len);
		for (i = 0; i < p->len; ++i) p->seq[i] = seq_nt6_table[(int)seq->seq.s[i]];
		p->cov = malloc(p->max_len);
		if (seq->qual.l == 0)
			for (i = 0; i < seq->seq.l; ++i)
				p->cov[i] = 34;
		else strcpy(p->cov, seq->qual.s);
	}
	// free and finalize the graph
	kseq_destroy(seq);
	gzclose(fp);
	free(nei.a);
	// finalize
	mag_g_build_hash(g);
	g->rdist = 0;
	return g;
}

void mag_v_destroy(magv_t *v)
{
	free(v->nei[0].a); free(v->nei[1].a);
	free(v->seq); free(v->cov);
	memset(v, 0, sizeof(magv_t));
	v->len = -1;
}

void mag_g_destroy(mag_t *g)
{
	int i;
	kh_destroy(64, g->h);
	for (i = 0; i < g->v.n; ++i)
		mag_v_destroy(&g->v.a[i]);
	free(g->v.a);
	free(g);
}

static inline uint64_t tid2idd(hash64_t *h, uint64_t tid)
{
	khint_t k = kh_get(64, h, tid);
	assert(k != kh_end(h));
	return kh_val(h, k);
}

void mag_g_print_gfa(const mag_t *g, int no_seq)
{
	int i, j, k;
	kstring_t out = {0,0,0};

	puts("H\tVN:Z:1.0");
	for (i = 0; i < g->v.n; ++i) {
		const magv_t *p = &g->v.a[i];
		printf("S\t%lld:%lld\t", (long long)p->k[0], (long long)p->k[1]);
		if (!no_seq) {
			out.l = 0;
			ks_resize(&out, p->len + 1);
			for (j = 0; j < p->len; ++j)
				out.s[out.l++] = "ACGT"[(int)p->seq[j] - 1];
			fwrite(out.s, 1, out.l, stdout);
			printf("\tRC:i:%d\tPD:Z:", p->nsr);
			fwrite(p->cov, 1, p->len, stdout);
			putchar('\n');
		} else printf("*\tRC:i:%d\n", p->nsr);
		for (j = 0; j < 2; ++j) {
			const ku128_v *r = &p->nei[j];
			for (k = 0; k < r->n; ++k) {
				uint64_t tid = r->a[k].x, idd;
				const magv_t *q;
				if (tid < p->k[j]) continue; // skip "dual" edge
				printf("L\t%lld:%lld\t%c", (long long)p->k[0], (long long)p->k[1], "-+"[j]);
				idd = tid2idd(g->h, tid);
				q = &g->v.a[idd>>1];
				printf("\t%lld:%lld\t%c\t%dM\n", (long long)q->k[0], (long long)q->k[1], "-+"[(idd&1)^1], (int32_t)r->a[k].y);
			}
		}
	}
	free(out.s);
}

int main(int argc, char *argv[])
{
	mag_t *g;
	int c, mag_out = 0, no_seq = 0;

	while ((c = getopt(argc, argv, "mS")) >= 0) {
		if (c == 'm') mag_out = 1;
		else if (c == 'S') no_seq = 1;
	}
	if (argc == optind) {
		fprintf(stderr, "Usage: mag2gfa [-Sm] <in.mag>\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "  -m    Fermi's native MAG format\n");
		fprintf(stderr, "  -S    don't output sequence in GFA (effective w/o -m)\n");
		return 1;
	}
	g = mag_g_read(argv[optind]);
	if (mag_out) mag_g_print(g);
	else mag_g_print_gfa(g, no_seq);
	mag_g_destroy(g);
	return 0;
}
