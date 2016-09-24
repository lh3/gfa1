#ifndef FM_MOG_H
#define FM_MOG_H

#include <stdint.h>
#include <stdlib.h>
#include "kstring.h"

#define MAG_F_NO_AMEND   0x40

#ifndef KINT_DEF
#define KINT_DEF
typedef struct { uint64_t x, y; } ku128_t;
typedef struct { size_t n, m; uint64_t *a; } ku64_v;
typedef struct { size_t n, m; ku128_t *a; } ku128_v;
#endif

typedef struct {
	int len, nsr;    // length; number supporting reads
	uint32_t max_len;// allocated seq/cov size
	uint64_t k[2];   // bi-interval
	ku128_v nei[2];  // neighbors
	char *seq, *cov; // sequence and coverage
	void *ptr;       // additional information
} magv_t;

typedef struct { size_t n, m; magv_t *a; } magv_v;

typedef struct __mog_t {
	magv_v v;
	float rdist;  // read distance
	int min_ovlp; // minimum overlap seen from the graph
	void *h;
} mag_t;

struct mogb_aux;
typedef struct mogb_aux mogb_aux_t;

#ifdef __cplusplus
extern "C" {
#endif

	void mag_g_destroy(mag_t *g);
	mag_t *mag_g_read(const char *fn);
	void mag_g_print(const mag_t *g);

#ifdef __cplusplus
}
#endif

#endif
