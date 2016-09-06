#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfa.h"

#include "kseq.h"
KSTREAM_INIT(gzFile, gzread, 65536)

#include "khash.h"
KHASH_MAP_INIT_STR(seg, uint32_t)
typedef khash_t(seg) seghash_t;

#include "ksort.h"
#define gfa_arc_key(a) ((a).v_lv)
KRADIX_SORT_INIT(arc, gfa_arc_t, gfa_arc_key, 8)

int gfa_verbose = 2;

#define gfa_arc_n(g, v) ((uint32_t)(g)->idx[(v)])
#define gfa_arc_a(g, v) (&(g)->arc[(g)->idx[(v)]>>32])

/*****************
 * Graph parsing *
 *****************/

gfa_t *gfa_init(void)
{
	gfa_t *g;
	g = (gfa_t*)calloc(1, sizeof(gfa_t));
	g->h_names = kh_init(seg);
	return g;
}

void gfa_destroy(gfa_t *g)
{
	uint32_t i;
	if (g == 0) return;
	kh_destroy(seg, (seghash_t*)g->h_names);
	for (i = 0; i < g->n_seg; ++i)
		free(g->seg[i].name);
	free(g->seg); free(g->arc);
	free(g);
}

uint32_t gfa_add_seg(gfa_t *g, const char *name)
{
	khint_t k;
	int absent;
	seghash_t *h = (seghash_t*)g->h_names;
	k = kh_put(seg, g->h_names, name, &absent);
	if (absent) {
		gfa_seg_t *s;
		if (g->n_seg == g->m_seg) {
			g->m_seg = g->m_seg? g->m_seg<<1 : 16;
			g->seg = (gfa_seg_t*)realloc(g->seg, g->m_seg * sizeof(gfa_seg_t));
		}
		s = &g->seg[g->n_seg++];
		kh_key(h, k) = s->name = strdup(name);
		s->del = s->len = 0;
		kh_val(h, k) = g->n_seg - 1;
	}
	return kh_val(h, k);
}

uint64_t gfa_add_arc1(gfa_t *g, uint32_t v, uint32_t w, uint32_t ov, uint32_t ow, int64_t link_id)
{
	gfa_arc_t *a;
	if (g->m_arc == g->n_arc) {
		g->m_arc = g->m_arc? g->m_arc<<1 : 16;
		g->arc = (gfa_arc_t*)realloc(g->arc, g->m_arc * sizeof(gfa_arc_t));
	}
	a = &g->arc[g->n_arc++];
	a->v_lv = (uint64_t)v << 32;
	a->w = w, a->ov = ov, a->ow = ow, a->lw = 0;
	a->link_id = link_id >= 0? link_id : g->n_arc - 1;
	return a->link_id;
}

void gfa_parse_S(gfa_t *g, char *s)
{
	int i;
	char *p, *q;
	uint32_t sid;
	for (i = 0, p = q = s + 2;; ++p) {
		if (*p == 0 || *p == '\t') {
			int c = *p;
			*p = 0;
			switch (i) {
				case 0: sid = gfa_add_seg(g, q); break;
				case 1: g->seg[sid].len = strtol(q, &q, 10); break;
				case 2: g->seg[sid].seq = q[0] == '*'? 0 : strdup(q); break;
			}
			++i, q = p + 1;
			if (c == 0) break;
		}
	}
}

void gfa_parse_L(gfa_t *g, char *s)
{
	int i;
	char *p, *q;
	uint32_t v, w, ov, ow;
	for (i = 0, p = q = s + 2;; ++p) {
		if (*p == 0 || *p == '\t') {
			int c = *p;
			*p = 0;
			switch (i) {
				case 0: v = gfa_add_seg(g, q) << 1; break;
				case 1: v |= (*q != '+'); break;
				case 2: w = gfa_add_seg(g, q) << 1; break;
				case 3: w |= (*q != '+'); break;
				case 4: ov = strtol(q, &q, 10); break;
				case 5: ow = *q == '*'? UINT32_MAX : strtol(q, &q, 10); break;
			}
			++i, q = p + 1;
			if (c == 0) break;
		}
	}
	gfa_add_arc1(g, v, w, ov, ow, -1);
}

uint32_t gfa_fix_no_seg(gfa_t *g)
{
	uint32_t i, n_err = 0;
	for (i = 0; i < g->n_seg; ++i) {
		gfa_seg_t *s = &g->seg[i];
		if (s->len == 0) {
			++n_err, s->del = 1;
			if (gfa_verbose >= 2)
				fprintf(stderr, "[W::%s] segment '%s' is used on an L-line but not defined on an S-line\n", __func__, s->name);
		}
	}
	return n_err;
}

static void gfa_fix_arc_len(gfa_t *g)
{
	uint64_t k;
	for (k = 0; k < g->n_arc; ++k) {
		gfa_arc_t *a = &g->arc[k];
		uint32_t v = gfa_arc_head(a), w = gfa_arc_tail(a);
		if (g->seg[v>>1].del || g->seg[w>>1].del) {
			a->del = 1;
		} else {
			a->v_lv |= g->seg[v>>1].len - a->ov;
			if (a->ow != UINT32_MAX)
				a->lw = g->seg[w>>1].len - a->ow;
		}
	}
}

