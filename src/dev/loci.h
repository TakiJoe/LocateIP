#ifndef __LOCATION_H_
#define __LOCATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

// 基本结构定义

#ifndef uint32_t
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
#endif

typedef struct loci_t loci;
typedef struct loci_iter_t loci_iter;

struct loci_t
{
	const uint8_t*		buffer;
	uint32_t            length;
	uint32_t			count;
	uint32_t			date;
	bool                (*iter)(loci_iter *);
	bool                (*find)(loci_iter *, uint32_t);
};

struct loci_iter_t
{
    loci*               ctx;
    uint32_t            index;
	const char*			zone;
	const char*			area;
	uint32_t			lower;
	uint32_t			upper;
};

loci* loci_create();
bool loci_dump(loci *, const char *);
void loci_release(loci *);

loci_iter* loci_iter_create(loci *);
bool loci_iter_next(loci_iter *);
void loci_iter_release(loci_iter *);

uint32_t loci_iter_index(const loci_iter *);

#ifdef __cplusplus
}
#endif

#endif // __LOCATION_H_
