#include <zlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "kstring.h"
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

/********************
 * Tag manipulation *
 ********************/

int gfa_aux_parse(char *s, uint8_t **data, int *max)
{
	char *q, *p;
	kstring_t str;
	if (s == 0) return 0;
	str.l = 0, str.m = *max, str.s = (char*)*data;
	if (*s == '\t') ++s;
	for (p = q = s;; ++p) {
		if (*p == 0 || *p == '\t') {
			int c = *p;
			*p = 0;
			if (p - q >= 5 && q[2] == ':' && q[4] == ':' && (q[3] == 'A' || q[3] == 'i' || q[3] == 'f' || q[3] == 'Z' || q[3] == 'B')) {
				int type = q[3];
				kputsn_(q, 2, &str);
				q += 5;
				if (type == 'A') {
					kputc_('A', &str);
					kputc_(*q, &str);
				} else if (type == 'i') {
					int32_t x;
					x = strtol(q, &q, 10);
					kputc_(type, &str); kputsn_((char*)&x, 4, &str);
				} else if (type == 'f') {
					float x;
					x = strtod(q, &q);
					kputc_('f', &str); kputsn_(&x, 4, &str);
				} else if (type == 'Z') {
					kputc_('Z', &str); kputsn_(q, p - q + 1, &str); // note that this include the trailing NULL
				} else if (type == 'B') {
					type = *q++; // q points to the first ',' following the typing byte
					if (p - q >= 2 && (type == 'c' || type == 'C' || type == 's' || type == 'S' || type == 'i' || type == 'I' || type != 'f')) {
						int32_t n;
						char *r;
						for (r = q, n = 0; *r; ++r)
							if (*r == ',') ++n;
						kputc_('B', &str); kputc_(type, &str); kputsn_(&n, 4, &str);
						// TODO: to evaluate which is faster: a) aligned array and then memmove(); b) unaligned array; c) kputsn_()
						if (type == 'c')      while (q + 1 < p) { int8_t   x = strtol(q + 1, &q, 0); kputc_(x, &str); }
						else if (type == 'C') while (q + 1 < p) { uint8_t  x = strtol(q + 1, &q, 0); kputc_(x, &str); }
						else if (type == 's') while (q + 1 < p) { int16_t  x = strtol(q + 1, &q, 0); kputsn_(&x, 2, &str); }
						else if (type == 'S') while (q + 1 < p) { uint16_t x = strtol(q + 1, &q, 0); kputsn_(&x, 2, &str); }
						else if (type == 'i') while (q + 1 < p) { int32_t  x = strtol(q + 1, &q, 0); kputsn_(&x, 4, &str); }
						else if (type == 'I') while (q + 1 < p) { uint32_t x = strtol(q + 1, &q, 0); kputsn_(&x, 4, &str); }
						else if (type == 'f') while (q + 1 < p) { float    x = strtod(q + 1, &q);    kputsn_(&x, 4, &str); }
					}
				} // should not be here, as we have tested all types
			}
			q = p + 1;
			if (c == 0) break;
		}
	}
	if (str.s) str.s[str.l] = 0;
	*max = str.m, *data = (uint8_t*)str.s;
	return str.l;
}

int gfa_aux_format(int l_aux, const uint8_t *aux, char **t, int *max)
{
	kstring_t str;
	const uint8_t *s = aux;
	str.l = 0, str.s = *t, str.m = *max;
	while (s < aux + l_aux) {
		uint8_t type, key[2];
		key[0] = s[0]; key[1] = s[1];
		s += 2; type = *s++;
		kputc('\t', &str); kputsn((char*)key, 2, &str); kputc(':', &str);
		if (type == 'A') { kputsn("A:", 2, &str); kputc(*s, &str); ++s; }
		else if (type == 'i') { kputsn("i:", 2, &str); kputw(*(int32_t*)s, &str); s += 4; }
		else if (type == 'f') { ksprintf(&str, "f:%g", *(float*)s); s += 4; }
		else if (type == 'Z') { kputc(type, &str); kputc(':', &str); while (*s) kputc(*s++, &str); ++s; }
		else if (type == 'B') {
			uint8_t sub_type = *(s++);
			int32_t i, n;
			memcpy(&n, s, 4);
			s += 4; // no point to the start of the array
			kputsn("B:", 2, &str); kputc(sub_type, &str); // write the typing
			for (i = 0; i < n; ++i) { // FIXME: for better performance, put the loop after "if"
				kputc(',', &str);
				if ('c' == sub_type)      { kputw(*(int8_t*)s, &str); ++s; }
				else if ('C' == sub_type) { kputw(*(uint8_t*)s, &str); ++s; }
				else if ('s' == sub_type) { kputw(*(int16_t*)s, &str); s += 2; }
				else if ('S' == sub_type) { kputw(*(uint16_t*)s, &str); s += 2; }
				else if ('i' == sub_type) { kputw(*(int32_t*)s, &str); s += 4; }
				else if ('I' == sub_type) { kputuw(*(uint32_t*)s, &str); s += 4; }
				else if ('f' == sub_type) { ksprintf(&str, "%g", *(float*)s); s += 4; }
			}
		}
	}
	*t = str.s, *max = str.m;
	return str.l;
}

