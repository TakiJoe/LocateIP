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
typedef struct loci_item_t loci_item;

struct loci_t
{
	const uint8_t*		buffer;
	uint32_t            length;
	uint32_t			count;
	uint32_t			date;
	bool                (*iter)(const loci *, loci_item *, uint32_t);
	bool                (*find)(const loci *, loci_item *, uint32_t);
};

struct loci_iter_t
{
    const loci*         ctx;
    uint32_t            index;
};

struct loci_item_t
{
	const char*			zone;
	const char*			area;
	uint32_t			lower;
	uint32_t			upper;
};

loci* loci_create();
void loci_release(loci *);

bool loci_dump(const loci *, const char *);
bool loci_iterator(loci_iter *, loci_item *);
bool loci_find(const loci *, loci_item *, const char *);

#ifdef __cplusplus
}
#endif

#endif // __LOCATION_H_
