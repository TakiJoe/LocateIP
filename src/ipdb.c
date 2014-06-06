#include "ipdb.h"
#include "util.h"

ipdb* ipdb_create(const ipdb_handle *handle, const uint8_t *buffer, uint32_t length, void *extend)
{
    ipdb *db = (ipdb *)calloc(1, sizeof(ipdb));
    db->handle = handle;
    db->buffer = buffer;
    db->length = length;
    db->extend = extend;
    if(db->handle->init)
    {
        db->handle->init(db);
    }
    return db;
}

void ipdb_release(ipdb *db)
{
    if(db->handle->quit)
    {
        db->handle->quit(db);
    }
    free(db);
}

bool ipdb_find(const ipdb *db, ipdb_item *item, const char *ip)
{
    if(db->handle->find)
    {
        return db->handle->find(db, item, str2ip(ip));
    }
    return false;
}

bool ipdb_next(ipdb_iter *iter, ipdb_item *item)
{
    if(iter->db->handle->iter)
    {
        return iter->db->handle->iter(iter->db, item, iter->index++);
    }
    return false;
}

bool ipdb_dump(const ipdb *db, const char *file)
{
    FILE *fp = fopen(file, "wb");
    if(fp)
    {
        ipdb_iter iter = {db, 0};
        ipdb_item item;
        while(ipdb_next(&iter, &item))
        {
            char ip1[16];
            char ip2[16];

            char *ip1_t = ip2str(ip1, sizeof(ip1), item.lower);
            char *ip2_t = ip2str(ip2, sizeof(ip2), item.upper);
            fprintf(fp, "%-16s%-16s%s%s%s\r\n", ip1_t, ip2_t, item.zone, item.area[0]?" ":"", item.area);
        }

        fprintf(fp, "\r\n\r\nIP数据库共有数据 ： %d 条\r\n", db->count);
        fclose(fp);
        return true;
    }
    return false;
}
