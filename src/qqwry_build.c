#include "ipdb.h"
#include "util.h"

bool qqwry_build(const ipdb *, const char *);


typedef struct
{
    uint32_t        offset;
    table*          extend;
} table_value;

table_value* make_table_value(buffer *buf, table_node *node, uint32_t offset)
{
    table_value *node_value = (table_value *)calloc(1, sizeof(table_value));
    node_value->offset = offset;
    node_value->extend = table_create(buf);
    node->value = (uint32_t)node_value;
    return node_value;
}
void release_table_value(table *t)
{
    uint32_t i = 0;
    for(;i<t->size;i++)
    {
        table_node* node = t->head + i;
        if (!is_empty_node(node))
        {
            table_value *node_value = (table_value*)node->value;
            release_table_value(node_value->extend);
            free(node_value);
        }
    }
    table_release(t);
}

bool qqwry_build(const ipdb *ctx, const char *file)
{
    buffer *record_buffer = buffer_create();
    buffer *index_buffer = buffer_create();
    buffer *string_buffer = buffer_create();
    table *string_table = table_create(string_buffer);

    uint32_t offset = 8;

    ipdb_iter iter = {ctx, 0};
    ipdb_item item;

    table_node *zone;
    table_node *node;
    table_node *area;

    uint8_t redirect;

    uint32_t zone_len;
    uint32_t area_len;

    table_value *zone_value;
    table_value *area_value;

    uint32_t idx_first;
    uint32_t idx_last;

    FILE *fp;

    while(ipdb_next(&iter, &item))
    {
        buffer_append(record_buffer, &item.upper, sizeof(item.upper));

        offset += 4;

        zone = table_get_key(string_table, item.zone);
        if(zone)
        {
            zone_value = (table_value*)zone->value;
            area = table_get_key(zone_value->extend, item.area);
            if(area)
            {
                area_value = (table_value*)area->value;
                redirect = 0x01;
                buffer_append(record_buffer, &redirect, sizeof(redirect));
                buffer_append(record_buffer, &area_value->offset, 3);
            }
            else
            {
                node = table_set_key(zone_value->extend, item.area);
                make_table_value(string_buffer, node, offset);

                redirect = 0x02;
                buffer_append(record_buffer, &redirect, sizeof(redirect));
                buffer_append(record_buffer, &zone_value->offset, 3);

                area = table_get_key(string_table, item.area);
                area_len = strlen(item.area) + 1;
                if(area_len>2 && area )
                {
                    area_value = (table_value*)area->value;
                    buffer_append(record_buffer, &redirect, sizeof(redirect));
                    buffer_append(record_buffer, &area_value->offset, 3);
                }
                else
                {
                    if(!area)
                    {
                        area = table_set_key(string_table, item.area);
                        make_table_value(string_buffer, area, offset + 4);
                    }

                    buffer_append(record_buffer, item.area, area_len);
                }
            }
        }
        else
        {
            zone = table_set_key(string_table, item.zone);
            zone_value = make_table_value(string_buffer, zone, offset);

            zone_len = strlen(item.zone) + 1;
            buffer_append(record_buffer, item.zone, zone_len);

            node = table_set_key(zone_value->extend, item.area);
            make_table_value(string_buffer, node, offset);

            area = table_get_key(string_table, item.area);

            area_len = strlen(item.area) + 1;
            if(area_len>2 && area)
            {
                area_value = (table_value*)area->value;

                redirect = 0x02;
                buffer_append(record_buffer, &redirect, sizeof(redirect));
                buffer_append(record_buffer, &area_value->offset, 3);
            }
            else
            {
                if(!area)
                {
                    area = table_set_key(string_table, item.area);
                    make_table_value(string_buffer, area, offset + zone_len);
                }

                buffer_append(record_buffer, item.area, area_len);
            }
        }

        offset -= 4;
        buffer_append(index_buffer, &item.lower, sizeof(item.lower));
        buffer_append(index_buffer, &offset, 3);

        offset = buffer_size(record_buffer) + 8;
    }


    idx_first = offset;
    idx_last = offset + buffer_size(index_buffer) - 7;

    fp = fopen(file, "wb");
    if(fp)
    {
        fwrite(&idx_first, 1, sizeof(idx_first), fp);
        fwrite(&idx_last, 1, sizeof(idx_last), fp);

        fwrite(buffer_get(record_buffer), 1, buffer_size(record_buffer), fp);
        fwrite(buffer_get(index_buffer), 1, buffer_size(index_buffer), fp);
        fclose(fp);
    }

    release_table_value(string_table);
    buffer_release(index_buffer);
    buffer_release(record_buffer);
    buffer_release(string_buffer);
    return true;
}
