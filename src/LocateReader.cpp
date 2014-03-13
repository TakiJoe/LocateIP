#include "Locate.h"
#include "Utility.h"
#include "LocateReader.h"

LocateReader::LocateReader(const wchar_t *path)
{
    available = false;
    Reload(path);
}

LocateReader::~LocateReader()
{
    Clearup();
}

bool LocateReader::Reload(const wchar_t *path)
{
    Clearup();
    Load(path);
    return IsAvailable();
}

LocateItem* LocateReader::GetLocate(const char *ip)
{
    return GetLocate( str2ip(ip) );
}

LocateItem* LocateReader::GetLocate(uint32_t ip)
{
    if(available)
    {
        PHEADER header = (PHEADER)buffer;
        PLOCATE locate = header->data;

        uint32_t low = 0;
        uint32_t high = header->total - 1;
        while (low <= high)
        {
            uint32_t mid = (low + high) / 2;
            if ( ip == locate[mid].begin_ip ) return GetItem(mid + 1);
            else if ( ip < locate[mid].begin_ip ) high = mid - 1;
            else low = mid + 1;
        }

        return GetItem(high + 1);
    }

    return GetItem(0);
}

LocateItem* LocateReader::GetItem(uint32_t index)
{
    if(available)
    {
        PHEADER header = (PHEADER)buffer;
        PLOCATE locate = header->data;

        index--;
        if(index>=0 && index<header->total)
        {
            item.begin_ip = locate[index].begin_ip;
            if(index==header->total - 1)
            {
                item.end_ip = 0xFFFFFFFF;
            }
            else
            {
                item.end_ip = locate[index + 1].begin_ip - 1;
            }

            item.region  = (const char *)(locate[index].table1 + header->table1 + buffer);
            item.address = (const char *)(locate[index].table2 + header->table2 + buffer);

            return &item;
        }
    }


    // ²éÑ¯Ê§°Ü
    item.begin_ip = 0;
    item.end_ip = 0;
    item.region = "";
    item.address = "";
    return &item;
}

// Ë½ÓÐº¯Êý
void LocateReader::Load(const wchar_t *path)
{
    available = false;
    info.count = 0;
    info.time = 0;

    HANDLE hfile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hfile!=INVALID_HANDLE_VALUE)
    {
        uint32_t file_length = GetFileSize(hfile, NULL);

        HANDLE hfilemap = CreateFileMapping(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
        CloseHandle(hfile);

        buffer = (const uint8_t*) MapViewOfFile(hfilemap, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(hfilemap);

        PHEADER header = (PHEADER)buffer;
        uint32_t data_size = file_length - sizeof(HEADER) - LZMA_PROPS_SIZE;
        if( data_size<0 || header->magic!=LOCATE_MAGIC || header->version>LOCATE_VERISON || CRC32_MEM(buffer, sizeof(HEADER) - 4)!=header->crc32 )
        {
            UnmapViewOfFile((void*)buffer);
        }
        else
        {
            if(header->compress)
            {
                wchar_t TempPath[MAX_PATH];
                GetTempPathW(MAX_PATH, TempPath);

                wchar_t TempLocatePath[MAX_PATH];
                swprintf(TempLocatePath, L"%sLocate-%d.loc", TempPath, header->time);
                DWORD dwAttributes = GetFileAttributesW(TempLocatePath);
                if( dwAttributes!=0xFFFFFFFF )
                {
                    UnmapViewOfFile((void*)buffer);
                    if( _wcsicmp(TempLocatePath, path) ) Load(TempLocatePath);
                }
                else
                {
                    uint32_t lzma_buffer_len = header->size;
                    uint8_t *lzma_buffer = (uint8_t *)malloc(sizeof(HEADER) + lzma_buffer_len);

                    if( LzmaUncompress(lzma_buffer + sizeof(HEADER), &lzma_buffer_len, (unsigned char*)header->data + LZMA_PROPS_SIZE, (uint32_t*)&data_size, (unsigned char*)header->data, LZMA_PROPS_SIZE)==SZ_OK )
                    {
                        memcpy(lzma_buffer, buffer, sizeof(HEADER));
                        UnmapViewOfFile((void*)buffer);
                        header = (PHEADER)lzma_buffer;
                        PLOCATE locate = header->data;
                        for(uint32_t i=1; i<header->total; i++)
                        {
                            if( locate[i].begin_ip<32 )
                            {
                                locate[i].begin_ip = locate[i-1].begin_ip + (1<<locate[i].begin_ip);
                            }
                        }

                        header->compress = 0;
                        header->crc32 = CRC32_MEM(lzma_buffer, sizeof(HEADER) - 4);

                        FILE *fp = _wfopen(TempLocatePath, L"wb");
                        if(fp)
                        {
                            fwrite(lzma_buffer, 1, sizeof(HEADER) + lzma_buffer_len, fp);
                            fclose(fp);
                        }

                        free(lzma_buffer);
                        Load(TempLocatePath);
                    }
                    else
                    {
                        free(lzma_buffer);
                        UnmapViewOfFile((void*)buffer);
                    }
                }
            }
            else
            {
                available = true;

                info.count = header->total;
                info.time = header->time;
            }
        }
    }
}

void LocateReader::Clearup()
{
    if(available)
    {
        UnmapViewOfFile((void*)buffer);
        available = false;
    }
}
