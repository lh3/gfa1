#ifndef __GFA_H__
#define __GFA_H__

#include <stdio.h>
#include <stdint.h>

#define GFA_ERR_NO_SEGLEN   1

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
#define gfa_arc_len(a) ((uint32_t)(a)->v_lv)

typedef struct {
	uint32_t max, cnt;
	uint8_t *data;
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
	uint64_t m_arc, n_arc:62, is_srt:1, is_symm:1;
	gfa_arc_t *arc;
	gfa_aux_t *aux_arc;
	uint64_t *idx;
	//
	int err_flag;
} gfa_t;

#ifdef __cplusplus
extern "C" {
#endif

gfa_t *gfa_read(const char *fn);
void gfa_destroy(gfa_t *g);
void gfa_print(const gfa_t *g, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
