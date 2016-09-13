#ifndef __GFA_H__
#define __GFA_H__

#include <stdio.h>
#include <stdint.h>

#define GFA_VERSION "r17"

#define gfa_n_vtx(g) ((g)->n_seg << 1)

/*
   vertex_id = segment_id << 1 | orientation

       |<--- lv --->|<-- ov -->|
    v  ------------------------>
                    ||overlap|||
	                -------------------------->  w
                    |<-- ow -->|<---- lw ---->|
 */

typedef struct {
	uint64_t v_lv;
	uint32_t w, lw;
	int32_t ov, ow;
	uint64_t link_id:62, del:1, comp:1;
} gfa_arc_t;

#define gfa_arc_head(a) ((uint32_t)((a)->v_lv>>32))
#define gfa_arc_tail(a) ((a)->w)
#define gfa_arc_len(a) ((uint32_t)(a)->v_lv) // different from the original string graph

typedef struct {
	uint32_t m_aux, l_aux;
	uint8_t *aux;
} gfa_aux_t;

typedef struct {
	uint32_t len:31, del:1;
	char *name, *seq;
	gfa_aux_t aux;
} gfa_seg_t;

typedef struct {
	// segments
	uint32_t m_seg, n_seg;
	gfa_seg_t *seg;
	void *h_names;
	// links
	uint64_t m_arc, n_arc;
	gfa_arc_t *arc;
	gfa_aux_t *arc_aux;
	uint64_t *idx;
} gfa_t;

#ifdef __cplusplus
extern "C" {
#endif

gfa_t *gfa_read(const char *fn);
void gfa_destroy(gfa_t *g);
void gfa_print(const gfa_t *g, FILE *fp);

// Trivial to implement, but not done yet:
int32_t gfa_seg_get(const gfa_t *g, const char *seg_name); // return segment index
void gfa_seg_del(gfa_t *g, int32_t seg_id);
gfa_arc_t *gfa_arc_neighbor(const gfa_t *g, int32_t seg_id, int ori, int *n_arc);

#ifdef __cplusplus
}
#endif

#endif
