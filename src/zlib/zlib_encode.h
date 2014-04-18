#ifndef _ZLIB_ENCODE_H_
#define _ZLIB_ENCODE_H_

#include <malloc.h>
#include <memory.h>

#define assert(x)	((void)0)
typedef unsigned int stbiw_uint32;

// stretchy buffer; stbi__sbpush() == vector<>::push_back() -- stbi__sbcount() == vector<>::size()
#define stbi__sbraw(a) ((int *) (a) - 2)
#define stbi__sbm(a)   stbi__sbraw(a)[0]
#define stbi__sbn(a)   stbi__sbraw(a)[1]

#define stbi__sbneedgrow(a,n)  ((a)==0 || stbi__sbn(a)+n >= stbi__sbm(a))
#define stbi__sbmaybegrow(a,n) (stbi__sbneedgrow(a,(n)) ? stbi__sbgrow(a,n) : 0)
#define stbi__sbgrow(a,n)  stbi__sbgrowf((void **) &(a), (n), sizeof(*(a)))

#define stbi__sbpush(a, v)      (stbi__sbmaybegrow(a,1), (a)[stbi__sbn(a)++] = (v))
#define stbi__sbcount(a)        ((a) ? stbi__sbn(a) : 0)
#define stbi__sbfree(a)         ((a) ? free(stbi__sbraw(a)),0 : 0)

static void *stbi__sbgrowf(void **arr, int increment, int itemsize)
{
   int m = *arr ? 2*stbi__sbm(*arr)+increment : increment+1;
   void *p = realloc(*arr ? stbi__sbraw(*arr) : 0, itemsize * m + sizeof(int)*2);
   assert(p);
   if (p) {
      if (!*arr) ((int *) p)[1] = 0;
      *arr = (void *) ((int *) p + 2);
      stbi__sbm(*arr) = m;
   }
   return *arr;
}

static unsigned char *stbi__zlib_flushf(unsigned char *data, unsigned int *bitbuffer, int *bitcount)
{
   while (*bitcount >= 8) {
      stbi__sbpush(data, (unsigned char) *bitbuffer);
      *bitbuffer >>= 8;
      *bitcount -= 8;
   }
   return data;
}

static int stbi__zlib_bitrev(int code, int codebits)
{
   int res=0;
   while (codebits--) {
      res = (res << 1) | (code & 1);
      code >>= 1;
   }
   return res;
}

static unsigned int stbi__zlib_countm(const unsigned char *a,const unsigned char *b, int limit)
{
   int i;
   for (i=0; i < limit && i < 258; ++i)
      if (a[i] != b[i]) break;
   return i;
}

static unsigned int stbi__zhash(const unsigned char *data)
{
   stbiw_uint32 hash = data[0] + (data[1] << 8) + (data[2] << 16);
   hash ^= hash << 3;
   hash += hash >> 5;
   hash ^= hash << 4;
   hash += hash >> 17;
   hash ^= hash << 25;
   hash += hash >> 6;
   return hash;
}

#define stbi__zlib_flush() (out = stbi__zlib_flushf(out, &bitbuf, &bitcount))
#define stbi__zlib_add(code,codebits) \
      (bitbuf |= (code) << bitcount, bitcount += (codebits), stbi__zlib_flush())
#define stbi__zlib_huffa(b,c)  stbi__zlib_add(stbi__zlib_bitrev(b,c),c)
// default huffman tables
#define stbi__zlib_huff1(n)  stbi__zlib_huffa(0x30 + (n), 8)
#define stbi__zlib_huff2(n)  stbi__zlib_huffa(0x190 + (n)-144, 9)
#define stbi__zlib_huff3(n)  stbi__zlib_huffa(0 + (n)-256,7)
#define stbi__zlib_huff4(n)  stbi__zlib_huffa(0xc0 + (n)-280,8)
#define stbi__zlib_huff(n)  ((n) <= 143 ? stbi__zlib_huff1(n) : (n) <= 255 ? stbi__zlib_huff2(n) : (n) <= 279 ? stbi__zlib_huff3(n) : stbi__zlib_huff4(n))
#define stbi__zlib_huffb(n) ((n) <= 143 ? stbi__zlib_huff1(n) : stbi__zlib_huff2(n))

#define stbi__ZHASH   0x4000

