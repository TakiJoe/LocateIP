#ifndef __CZ_UPDATE_H_
#define __CZ_UPDATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ipdb.h"

typedef struct cz_update_t cz_update;

/* 解析更新元数据，失败返回NULL */
const cz_update* parse_cz_update(const uint8_t* buffer, uint32_t length);

/* 更新元数据日期 */
uint32_t get_cz_update_date(const cz_update *ctx);

/* 解压更新数据，失败返回NULL，成功返回的buffer需要手动调用free()释放 */
uint8_t* decode_cz_update(const cz_update *ctx, uint8_t* buffer, uint32_t length, uint32_t *output);

#ifdef __cplusplus
}
#endif

#endif
