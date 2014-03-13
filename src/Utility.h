#ifndef __UTILITY_H_
#define __UTILITY_H_

#pragma warning (disable:4200)

#include <string.h>

extern "C"
{
#include "lzma/LzmaLib.c"
#include "lzma/LzmaDec.c"
#include "lzma/LzmaEnc.c"
#include "lzma/Threads.c"
#include "lzma/LzFindMt.c"
#include "lzma/LzFind.c"
#include "lzma/Alloc.c"
}


////////////////////////////////////////////////////////////////////////////////////
// loc文件格式定义

#define LOCATE_MAGIC            0x636F6C00
#define LOCATE_VERISON          0x01

// 数据条目
typedef struct _LOCATE
{
    uint32_t begin_ip;          // 开始IP
    uint32_t table1;            // 区域偏移
    uint32_t table2;            // 地址偏移
} LOCATE, *PLOCATE;

// 文件头
typedef struct _HEADER
{
    uint32_t magic;             // 文件签名，永远为LOCATE_MAGIC
    uint16_t version;           // 生成版本
    uint16_t compress;          // 压缩方式 0未压缩 1 LZMA压缩

    uint32_t total;             // 总条数
    uint32_t time;              // 数据库更新时间

    uint32_t table1;            // 区域字符串表开始位置｛未压缩前｝
    uint32_t table2;            // 地址字符串表开始位置｛未压缩前｝
    uint32_t size;              // 数据区大小｛未压缩前｝
    uint32_t crc32;             // 前面数据的CRC32校验码
    LOCATE data[0];             // 数据区｛使用LZMA压缩｝不占用实际大小
} HEADER, *PHEADER;


////////////////////////////////////////////////////////////////////////////////////
// dif文件格式定义

#define DIFF_MAGIC              0x66696400

// 修改方法定义
typedef enum _DIFF_METHOD
{
    INSERT = 0,                 // 追加
    REMOVE = 1,                 // 删除
    MODIFY = 2                  // 修改
} DIFF_METHOD;

// 数据条目
typedef struct _DIFFITEM
{
    uint32_t line;              // 匹配行
    uint32_t method;            // 修改方法 DIFF_METHOD
    uint32_t begin_ip;          // 开始IP
    uint32_t table1;            // 区域偏移
    uint32_t table2;            // 地址偏移
} DIFFITEM, *PDIFFITEM;

// 文件头
typedef struct _DIFFHEADER
{
    uint32_t magic;             // 文件签名，永远为DIFF_MAGIC

    uint32_t total1;            // 老数据库总条数
    uint32_t total2;            // 新数据库总条数

    uint32_t time1;             // 老数据库更新时间
    uint32_t time2;             // 新数据库更新时间

    uint32_t table1;            // 区域字符串表开始位置｛未压缩前｝
    uint32_t table2;            // 地址字符串表开始位置｛未压缩前｝

    uint32_t size;              // 数据区大小｛未压缩前｝
    uint32_t crc32;             // 前面数据的CRC32校验码
    DIFFITEM data[0];           // 数据区｛使用LZMA压缩｝不占用实际大小
} DIFFHEADER, *PDIFFHEADER;


////////////////////////////////////////////////////////////////////////////////////

uint32_t str2ip(const char *lp)
{
    uint32_t ret = 0;
    uint8_t now = 0;

    while(*lp)
    {
        if('.' == *lp)
        {
            ret = 256 * ret + now;
            now = 0;
        }
        else
        {
            now = 10 * now + *lp - '0';
        }

        ++lp;
    }
    ret = 256 * ret + now;

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <vector>
using namespace std;

typedef map <string, uint32_t> StringPool;
typedef vector <uint8_t> Buffer;

// 字符串表
class StringTable
{
public:
    StringTable()
    {
        offset = 0;
    }

    uint32_t Append(const char *string)
    {
        if ( string_pool.count(string)==0 )
        {
            string_pool[string] = offset;

            size_t len = strlen(string);
            AppendBuffer((uint8_t*)string, len + 1);

            offset = offset + len + sizeof(uint8_t);
        }

        return string_pool.find(string)->second;
    }

    operator Buffer* const()
    {
        return &string_buffer;
    }
private:
    void AppendBuffer(const uint8_t *ptr, size_t len)
    {
        std::copy(ptr, ptr + len, std::back_inserter(string_buffer));
    }

    uint32_t offset;
    StringPool string_pool;
    Buffer string_buffer;
};

void AppendBuffer(Buffer &buffer, const uint8_t *ptr, size_t len)
{
    std::copy(ptr, ptr + len, std::back_inserter(buffer));
}


////////////////////////////////////////////////////////////////////////////////////

uint32_t CRC32_MEM(const uint8_t* buffer, uint32_t len)
{
    //生成CRC32的查询表
    static uint32_t CRC32Table[256] = {0};
    if ( CRC32Table[1]==0 )
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            CRC32Table[i] = i;
            for (uint32_t j = 0; j < 8; j++)
            {
                if (CRC32Table[i] & 1)
                    CRC32Table[i] = (CRC32Table[i] >> 1) ^ 0xEDB88320;
                else
                    CRC32Table[i] >>= 1;
            }
        }
    }

    //开始计算CRC32校验值
    uint32_t crc32 = ~0;
    for ( uint32_t i = 0 ; i < len ; i++ )
    {
        crc32 = (crc32 >> 8) ^ CRC32Table[(crc32 & 0xFF) ^ buffer[i]];
    }

    return ~crc32;
}

#endif // __UTILITY_H_