static inline int gfa_aux_type2size(int x)
{
	if (x == 'C' || x == 'c' || x == 'A') return 1;
	else if (x == 'S' || x == 's') return 2;
	else if (x == 'I' || x == 'i' || x == 'f') return 4;
	else return 0;
}

#define __skip_tag(s) do { \
		int type = toupper(*(s)); \
		++(s); \
		if (type == 'Z') { while (*(s)) ++(s); ++(s); } \
		else if (type == 'B') (s) += 5 + gfa_aux_type2size(*(s)) * (*(int32_t*)((s)+1)); \
		else (s) += gfa_aux_type2size(type); \
	} while(0)

uint8_t *gfa_aux_get(int l_data, const uint8_t *data, const char tag[2])
{
	const uint8_t *s = data;
	int y = tag[0]<<8 | tag[1];
	while (s < data + l_data) {
		int x = (int)s[0]<<8 | s[1];
		s += 2;
		if (x == y) return (uint8_t*)s;
		__skip_tag(s);
	}
	return 0;
}

/******************
 * Basic routines *
 ******************/

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
	uint64_t k;
	if (g == 0) return;
	kh_destroy(seg, (seghash_t*)g->h_names);
	for (i = 0; i < g->n_seg; ++i) {
		free(g->seg[i].name);
		free(g->seg[i].aux.aux);
	}
	for (k = 0; k < g->n_arc; ++k)
		free(g->arc_aux[k].aux);
	free(g->idx); free(g->seg); free(g->arc); free(g->arc_aux);
	free(g);
}

uint32_t gfa_add_seg(gfa_t *g, const char *name)
{
	khint_t k;
	int absent;
	seghash_t *h = (seghash_t*)g->h_names;
	k = kh_put(seg, h, name, &absent);
	if (absent) {
		gfa_seg_t *s;
		if (g->n_seg == g->m_seg) {
			uint32_t old_m = g->m_seg;
			g->m_seg = g->m_seg? g->m_seg<<1 : 16;
			g->seg = (gfa_seg_t*)realloc(g->seg, g->m_seg * sizeof(gfa_seg_t));
			memset(&g->seg[old_m], 0, (g->m_seg - old_m) * sizeof(gfa_seg_t));
		}
		s = &g->seg[g->n_seg++];
		kh_key(h, k) = s->name = strdup(name);
		s->del = s->len = 0;
		kh_val(h, k) = g->n_seg - 1;
	}
	return kh_val(h, k);
}

uint64_t gfa_add_arc1(gfa_t *g, uint32_t v, uint32_t w, int32_t ov, int32_t ow, int64_t link_id, int comp)
{
	gfa_arc_t *a;
	if (g->m_arc == g->n_arc) {
		uint64_t old_m = g->m_arc;
		g->m_arc = g->m_arc? g->m_arc<<1 : 16;
		g->arc = (gfa_arc_t*)realloc(g->arc, g->m_arc * sizeof(gfa_arc_t));
		memset(&g->arc[old_m], 0, (g->m_arc - old_m) * sizeof(gfa_arc_t));
		g->arc_aux = (gfa_aux_t*)realloc(g->arc_aux, g->m_arc * sizeof(gfa_aux_t));
		memset(&g->arc_aux[old_m], 0, (g->m_arc - old_m) * sizeof(gfa_aux_t));
	}
	a = &g->arc[g->n_arc++];
	a->v_lv = (uint64_t)v << 32;
	a->w = w, a->ov = ov, a->ow = ow, a->lw = 0;
	a->link_id = link_id >= 0? link_id : g->n_arc - 1;
	a->del = 0;
	a->comp = comp;
	return a->link_id;
}

