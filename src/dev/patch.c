#include "ipdb.h"
#include "util.h"

bool make_patch(const ipdb *, const ipdb *);
bool apply_patch(const ipdb *, const uint8_t *, uint32_t, const char *);

//
typedef enum
{
    INSERT = 0,
    REMOVE = 1,
    MODIFY = 2
} patch_type;

typedef struct
{
    uint32_t            line:24;
    uint32_t            method:8;
    uint32_t            lower;
    uint32_t            zone;
    uint32_t            area;
} patch_item;

#define PATCH_MAGIC     0x74617000
typedef struct
{
    uint32_t            magic;
    uint32_t            count1;
    uint32_t            count2;
    uint32_t            date1;
    uint32_t            date2;

    uint32_t            size;
    uint32_t            crc32;
    patch_item          item[0];
    //string table
} patch_head;

typedef struct
{
    const ipdb*         db;
    patch_head*         header;
    patch_item*         item;
    const char*         string;
} patch_proxy;

bool make_patch(const ipdb *db1, const ipdb *db2)
{
    const ipdb *temp;
    buffer *record_buffer;
    buffer *string_buffer;

    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t k = 0;

    uint32_t n = 0;
    uint32_t m = 0;

    bool flag = false;

    ipdb_item item1;
    ipdb_item item2;

    uint32_t zone_offset;
    uint32_t area_offset;

    patch_item item;

    if(db1->date==db2->date) return false;
    if(db1->date>db2->date)
    {
        temp = db1;
        db1 = db2;
        db2 = temp;
    }
    record_buffer = buffer_create();
    string_buffer = buffer_create();

    while(i<db1->count && j<db2->count)
    {
        db1->handle->iter(db1, &item1, i);
        db2->handle->iter(db2, &item2, j);

        if(item1.lower!=item2.lower)
        {
            if(item1.lower<item2.lower) i++;
            else j++;

            flag = true;
            continue;
        }

        if(item1.upper!=item2.upper)
        {
            if(item1.upper<item2.upper) i++;
            else j++;

            flag = true;
            continue;
        }

        if(flag)
        {
            flag = false;

            for(k=0; k<i-n; k++)
            {
                item.line = n + k;
                item.method = REMOVE;
                item.lower = 0;
                item.zone = 0;
                item.area = 0;
                buffer_append(record_buffer, &item, sizeof(patch_item));
            }

            for(k=0; k<j-m; k++)
            {
                db2->handle->iter(db2, &item2, m + k);
                zone_offset = buffer_size(string_buffer);
                buffer_append(string_buffer, &item2.zone, strlen(item2.zone) + 1);
                area_offset = buffer_size(string_buffer);
                buffer_append(string_buffer, &item2.area, strlen(item2.area) + 1);

                item.line = i - 1;
                item.method = INSERT;
                item.lower = item2.lower;
                item.zone = zone_offset;
                item.area = area_offset;
                buffer_append(record_buffer, &item, sizeof(patch_item));
            }
        }

        db1->handle->iter(db1, &item1, i);
        db2->handle->iter(db2, &item2, j);
        if( strcmp(item1.zone, item2.zone) || strcmp(item1.area, item2.area) )
        {
            zone_offset = buffer_size(string_buffer);
            buffer_append(string_buffer, &item2.zone, strlen(item2.zone) + 1);
            area_offset = buffer_size(string_buffer);
            buffer_append(string_buffer, &item2.area, strlen(item2.area) + 1);

            item.line = i;
            item.method = MODIFY;
            item.lower = item2.lower;
            item.zone = zone_offset;
            item.area = area_offset;
            buffer_append(record_buffer, &item, sizeof(patch_item));
        }

        i++;
        j++;
        n = i;
        m = j;
    }

    flag = false;
    {
        char file[1024];
        FILE *fp;
        sprintf(file, "%d-%d.db", db1->date, db2->date);
        fp = fopen(file, "wb");
        if(fp)
        {
            uint32_t crc32 = 0;

            crc32 = crc32_mem(crc32, buffer_get(record_buffer), buffer_size(record_buffer));
            crc32 = crc32_mem(crc32, buffer_get(string_buffer), buffer_size(string_buffer));
            {
                patch_head header = {PATCH_MAGIC, db1->count, db2->count, db1->date, db2->date, buffer_size(record_buffer), crc32};
                fwrite(&header, sizeof(header), 1, fp);
                fwrite(buffer_get(record_buffer), buffer_size(record_buffer), 1, fp);
                fwrite(buffer_get(string_buffer), buffer_size(string_buffer), 1, fp);
            }
            fclose(fp);
            flag = true;
        }
    }

    buffer_release(record_buffer);
    buffer_release(string_buffer);
    return flag;
}

static bool proxy_iter(const ipdb *db, ipdb_item *item, uint32_t index)
{
    return false;
}

static bool proxy_init(ipdb* db)
{
    patch_proxy *ctx = (patch_proxy *)db->extend;
    db->count = ctx->header->count2;
    db->date = ctx->header->date2;
    return db->count!=0;
}
const ipdb_handle proxy_handle = {proxy_init, proxy_iter, NULL, NULL};

bool apply_patch(const ipdb *db, const uint8_t *buffer, uint32_t length, const char *file)
{
    patch_head *header = (patch_head*)buffer;
    if(length<sizeof(patch_head)) return false;
    if(header->magic!=PATCH_MAGIC) return false;
    if(header->crc32!=crc32_mem(0, (uint8_t*)header->item, length - sizeof(patch_head))) return false;
    {
        patch_proxy ctx = {db, header, header->item, (const char*)buffer + sizeof(patch_head) + header->size};

        ipdb *proxy = ipdb_create(&proxy_handle, NULL, 0, &ctx);
        //qqwry_build(proxy, file);
        ipdb_dump(proxy, "525 new.txt");

        ipdb_release(proxy);
    }
    return true;
}
