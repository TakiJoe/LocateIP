#ifndef __QQWRY_READER_H_
#define __QQWRY_READER_H_

#include "Locate.h"

// QQWry.dat文件读取类
class QQWryReader : public LocateBase
{
public:
    QQWryReader(const wchar_t *path);             // 打开文件
    ~QQWryReader();

    LocateItem* GetItem(uint32_t index);          // 根据索引查询地址
    LocateItem* GetLocate(const char *ip);        // 根据字符串IP查询地址
    LocateItem* GetLocate(uint32_t ip);           // 根据小端数字IP查询地址
};

#endif // __QQWRY_READER_H_
