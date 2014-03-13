#ifndef __LOCATE_READER_H_
#define __LOCATE_READER_H_

#include "Locate.h"

// location.loc文件读取类
class LocateReader : public LocateBase
{
public:
    LocateReader(const wchar_t *path);            // 打开文件
    ~LocateReader();

    bool Reload(const wchar_t *path);             // 重新载入文件

    LocateItem* GetItem(uint32_t index);          // 根据索引查询地址
    LocateItem* GetLocate(const char *ip);        // 根据字符串IP查询地址
    LocateItem* GetLocate(uint32_t ip);           // 根据小端数字IP查询地址
private:
    void Load(const wchar_t *path);
    void Clearup();
};

#endif // __LOCATE_READER_H_
