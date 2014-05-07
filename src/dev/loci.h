#ifndef __LOCATION_H_
#define __LOCATION_H_

#include <stdio.h>
#include <string.h>

// 基本结构定义

#ifndef uint32_t
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
#endif

typedef struct
{
	uint16_t		year;
	uint8_t			month;
	uint8_t			day;
} LoCiTime, *PLoCiTime;

typedef struct
{
	uint32_t			count;
	loCiTime			time;
} LoCiInfo;

typedef struct
{
	const char*			zone;
	const char*			area;
	uint32_t			lower;
	uint32_t			upper;
} LoCiItem;

typedef struct
{
	const uint8_t*		buffer;
	LoCiInfo			info;
	LoCiItem			item;
} *LoCi;

#endif // __LOCATION_H_