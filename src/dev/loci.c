#include "loci.h"

inline static char* ip2str(char *buf, size_t len, int ip)
{
    buf[--len] = 0;

    int i=0;
    for(i=0;i<4;i++)
    {
        int dec = ip & 0xFF;
        do
        {
            buf[--len] = dec%10 + '0';
            dec/=10;
        }while(dec!=0);

        if(i<3) buf[--len] = '.';
        ip = ip>>8;
    }
    return buf + len;
    //sprintf(buf, "%d.%d.%d.%d", (uint8_t)(ip>>24), (uint8_t)(ip>>16), (uint8_t)(ip>>8), (uint8_t)ip);
}

loci* loci_create()
{
    return calloc(1, sizeof(loci));
}

bool loci_dump(loci *ctx, const char *file)
{
    FILE *fp = fopen(file, "wb");
    if(fp)
    {
        loci_iter* iter = loci_iter_create(ctx);
        while(loci_iter_next(iter))
        {
            char ip1[16];
            char ip2[16];
            fprintf(fp, "%-16s%-16s%s%s%s\r\n", ip2str(ip1, sizeof(ip1), iter->lower), ip2str(ip2, sizeof(ip2), iter->upper), iter->zone, strlen(iter->area)>0?" ":"", iter->area);
        }

        fprintf(fp, "\r\n\r\nIP数据库共有数据 ： %d 条\r\n", ctx->count);
        fclose(fp);
        return true;
    }
    return false;
}

void loci_release(loci *ctx)
{
    free(ctx);
}

loci_iter* loci_iter_create(loci *ctx)
{
    loci_iter* iter = calloc(1, sizeof(loci_iter));
    iter->ctx = ctx;
    iter->index = 0;
    return iter;
}

bool loci_iter_next(loci_iter *iter)
{
    if(iter->index<iter->ctx->count)
    {
        iter->ctx->iter(iter);
        iter->index++;
        return true;
    }
    return false;
}

void loci_iter_release(loci_iter *iter)
{
    free(iter);
}

uint32_t loci_iter_index(const loci_iter *iter)
{
    return iter->index;
}
