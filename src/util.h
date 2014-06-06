#ifndef __UTIL_H_
#define __UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ipdb.h"

/* ip to string, string to ip */
char* ip2str(char *buf, size_t len, int ip);
uint32_t str2ip(const char *lp);

/* memory buffer */
typedef struct buffer_t buffer;

buffer* buffer_create();
uint32_t buffer_expand(buffer *buf, uint32_t length);
uint32_t buffer_append(buffer *buf, const void* src, uint32_t length);
uint8_t* buffer_get(const buffer *buf);
uint32_t buffer_size(const buffer *buf);
void buffer_release(buffer *buf);

/* crc32 */
uint32_t crc32_mem(uint32_t crc32, const uint8_t* buffer, uint32_t len);

/* hash table */
typedef struct table_t table;
typedef struct table_node_t table_node;

table* table_create(buffer *buf);
table_node* table_set_key(table *t, const char* name);
table_node* table_get_key(table *t, const char* name);
void table_release(table *t);
void show_table_key(table *t);

#ifdef __cplusplus
}
#endif

#endif