static uint32_t gfa_fix_semi_arc(gfa_t *g)
{
	uint32_t n_err = 0, v, n_vtx = gfa_n_vtx(g);
	int i, j;
	for (v = 0; v < n_vtx; ++v) {
		int nv = gfa_arc_n(g, v);
		gfa_arc_t *av = gfa_arc_a(g, v);
		for (i = 0; i < nv; ++i) {
			if (!av[i].del && av[i].ow == UINT32_MAX) { // overlap length is missing
				uint32_t w = av[i].w^1;
				int c, jv = -1, nw = gfa_arc_n(g, w);
				gfa_arc_t *aw = gfa_arc_a(g, w);
				for (j = 0, c = 0; j < nw; ++j)
					if (!aw[j].del && aw[j].w == (v^1)) ++c, jv = j;
				if (c != 1) { // no complement edge or multiple edges
					if (gfa_verbose >= 2)
						fprintf(stderr, "[W::%s] can't infer complement overlap length for %s%c -> %s%c\n",
								__func__, g->seg[v>>1].name, "+-"[v&1], g->seg[w>>1].name, "+-"[w&1]);
					++n_err;
					av[i].del = 1;
				} else if (aw[jv].ow != UINT32_MAX && aw[jv].ow != av[i].ov) {
					if (gfa_verbose >= 2)
						fprintf(stderr, "[W::%s] inconsistent overlap length between %s%c -> %s%c and its complement\n",
								__func__, g->seg[v>>1].name, "+-"[v&1], g->seg[w>>1].name, "+-"[w&1]);
					av[i].del = 1;
				} else {
					av[i].ow = aw[jv].ov;
					av[i].lw = gfa_arc_len(&aw[jv]);
				}
			}
		}
	}
	return n_err;
}

void gfa_arc_sort(gfa_t *g)
{
	radix_sort_arc(g->arc, g->arc + g->n_arc);
	g->is_srt = 1;
}

uint64_t *gfa_arc_index_core(size_t max_seq, size_t n, const gfa_arc_t *a)
{
	size_t i, last;
	uint64_t *idx;
	idx = (uint64_t*)calloc(max_seq * 2, 8);
	for (i = 1, last = 0; i <= n; ++i)
		if (i == n || gfa_arc_head(&a[i-1]) != gfa_arc_head(&a[i]))
			idx[gfa_arc_head(&a[i-1])] = (uint64_t)last<<32 | (i - last), last = i;
	return idx;
}

void gfa_arc_index(gfa_t *g)
{
	if (g->idx) free(g->idx);
	g->idx = gfa_arc_index_core(g->n_seg, g->n_arc, g->arc);
}

gfa_t *gfa_read(const char *fn)
{
	gzFile fp;
	gfa_t *g;
	kstring_t s = {0,0,0};
	kstream_t *ks;
	int dret;

	fp = fn && strcmp(fn, "-")? gzopen(fn, "r") : gzdopen(fileno(stdin), "r");
	if (fp == 0) return 0;
	ks = ks_init(fp);
	g = gfa_init();
	while (ks_getuntil(ks, KS_SEP_LINE, &s, &dret) >= 0) {
		if (s.l < 3 || s.s[1] != '\t') continue; // empty line
		if (s.s[0] == 'S') gfa_parse_S(g, s.s);
		else if (s.s[0] == 'L') gfa_parse_L(g, s.s);
	}
	gfa_fix_no_seg(g);
	gfa_fix_arc_len(g);
	gfa_arc_sort(g);
	gfa_arc_index(g);
	gfa_fix_semi_arc(g);
	ks_destroy(ks);
	gzclose(fp);
	return g;
}

/*
void gfa_fix_symm_by_add(gfa_t *g)
{
	uint64_t n_arc = 0, m_arc = 0;
	uint32_t v, n_vtx = gfa_n_vtx(g);
	gfa_arc_t *arc = 0;
	if (!g->is_srt) gfa_arc_sort(g);
	gfa_arc_index(g);
	for (v = 0; v < n_vtx; ++v) {
		int nv;
		gfa_arc_t *av;
		if (g->seg[v>>1].del) continue;
		nv = gfa_arc_n(g, v);
		av = gfa_arc_v(g, v);
		for (i = 0; i < nv; ++i) {
			gfa_arc_t *v2w = &av[i];
			uint32_t w = gfa_arc_tail(v2w);
			int nw = gfa_arc_n(g, w);
			gfa_arc_t *aw = gfa_arc_n(g, w);
			for (j = 0; j < nw; ++j) {
				gfa_arc_t *w2v;
				if (gfa_arc_tail(&aw[j]) != v) continue;
				w2v = &aw[j];
			}
		}
	}
}
*/
void gfa_print(const gfa_t *g, FILE *fp)
{
	uint32_t i;
	uint64_t k;
	for (i = 0; i < g->n_seg; ++i) {
		const gfa_seg_t *s = &g->seg[i];
		if (s->del) continue;
		fprintf(fp, "S\t%s\t%d\t", s->name, s->len);
		if (s->seq) fputs(s->seq, fp);
		else fputc('*', fp);
		fputc('\n', fp);
	}
	for (k = 0; k < g->n_arc; ++k) {
		const gfa_arc_t *a = &g->arc[k];
		if (a->del) continue;
		fprintf(fp, "L\t%s\t%c\t%s\t%c\t%d\t%d\tL1:i:%d\tL2:i:%d\n", g->seg[a->v_lv>>33].name, "+-"[a->v_lv>>32&1],
				g->seg[a->w>>1].name, "+-"[a->w&1], a->ov, a->ow, gfa_arc_len(a), a->lw);
	}
}
