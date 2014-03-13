#include "Locate.h"
#include "Utility.h"
#include "QQWryReader.h"

QQWryReader::QQWryReader(const wchar_t *path)
{
    available = false;
    info.count = 0;
    info.time = 0;

    HANDLE hfile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hfile!=INVALID_HANDLE_VALUE)
    {
        uint32_t file_length = GetFileSize(hfile, NULL);

        if(file_length>8)
        {
            HANDLE hfilemap = CreateFileMapping(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
            CloseHandle(hfile);

            buffer = (const uint8_t*) MapViewOfFile(hfilemap, FILE_MAP_READ, 0, 0, 0);
            CloseHandle(hfilemap);

            uint32_t *pos = (uint32_t*)buffer;

            uint32_t idx_first = *pos;
            uint32_t idx_last = *(pos + 1);

            info.count = idx_last - idx_first;
            if( (info.count % 7 == 0) && (file_length - idx_last == 7) )
            {
                info.count /= 7;
                info.count++;
                available = true;   // 接下来要调用GetItem，因此这里提前设置为加载成功

                uint32_t y = 0, m = 0, d = 0;
                if( sscanf(GetItem(info.count)->address, "%d年%d月%d日", &y, &m, &d)!=3 ) // 纯真IP数据库
                {
                    if( sscanf(GetItem(info.count)->address, "%4d%2d%2d", &y, &m, &d)!=3 ) // 珊瑚虫IP数据库
                    {
                        y = 0, m = 0, d = 0; // 未知数据库
                    }
                }

                info.time = y*10000 + m*100 + d;
            }
            else
            {
                UnmapViewOfFile((void*)buffer);
            }
        }
        else
        {
            CloseHandle(hfile);
        }
    }
}

QQWryReader::~QQWryReader()
{
    if(available)
    {
        UnmapViewOfFile((void*)buffer);
        available = false;
    }
}

LocateItem* QQWryReader::GetLocate(const char *ip)
{
    return GetLocate( str2ip(ip) );
}

LocateItem* QQWryReader::GetLocate(uint32_t ip)
{
    if(available)
    {
        char *ptr = (char*)buffer;
        char *p = ptr + *(uint32_t*)ptr;

        uint32_t low = 0;
        uint32_t high = info.count;
        while (1)
        {
            if( low >= high - 1 )
                break;

            if( ip < *(uint32_t*)(p + (low + high)/2 * 7) )
                high = (low + high)/2;
            else
                low = (low + high)/2;
        }

        return GetItem(low + 1);
    }

    return GetItem(0);
}

inline static uint32_t get_3b(const char *mem)
{
    return 0x00ffffff & *(uint32_t*)(mem);
}

LocateItem* QQWryReader::GetItem(uint32_t index)
{
    index--;
    if(available && index>=0 && index<info.count)
    {
        char *ptr = (char*)buffer;
        char *p = ptr + *(uint32_t*)ptr;

        uint32_t temp = get_3b(p + 7 * index + 4);

        item.begin_ip = *(uint32_t*)(p + 7 * index);
        item.end_ip = *(uint32_t*)(ptr + temp);

        char *offset = ptr + temp + 4;

        if( 0x01 == *offset )
            offset = ptr + get_3b(offset + 1);

        // region
        if( 0x02 == *offset )
        {
            item.region = (const char *)( ptr + get_3b(offset + 1) );
            offset += 4;
        }
        else
        {
            item.region = (const char *)offset;
            offset += strlen(offset) + 1;
        }

        // address
        if( 0x02 == *offset )
            item.address = (const char *)( ptr + get_3b(offset + 1) );
        else
            item.address = (const char *)offset;

        return &item;
    }

    // 查询失败
    item.begin_ip = 0;
    item.end_ip = 0;
    item.region = "";
    item.address = "";
    return &item;
}

