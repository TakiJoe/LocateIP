#include "loci.h"

inline static uint32_t get_3b(const char *mem)
{
    return 0x00ffffff & *(uint32_t*)(mem);
}

static bool qqwry_iter(loci_iter *iter)
{
    if(iter->index<iter->ctx->count)
    {
        char *ptr = (char*)iter->ctx->buffer;
        char *p = ptr + *(uint32_t*)ptr;

        uint32_t temp = get_3b(p + 7 * iter->index + 4);

        iter->lower = *(uint32_t*)(p + 7 * iter->index);
        iter->upper = *(uint32_t*)(ptr + temp);

        char *offset = ptr + temp + 4;

        if( 0x01 == *offset )
            offset = ptr + get_3b(offset + 1);

        // zone
        if( 0x02 == *offset )
        {
            iter->zone = (const char *)( ptr + get_3b(offset + 1) );
            offset += 4;
        }
        else
        {
            iter->zone = (const char *)offset;
            offset += strlen(offset) + 1;
        }

        // area
        if( 0x02 == *offset )
            iter->area = (const char *)( ptr + get_3b(offset + 1) );
        else
            iter->area = (const char *)offset;
    }
    return false;
}

static bool qqwry_find(loci_iter *iter)
{
    return true;
}

loci* qqwry_create(const uint8_t *buffer, uint32_t length)
{
    loci* ctx = loci_create();
    ctx->buffer = buffer;
    ctx->length = length;
    ctx->iter = qqwry_iter;

    if(length>=8)
    {
        uint32_t *pos = (uint32_t*)buffer;
        uint32_t idx_first = *pos;
        uint32_t idx_last = *(pos + 1);
        ctx->count = idx_last - idx_first;
        if( (ctx->count % 7 == 0) && (length - idx_last == 7) )
        {
            ctx->count /= 7;
            ctx->count++;
        }
        else
        {
            ctx->count = 0;
        }
    }
    return ctx;
}
