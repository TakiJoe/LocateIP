#ifndef __LOCATE_CONVERTER_H_
#define __LOCATE_CONVERTER_H_

#include "Locate.h"

// 进度回调函数定义
typedef void (*OnProgress)(double rate);

// 格式转换类，提供一系列转换函数
class LocateConverter
{
public:
    // 转换QQWry.DAT文件为location.loc，compress表示是否使用LZMA进行压缩
    bool QQWryToLocate(const wchar_t *qqwry, const wchar_t *locate, bool compress = false, OnProgress progress = NULL);

    // 转换location.loc文件为QQWry.DAT
    bool LocateToQQWry(const wchar_t *locate, const wchar_t *qqwry);

    // 转换老location.loc文件为新location.loc，compress表示是否使用LZMA进行压缩
    bool LocateUpdate(const wchar_t *locate, const wchar_t *patch, const wchar_t *path, bool compress = false, OnProgress progress = NULL);

    // 通过两个不同版本的QQWry.DAT文件生成补丁文件
    bool GeneratePatch(const wchar_t *qqwry1_path, const wchar_t *qqwry2_path);
};

#endif // __LOCATE_CONVERTER_H_