unsigned char * stbi_zlib_compress(const unsigned char *data, int data_len, int *out_len, int quality)
{
   static unsigned short lengthc[] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258, 259 };
   static unsigned char  lengtheb[]= { 0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0 };
   static unsigned short distc[]   = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577, 32768 };
   static unsigned char  disteb[]  = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
   unsigned int bitbuf=0;
   int i,j, bitcount=0;
   unsigned char *out = NULL;
   unsigned char **hash_table[stbi__ZHASH]; // 64KB on the stack!
   if (quality < 5) quality = 5;

   stbi__sbpush(out, 0x78);   // DEFLATE 32K window
   stbi__sbpush(out, 0x5e);   // FLEVEL = 1
   stbi__zlib_add(1,1);  // BFINAL = 1
   stbi__zlib_add(1,2);  // BTYPE = 1 -- fixed huffman

   for (i=0; i < stbi__ZHASH; ++i)
      hash_table[i] = NULL;

   i=0;
   while (i < data_len-3) {
      // hash next 3 bytes of data to be compressed
      int h = stbi__zhash(data+i)&(stbi__ZHASH-1), best=3;
      unsigned char *bestloc = 0;
      unsigned char **hlist = hash_table[h];
      int n = stbi__sbcount(hlist);
      for (j=0; j < n; ++j) {
         if (hlist[j]-data > i-32768) { // if entry lies within window
            int d = stbi__zlib_countm(hlist[j], data+i, data_len-i);
            if (d >= best) best=d,bestloc=hlist[j];
         }
      }
      // when hash table entry is too long, delete half the entries
      if (hash_table[h] && stbi__sbn(hash_table[h]) == 2*quality) {
         memcpy(hash_table[h], hash_table[h]+quality, sizeof(hash_table[h][0])*quality);
         stbi__sbn(hash_table[h]) = quality;
      }
      stbi__sbpush(hash_table[h],(unsigned char*)(data+i));

      if (bestloc) {
         // "lazy matching" - check match at *next* byte, and if it's better, do cur byte as literal
         h = stbi__zhash(data+i+1)&(stbi__ZHASH-1);
         hlist = hash_table[h];
         n = stbi__sbcount(hlist);
         for (j=0; j < n; ++j) {
            if (hlist[j]-data > i-32767) {
               int e = stbi__zlib_countm(hlist[j], data+i+1, data_len-i-1);
               if (e > best) { // if next match is better, bail on current match
                  bestloc = NULL;
                  break;
               }
            }
         }
      }

      if (bestloc) {
         int d = data+i - bestloc; // distance back
         assert(d <= 32767 && best <= 258);
         for (j=0; best > lengthc[j+1]-1; ++j);
         stbi__zlib_huff(j+257);
         if (lengtheb[j]) stbi__zlib_add(best - lengthc[j], lengtheb[j]);
         for (j=0; d > distc[j+1]-1; ++j);
         stbi__zlib_add(stbi__zlib_bitrev(j,5),5);
         if (disteb[j]) stbi__zlib_add(d - distc[j], disteb[j]);
         i += best;
      } else {
         stbi__zlib_huffb(data[i]);
         ++i;
      }
   }
   // write out final bytes
   for (;i < data_len; ++i)
      stbi__zlib_huffb(data[i]);
   stbi__zlib_huff(256); // end of block
   // pad with 0 bits to byte boundary
   while (bitcount)
      stbi__zlib_add(0,1);

   for (i=0; i < stbi__ZHASH; ++i)
      (void) stbi__sbfree(hash_table[i]);

   {
      // compute adler32 on input
      unsigned int i=0, s1=1, s2=0, blocklen = data_len % 5552;
      int j=0;
      while (j < data_len) {
         for (i=0; i < blocklen; ++i) s1 += data[j+i], s2 += s1;
         s1 %= 65521, s2 %= 65521;
         j += blocklen;
         blocklen = 5552;
      }
      stbi__sbpush(out, (unsigned char) (s2 >> 8));
      stbi__sbpush(out, (unsigned char) s2);
      stbi__sbpush(out, (unsigned char) (s1 >> 8));
      stbi__sbpush(out, (unsigned char) s1);
   }
   *out_len = stbi__sbn(out);
   // make returned pointer freeable
   memmove(stbi__sbraw(out), out, *out_len);
   return (unsigned char *) stbi__sbraw(out);
}

#endif // _ZLIB_ENCODE_H_
