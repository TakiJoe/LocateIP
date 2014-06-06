#include "cz_update.h"

/*
本文件提供对纯真IP数据库自动更新的解析

http://update.cz88.net/ip/copywrite.rar  元数据，280字节
http://update.cz88.net/ip/qqwry.rar      数据库，zlib压缩，其中前0x200数据被加密
*/

struct cz_update_t
{
    uint32_t sign;      /* "CZIP"文件头 */
    uint32_t version;   /* 日期，用作版本号 */
    uint32_t unknown1;  /* 未知数据，似乎永远取值0x01 */
    uint32_t size;      /* qqwry.rar大小 */
    uint32_t unknown2;  /* 未知数据 */
    uint32_t key;       /* 解密qqwry.rar前0x200字节密钥 */
    char text[128];     /* 描述 */
    char link[128];     /* 网址 */
};

static uint32_t date_to_version(uint32_t year, uint32_t month, uint32_t day)
{
    month = (month + 9) % 12;
    year = year - month / 10;
    day = 365 * year + year / 4 - year/100 + year/400 + (month * 153 + 2) / 5 + day - 1;
    return day;
}

static void version_to_date(uint32_t version, uint32_t *year, uint32_t *month, uint32_t *day)
{
    uint32_t y, t, m;
    y = (version * 33 + 999) / 12053;
    t = version - y * 365 - y / 4 + y / 100 - y/400;
    m = (t * 5 + 2 ) / 153 + 2;

    *year = y + m / 12;
    *month = m % 12 + 1;
    *day = t - (m * 153 - 304) / 5 + 1;
}

const cz_update* parse_cz_update(const uint8_t* buffer, uint32_t length)
{
    if(length==sizeof(cz_update))
    {
        if(memcmp(buffer, "CZIP", 4)==0)
        {
            return (const cz_update*)buffer;
        }
    }
    return NULL;
}

uint32_t get_cz_update_date(const cz_update *ctx)
{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    version_to_date(ctx->version + date_to_version(1899, 12, 30), &year, &month, &day);
    return year * 10000 + month * 100 + day;
}

uint8_t* decode_cz_update(const cz_update *ctx, uint8_t* buffer, uint32_t length, uint32_t *output)
{
    uint32_t i = 0;
    uint32_t k = ctx->key;
    if(ctx->size<0x200 || length!=ctx->size) return NULL;
    for(;i<0x200;i++)
    {
        k *= 0x805;
        k += 1;
        k &= 0xFF;
        buffer[i] ^= k;
    }
    return stbi_zlib_decode_malloc(buffer, &length, output);
}
