#ifndef __LPDB_H_
#define __LPDB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __GNUC__
    #include <stdbool.h>
#else
    #define bool int
    #define true 1
    #define false 0
#endif

#ifndef uint32_t
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
#endif

typedef struct ipdb_t ipdb;
typedef struct ipdb_iter_t ipdb_iter;
typedef struct ipdb_handle_t ipdb_handle;
typedef struct
{
    uint32_t            lower;      /* IP段开始 */
    uint32_t            upper;      /* IP段结束 */
    const char*         zone;       /* 区域1 */
    const char*         area;       /* 区域2 */
} ipdb_item;

/* 创建一个ip数据库解析引擎，失败返回NULL */
ipdb* ipdb_create(const ipdb_handle *handle, const uint8_t *buffer, uint32_t length, void *extend);

/* 不再需要时需要释放 */
void ipdb_release(ipdb *db);

/* 查找一个ip所对应的地址 */
bool ipdb_find(const ipdb *db, ipdb_item *item, const char *ip);

/* 遍历一个ip数据库 */
bool ipdb_next(ipdb_iter *iter, ipdb_item *item);

/* 导出全部内容 */
bool ipdb_dump(const ipdb *db, const char *file);

#ifdef __cplusplus
}
#endif

#endif
