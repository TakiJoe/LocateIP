#include "ipdb.h"

/*
http://tool.17mon.cn/ipdb.html

查询更新 http://api.17mon.cn/api.php?a=ipdb
返回字符串 112.121.182.84|20140501|http://s.qdcdn.com/17mon/17monipdb.dat
*/

static uint32_t swap32(uint32_t n)
{
    return ((n<<24)|((n<<8)&0x00FF0000)|((n>>8)&0x0000FF00)|(n>>24));
}

typedef int (*compare_cb)(void *a, void *b);
static int array_unique(void *array, int length, int size, compare_cb cmp)
{
    void *begin = array;
    void *end = array + (length - 1) * size;

    while(begin<end)
    {
        void *current = begin + size;
        while(current<=end)
        {
            if(cmp(current, begin))
            {
                memmove(current, current + size, end - current);
                end-=size;
            }
            else current+= size;
        }
        begin += size;
    }

    return (end - array + size)/size;
}

typedef struct
{
    uint32_t upper;
    uint32_t offset:24;
    uint32_t length:8;
} mon17_item;


typedef char* string;
int is_equal(void *a, void *b)
{
    return strcmp(*(string*)a, *(string*)b)==0;
}

static bool mon17_iter(const ipdb *db, ipdb_item *item, uint32_t index)
{
    static char buf[256];
    if(index<db->count)
    {
        mon17_item *ptr = (mon17_item*)(db->buffer + 4 + 256*4);

        const char *text = (const char*)db->buffer + 4 + 256*4 + db->count*8 + ptr[index].offset;

        item->lower = index==0?0:(swap32(ptr[index-1].upper)+1);
        item->upper = swap32(ptr[index].upper);

        memcpy(buf, text, ptr[index].length);
        buf[ptr[index].length] = 0;

        string a = buf;
        string b = strchr(a, '\t');
        string c = strchr(b + 1, '\t');
        string d = strchr(c + 1, '\t');

        *b++ = 0;
        *c++ = 0;
        *d++ = 0;

        string list[] = {a, b, c, d};
        int count = array_unique(list, 4, sizeof(string), is_equal);
        int i = 1;
        for (; i < count; i++)
        {
            if(i==1)
                strcpy(b, list[i]);
            else
                strcat(b, list[i]);
        }

        item->zone = a;
        item->area = b;
        return true;
    }
    return false;
}

static bool mon17_find(const ipdb *db, ipdb_item *item, uint32_t ip)
{
    uint32_t *index = (uint32_t*)(db->buffer + 4);
    uint32_t offset = index[ip>>24];
    uint32_t _ip = swap32(ip);

    mon17_item *ptr = (mon17_item*)(db->buffer + 4 + 256*4);
    for(;offset<db->count;offset++)
    {
        if( memcmp(&ptr[offset].upper, &_ip, 4)>=0 )
        {
            break;
        }
    }
    return mon17_iter(db, item, offset);
}

static bool mon17_init(ipdb* db)
{
    if(db->length>=4 && sizeof(mon17_item)==8)
    {
        uint32_t *pos = (uint32_t*)db->buffer;
        uint32_t index_length = swap32(*pos);

        ipdb_item item;
        uint32_t year = 0, month = 0, day = 0;

        db->count = (index_length - 4 - 256*4 - 1024)/8;

        if(mon17_iter(db, &item, db->count-1))
        {
            if( sscanf(item.area, "%4d%2d%2d", &year, &month, &day)!=3 ) /* 17mon数据库 */
            {
                year = 1899, month = 12, day = 30; /* 未知数据库 */
            }
        }
        db->date = year*10000 + month*100 + day;
    }
    return db->count!=0;
}

const ipdb_handle mon17_handle = {mon17_init, mon17_iter, mon17_find, NULL};
