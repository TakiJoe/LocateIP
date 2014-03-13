#include "Locate.h"
#include "Utility.h"
#include "QQWryReader.h"
#include "LocateReader.h"
#include "LocateConverter.h"

////////////////////////////////////////////////////////////////////////////////////
// 实用函数

inline bool isPowerOf2(uint32_t x)
{
    return ((x & (x - 1)) == 0);
}

inline uint32_t LogBase2(uint32_t v)
{
    static const uint32_t MultiplyDeBruijnBitPosition2[32] =
    {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };

    return MultiplyDeBruijnBitPosition2[(uint32_t)(v * 0x077CB531U) >> 27];
}


////////////////////////////////////////////////////////////////////////////////////
// 记录表
class RecordTable 
{
public:
    void Append(PLOCATE record)
    {
        AppendBuffer( (uint8_t*)record, sizeof(LOCATE));
    }
    operator Buffer* const()
    {
        return &record_buffer;
    }
private:
    void AppendBuffer(const uint8_t *ptr, size_t len)
    {
        std::copy(ptr, ptr + len, std::back_inserter(record_buffer));
    }
    Buffer record_buffer;
};


////////////////////////////////////////////////////////////////////////////////////
// 压缩进度回调

typedef SRes (*Progress_)(void *p, UInt64 inSize, UInt64 outSize);

struct ProgressCallback
{
    Progress_ Progress;
    UInt64 totalInSize;
    OnProgress progress;
};

SRes LzmaOnProgress(void *p, UInt64 inSize, UInt64 outSize)
{
    if(p!=0)
    {
        ProgressCallback *cb = (ProgressCallback*)p;
        if(cb->progress!=0)
        {
            cb->progress(1.0 * inSize/ cb->totalInSize );
        }
    }
    return SZ_OK;
}


////////////////////////////////////////////////////////////////////////////////////
// 类实现

bool LocateConverter::QQWryToLocate(const wchar_t *qqwry, const wchar_t *locate, bool compress/* = false */, OnProgress progress/* = NULL */)
{
    QQWryReader QQWry(qqwry);
    if ( !QQWry.IsAvailable() ) return false;

    StringTable string_table1;
    StringTable string_table2;
    RecordTable record_table;

    Buffer buffer;

    uint32_t last_begin_ip = 0;
    uint32_t last_end_ip = 0;

    for(uint32_t i=0; i<QQWry.GetInfo()->count; i++)
    {
        LocateItem *item = QQWry.GetItem(i + 1);

        LOCATE record;
        record.begin_ip = item->begin_ip;
        record.table1 = string_table1.Append(item->region);
        record.table2 = string_table2.Append(item->address);

        if ( i > 0 )
        {
            if ( record.begin_ip - last_end_ip != 1 )
            {
                //printf("逻辑错误，IP段不连续！ 行:%d\n", i + 1);
                //getchar();
                return false;
            }

            uint32_t diff = record.begin_ip - last_begin_ip;
            if ( compress && isPowerOf2(diff) )
            {
                record.begin_ip = LogBase2(diff);
            }
        }


        record_table.Append(&record);

        last_begin_ip = item->begin_ip;
        last_end_ip = item->end_ip;
    }

    //合并数据区
    Buffer *record_table_buffer = record_table;
    Buffer *string_table1_buffer = string_table1;
    Buffer *string_table2_buffer = string_table2;

    std::copy(record_table_buffer->begin(), record_table_buffer->end(), std::back_inserter(buffer));
    std::copy(string_table1_buffer->begin(), string_table1_buffer->end(), std::back_inserter(buffer));
    std::copy(string_table2_buffer->begin(), string_table2_buffer->end(), std::back_inserter(buffer));

    //生成文件头
    HEADER header;
    header.magic = LOCATE_MAGIC;
    header.version = LOCATE_VERISON;
    header.compress = compress?1:0;
    header.total = QQWry.GetInfo()->count;
    header.time = QQWry.GetInfo()->time;

    header.table1 = sizeof(header) + record_table_buffer->size(); // 这里不加LZMA_PROPS_SIZE的原因是解压后，抛弃props信息
    header.table2 = header.table1 + string_table1_buffer->size();
    header.size = buffer.size();
    header.crc32 = CRC32_MEM((uint8_t*)&header, sizeof(header) - 4);

    uint32_t lzma_buffer_len = buffer.size();
    uint8_t *lzma_buffer = 0;

    size_t prop_size = LZMA_PROPS_SIZE;
    BYTE outProps[LZMA_PROPS_SIZE];

    //准备压缩
    if(compress)
    {
        lzma_buffer = (uint8_t *)malloc(lzma_buffer_len);
        
        ProgressCallback LzmaCompressProgress;
        LzmaCompressProgress.Progress = LzmaOnProgress;
        LzmaCompressProgress.totalInSize = buffer.size();
        LzmaCompressProgress.progress = progress;
        LzmaCompress(lzma_buffer, &lzma_buffer_len, &buffer[0], buffer.size(), (ICompressProgress*)&LzmaCompressProgress, outProps, &prop_size, 5, 1<<27, 8, 0, 2, 64, 4);
    }

    //保存文件
    FILE * out = _wfopen(locate, L"wb");
    fwrite(&header, 1, sizeof(header), out);

    if(compress)
    {
        fwrite(outProps, 1, sizeof(outProps), out);
        fwrite(lzma_buffer, 1, lzma_buffer_len, out);
    }
    else
    {
        fwrite(&buffer[0], 1, buffer.size(), out);
    }
    fclose(out);

    if(compress)
    {
        free(lzma_buffer);
    }
    return true;
}

