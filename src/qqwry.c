#include "ipdb.h"

/* http://www.cz88.net/ */

#define INDEX_SIZE          7

static uint32_t get_3b(const char *mem)
{
    return 0x00ffffff & *(uint32_t*)(mem);
}

static bool qqwry_iter(const ipdb *db, ipdb_item *item, uint32_t index)
{
    if(index<db->count)
    {
        char *ptr = (char*)db->buffer;
        char *p = ptr + *(uint32_t*)ptr;
        uint32_t temp = get_3b(p + INDEX_SIZE * index + 4);
        char *offset = ptr + temp + 4;

        item->lower = *(uint32_t*)(p + INDEX_SIZE * index);
        item->upper = *(uint32_t*)(ptr + temp);

        if( 0x01 == *offset )
            offset = ptr + get_3b(offset + 1);

        if( 0x02 == *offset )
        {
            item->zone = (const char *)( ptr + get_3b(offset + 1) );
            offset += 4;
        }
        else
        {
            item->zone = (const char *)offset;
            offset += strlen(offset) + 1;
        }

        if( 0x02 == *offset )
            item->area = (const char *)( ptr + get_3b(offset + 1) );
        else
            item->area = (const char *)offset;
        return true;
    }
    return false;
}

static bool qqwry_find(const ipdb *db, ipdb_item *item, uint32_t ip)
{
    char *ptr = (char*)db->buffer;
    char *p = ptr + *(uint32_t*)ptr;

    uint32_t low = 0;
    uint32_t high = db->count;
    while ( low < high - 1 )
    {
        uint32_t mid = low + (high - low)/2;
        if( ip < *(uint32_t*)(p + mid * INDEX_SIZE) )
            high = mid;
        else
            low = mid;
    }
    return qqwry_iter(db, item, low);
}

static bool qqwry_init(ipdb * db)
{
    if(db->length>=8)
    {
        uint32_t *pos = (uint32_t*)db->buffer;
        uint32_t idx_first = *pos;
        uint32_t idx_last = *(pos + 1);
        db->count = idx_last - idx_first;
        if( (db->count % INDEX_SIZE == 0) && (db->length - idx_last == INDEX_SIZE) )
        {
            ipdb_item item;
            uint32_t year = 0, month = 0, day = 0;

            db->count /= INDEX_SIZE;
            db->count++;

            if(qqwry_iter(db, &item, db->count-1))
            {
                if( sscanf(item.area, "%d年%d月%d日", &year, &month, &day)!=3 ) /* 纯真IP数据库，需要本文件编码为GB2312 */
                {
                    if( sscanf(item.area, "%4d%2d%2d", &year, &month, &day)!=3 ) /* 珊瑚虫IP数据库 */
                    {
                        year = 1899, month = 12, day = 30; /* 未知数据库 */
                    }
                }
            }
            db->date = year*10000 + month*100 + day;
        }
        else
        {
            db->count = 0;
        }
    }
    return db->count!=0;
}

const ipdb_handle qqwry_handle = {qqwry_init, qqwry_iter, qqwry_find, NULL};
