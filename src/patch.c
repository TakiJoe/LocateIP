#include "ipdb.h"
#include "util.h"

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
    uint32_t            upper;
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
/*  string              table[0] */
} patch_head;

typedef struct
{
    const ipdb*         db;
    patch_head*         header;
    patch_item*         item;
    const char*         string;
    void *              extend;
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
                item.upper = 0;
                item.zone = 0;
                item.area = 0;
                buffer_append(record_buffer, &item, sizeof(patch_item));
            }

            for(k=0; k<j-m; k++)
            {
                db2->handle->iter(db2, &item2, m + k);
                zone_offset = buffer_size(string_buffer);
                buffer_append(string_buffer, item2.zone, strlen(item2.zone) + 1);
                area_offset = buffer_size(string_buffer);
                buffer_append(string_buffer, item2.area, strlen(item2.area) + 1);

                item.line = i - 1;
                item.method = INSERT;
                item.lower = item2.lower;
                item.upper = item2.upper;
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
            buffer_append(string_buffer, item2.zone, strlen(item2.zone) + 1);
            area_offset = buffer_size(string_buffer);
            buffer_append(string_buffer, item2.area, strlen(item2.area) + 1);

            item.line = i;
            item.method = MODIFY;
            item.lower = item2.lower;
            item.upper = item2.upper;
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
    patch_proxy *ctx = (patch_proxy *)db->extend;
    if(index<db->count)
    {
        memcpy(item, (ipdb_item *)ctx->extend + index, sizeof(ipdb_item));
        return true;
    }

    return false;
}

static bool proxy_init(ipdb* db)
{
    patch_proxy *ctx = (patch_proxy *)db->extend;
    db->count = ctx->header->count2;
    db->date = ctx->header->date2;

    ctx->extend = malloc(db->count * sizeof(ipdb_item));
    {
        int patch = ctx->header->size / sizeof(patch_item) - 1;
        int i = ctx->header->count1 - 1;
        int j = ctx->header->count2 - 1;
        ipdb_item item;
        ipdb_item *temp = (ipdb_item *)ctx->extend;
        for(;i>=0;i--)
        {
            ctx->db->handle->iter(ctx->db, &item, i);
            if(i!=ctx->item[patch].line)
            {
                memcpy(&temp[j], &item, sizeof(ipdb_item));
                j--;
            }
            else
            {
                switch(ctx->item[patch].method)
                {
                    case INSERT:
                        i++;/* don't use break */
                    case MODIFY:
                        temp[j].lower = ctx->item[patch].lower;
                        temp[j].upper = ctx->item[patch].upper;
                        temp[j].zone = (const char*)(ctx->string + ctx->item[patch].zone);
                        temp[j].area = (const char*)(ctx->string + ctx->item[patch].area);
                        j--;
                        break;
                    case REMOVE:
                        break;
                }
                patch--;
            }
        }
        if ( patch!=-1 || j!=-1 ) return false;
    }

    return db->count!=0;
}
static bool proxy_quit(ipdb* db)
{
    patch_proxy *ctx = (patch_proxy *)db->extend;
    free(ctx->extend);
    return true;
}
const ipdb_handle proxy_handle = {proxy_init, proxy_iter, NULL, proxy_quit};

ipdb* apply_patch(const ipdb *db, const uint8_t *buffer, uint32_t length)
{
    patch_head *header = (patch_head*)buffer;
    if(length<sizeof(patch_head)) return NULL;
    if(header->magic!=PATCH_MAGIC) return NULL;
    if(header->date1!=db->date) return NULL;
    if(header->count1!=db->count) return NULL;
    if(header->crc32!=crc32_mem(0, (uint8_t*)header->item, length - sizeof(patch_head))) return NULL;
    {
        patch_proxy ctx = {db, header, header->item, (const char*)buffer + sizeof(patch_head) + header->size, 0};

        return ipdb_create(&proxy_handle, NULL, 0, &ctx);
    }
}