typedef struct
{
    uint32_t offset;
    map<string, uint32_t> addresss;
} StrResource;

bool LocateConverter::LocateToQQWry(const wchar_t *locate, const wchar_t *qqwry)
{
    LocateReader loc(locate);
    if ( !loc.IsAvailable() ) return false;

    uint32_t idx_first = 0;
    uint32_t idx_last = 0;

    uint32_t offset = 8;

    Buffer RecordBuffer;
    Buffer IndexBuffer;

    map <string, StrResource> strings;

    for(uint32_t i=0; i<loc.GetInfo()->count; i++)
    {
        LocateItem *item = loc.GetItem(i + 1);

        AppendBuffer(RecordBuffer, (uint8_t*)&item->end_ip, sizeof(item->end_ip));
        offset += 4;

        if( strings.count(item->region)!=0 )
        {
            if( strings.find(item->region)->second.addresss.count(item->address)!=0 )
            {
                unsigned char redirect = 0x01;
                AppendBuffer(RecordBuffer, (uint8_t*)&redirect, sizeof(redirect));
                AppendBuffer(RecordBuffer, (uint8_t*)&strings.find(item->region)->second.addresss.find(item->address)->second, 3);
                //printf("%s %s ",item->region,item->address);
                //printf("%d", strings.find(item->region)->second.addresss.find(item->address)->second);
                //getchar();
            }
            else
            {
                //getchar();
                strings.find(item->region)->second.addresss[item->address] = offset;

                unsigned char redirect = 0x02;
                AppendBuffer(RecordBuffer, (uint8_t*)&redirect, sizeof(redirect));
                AppendBuffer(RecordBuffer, (uint8_t*)&strings.find(item->region)->second.offset, 3);

                if( strlen(item->address)>3 && strings.count(item->address)!=0 )
                {
                    unsigned char redirect = 0x02;
                    AppendBuffer(RecordBuffer, (uint8_t*)&redirect, sizeof(redirect));
                    AppendBuffer(RecordBuffer, (uint8_t*)&strings.find(item->address)->second.offset, 3);
                    //printf("%s %s ",item->region,item->address);
                    //printf("%X", strings.find(item->region)->second.addresss.find(item->address)->second);
                    //getchar();
                }
                else
                {
                    StrResource tr;
                    tr.offset = offset + 4;
                    strings[item->address] = tr;
                    AppendBuffer(RecordBuffer, (uint8_t*)item->address, strlen(item->address) + 1);
                }
            }
        }
        else
        {
            StrResource rec;
            rec.offset = offset;

            uint32_t len = strlen(item->region) + 1;
            AppendBuffer(RecordBuffer, (uint8_t*)item->region, len);

            rec.addresss[item->address] = rec.offset;

            if( strlen(item->address)>3 && strings.count(item->address)!=0 )
            {
                unsigned char redirect = 0x02;
                AppendBuffer(RecordBuffer, (uint8_t*)&redirect, sizeof(redirect));
                AppendBuffer(RecordBuffer, (uint8_t*)&strings.find(item->address)->second.offset, 3);
                //printf("1 %s %s %d %d\n",item->region,item->address, strings.find(item->address)->second.offset,rec.offset);
            }
            else
            {
                StrResource tr;
                tr.offset = offset + len;
                strings[item->address] = tr;
                //printf("2 %s %s %d %d\n",item->region,item->address,tr.offset,rec.offset);
                AppendBuffer(RecordBuffer, (uint8_t*)item->address, strlen(item->address) + 1);
            }

            strings[item->region] = rec;
        }

        offset -= 4;
        AppendBuffer(IndexBuffer, (uint8_t*)&item->begin_ip, sizeof(item->begin_ip));
        AppendBuffer(IndexBuffer, (uint8_t*)&offset, 3);

        offset = RecordBuffer.size() + 8;
    }

    idx_first = offset;
    idx_last = offset + IndexBuffer.size() - 7;

    FILE * out = _wfopen(qqwry, L"wb");
    fwrite(&idx_first, 1, sizeof(idx_first), out);
    fwrite(&idx_last, 1, sizeof(idx_last), out);

    fwrite(&RecordBuffer[0], 1, RecordBuffer.size(), out);
    fwrite(&IndexBuffer[0], 1, IndexBuffer.size(), out);
    fclose(out);

    return true;
}

