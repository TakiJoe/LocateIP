#ifndef __LOCATE_H_
#define __LOCATE_H_

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <stdio.h>
#include <string.h>

#ifndef uint32_t
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
#endif

#ifndef PURE
#define PURE = 0
#endif

// 数据库信息
typedef struct LocateInfo
{
    uint32_t count;             // 总条数                  例如：444382
    uint32_t time;              // 数据库更新时间          例如：20130515
} LocateInfo;

// 查询结果
typedef struct LocateItem
{
    uint32_t begin_ip;          // 开始IP          小端模式保存的IP地址
    uint32_t end_ip;            // 结束IP          同上
    const char *region;         // 区域            例如："澳大利亚"
    const char *address;        // 地址            例如："北领地Territory Technology Solutions公司"
} LocateItem;

// 把IP地址转换成字符串格式，str为16个字节
inline uint32_t ip2str(char *str, uint32_t ip)
{
    return sprintf(str, "%d.%d.%d.%d", (uint8_t)(ip>>24), (uint8_t)(ip>>16), (uint8_t)(ip>>8), (uint8_t)ip);
}

// Locate IP SDK基类
class LocateBase
{
public:
    // 是否加载成功
    bool IsAvailable()
    {
        return available;
    }

    // 另存为txt文件，导出二进制和纯真IP数据库管理工具解压出的相同
    bool Save2Text(const wchar_t *path)
    {
        FILE *fp = _wfopen(path, L"wb");
        if(fp)
        {
            char ip1[16];
            char ip2[16];
            for(uint32_t i=0; i<GetInfo()->count; i++)
            {
                LocateItem *item = GetItem(i + 1);
                ip2str(ip1, item->begin_ip);
                ip2str(ip2, item->end_ip);
                fprintf(fp, "%-16s%-16s%s%s%s\r\n", ip1, ip2, item->region, strlen(item->address)>0?" ":"", item->address);
            }

            fprintf(fp, "\r\n\r\nIP数据库共有数据 ： %d 条\r\n", GetInfo()->count);

            fclose(fp);
            return true;
        }

        return false;
    }

    // 获取基本信息
    LocateInfo* GetInfo()
    {
        return &info;
    }

    // 子类必须实现这三个接口
    virtual LocateItem* GetItem(uint32_t index) PURE;        // 根据索引查询地址 1 ~ GetInfo()->count
    virtual LocateItem* GetLocate(const char *ip) PURE;      // 根据字符串IP查询地址
    virtual LocateItem* GetLocate(uint32_t ip) PURE;         // 根据小端数字IP查询地址
protected:
    bool available;
    const uint8_t* buffer;
    LocateInfo info;
    LocateItem item;
};

#endif // __LOCATE_H_