void gfa_arc_sort(gfa_t *g)
{
	radix_sort_arc(g->arc, g->arc + g->n_arc);
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

/****************
 * Line parsers *
 ****************/

int gfa_parse_S(gfa_t *g, char *s)
{
	int i, is_gfa1 = 0, is_ok = 0;
	char *p, *q, *seg = 0, *seq = 0, *rest = 0;
	uint32_t sid, len = 0;
	for (i = 0, p = q = s + 2;; ++p) {
		if (*p == 0 || *p == '\t') {
			int c = *p;
			*p = 0;
			if (i == 0) {
				seg = q;
			} else if (i == 1) {
				if (isalpha(*q) || *q == '*') seq = *q == '*'? 0 : q, is_gfa1 = 1;
				else if (!isdigit(*q)) return -2;
				len = strtol(q, &q, 10);
			} else if (i == 2) {
				seq = q[0] == '*'? 0 : strdup(q);
			}
			++i, q = p + 1;
			if ((is_gfa1 && i == 2) || i == 3) {
				rest = c == 0? 0 : q, is_ok = 1;
				break;
			}
			if (c == 0) break;
		}
	}
	if (is_ok) { // all mandatory fields read
		int l_aux, m_aux = 0;
		uint8_t *aux = 0;
		gfa_seg_t *s;
		l_aux = gfa_aux_parse(rest, &aux, &m_aux); // parse optional tags
		if (is_gfa1) { // find sequence length for GFA1
			if (!seq) {
				uint8_t *s;
				s = gfa_aux_get(l_aux, aux, "LN");
				if (s && s[0] == 'i')
					len = *(int32_t*)(s+1);
			} else len = strlen(seq);
		}
		if (len == 0 || (seq && len != strlen(seq))) { // no sequence length, or different from the actual sequence length
			free(aux);
			return -3;
		}
		sid = gfa_add_seg(g, seg);
		s = &g->seg[sid];
		s->len = len, s->seq = seq;
		s->aux.m_aux = m_aux, s->aux.l_aux = l_aux, s->aux.aux = aux;
	} else return -1;
	return 0;
}

int gfa_parse_L(gfa_t *g, char *s)
{
	int i, oriv = -1, oriw = -1, is_gfa1 = 0, is_ok = 0;
	char *p, *q, *segv = 0, *segw = 0, *rest = 0;
	int32_t ov = INT32_MAX, ow = INT32_MAX;
	for (i = 0, p = q = s + 2;; ++p) {
		if (*p == 0 || *p == '\t') {
			int c = *p;
			*p = 0;
			if (i == 0) {
				segv = q;
			} else if (i == 1) {
				if (*q != '+' && *q != '-') return -2;
				oriv = (*q != '+');
			} else if (i == 2) {
				segw = q;
			} else if (i == 3) {
				if (*q != '+' && *q != '-') return -2;
				oriw = (*q != '+');
			} else if (i == 4) {
				char *r;
				if (!isdigit(*q)) return -2;
				ov = strtol(q, &r, 10);
				if (isupper(*r)) { // GFA1 L-line
					ov = ow = 0;
					do {
						long l;
						l = strtol(q, &q, 10);
						if (*q == 'M' || *q == 'D' || *q == 'N') ov += l;
						if (*q == 'M' || *q == 'I' || *q == 'S') ow += l;
						++q;
					} while (isdigit(*q));
					is_gfa1 = 1;
				}
			} else if (!is_gfa1 && i == 5) {
				if (!isdigit(*q) && *q != '*') return -2;
				ow = *q == '*'? INT32_MAX : strtol(q, &q, 10);
			}
			++i, q = p + 1;
			if ((is_gfa1 && i == 5) || i == 6) {
				rest = c == 0? 0 : q, is_ok = 1;
				break;
			}
			if (c == 0) break;
		}
	}
	if (is_ok) {
		uint32_t v, w;
		uint64_t link_id;
		int l_aux, m_aux = 0;
		uint8_t *aux = 0;
		v = gfa_add_seg(g, segv) << 1 | oriv;
		w = gfa_add_seg(g, segw) << 1 | oriw;
		link_id = gfa_add_arc1(g, v, w, ov, ow, -1, 0);
		l_aux = gfa_aux_parse(rest, &aux, &m_aux); // parse optional tags
		if (l_aux) {
			gfa_aux_t *a = &g->arc_aux[link_id];
			a->l_aux = l_aux, a->m_aux = m_aux, a->aux = aux;
		}
	} else return -1;
	return 0;
}

/********************
 * Fix graph issues *
 ********************/

uint32_t gfa_fix_no_seg(gfa_t *g)
{
	uint32_t i, n_err = 0;
	for (i = 0; i < g->n_seg; ++i) {
		gfa_seg_t *s = &g->seg[i];
		if (s->len == 0) {
			++n_err, s->del = 1;
			if (gfa_verbose >= 2)
				fprintf(stderr, "WARNING: segment '%s' is used on an L-line but not defined on an S-line\n", s->name);
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
			if (a->ow != INT32_MAX)
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
			if (!av[i].del && av[i].ow == INT32_MAX) { // overlap length is missing
				uint32_t w = av[i].w^1;
				int c, jv = -1, nw = gfa_arc_n(g, w);
				gfa_arc_t *aw = gfa_arc_a(g, w);
				for (j = 0, c = 0; j < nw; ++j)
					if (!aw[j].del && aw[j].w == (v^1)) ++c, jv = j;
				if (c != 1 || (aw[jv].ow != INT32_MAX && aw[jv].ow != av[i].ov)) { // no complement edge or multiple edges
					if (gfa_verbose >= 2)
						fprintf(stderr, "WARNING: can't infer complement overlap length for %s%c -> %s%c\n",
								g->seg[v>>1].name, "+-"[v&1], g->seg[w>>1].name, "+-"[(w^1)&1]);
					++n_err;
					av[i].del = 1;
				} else av[i].ow = aw[jv].ov;
			}
		}
	}
	return n_err;
}

uint32_t gfa_fix_symm(gfa_t *g)
{
	uint32_t n_err = 0, v, n_vtx = gfa_n_vtx(g);
	int i;
	for (v = 0; v < n_vtx; ++v) {
		int nv = gfa_arc_n(g, v);
		gfa_arc_t *av = gfa_arc_a(g, v);
		for (i = 0; i < nv; ++i) {
			int j, nw;
			gfa_arc_t *aw, *avi = &av[i];
			if (avi->del || avi->comp) continue;
			nw = gfa_arc_n(g, avi->w^1);
			aw = gfa_arc_a(g, avi->w^1);
			for (j = 0; j < nw; ++j) {
				gfa_arc_t *awj = &aw[j];
				if (awj->del || awj->comp) continue;
				if (awj->w == (v^1) && awj->ov == avi->ow && awj->ow == avi->ov) { // complement found
					awj->comp = 1;
					awj->link_id = avi->link_id;
					break;
				}
			}
			if (j == nw) gfa_add_arc1(g, avi->w^1, v^1, avi->ow, avi->ov, avi->link_id, 1);
		}
	}
	if (n_vtx < gfa_n_vtx(g)) {
		gfa_arc_sort(g);
		gfa_arc_index(g);
	}
	return n_err;
}

/****************
 * User-end I/O *
 ****************/

gfa_t *gfa_read(const char *fn)
{
	gzFile fp;
	gfa_t *g;
	kstring_t s = {0,0,0};
	kstream_t *ks;
	int dret;
	uint64_t lineno = 0;

	fp = fn && strcmp(fn, "-")? gzopen(fn, "r") : gzdopen(fileno(stdin), "r");
	if (fp == 0) return 0;
	ks = ks_init(fp);
	g = gfa_init();
	while (ks_getuntil(ks, KS_SEP_LINE, &s, &dret) >= 0) {
		int ret = 0;
		++lineno;
		if (s.l < 3 || s.s[1] != '\t') continue; // empty line
		if (s.s[0] == 'S') ret = gfa_parse_S(g, s.s);
		else if (s.s[0] == 'L') ret = gfa_parse_L(g, s.s);
		if (ret < 0 && gfa_verbose >= 1)
			fprintf(stderr, "ERROR: invalid %c-line at line %ld (error code %d)\n", s.s[0], (long)lineno, ret);
	}
	free(s.s);
	gfa_fix_no_seg(g);
	gfa_arc_sort(g);
	gfa_arc_index(g);
	gfa_fix_semi_arc(g);
	gfa_fix_symm(g);
	gfa_fix_arc_len(g);
	ks_destroy(ks);
	gzclose(fp);
	return g;
}

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
		if (s->aux.l_aux > 0) {
			char *t = 0;
			int max = 0, len;
			len = gfa_aux_format(s->aux.l_aux, s->aux.aux, &t, &max);
			fputs(t, fp);
			free(t);
		}
		fputc('\n', fp);
	}
	for (k = 0; k < g->n_arc; ++k) {
		const gfa_arc_t *a = &g->arc[k];
		if (a->del || a->comp) continue;
		fprintf(fp, "L\t%s\t%c\t%s\t%c\t%d\t%d\tL1:i:%d\tL2:i:%d", g->seg[a->v_lv>>33].name, "+-"[a->v_lv>>32&1],
				g->seg[a->w>>1].name, "+-"[a->w&1], a->ov, a->ow, gfa_arc_len(a), a->lw);
		if (g->arc_aux[a->link_id].aux) {
			char *t = 0;
			int max = 0, len;
			len = gfa_aux_format(g->arc_aux[a->link_id].l_aux, g->arc_aux[a->link_id].aux, &t, &max);
			fputs(t, fp);
			free(t);
		}
		fputc('\n', fp);
	}
}
