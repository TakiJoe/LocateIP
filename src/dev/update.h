#ifndef __UPDATE_H_
#define __UPDATE_H_

#include "loci.h"

// 本文件提供对纯真IP数据库自动更新的解析以及制作增量更新


// http://update.cz88.net/ip/copywrite.rar	元数据，280字节
// http://update.cz88.net/ip/qqwry.rar		数据库，zlib压缩，其中前0x200数据被加密

typedef struct
{
	uint32_t sign;		// "CZIP"文件头
	uint32_t version;	// 日期，用作版本号
	uint32_t unknown1;	// 未知数据，似乎永远取值0x01
	uint32_t size;		// qqwry.rar大小
	uint32_t unknown2;	// 未知数据
	uint32_t key;		// 解密qqwry.rar前0x200字节密钥
	char text[128];		// 提供商
	char link[128];		// 网址
} UpdateRecord, *UpdateContext;


// 日期转换为版本号
static uint32_t Date2Version(uint32_t year, uint32_t month, uint32_t day)
{
	month = (month + 9) % 12;
	year = year - month / 10;
	day = 365 * year + year / 4 - year/100 + year/400 + (month * 153 + 2) / 5 + day - 1;
	return day;
}

// 版本号转换为日期
static void Version2Date(uint32_t version, PLoCiTime time)
{
	uint32_t y, t, m;
	y = (version * 33 + 999) / 12053;
	t = version - y * 365 - y / 4 + y / 100 - y/400;
	m = (t * 5 + 2 ) / 153 + 2;

	time->year += m / 12;
	time->month = m % 12 + 1;
	time->day = t - (m * 153 - 304) / 5 + 1;
}

// 根据日期生成版本号
uint32_t GetVersion(uint32_t year, uint32_t month, uint32_t day)
{
	return Date2Version(year, month, day) - Date2Version(1899, 12, 30);
}

// 获取更新日期
void GetDate(UpdateContext ctx, PLoCiTime time)
{
	Version2Date(ctx->version + Date2Version(1899, 12, 30), time);
}

// 解析更新数据，失败返回NULL
UpdateContext ParseMetadata(const uint8_t* buffer, uint32_t length)
{
	if(length==sizeof(UpdateRecord))
	{
		if(memcmp(buffer, "CZIP", 4)==0)
		{
			return (UpdateContext)buffer;
		}
	}

	return NULL;
}


#endif // __UPDATE_H_
