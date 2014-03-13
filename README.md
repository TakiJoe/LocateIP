#LoCi SDK v1.2

##基本说明：
- LoCi，即LocateIP的简写，是一种数据库格式，用于查询IP对应的地址。通过
开放源代码的方式提供接口，非常适合集成到您的程序中。LoCi SDK给您提供了源
代码以及使用本SDK开发的应用程序例子。详细调用说明可查看文件中的注释。
- 您可以随意使用或修改源代码而无需申明，但是请不要调整本数据库的文件结
构，造成版本间不兼容。如有必要理由需要修改结构，请联系我。

##主要优势：
- **极小** 文件大小约为QQWry.Dat的20%，下载快多了。
- **极新** 每次升级版本仅需下载微量数据，还有什么理由不更新？
- **极快** 经过特别设计的轻巧结构，查询速度飞快。
- **兼容** 本格式可以和QQWry.dat互相转换，或者导出为文本文件。
- **稳定** 具有文件校验，防止文件损坏时强制读取导致程序崩溃。

##其它说明：
- 如果使用 LZMA SDK 来压缩数据库，则生成时速度较慢
- 转换出的QQWry.dat和纯真的文件二进制对比不太一样，但实际内容相同
- 为了保持数据一致，没有过滤 CZ88.NET 字符串、也没有转换字符编码
- 搭建更新服务器可参考 http://shuax.aliapp.com/download/LocateUpdate.rar

##文件解释：
- `QQWry.dat` 纯真版IP数据库文件，通常使用此文件名
- `location.loc` 本数据库文件，建议使用此文件名
- `20130510-20130515.dif` 增量更新文件，建议使用这样的文件名
- `2013年05月10日.txt` 解压出的纯文本文件，建议使用这样的文件名

##代码示例
```
#ifdef __MINGW32__
	// 直接包含源代码，MingW测试
	#include "Locate.cpp"
#else
	// 使用静态库链接，VC6测试
	#include <stdio.h>
	#include <windows.h>

	#include "Locate.h"
	#include "QQWryReader.h"
	#include "LocateReader.h"
	#include "LocateConverter.h"

	#pragma comment(lib, "Locate.lib")
#endif

void WryToLocateProgress(double rate)
{
	printf("%f\n", rate);
}

int main(int argc, char *argv[])
{
	LocateConverter converter;

	// 生成补丁20130515-20130520.dif
	converter.GeneratePatch(L"2013年5月15日.DAT", L"2013年5月20日.DAT");

	// 转换2013年5月15日.DAT为LocateIP格式，压缩，并且查看压缩进度
	converter.QQWryToLocate(L"2013年5月15日.DAT", L"location~.loc", true, WryToLocateProgress);

	// 通过更新补丁，升级2013年5月15日的LocateIP格式
	converter.LocateUpdate(L"location~.loc", L"20130515-20130520.dif", L"location.loc");

	// 分别解压两个数据库，执行完毕后两个TXT文件应当一模一样
	QQWryReader wry(L"2013年5月20日.dat");
	if(wry.IsAvailable()) wry.Save2Text(L"test1.txt");

	LocateReader loc(L"location.loc");
	if(loc.IsAvailable()) loc.Save2Text(L"test2.txt");

	// 转换出的QQWry.DAT可以用纯真数据库工具打开并且验证结果
	//converter.LocateToQQWry(L"location.loc", L"QQWry.DAT");
	return 0;
}
```

