#include "ipdb.h"
#include "util.h"

char* ip2str(char *buf, size_t len, int ip)
{
    int i=0;
    buf[--len] = 0;

    for(i=0;i<4;i++)
    {
        int dec = ip & 0xFF;
        do
        {
            buf[--len] = (char)(dec%10 + '0');
            dec/=10;
        }while(dec!=0);

        if(i<3) buf[--len] = '.';
        ip = ip>>8;
    }
    return buf + len;
}

uint32_t str2ip(const char *lp)
{
    uint32_t ret = 0;
    uint8_t now = 0;

    while(*lp)
    {
        if('.' == *lp)
        {
            ret = 256 * ret + now;
            now = 0;
        }
        else
        {
            now = (unsigned char)(10 * now + *lp - '0');
        }

        ++lp;
    }
    ret = 256 * ret + now;

    return ret;
}


struct buffer_t
{
    uint8_t*        data;
    uint32_t        size;
    uint32_t        capacity;
};

buffer* buffer_create()
{
    return (buffer *)calloc(1, sizeof(buffer));
}

uint32_t buffer_expand(buffer *buf, uint32_t length)
{
    buf->size += length;
    if(buf->size>buf->capacity)
    {
        buf->capacity = (buf->capacity + length)*3/2;
        buf->data = (uint8_t*)realloc(buf->data, buf->capacity);
    }
    return buf->size - length;
}

uint32_t buffer_append(buffer *buf, const void* src, uint32_t length)
{
    uint32_t offset = buffer_expand(buf, length);
    memcpy(buf->data + offset, src, length);
    return offset;
}

uint8_t* buffer_get(const buffer *buf)
{
    return buf->data;
}

uint32_t buffer_size(const buffer *buf)
{
    return buf->size;
}

void buffer_release(buffer *buf)
{
    if(buf->data) free(buf->data);
    free(buf);
}


uint32_t crc32_mem(uint32_t crc32, const uint8_t* buffer, uint32_t len)
{
    static uint32_t crc32_table[256] = {0};
    uint32_t i = 0;
    if ( crc32_table[1]==0 )
    {
        for (i = 0; i < 256; i++)
        {
            uint32_t j = 0;
            crc32_table[i] = i;
            for (; j < 8; j++)
            {
                if (crc32_table[i] & 1)
                    crc32_table[i] = (crc32_table[i] >> 1) ^ 0xEDB88320;
                else
                    crc32_table[i] >>= 1;
            }
        }
    }

    crc32 = ~crc32;
    i = 0;
    for (; i < len ; i++ )
    {
        crc32 = (crc32 >> 8) ^ crc32_table[(crc32 & 0xFF) ^ buffer[i]];
    }

    return ~crc32;
}


typedef struct
{
    uint32_t        hash;
    char            str[0];
} table_key;


static uint32_t calc_hash(const char* str, size_t len, uint32_t seed)
{
    uint32_t h = (uint32_t)len ^ seed;
    size_t step = (len >> 5) + 1;
    size_t i = len;
    for (; i >= step; i -= step)
        h = h ^ ((h << 5) + (h >> 2) + (uint32_t)str[i - 1]);
    return h & (0xFFFFFFFF >> 10);
}
static const table_key* make_table_key(const char* str, size_t len, uint32_t seed)
{
    static char buffer[1 << 10];
    table_key *key = (table_key*)buffer;
    key->hash = (len << 22) | calc_hash(str, len, seed);
    memcpy(key->str, str, len * sizeof(char));
    return key;
}
static uint32_t get_name_len(const table_key* key)
{
    return key->hash >> 22;
}
static const table_node* make_node(table *t, const table_key* key)
{
    static table_node node;
    node.key = buffer_size(t->str);
    buffer_append(t->str, key, sizeof(table_key));
    buffer_append(t->str, key->str, get_name_len(key));
    node.next = 0;
    node.value = 0;
    return &node;
}
static table_node* node_position(table *t, const table_key* key)
{
    return t->head + (key->hash & (t->size - 1));
}
static bool is_empty_node(const table_node* node)
{
    return node->key == 0;
}
static bool is_same_key(const table_key* key, const table_key* cmpkey)
{
    return key->hash == cmpkey->hash && memcmp(key->str, cmpkey->str, get_name_len(cmpkey)) == 0;
}
static const table_key* get_key(table *t, const table_node* node)
{
    uint8_t *buf = buffer_get(t->str);
    return (const table_key*)&buf[node->key];
}
static table_node* get_next(const table_node* node)
{
    return node->next;
}
static table_node* get_idle_node(table *t)
{
    while (t->idle > t->head)
    {
        t->idle--;
        if (is_empty_node(t->idle)) return t->idle;
    }
    return 0;
}
static void set_node(table_node* node, const table_node* data)
{
    memcpy(node, data, sizeof(table_node));
}

table_node* table_insert(table *t, const table_node* data);
static void resize(table *t, uint32_t newsize)
{
    uint32_t i = 0;
    uint32_t oldsize = t->size;
    table_node *oldhead = t->head;
    t->size = newsize;
    t->head = (table_node*)calloc(t->size, sizeof(table_node));
    t->idle = t->head + t->size;
    for (; i < oldsize; i++)
    {
        table_node *node = oldhead + i;
        if (!is_empty_node(node))
        {
            node->next = 0;
            table_insert(t, node);
        }
    }
    free(oldhead);
}
static table_node* table_find(table *t, const table_key* key)
{
    table_node *node = node_position(t, key);
    while (!is_empty_node(node))
    {
        if (is_same_key(get_key(t, node), key)) return node;
        node = get_next(node);
        if (!node) break;
    }
    return 0;
}
table_node* table_insert(table *t, const table_node* data)
{
    table_node *node = node_position(t, get_key(t, data));
    if (!is_empty_node(node))
    {
        if (!is_same_key(get_key(t, node), get_key(t, data)))
        {
            table_node *next = get_idle_node(t);
            if (next == NULL)
            {
                resize(t, t->size << 1);
                return table_insert(t, data);
            }
            else
            {
                table_node *other = node_position(t, get_key(t, node));
                if (other != node)
                {
                    while (get_next(other) != node) other = get_next(other);
                    other->next = next;
                    set_node(next, node);
                }
                else
                {
                    while (get_next(other)) other = get_next(other);
                    other->next = next;
                    set_node(next, data);
                    return next;
                }
            }
        }
    }
    set_node(node, data);
    return node;
}

table* table_create(buffer *buf)
{
    table *t = (table *)calloc(1, sizeof(table));
    t->size = 1;
    t->head = (table_node *)calloc(t->size, sizeof(table_node));
    t->idle = t->head + t->size;
    t->seed = (uint32_t)&t->head;
    t->str = buf;
    if(!buffer_size(t->str))
    {
        buffer_append(t->str, &t->seed, sizeof(t->seed));
    }
    return t;
}

table_node* table_set_key(table *t, const char* name)
{
    const table_key *key = make_table_key(name, strlen(name), t->seed);
    table_node *node = table_find(t, key);
    if(!node) return table_insert(t, make_node(t, key));
    return node;
}

table_node* table_get_key(table *t, const char* name)
{
    const table_key *key = make_table_key(name, strlen(name), t->seed);
    return table_find(t, key);
}

void table_release(table *t)
{
    free(t->head);
    free(t);
}

void show_table_key(table *t)
{
    uint32_t i = 0;
    for(;i<t->size;i++)
    {
        table_node* node = t->head + i;
        if (!is_empty_node(node))
        {
            const table_key* key = get_key(t, node);
            printf("%.*s\n", get_name_len(key), key->str);
        }
    }
}
