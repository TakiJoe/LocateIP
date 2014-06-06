#include "ipdb.h"
#include "util.h"
#include <ctype.h>

uint32_t skip_linefeed(const uint8_t *buffer, uint32_t length)
{
    uint32_t offset = 0;
    for(;offset<length;offset++)
    {
        if(buffer[offset]!='\r' && buffer[offset]!='\n') break;
    }
    return offset;
}
uint32_t find_linefeed(const uint8_t *buffer, uint32_t length, char *buf)
{
    uint32_t offset = 0;
    for(;offset<length;offset++)
    {
        if(buffer[offset]=='\r' || buffer[offset]=='\n')
        {
            memcpy(buf, buffer, offset);
            buf[offset] = '\0';
            return offset;
        };
    }
    return 0;
}
uint32_t readline(const uint8_t *buffer, uint32_t length, char *buf)
{
    uint32_t skip = skip_linefeed(buffer, length);
    uint32_t offset = find_linefeed(buffer + skip, length - skip, buf);
    if(offset)
    {
        return offset + skip;
    }
    return 0;
}

bool is_legal_ip(const char *ip)
{
    uint8_t i = 0;
    uint32_t n = 0;
    while(*ip)
    {
        if(*ip=='.')
        {
            i++;
            n = 0;
        }
        else if(isdigit(*ip))
        {
            n = n * 10 + *ip - '0';
            if(n>255) return false;
        }
        else
        {
            //return false;
        }
        ip++;
    }
    return i==3;
}

bool split_line(char *buf, const char **lower, const char **upper, const char **zone, const char **area)
{
    uint8_t state = 1;
    char *ptr = buf;

    while(*ptr)
    {
        switch(state)
        {
        case 1:
            if(!isspace(*ptr))
            {
                *lower = ptr;
                state = 2;
            }
            break;
        case 2:
            if(isspace(*ptr))
            {
                *ptr = 0;
                state = 3;
            }
            break;
        case 3:
            if(!isspace(*ptr))
            {
                *upper = ptr;
                state = 4;
            }
            break;
        case 4:
            if(isspace(*ptr))
            {
                *ptr = 0;
                state = 5;
            }
            break;
        case 5:
            if(!isspace(*ptr))
            {
                *zone = ptr;
                state = 6;
            }
            break;
        case 6:
            if(isspace(*ptr))
            {
                *ptr = 0;
                *area = ptr + 1;
                state = 7;
            }
            break;
        }
        ptr++;
    }

    if(!is_legal_ip(*lower)) return false;
    if(!is_legal_ip(*upper)) return false;

    if(state==6)
    {
        state = 7;
        *zone = *zone - 1;
        *area = "";
    }
    return state==7;
}

static bool txtdb_iter(const ipdb *db, ipdb_item *item, uint32_t index)
{
    const buffer *record_buffer = (const buffer *)db->extend;
    const uint32_t *record = (const uint32_t *)buffer_get(record_buffer);
    if(index<db->count)
    {
        static char buf[1024];
        uint32_t offset = record[index*2];
        if(readline(db->buffer + offset, db->length - offset, buf))
        {
            const char *lower, *upper, *zone, *area;
            if(split_line(buf, &lower, &upper, &zone, &area))
            {
                item->lower = str2ip(lower);
                item->upper = str2ip(upper);
                item->zone = zone;
                item->area = area;
                return true;
            }
        }
    }
    return false;
}

static bool txtdb_find(const ipdb *db, ipdb_item *item, uint32_t ip)
{
    const buffer *record_buffer = (const buffer *)db->extend;
    const uint32_t *record = (const uint32_t *)buffer_get(record_buffer);

    uint32_t low = 0;
    uint32_t high = db->count;
    while (1)
    {
        if( low >= high - 1 )
            break;

        if( ip < record[(low + high)/2*2 + 1] )
            high = (low + high)/2;
        else
            low = (low + high)/2;
    }
    return txtdb_iter(db, item, low);
}

static bool txtdb_init(ipdb * db)
{
    char buf[1024];
    uint32_t last_offset = 0;
    uint32_t offset = 0;

    buffer *record_buffer = buffer_create();

    while((offset = readline(db->buffer + last_offset, db->length - last_offset, buf)))
    {
        const char *lower, *upper, *zone, *area;
        if(split_line(buf, &lower, &upper, &zone, &area))
        {
            uint32_t ip = str2ip(lower);
            buffer_append(record_buffer, &last_offset, sizeof(uint32_t));
            buffer_append(record_buffer, &ip, sizeof(uint32_t));
            db->count++;
        }
        last_offset += offset;
    }

    db->extend = record_buffer;
    return db->count!=0;
}

static bool txtdb_quit(ipdb* db)
{
    buffer *record_buffer = (buffer *)db->extend;
    buffer_release(record_buffer);
    return true;
}

const ipdb_handle txtdb_handle = {txtdb_init, txtdb_iter, txtdb_find, txtdb_quit};
