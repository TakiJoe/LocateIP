#include "cz_update.h"

// 日期转换为版本
static uint32_t Date2Version(uint32_t year, uint32_t month, uint32_t day)
{
    month = (month + 9) % 12;
    year = year - month / 10;
    day = 365 * year + year / 4 - year/100 + year/400 + (month * 153 + 2) / 5 + day - 1;
    return day;
}

// 版本转换为日期
static void Version2Date(uint32_t version, uint32_t *year, uint32_t *month, uint32_t *day)
{
    uint32_t y, t, m;
    y = (version * 33 + 999) / 12053;
    t = version - y * 365 - y / 4 + y / 100 - y/400;
    m = (t * 5 + 2 ) / 153 + 2;

    *year = y + m / 12;
    *month = m % 12 + 1;
    *day = t - (m * 153 - 304) / 5 + 1;
}

// 根据日期生成版本号
static uint32_t ToVersion(uint32_t year, uint32_t month, uint32_t day)
{
    return Date2Version(year, month, day) - Date2Version(1899, 12, 30);
}

// 根据版本号获取更新日期
static uint32_t ToDate(uint32_t version)
{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    Version2Date(version + Date2Version(1899, 12, 30), &year, &month, &day);
    return year * 10000 + month * 100 + day;
}

// 解析更新数据，失败返回NULL
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

uint8_t* decode_cz_update(const cz_update *ctx, uint8_t* buffer, uint32_t length, uint32_t *output)
{
    uint32_t i = 0;
    uint32_t k = ctx->key;
    if(length<0x200) return NULL;
    for(;i<0x200;i++)
    {
        k *= 0x805;
        k += 1;
        k &= 0xFF;
        buffer[i] ^= k;
    }
    return stbi_zlib_decode_malloc(buffer, &length, output);
}