bool LocateConverter::LocateUpdate(const wchar_t *locate, const wchar_t *patch, const wchar_t *path, bool compress/* = false */, OnProgress progress/* = NULL */)
{
    LocateReader loc(locate);
    if ( !loc.IsAvailable() ) return false;

    const uint8_t *diff_buffer = 0;
    PDIFFHEADER diff_header = 0;

    HANDLE hfile = CreateFileW(patch, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hfile!=INVALID_HANDLE_VALUE)
    {
        uint32_t file_length = GetFileSize(hfile, NULL);

        HANDLE hfilemap = CreateFileMapping(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
        CloseHandle(hfile);

        diff_buffer = (const uint8_t*) MapViewOfFile(hfilemap, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(hfilemap);

        diff_header = (PDIFFHEADER)diff_buffer;
        int data_size = file_length - sizeof(DIFFHEADER) - LZMA_PROPS_SIZE;
        if( data_size<0 || diff_header->magic != DIFF_MAGIC || CRC32_MEM(diff_buffer, sizeof(DIFFHEADER) - 4)!=diff_header->crc32 )
        {
            UnmapViewOfFile((void*)diff_buffer);
            return false;
        }
        else
        {
            uint32_t lzma_buffer_len = diff_header->size;
            uint8_t *lzma_buffer = (uint8_t *)malloc(sizeof(DIFFHEADER) + lzma_buffer_len);

            if( LzmaUncompress(lzma_buffer + sizeof(DIFFHEADER), &lzma_buffer_len, (unsigned char*)diff_header->data + LZMA_PROPS_SIZE, (uint32_t*)&data_size, (unsigned char*)diff_header->data, LZMA_PROPS_SIZE)==SZ_OK )
            {
                memcpy(lzma_buffer, diff_buffer, sizeof(DIFFHEADER));
                UnmapViewOfFile((void*)diff_buffer);
                diff_buffer = lzma_buffer;

                diff_header = (PDIFFHEADER)diff_buffer;
                diff_header->table1 += (uint32_t)diff_buffer;
                diff_header->table2 += (uint32_t)diff_buffer;
            }
            else
            {
                free(lzma_buffer);
                UnmapViewOfFile((void*)diff_buffer);
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    if ( loc.GetInfo()->count!=diff_header->total1 || loc.GetInfo()->time!=diff_header->time1 ) return false;

    PDIFFITEM diffitem = diff_header->data;
    LocateItem *Locate = (LocateItem *)malloc( diff_header->total2 * sizeof(LocateItem) );

    uint32_t last_diff = ( diff_header->table1 - sizeof(DIFFHEADER) - (uint32_t)diff_buffer ) / sizeof(DIFFITEM) - 1;
    uint32_t last_line = diff_header->total2 - 1;

    uint32_t i = loc.GetInfo()->count;
    for(; i>0; i--)
    {
        LocateItem *item = loc.GetItem(i);

        if(i!=diffitem[last_diff].line)
        {
            Locate[last_line].begin_ip = item->begin_ip;
            Locate[last_line].region = item->region;
            Locate[last_line].address = item->address;
            last_line--;
        }
        else
        {
            switch(diffitem[last_diff].method)
            {
            case INSERT:
                //printf("INSERT %d %d\n", i, diffitem[last_diff-1].line);
                Locate[last_line].begin_ip = diffitem[last_diff].begin_ip;
                Locate[last_line].region = (const char*)( diffitem[last_diff].table1 + diff_header->table1 );
                Locate[last_line].address = (const char*)( diffitem[last_diff].table2 + diff_header->table2 );
                last_line--;
                i++;
                break;
            case REMOVE:
                //printf("REMOVE %d %d %d\n", i, diffitem[last_diff-1].line, diffitem[last_diff-2].line);
                break;
            case MODIFY:
                Locate[last_line].begin_ip = item->begin_ip;
                Locate[last_line].region = (const char*)( diffitem[last_diff].table1 + diff_header->table1 );
                Locate[last_line].address = (const char*)( diffitem[last_diff].table2 + diff_header->table2 );
                //printf("MODIFY %d %s %s\n", last_line+1, Locate[last_line].region, Locate[last_line].address);
                //getchar();
                last_line--;
                break;
            }
            last_diff--;
        }
    }

    //printf("%d %d\n",last_diff,last_line);
    if ( last_diff!=-1 || last_line!=-1 ) return false;

    StringTable string_table1;
    StringTable string_table2;
    RecordTable record_table;

    Buffer buffer;

    uint32_t last_begin_ip = 0;

    for(i=last_line+1; i<diff_header->total2; i++)
    {
        LocateItem *item = &Locate[i];

        LOCATE record;
        record.begin_ip = item->begin_ip;
        record.table1 = string_table1.Append(item->region);
        record.table2 = string_table2.Append(item->address);

        if ( i > 0 )
        {
            uint32_t diff = record.begin_ip - last_begin_ip;
            if ( compress && isPowerOf2(diff) )
            {
                record.begin_ip = LogBase2(diff);
            }
        }

        record_table.Append(&record);

        last_begin_ip = item->begin_ip;
    }
    free(Locate);

    //合并数据区
    Buffer *record_table_buffer = record_table;
    Buffer *string_table1_buffer = string_table1;
    Buffer *string_table2_buffer = string_table2;

    std::copy(record_table_buffer->begin(), record_table_buffer->end(), std::back_inserter(buffer));
    std::copy(string_table1_buffer->begin(), string_table1_buffer->end(), std::back_inserter(buffer));
    std::copy(string_table2_buffer->begin(), string_table2_buffer->end(), std::back_inserter(buffer));

    //生成文件头
    HEADER header;
    header.magic = LOCATE_MAGIC;
    header.version = LOCATE_VERISON;
    header.compress = compress?1:0;
    header.total = diff_header->total2;
    header.time = diff_header->time2;

    header.table1 = sizeof(header) + record_table_buffer->size(); // 这里不加LZMA_PROPS_SIZE的原因是解压后，抛弃props信息
    header.table2 = header.table1 + string_table1_buffer->size();
    header.size = buffer.size();
    header.crc32 = CRC32_MEM((uint8_t*)&header, sizeof(header) - 4);

    uint32_t lzma_buffer_len = buffer.size();
    uint8_t *lzma_buffer = 0;

    size_t prop_size = LZMA_PROPS_SIZE;
    BYTE outProps[LZMA_PROPS_SIZE];

    //准备压缩
    if(compress)
    {
        lzma_buffer = (uint8_t *)malloc(lzma_buffer_len);
        
        ProgressCallback LzmaCompressProgress;
        LzmaCompressProgress.Progress = LzmaOnProgress;
        LzmaCompressProgress.totalInSize = buffer.size();
        LzmaCompressProgress.progress = progress;
        LzmaCompress(lzma_buffer, &lzma_buffer_len, &buffer[0], buffer.size(), (ICompressProgress*)&LzmaCompressProgress, outProps, &prop_size, 5, 1<<27, 8, 0, 2, 64, 4);
    }

    //保存文件
    FILE * out = _wfopen(path, L"wb");
    fwrite(&header, 1, sizeof(header), out);

    if(compress)
    {
        fwrite(outProps, 1, sizeof(outProps), out);
        fwrite(lzma_buffer, 1, lzma_buffer_len, out);
    }
    else
    {
        fwrite(&buffer[0], 1, buffer.size(), out);
    }
    fclose(out);

    if(compress)
    {
        free(lzma_buffer);
    }

    free((void*)diff_buffer);
    return true;
}

bool LocateConverter::GeneratePatch(const wchar_t *qqwry1_path, const wchar_t *qqwry2_path)
{
    QQWryReader QQWry1(qqwry1_path);
    QQWryReader QQWry2(qqwry2_path);

    if ( !QQWry1.IsAvailable() ) return false;
    if ( !QQWry2.IsAvailable() ) return false;

    if(QQWry1.GetInfo()->time==QQWry2.GetInfo()->time) return false;

    // 根据时间交换，低版本在前
    QQWryReader *qqwry1 = &QQWry1;
    QQWryReader *qqwry2 = &QQWry2;
    if(QQWry1.GetInfo()->time>QQWry2.GetInfo()->time)
    {
        QQWryReader *temp = qqwry1;
        qqwry1 = qqwry2;
        qqwry2 = temp;
    }

    uint32_t i = 0;
    uint32_t j = 0;

    uint8_t flag = 0;

    uint32_t n = 0;
    uint32_t m = 0;
    //int x = 1;

    StringTable string_table1;
    StringTable string_table2;
    Buffer RecordBuffer;         // 记录
    Buffer ResultBuffer;         // 最终结果

    while(i<qqwry1->GetInfo()->count && j<qqwry2->GetInfo()->count)
    {
        LocateItem *Item1 = qqwry1->GetItem(i + 1);
        LocateItem *Item2 = qqwry2->GetItem(j + 1);

        if( Item1->begin_ip != Item2->begin_ip )
        {
            if( Item1->begin_ip < Item2->begin_ip ) i++;
            else j++;

            flag = 1;
            continue;
        }
        if( Item1->end_ip != Item2->end_ip )
        {
            if( Item1->end_ip < Item2->end_ip ) i++;
            else j++;

            flag = 1;
            continue;
        }

        if(flag==1)
        {
            flag = 0;

            //printf("%d-%d,%d-%d\n",n+1,i,m+1,j);
            for(uint32_t k=0; k<i-n; k++)
            {
                DIFFITEM record;
                record.line = n + 1 + k;
                record.method = REMOVE;
                record.begin_ip = 0;
                record.table1 = 0;
                record.table2 = 0;

                AppendBuffer(RecordBuffer, (uint8_t*)&record, sizeof(record));
                //printf("%d: 删除 %d\n",x++,n+1+k);
            }
            for(uint32_t l=0; l<j-m; l++)
            {
                Item2 = qqwry2->GetItem(m + l + 1);

                DIFFITEM record;
                record.line = i;
                record.method = INSERT;
                record.begin_ip = Item2->begin_ip;
                record.table1 = string_table1.Append(Item2->region);
                record.table2 = string_table2.Append(Item2->address);

                AppendBuffer(RecordBuffer, (uint8_t*)&record, sizeof(record));
                //printf("%d: 添加 (%d) -> %d\n", x++, m+1+k, i);
            }
        }

        Item1 = qqwry1->GetItem(i + 1);
        Item2 = qqwry2->GetItem(j + 1);
        if( strcmp(Item1->region, Item2->region) || strcmp(Item1->address, Item2->address) )
        {
            DIFFITEM record;
            record.line = i + 1;
            record.method = MODIFY;
            record.begin_ip = 0;
            record.table1 = string_table1.Append(Item2->region);
            record.table2 = string_table2.Append(Item2->address);

            AppendBuffer(RecordBuffer, (uint8_t*)&record, sizeof(record));
            //printf("%d: 修改 (%d) -> %d\n", x++, j+1, i+1);
        }

        i++;
        j++;
        n = i;
        m = j;
    }

    //合并数据区
    Buffer *string_table1_buffer = string_table1;
    Buffer *string_table2_buffer = string_table2;

    std::copy(RecordBuffer.begin(), RecordBuffer.end(), std::back_inserter(ResultBuffer));
    std::copy(string_table1_buffer->begin(), string_table1_buffer->end(), std::back_inserter(ResultBuffer));
    std::copy(string_table2_buffer->begin(), string_table2_buffer->end(), std::back_inserter(ResultBuffer));

    //生成文件头
    DIFFHEADER header;
    header.magic = DIFF_MAGIC;
    header.total1 = qqwry1->GetInfo()->count;
    header.total2 = qqwry2->GetInfo()->count;
    header.time1 = qqwry1->GetInfo()->time;
    header.time2 = qqwry2->GetInfo()->time;
    header.table1 = sizeof(header) + RecordBuffer.size();;
    header.table2 = header.table1 + string_table1_buffer->size();
    header.size = ResultBuffer.size();
    header.crc32 = CRC32_MEM((uint8_t*)&header, sizeof(header) - 4);

    //准备压缩
    uint32_t lzma_buffer_len = ResultBuffer.size();
    uint8_t *lzma_buffer = (uint8_t *)malloc(lzma_buffer_len);

    size_t prop_size = LZMA_PROPS_SIZE;
    BYTE outProps[LZMA_PROPS_SIZE];
    LzmaCompress(lzma_buffer, &lzma_buffer_len, &ResultBuffer[0], ResultBuffer.size(), NULL, outProps, &prop_size, 5, 1<<27, 8, 0, 2, 64, 4);

    //保存文件
    char temp[1024];
    sprintf(temp,"%d-%d.dif",qqwry1->GetInfo()->time,qqwry2->GetInfo()->time);
    FILE * out = fopen(temp, "wb");
    fwrite(&header, 1, sizeof(header), out);
    fwrite(outProps, 1, sizeof(outProps), out);
    fwrite(lzma_buffer, 1, lzma_buffer_len, out);
    //fwrite(&ResultBuffer[0], 1, ResultBuffer.size(), out);
    fclose(out);

    free(lzma_buffer);
    return true;
}
