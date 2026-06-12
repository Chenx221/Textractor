#include "match.h"
#include "main.h"
#include "texthook.h"
#include "native/pchooks.h"
#include "mono/monoobject.h"
#include "mono/funcinfo.h"
#include "engine.h"
#include "util.h"
#include "clr/clrhelper.h"
#include "cpputil/cppcstring.h"

// Warning: The offset in ITH has -4 offset comparing to pusha and AGTH
enum pusha_off
{
	pusha_rax_off = -0xC,
	pusha_rbx_off = -0x14,
	pusha_rcx_off = -0x1c,
	pusha_rdx_off = -0x24,
	pusha_rsp_off = -0x2c,
	pusha_rbp_off = -0x34,
	pusha_rsi_off = -0x3c,
	pusha_rdi_off = -0x44,
	pusha_r8_off = -0x4c,
	pusha_r9_off = -0x54,
	pusha_r10_off = -0x5c,
	pusha_r11_off = -0x64,
	pusha_r12_off = -0x6c,
	pusha_r13_off = -0x74,
	pusha_r14_off = -0x7c,
	pusha_r15_off = -0x84,
	pusha_off = -0x8c // pushad offset
};

#define retof(rsp_base)         *(uintptr_t *)(rsp_base) // return address
#define regof(name, rsp_base)   *(uintptr_t *)((rsp_base) + pusha_##name##_off - 4)
#define argof(count, rsp_base)  *(uintptr_t *)((rsp_base) + 4 * (count)) // starts from 1 instead of 0

enum { VNR_TEXT_CAPACITY = 1500 }; // estimated max number of bytes allowed in VNR, slightly larger than VNR's text limit (1000)

namespace { // unnamed helpers

#define XX2 XX,XX       // WORD
#define XX4 XX2,XX2     // DWORD
#define XX8 XX4,XX4     // QWORD

// jichi 8/18/2013: Original maximum relative address in ITH
//enum { MAX_REL_ADDR = 0x200000 };

// jichi 10/1/2013: Increase relative address limit. Certain game engine like Artemis has larger code region
	enum : DWORD { MAX_REL_ADDR = 0x00300000 };

	static union {
		char text_buffer[0x1000];
		wchar_t wc_buffer[0x800];

		struct { // CodeSection
			DWORD base;
			DWORD size;
		} code_section[0x200];
	};
	DWORD text_buffer_length;

	// 7/29/2014 jichi: I should move these functions to different files
	// String utilities
	// Return the address of the first non-zero address
	LPCSTR reverse_search_begin(const char* s, int maxsize = VNR_TEXT_CAPACITY)
	{
		if (*s)
			for (int i = 0; i < maxsize; i++, s--)
				if (!*s)
					return s + 1;
		return nullptr;
	}

	bool all_ascii(const char* s, int maxsize = VNR_TEXT_CAPACITY)
	{
		if (s)
			for (int i = 0; i < maxsize && *s; i++, s++)
				if ((BYTE)*s > 127) // unsigned char
					return false;
		return true;
	}

	bool all_ascii(const wchar_t* s, int maxsize = VNR_TEXT_CAPACITY)
	{
		if (s)
			for (int i = 0; i < maxsize && *s; i++, s++)
				if (*s > 127) // unsigned char
					return false;
		return true;
	}

	// String filters

	void CharReplacer(char* str, size_t* size, char fr, char to)
	{
		size_t len = *size;
		for (size_t i = 0; i < len; i++)
			if (str[i] == fr)
				str[i] = to;
	}

	void WideCharReplacer(wchar_t* str, size_t* size, wchar_t fr, wchar_t to)
	{
		size_t len = *size / 2;
		for (size_t i = 0; i < len; i++)
			if (str[i] == fr)
				str[i] = to;
	}

	void CharFilter(char* str, size_t* size, char ch)
	{
		size_t len = *size,
			curlen;
		for (char* cur = (char*)::memchr(str, ch, len);
			(cur && --len && (curlen = len - (cur - str)));
			cur = (char*)::memchr(cur, ch, curlen))
			::memmove(cur, cur + 1, curlen);
		*size = len;
	}

	void WideCharFilter(wchar_t* str, size_t* size, wchar_t ch)
	{
		size_t len = *size / 2,
			curlen;
		for (wchar_t* cur = cpp_wcsnchr(str, ch, len);
			(cur && --len && (curlen = len - (cur - str)));
			cur = cpp_wcsnchr(cur, ch, curlen))
			::memmove(cur, cur + 1, 2 * curlen);
		*size = len * 2;
	}

	void CharsFilter(char* str, size_t* size, const char* chars)
	{
		size_t len = *size,
			curlen;
		for (char* cur = cpp_strnpbrk(str, chars, len);
			(cur && --len && (curlen = len - (cur - str)));
			cur = cpp_strnpbrk(cur, chars, curlen))
			::memmove(cur, cur + 1, curlen);
		*size = len;
	}

	void WideCharsFilter(wchar_t* str, size_t* size, const wchar_t* chars)
	{
		size_t len = *size / 2,
			curlen;
		for (wchar_t* cur = cpp_wcsnpbrk(str, chars, len);
			(cur && --len && (curlen = len - (cur - str)));
			cur = cpp_wcsnpbrk(cur, chars, curlen))
			::memmove(cur, cur + 1, 2 * curlen);
		*size = len * 2;
	}

	void StringFilter(char* str, size_t* size, const char* remove, size_t removelen)
	{
		size_t len = *size,
			curlen;
		for (char* cur = cpp_strnstr(str, remove, len);
			(cur && (len -= removelen) && (curlen = len - (cur - str)));
			cur = cpp_strnstr(cur, remove, curlen))
			::memmove(cur, cur + removelen, curlen);
		*size = len;
	}

	void WideStringFilter(wchar_t* str, size_t* size, const wchar_t* remove, size_t removelen)
	{
		size_t len = *size / 2,
			curlen;
		for (wchar_t* cur = cpp_wcsnstr(str, remove, len);
			(cur && (len -= removelen) && (curlen = len - (cur - str)));
			cur = cpp_wcsnstr(cur, remove, curlen))
			::memmove(cur, cur + removelen, 2 * curlen);
		*size = len * 2;
	}

	void StringFilterBetween(char* str, size_t* size, const char* fr, size_t frlen, const char* to, size_t tolen)
	{
		size_t len = *size,
			curlen;
		for (char* cur = cpp_strnstr(str, fr, len);
			cur;
			cur = cpp_strnstr(cur, fr, curlen)) {
			curlen = (len - frlen) - (cur - str);
			auto end = cpp_strnstr(cur + frlen, to, curlen);
			if (!end)
				break;
			curlen = len - (end - str) - tolen;
			::memmove(cur, end + tolen, curlen);
			len -= tolen + (end - cur);
		}
		*size = len;
	}

	void WideStringFilterBetween(wchar_t* str, size_t* size, const wchar_t* fr, size_t frlen, const wchar_t* to, size_t tolen)
	{
		size_t len = *size / 2,
			curlen;
		for (wchar_t* cur = cpp_wcsnstr(str, fr, len);
			cur;
			cur = cpp_wcsnstr(cur, fr, curlen)) {
			curlen = (len - frlen) - (cur - str);
			auto end = cpp_wcsnstr(cur + frlen, to, curlen);
			if (!end)
				break;
			curlen = len - (end - str) - tolen;
			::memmove(cur, end + tolen, 2 * curlen);
			len -= tolen + (end - cur);
		}
		*size = len * 2;
	}

	void StringCharReplacer(char* str, size_t* size, const char* src, size_t srclen, char ch)
	{
		size_t len = *size,
			curlen;
		for (char* cur = cpp_strnstr(str, src, len);
			cur && len;
			cur = cpp_strnstr(cur, src, curlen)) {
			*cur++ = ch;
			len -= srclen - 1;
			curlen = len - (cur - str);
			if (curlen == 0)
				break;
			::memmove(cur, cur + srclen - 1, curlen);
		}
		*size = len;
	}

	void WideStringCharReplacer(wchar_t* str, size_t* size, const wchar_t* src, size_t srclen, wchar_t ch)
	{
		size_t len = *size / 2,
			curlen;
		for (wchar_t* cur = cpp_wcsnstr(str, src, len);
			cur && len;
			cur = cpp_wcsnstr(cur, src, curlen)) {
			*cur++ = ch;
			len -= srclen - 1;
			curlen = len - (cur - str);
			if (curlen == 0)
				break;
			::memmove(cur, cur + srclen - 1, 2 * curlen);
		}
		*size = len * 2;
	}

	// NOTE: I assume srclen >= dstlen
	void StringReplacer(char* str, size_t* size, const char* src, size_t srclen, const char* dst, size_t dstlen)
	{
		size_t len = *size,
			curlen;
		for (char* cur = cpp_strnstr(str, src, len);
			cur && len;
			cur = cpp_strnstr(cur, src, curlen)) {
			::memcpy(cur, dst, dstlen);
			cur += dstlen;
			len -= srclen - dstlen;
			curlen = len - (cur - str);
			if (curlen == 0)
				break;
			if (srclen > dstlen)
				::memmove(cur, cur + srclen - dstlen, curlen);
		}
		*size = len;
	}

	void WideStringReplacer(wchar_t* str, size_t* size, const wchar_t* src, size_t srclen, const wchar_t* dst, size_t dstlen)
	{
		size_t len = *size / 2,
			curlen;
		for (wchar_t* cur = cpp_wcsnstr(str, src, len);
			cur && len;
			cur = cpp_wcsnstr(cur, src, curlen)) {
			::memcpy(cur, dst, 2 * dstlen);
			cur += dstlen;
			len -= srclen - dstlen;
			curlen = len - (cur - str);
			if (curlen == 0)
				break;
			if (srclen > dstlen)
				::memmove(cur, cur + srclen - dstlen, 2 * curlen);
		}
		*size = len * 2;
	}

	void RegexReplacer(char* str, size_t* size, const std::regex& pattern, const std::string& replacement) {
		std::string s(str, *size);
		s = std::regex_replace(s, pattern, replacement);
		*size = s.size();
		std::memcpy(str, s.c_str(), *size);
		str[*size] = '\0';
	}

	void RegexReplacerW(wchar_t* str, size_t* size, const std::wregex& pattern, const std::wstring& replacement) {
		std::wstring s(str, *size / sizeof(wchar_t));
		s = std::regex_replace(s, pattern, replacement);
		*size = s.size() * sizeof(wchar_t);
		std::memcpy(str, s.c_str(), *size);
		str[s.size()] = L'\0';
	}

	bool NewLineCharFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		CharFilter(reinterpret_cast<LPSTR>(data), reinterpret_cast<size_t*>(size),
			'\n');
		return true;
	}
	bool NewLineWideCharFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		CharFilter(reinterpret_cast<LPSTR>(data), reinterpret_cast<size_t*>(size),
			L'\n');
		return true;
	}
	bool NewLineStringFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		StringFilter(reinterpret_cast<LPSTR>(data), reinterpret_cast<size_t*>(size),
			"\\n", 2);
		return true;
	}
	bool NewLineWideStringFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		WideStringFilter(reinterpret_cast<LPWSTR>(data), reinterpret_cast<size_t*>(size),
			L"\\n", 2);
		return true;
	}
	bool NewLineCharToSpaceFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		CharReplacer(reinterpret_cast<LPSTR>(data), reinterpret_cast<size_t*>(size), '\n', ' ');
		return true;
	}
	bool NewLineWideCharToSpaceFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		WideCharReplacer(reinterpret_cast<LPWSTR>(data), reinterpret_cast<size_t*>(size), L'\n', L' ');
		return true;
	}

	// Remove every characters <= 0x1f (i.e. before space ' ') except 0xa and 0xd.
	bool IllegalCharsFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		CharsFilter(reinterpret_cast<LPSTR>(data), reinterpret_cast<size_t*>(size),
			"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0b\x0c\x0e\x0f\x10\x11\x12\x12\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f");
		return true;
	}
	bool IllegalWideCharsFilter(LPVOID data, DWORD* size, HookParam*, BYTE)
	{
		WideCharsFilter(reinterpret_cast<LPWSTR>(data), reinterpret_cast<size_t*>(size),
			L"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0b\x0c\x0e\x0f\x10\x11\x12\x12\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f");
		return true;
	}

} // unnamed namespace

#define XX2 XX,XX       // WORD
#define XX4 XX2,XX2     // DWORD
#define XX8 XX4,XX4     // QWORD

namespace Engine
{
	enum : DWORD { X64_MAX_REL_ADDR = 0x00300000 };
	/** Artikash 6/7/2019
*   PPSSPP JIT code has pointers, but they are all added to an offset before being used.
	Find that offset so that hook searching works properly.
	To find the offset, find a page of mapped memory with size 0x1f00000, read and write permissions, take its address and subtract 0x8000000.
	The above is useful for emulating PSP hardware, so unlikely to change between versions.
*/
	bool FindPPSSPP()
	{
		bool found = false;
		SYSTEM_INFO systemInfo;
		GetNativeSystemInfo(&systemInfo);
		for (BYTE* probe = NULL; probe < systemInfo.lpMaximumApplicationAddress;)
		{
			MEMORY_BASIC_INFORMATION info;
			if (!VirtualQuery(probe, &info, sizeof(info)))
			{
				probe += systemInfo.dwPageSize;
			}
			else
			{
				if (info.RegionSize == 0x1f00000 && info.Protect == PAGE_READWRITE && info.Type == MEM_MAPPED)
				{
					found = true;
					ConsoleOutput("Textractor: PPSSPP memory found: searching for hooks should yield working hook codes");
					// PPSSPP 1.8.0 compiles jal to sub dword ptr [r14+0x360],??
					memcpy(spDefault.pattern, Array<BYTE>{ 0x41, 0x83, 0xae, 0x60, 0x03, 0x00, 0x00 }, spDefault.length = 7);
					spDefault.offset = 0;
					spDefault.minAddress = 0;
					spDefault.maxAddress = -1ULL;
					spDefault.padding = (uintptr_t)probe - 0x8000000;
					spDefault.hookPostProcessor = [](HookParam& hp)
					{
						hp.type |= NO_CONTEXT | USING_SPLIT | SPLIT_INDIRECT;
						hp.split = -0x80; // r14
						hp.split_index = -8; // this is where PPSSPP 1.8.0 stores its return address stack
					};
				}
				probe += info.RegionSize;
			}
		}
		return found;
	}
		//MonoImage* mono_assembly_get_image(MonoAssembly* assembly)：获取程序集的镜像。后面几乎所有的操作都会以MonoImage* 为第一个参数。
	static uintptr_t (*mono_assembly_get_image)(uintptr_t) = NULL;
	// const char* mono_image_get_name(MonoImage * image) :获取程序集名。我们用它判断哪个程序集是我们的目标
	static char* (*mono_image_get_name)(uintptr_t) = NULL;
	//MonoClass* mono_class_from_name (MonoImage *image, const char* name_space, const char *name):通过类名获取类(非实例)。
	static uintptr_t(*mono_class_from_name)(uintptr_t,const char*,const char*) = NULL;
	//MonoVTable* mono_class_vtable (MonoDomain *domain, MonoClass *klass)：获取vtable，我们通过它可以找到静态字段的起始地址。
	static uintptr_t(*mono_class_vtable)(uintptr_t, uintptr_t) = NULL;
	//void* mono_vtable_get_static_field_data (MonoVTable *vt)：获取静态字段的起始地址。
	static void*  (*mono_vtable_get_static_field_data)(uintptr_t) = NULL;
	//MonoMethod* mono_class_get_method_from_name (MonoClass *klass, const char *name, int param_count):获取方法(非native code地址)。
	//其中param_count是参数数量，可以输入-1来省略。此函数无法获取重载的方法，但对于我们来说足够了。
	static uintptr_t(*mono_class_get_method_from_name)(uintptr_t,const char*,int) = NULL;
	//获取属性。用它可以进一步获得属性的getter和setter。
	//MonoProperty* mono_class_get_property_from_name(MonoClass* klass, const char* name)：
	static uintptr_t(*mono_class_get_property_from_name)(uintptr_t,const char*) = NULL;
	//获取属性的getter和setter。
	//MonoMethod* mono_property_get_get_method(MonoProperty* prop) 与 MonoMethod* mono_property_get_set_method(MonoProperty* prop)：
	static uintptr_t(*mono_property_get_set_method)(uintptr_t) = NULL;
	// （不安全）返回方法的地址，如果方法尚未编译，则JIT开始编译。这个是解决问题的核心方法。 gpointer mono_compile_method (MonoMethod *method):
	static  uint64_t* (*mono_compile_method)(uintptr_t) = NULL;

	//获取函数的非托管块指针 (native)   gpointer mono_method_get_unmanaged_thunk (MonoMethod *method)
	//使用这个来获取native代码 方法尚未编译，会执行编译 并提取 x86版本可能使用的是__stdcall
	static  uint64_t* (*mono_method_get_unmanaged_thunk)(uintptr_t) = NULL;
	//MonoDomain* mono_get_root_domain (void) ：获取主作用域。用于附加线程以及获取静态字段的地址。
	static  MonoDomain* (*mono_get_root_domain)() = NULL;
	//void mono_thread_attach (MonoDomain*)：附加到进程的主线程。这个操作是必须的。
	static  void (*mono_thread_attach)(MonoDomain*) = NULL;

	//MonoAssembly* assembly，而后者则是void* user_data
	int getV8StringLength(uintptr_t stack, uintptr_t data) {
		int len = *(int*)(data - 4);
		int checkLength = len > 0 && len < PIPE_BUFFER_SIZE ? len : 0;
		//检查是否为错误的unicode字符
		for (size_t i = 0; i < checkLength; i++)
		{
			if (*(WORD*)(data + i * 2) == 0x0)
				return 0;
		}
		return checkLength * 2;

	}

void MonoCallBack(uintptr_t assembly, void *userData) {
    uintptr_t image = mono_assembly_get_image(assembly);

	// if (image) {
	// 	char* image_name = mono_image_get_name(image);
	// 	if (image_name) {
	// 		ConsoleOutput("Scanning Assembly: %s", image_name);
	// 	}
	// }

    auto mono_tmp_class = mono_class_from_name(image, "TMPro", "TMP_Text");
    auto mono_ugui_class = mono_class_from_name(image, "UnityEngine.UI", "Text");
    auto mono_ngui_class = mono_class_from_name(image, "", "UILabel");
	auto mono_rpg_unite_class = mono_class_from_name(image, "RPGMaker.Codebase.Runtime.Common.Component", "HudHandler");
	auto mono_naninovel_class = mono_class_from_name(image, "Naninovel.UI", "RevealableText");

	// sp
	auto mono_schooldays_textwork_class = mono_class_from_name(image, "", "TextWork");
	auto mono_advsys_class = mono_class_from_name(image,"","ADVsys"); // FuriKuru01

    if (!mono_tmp_class && !mono_ugui_class && !mono_ngui_class && !mono_rpg_unite_class && !mono_naninovel_class && !mono_schooldays_textwork_class && !mono_advsys_class)
        return;

    // 附加到主domain（只需调用一次）
    mono_thread_attach(mono_get_root_domain());

    if (mono_tmp_class) {
        auto mono_property = mono_class_get_property_from_name(mono_tmp_class, "text");
        if (mono_property) {
            auto mono_set_method = mono_property_get_set_method(mono_property);
            uint64_t *method_pointer = mono_compile_method(mono_set_method);
            if (method_pointer) {
                HookParam hp = {};
                hp.type = USING_STRING | USING_UNICODE;
                hp.address = (uint64_t)method_pointer;
                hp.offset = -0x28; // rdx
                hp.padding = 0x14;
                hp.length_fun = getV8StringLength;
                ConsoleOutput("Mono_X64,Insert: TextMeshProUGUI_set_text Hook BY:IOV");
                NewHook(hp, "TextMeshProUGUI_set_text");
            }
        }
    }

    if (mono_ugui_class) {
        auto mono_property = mono_class_get_property_from_name(mono_ugui_class, "text");
        if (mono_property) {
            auto mono_set_method = mono_property_get_set_method(mono_property);
            uint64_t *method_pointer = mono_compile_method(mono_set_method);
            if (method_pointer) {
                HookParam hp = {};
                hp.type = USING_STRING | USING_UNICODE;
                hp.address = (uint64_t)method_pointer;
                hp.offset = -0x28; // rdx
                hp.padding = 0x14;
                hp.length_fun = getV8StringLength;
                ConsoleOutput("Mono_X64,Insert: UGUI_set_text Hook BY:IOV");
                NewHook(hp, "UGUI_set_text");
            }
        }
    }

    if (mono_ngui_class) {
        auto mono_property = mono_class_get_property_from_name(mono_ngui_class, "text");
        if (mono_property) {
            auto mono_set_method = mono_property_get_set_method(mono_property);
            uint64_t *method_pointer = mono_compile_method(mono_set_method);
            if (method_pointer) {
                HookParam hp = {};
                hp.type = USING_STRING | USING_UNICODE;
                hp.address = (uint64_t)method_pointer;
                hp.offset = -0x28; // rdx
                hp.padding = 0x14;
                hp.length_fun = getV8StringLength;
                ConsoleOutput("Mono_X64,Insert: NGUI_set_text Hook BY:IOV");
                NewHook(hp, "NGUI_set_text");
            }
        }
    }

	if (mono_rpg_unite_class) {
		auto mono_method = mono_class_get_method_from_name(mono_rpg_unite_class,"SetShowMessage",-1);
		if (mono_method) {
			uint64_t *method_pointer = mono_compile_method(mono_method);
			if (method_pointer) {
				HookParam hp = {};
				hp.type = USING_STRING | USING_UNICODE;
				hp.address = (uint64_t)method_pointer;
				hp.offset = -0x28; // rdx (第一个参数 message)
				hp.padding = 0x14;
				hp.length_fun = getV8StringLength;
				ConsoleOutput("Mono_X64,Insert: RPGMaker_Unite_SetShowMessage");
				NewHook(hp, "RPGMaker_Unite_SetShowMessage");
			}
		}
	}

	if (mono_naninovel_class) {
		auto mono_property = mono_class_get_property_from_name(mono_naninovel_class, "text");
		if (mono_property) {
			auto mono_set_method = mono_property_get_set_method(mono_property);
			uint64_t *method_pointer = mono_compile_method(mono_set_method);
			if (method_pointer) {
				HookParam hp = {};
				hp.type = USING_STRING | USING_UNICODE;
				hp.address = (uint64_t)method_pointer;
				hp.offset = -0x28; // rdx
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = reinterpret_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);

					if (len == 0)
						return false;
					WideStringCharReplacer(text,&len,L"<br>",4,L'\n');
					std::wregex pattern(LR"(<[^>]*>)");
					RegexReplacerW(text, &len, pattern, L"");
					*size = static_cast<DWORD>(len);
					return true;
				};
				hp.length_fun = getV8StringLength;
				ConsoleOutput("Mono_X64,Insert: NaninovelRevealableText_set_text Hook");
				NewHook(hp, "NaninovelRevealableText_set_text");
			}
		}
	}

	if (mono_schooldays_textwork_class) {
		auto mono_method = mono_class_get_method_from_name(mono_schooldays_textwork_class,"Queue",-1);
		if (mono_method) {
			uint64_t *method_pointer = mono_compile_method(mono_method);
			if (method_pointer) {
				HookParam hp = {};
				hp.type = USING_STRING | USING_UNICODE;
				hp.address = (uint64_t)method_pointer;
				hp.offset = -0x28; // string phrase
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = reinterpret_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);

					if (len == 0)
						return false;
					WideStringCharReplacer(text,&len,L"\\n",2,L'\n');
					*size = static_cast<DWORD>(len);
					return true;
				};
				hp.length_fun = getV8StringLength;
				ConsoleOutput("Mono_X64,Insert: Mono SP(School Days REMASTERED)");
				NewHook(hp, "Mono_SP_SDR");
			}
		}
	}

	if (mono_advsys_class) {
		auto mono_method = mono_class_get_method_from_name(mono_advsys_class,"TextUpdate",-1);
		if (mono_method) {
			uint64_t *method_pointer = mono_compile_method(mono_method);
			if (method_pointer) {
				HookParam hp = {};
				hp.type = USING_STRING | USING_UNICODE;
				hp.address = (uint64_t)method_pointer;
				hp.offset = -0x28; // string phrase
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = reinterpret_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);

					if (len == 0)
						return false;
					WideStringFilter(text,&len,L"[cm]",4);
					WideStringFilter(text,&len,L"[r]",3);
					WideStringFilter(text,&len,L"[l]",3);
					*size = static_cast<DWORD>(len);
					return true;
				};
				hp.length_fun = getV8StringLength;
				ConsoleOutput("Mono_X64,Insert: Mono SP(FuriKuru ADVsys)");
				NewHook(hp, "Mono_SP_ADVSYS");
			}
		}
	}
}


	bool InsertMonoHooksByAssembly(HMODULE module) {
		//void mono_assembly_foreach (GFunc func, gpointer user_data)
		//遍历程序集。用于获取目标程序集的指针。其中的func 是一个回调函数，要自己写。它有两个参数，前者就是MonoAssembly*，而后者则是user_data
		static auto mono_assembly_foreach = (void (*)(void (*)(uintptr_t, void*), uintptr_t))GetProcAddress(module, "mono_assembly_foreach");
		mono_assembly_get_image= (uintptr_t(*)(uintptr_t))GetProcAddress(module, "mono_assembly_get_image");
		mono_image_get_name = (char* (*)(uintptr_t))GetProcAddress(module, "mono_image_get_name");
		mono_class_from_name = (uintptr_t(*)(uintptr_t,const char*,const char*))GetProcAddress(module, "mono_class_from_name");
		mono_class_get_property_from_name = (uintptr_t(*)(uintptr_t,const char*))GetProcAddress(module, "mono_class_get_property_from_name");
		mono_class_get_method_from_name = (uintptr_t(*)(uintptr_t,const char*, int))GetProcAddress(module, "mono_class_get_method_from_name");
		mono_property_get_set_method = (uintptr_t(*)(uintptr_t))GetProcAddress(module, "mono_property_get_set_method");
		mono_compile_method = (uint64_t * (*)(uintptr_t))GetProcAddress(module, "mono_compile_method");
		//mono_method_get_unmanaged_thunk= (uint64_t * (*)(uintptr_t))GetProcAddress(module, "mono_method_get_unmanaged_thunk");
		mono_get_root_domain = (MonoDomain * (*)())GetProcAddress(module, "mono_get_root_domain");

		mono_thread_attach = (void (*)(MonoDomain*))GetProcAddress(module, "mono_thread_attach");
		if (mono_assembly_foreach && mono_assembly_get_image && mono_image_get_name && mono_class_from_name &&
			mono_class_get_property_from_name && mono_class_get_method_from_name && mono_property_get_set_method && mono_compile_method &&
			 mono_get_root_domain && mono_thread_attach) {
			mono_assembly_foreach(MonoCallBack, NULL);
			return true;
		}
		else
		{
			return false;
		}
	}
	bool InsertMonoHooks(HMODULE module)
	{
		return InsertMonoHooksByAssembly(module);
		auto SpecialHookMonoString = nullptr;
		static HMODULE mono = module;
		bool ret = false;
		for (auto func : Array<MonoFunction>{ MONO_FUNCTIONS_INITIALIZER })
		{
			HookParam hp = {};
			if (!(hp.address = (uintptr_t)GetProcAddress(mono, func.functionName))) continue;
			hp.type = HOOK_EMPTY;
			NewHook(hp, "Mono Searcher");
			ret = true;
		}
		/* Artikash 2/13/2019:
		How to hook Mono/Unity3D:
		Find all standard function prologs in memory with write/execute permission: these represent possible JIT compiled functions
		Then use Mono APIs to reflect what these functions are, and hook them if they are string member functions
		Mono calling convention uses 'this' as first argument
		Must be dynamic hook bootstrapped from other mono api or mono_domain_get won't work
		*/
		trigger_fun = [](LPVOID addr, DWORD, DWORD)
		{
			static auto getDomain = (MonoDomain * (*)())GetProcAddress(mono, "mono_domain_get");
			static auto getJitInfo = (MonoObject * (*)(MonoDomain*, uintptr_t))GetProcAddress(mono, "mono_jit_info_table_find");
			static auto getName = (char* (*)(uintptr_t))GetProcAddress(mono, "mono_pmip");
			if (!getDomain || !getName || !getJitInfo) {
				ConsoleOutput("Textractor: Mono Dynamic failed");
				return true;
			}
			static auto domain = getDomain();
			if (!domain) {
				ConsoleOutput("Textractor: Mono Dynamic failed");
				return true;
			}
			ConsoleOutput("Textractor: Mono Dynamic ENTER (hooks = %s)", *loadedConfig ? loadedConfig : "brute force");
			const BYTE prolog1[] = { 0x55, 0x48, 0x8b, 0xec };
			const BYTE prolog2[] = { 0x48, 0x83, 0xec };
			for (auto [prolog, size] : Array<const BYTE*, size_t>{ { prolog1, sizeof(prolog1) }, { prolog2, sizeof(prolog2) } })
				for (auto addr : Util::SearchMemory(prolog, size, PAGE_EXECUTE_READWRITE))
				{
					[](uint64_t addr)
					{
						__try
						{
							if (getJitInfo(domain, addr))
								if (char* name = getName(addr))
									if (strstr(name, "0x0") && ShouldMonoHook(name))
									{
										HookParam hp = {};
										hp.address = addr;
										hp.type = USING_STRING | USING_UNICODE | FULL_STRING;
										if (!*loadedConfig) hp.type |= KNOWN_UNSTABLE;
										hp.offset = -0x20; // rcx
										hp.padding = 20;
										char nameForUser[HOOK_NAME_SIZE] = {};
										strncpy_s(nameForUser, name + 1, HOOK_NAME_SIZE - 1);
										if (char* end = strstr(nameForUser, " + 0x0")) *end = 0;
										if (char* end = strstr(nameForUser, "{")) *end = 0;
										hp.length_fun = [](uintptr_t, uintptr_t data)
										{
											/* Artikash 6/18/2019:
											even though this should get the true length mono uses internally
											there's still some garbage picked up on https://vndb.org/v20403 demo, don't know why */
											int len = *(int*)(data - 4);
											return len > 0 && len < PIPE_BUFFER_SIZE ? len * 2 : 0;
										};
										NewHook(hp, nameForUser);
									}
						}
						__except (EXCEPTION_EXECUTE_HANDLER) {}
					}(addr);
				}

			if (!*loadedConfig) ConsoleOutput("Textractor: Mono Dynamic used brute force: if performance issues arise, please specify the correct hook in the game configuration");
			return true;
		failed:
			ConsoleOutput("Textractor: Mono Dynamic failed");
			return true;
		};
		return ret;
	}

	// Artikash 6/23/2019: V8 (JavaScript runtime) has rcx = string** at v8::String::Write
	// sample game https://www.freem.ne.jp/dl/win/18963
	bool InsertV8Hook(HMODULE module)
	{
		auto getV8Length = [](uintptr_t, uintptr_t data)
		{
			int len = *(int*)(data - 4);
			return len > 0 && len < PIPE_BUFFER_SIZE ? len * 2 : 0;
		};

		uint64_t addr1 = (uint64_t)GetProcAddress(module, "?Write@String@v8@@QEBAHPEAGHHH@Z"),
			// Artikash 6/7/2021: Add new hook for new version of V8 used by RPG Maker MZ
			addr2 = (uint64_t)GetProcAddress(module, "??$WriteToFlat@G@String@internal@v8@@SAXV012@PEAGHH@Z");

		if (addr1 || addr2)
		{
			std::tie(spDefault.minAddress, spDefault.maxAddress) = Util::QueryModuleLimits(module);
			spDefault.maxRecords = Util::SearchMemory(spDefault.pattern, spDefault.length, PAGE_EXECUTE, spDefault.minAddress, spDefault.maxAddress).size() * 20;
			ConsoleOutput("Textractor: JavaScript hook is known to be low quality: try searching for hooks if you don't like it");
		}
		if (addr1)
		{
			HookParam hp = {};
			hp.type = USING_STRING | USING_UNICODE | DATA_INDIRECT;
			hp.address = addr1;
			hp.offset = -0x20; // rcx
			hp.index = 0;
			hp.padding = 23;
			hp.length_fun = getV8Length;
			NewHook(hp, "JavaScript");
		}
		if (addr2)
		{
			HookParam hp = {};
			hp.type = USING_STRING | USING_UNICODE;
			hp.address = addr2;
			hp.offset = -0x20; // rcx
			hp.padding = 11;
			hp.length_fun = getV8Length;
			NewHook(hp, "JavaScript");
		}
		return addr1 || addr2;
	}

	/** Artikash 8/10/2018: Ren'py
	*
	*  Sample games: https://vndb.org/v19843 https://vndb.org/v12038 and many more OELVNs
	*
	*  Uses CPython, and links to python27.dll. PyUicodeUCS2_Format is the function used to process text.
	*  first argument. offset 0x18 from that is a wchar_t* to the actual string
	*  ebx seems to work well as the split param, not sure why
	*/
	bool InsertRenpyHook()
	{
		wchar_t python[] = L"python2X.dll", libpython[] = L"libpython2.X.dll";
		for (wchar_t* name : { python, libpython })
		{
			wchar_t* pos = wcschr(name, L'X');
			for (int pythonMinorVersion = 0; pythonMinorVersion <= 8; ++pythonMinorVersion)
			{
				*pos = L'0' + pythonMinorVersion;
				if (HMODULE module = GetModuleHandleW(name))
				{
					wcscpy_s(spDefault.exportModule, name);
					HookParam hp = {};
					hp.address = (uintptr_t)GetProcAddress(module, "PyUnicodeUCS2_Format");
					if (!hp.address)
					{
						ConsoleOutput("Textractor: Ren'py failed: failed to find PyUnicodeUCS2_Format");
						return false;
					}
					hp.offset = pusha_rcx_off - 4; // rcx
					hp.index = 0x18;
					hp.length_offset = 0;
					//hp.split = pusha_rsp_off -4;
					hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT | DATA_INDIRECT /* | USING_SPLIT*/;
					hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
					{
						static std::wstring prevText;
						auto text = reinterpret_cast<LPWSTR>(data);
						auto len =  static_cast<size_t>(*size);

						if (cpp_wcsnstr(text, L"%", len / sizeof(wchar_t)))
							return false;

						if (cpp_wcsnstr(text, L"{", len / sizeof(wchar_t))) {
							WideStringCharReplacer(text, &len, L"{i}", 3, L'\'');
							WideStringCharReplacer(text, &len, L"{/i}", 4, L'\'');
							WideStringFilterBetween(text, &len, L"{", 1, L"}", 1);
						}
						WideStringFilter(text, &len, L"^", 2); // remove ^ followed by 1 char
						WideCharReplacer(text, &len, L'\n', L' ');

						if (prevText.length() == len / sizeof(wchar_t) && prevText.find(text, 0, len / sizeof(wchar_t)) != std::string::npos) // Check if the string is the same as the previous one
							return false;
						prevText.assign(text, len / sizeof(wchar_t));
						*size = static_cast<DWORD>(len);
						return true;
					};
					NewHook(hp, "Ren'py");
					return true;
				}
			}
		}
		ConsoleOutput("Textractor: Ren'py failed: failed to find python2X.dll");
		return false;
	}
	bool InsertGodotHook2_X64() {
		//by Blu3train
		/*
		* Sample games:
		* https://vndb.org/r109138
		*/
		const BYTE bytes[] = {
			0x48, 0x8B, 0x94, 0x24, XX4,       // mov rdx,[rsp+000001C0]	<- hook here
			0x4C, 0x89, 0xE1,                  // mov rcx,r12
			0xE8, XX4,                         // call NULL-Windows.exe+D150
			0x49, 0x8B, 0x06,                  // mov rax,[r14]
			0x48, 0x85, 0xC0,                  // test rax,rax
			0x0F, 0x85, XX4                    // jne NULL-Windows.exe+A359D4

		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr;
			hp.offset = pusha_rcx_off -4; //RCX
			hp.type = USING_STRING | USING_UNICODE;
			ConsoleOutput("vnreng: INSERT Godot2_x64 Hook ");
			NewHook(hp, "Godot2_x64");
			return true;
		}

		ConsoleOutput("vnreng:Godot2_x64: pattern not found");
		return false;
	}

	bool InsertGodotHooks_X64()
	{
	  return InsertGodotHook2_X64();
	}

	bool LucaSystemFilter(LPVOID data, DWORD* size, HookParam*, BYTE) {
		auto text = reinterpret_cast<LPWSTR>(data);
		auto len = reinterpret_cast<size_t *>(size);

		if (text[0] == L'\x3000') { //removes space at the beginning of the sentence
			*len -= 2;
			::memmove(text, text + 1, *len);
		}

		if ( *text == L'@' ) //Name in square brackets instead of '@'
			if ( wchar_t *match2 = cpp_wcsnchr(text+1, L'@', *len/2-1) ) {
				*text = L'[';
				*match2 = L']';
			}

		WideStringFilterBetween(text, len, L"$C(", 3, L")", 1);
		WideStringFilter(text, len, L"$A", 3); // remove $A followed by 1 char
		WideStringCharReplacer(text, len, L"$d", 2, L'\n');
		WideCharFilter(text, len, L'\xFF3F');
		//ruby
		WideStringFilter(text, len, L"$[", 2);
		WideStringFilterBetween(text, len, L"$/", 2, L"$]", 2);

		return true;
	}

	bool LucaSystemEnFilter(LPVOID data, DWORD* size, HookParam*, BYTE) {
		auto text = reinterpret_cast<char*>(data);
		auto len =  static_cast<size_t>(*size);

		StringCharReplacer(text, &len, "\xE2\x9D\x9D", 3, '"');
		StringCharReplacer(text, &len, "\xE2\x9D\x9E", 3, '"');
		StringCharReplacer(text, &len, "\xE2\x9D\x9B", 3, '\'');
		StringCharReplacer(text, &len, "\xE2\x9D\x9C", 3, '\'');
		*size = static_cast<DWORD>(len);
		return true;
	}


	bool InsertLucaSystemHook()
	{
		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);

		//by Chenx221
		//适用于更新的LucaSystem引擎
		/*
		* Sample games:
		* https://vndb.org/r132097
		* https://vndb.org/r133677
		*/
		const BYTE bytes0[] = {
			0xCC,                                  // int 3
			0x48, XX4,								// mov qword ptr [rsp+20], rbx   <- hook here
			0x55,                                  // push rbp
			0x56,                                  // push rsi
			0x57,                                  // push rdi
			0x41, 0x54,                            // push r12
			0x41, 0x55,                            // push r13
			0x41, 0x56,                            // push r14
			0x41, 0x57,                            // push r15
			0x48, 0x8D, 0xAC, 0x24, XX4,			// lea rbp, [rsp-2050]
			0xB8, XX4,								// mov eax, 2150
			0xE8, XX4,								// call 0x00007FF71B020940
			0x48, 0x2B, 0xE0,						// sub rsp, rax
			0x48, 0x8B, 0x05, XX4,					// mov rax, qword ptr ds:[0x00007FF71B115040]
			0x48, 0x33, 0xC4,						// xor rax, rsp
			0x48, 0x89, 0x85, XX4,					// mov qword ptr ss:[rbp+0x2040], rax
			0x41, 0x8B, 0xC0						// mov eax, r8d
		};
		const BYTE bytes1[] = {
			0xCC,                                  // int 3
			0x48, XX4,								// mov qword ptr [rsp+20], rbx   <- hook here
			0x55,                                  // push rbp
			0x56,                                  // push rsi
			0x57,                                  // push rdi
			0x41, 0x54,                            // push r12
			0x41, 0x55,                            // push r13
			0x41, 0x56,                            // push r14
			0x41, 0x57,                            // push r15
			0x48, 0x8D, 0xAC, 0x24, XX4,			// lea rbp, [rsp-2050]
			0xB8, XX4,								// mov eax, 2150
			0xE8, XX4,								// call 0x00007FF71B020940
			0x48, 0x2B, 0xE0,						// sub rsp, rax
			0x48, 0x8B, 0x05, XX4,					// mov rax, qword ptr ds:[0x00007FF71B115040]
			0x48, 0x33, 0xC4,						// xor rax, rsp
			0x48, 0x89, 0x85, XX4,					// mov qword ptr ss:[rbp+0x2040], rax
			0x48, 0x8B, XX							// mov ???, rdx
		};

		for (auto addr : Util::SearchMemory(bytes0, sizeof(bytes0), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = pusha_rdx_off -4; //RDX
			hp.filter_fun = LucaSystemEnFilter;
			hp.type = USING_UTF8 | USING_STRING | DATA_INDIRECT;
			ConsoleOutput("vnreng: INSERT LucaSystemEN Hook ");
			NewHook(hp, "LucaSystemNewEN");
			break;
		}
		for (auto addr : Util::SearchMemory(bytes1, sizeof(bytes1), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = pusha_rdx_off -4; //RDX
			hp.filter_fun = LucaSystemFilter;
			hp.split = -0x14 - 4; //RBX RCX RDI ok?
			hp.type = USING_UNICODE | USING_STRING | DATA_INDIRECT | USING_SPLIT;
			ConsoleOutput("vnreng: INSERT LucaSystem Hook (new)");
			NewHook(hp, "LucaSystemNew");
			break;
		}

		//by Blu3train
		/*
		* Sample games:
		* https://vndb.org/r108105
		*/
		const BYTE bytes[] = {
			0xCC,                                // int 3
			0x48, XX4,                           // mov [rsp+18],rbx       <- hook here
			0x55,                                // push rbp
			0x56,                                // push rsi
			0x57,                                // push rdi
			0x41, 0x54,                          // push r12
			0x41, 0x55,                          // push r13
			0x41, 0x56,                          // push r14
			0x41, 0x57,                          // push r15
			0x48, 0x8D, 0xAC, 0x24, XX4,         // lea rbp,[rsp-00003810]
			0xB8, XX4                            // mov eax,00003910
		};

		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = pusha_rdx_off -4; //RDX
			hp.filter_fun = LucaSystemFilter;
			hp.type = USING_UNICODE | USING_STRING;
			ConsoleOutput("vnreng: INSERT LucaSystem Hook ");
			NewHook(hp, "LucaSystem");
			return true;
		}

		ConsoleOutput("vnreng:LucaSystem: pattern not found");
		return false;
	}

	bool InsertKiriKiriZHook()
	{
		//by Blu3train
		/*
		* Sample games:
		* RJ351843
		*/
		const BYTE bytes[] = {
			0xCC,                                                // int 3
			0x4C, 0x89, 0x44, 0x24, 0x18,                        // mov [rsp+18],r8       <- hook here
			0x48, 0x89, 0x54, 0x24, 0x10,                        // mov [rsp+10],rdx
			0x53,                                                // push rbx
			0x56,                                                // push rsi
			0x57,                                                // push rdi
			0x41, 0x54,                                          // push r12
			0x41, 0x55,                                          // push r13
			0x41, 0x56,                                          // push r14
			0x41, 0x57,                                          // push r15
			0x48, 0x83, 0xEC, 0x40,                              // sub rsp,40
			0x48, 0xC7, 0x44, 0x24, 0x30, 0xFE, 0xFF, 0xFF, 0xFF // mov qword ptr [rsp+30],FFFFFFFFFFFFFFFE
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = -0x1C -4; //RCX
			hp.index = 0x18;
			hp.length_offset = 1;
			hp.type = USING_UNICODE | DATA_INDIRECT;
			ConsoleOutput("vnreng: INSERT KiriKiriZ Hook ");
			NewHook(hp, "KiriKiriZ");
			return true;
		}

		ConsoleOutput("vnreng:KiriKiriZ: pattern not found");
		return false;
	}

	bool hookv8addr(HMODULE module) {
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		bool ok = false;
		const BYTE bytes[] = {
			0x44,0x0f,0xb7,0xe8,
			0x89,0xc1,
			0x81,0xe1,0x00,0xfc,0x00,0x00,
			0x81,0xf9,0x00,0xd8,0x00,0x00
			//���⤪��.exe
			// �ú����ṹ���»�������ȫһ��������֪��Ϊʲô���ǲ���ȡ����
			//_QWORD *__fastcall sub_14150CF00(_QWORD *a1, _QWORD *a2, __int64 a3, char a4)
			//.text:000000014150D0CC 44 0F B7 E8                   movzx   r13d, ax
			//.text:000000014150D0D0 89 C1                         mov     ecx, eax
			//.text : 000000014150D0D2 81 E1 00 FC 00 00 and ecx, 0FC00h
			//.text : 000000014150D0D8 81 F9 00 D8 00 00             cmp     ecx, 0D800h
		};
		auto addrs = Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE_READ, minAddress, maxAddress);
		for (auto addr : addrs) {
			HookParam hp = {};
			hp.address = addr;

			hp.offset = -8;

			hp.type = USING_UNICODE | NO_CONTEXT;
			hp.length_offset = 1;
			ConsoleOutput("Textractor: INSERT extra_v8addr  %p", addr);

			NewHook(hp, "extra_v8addr");
			ok = true;
		}
		if(!ok)ConsoleOutput("extra_v8addr not found");
		return ok;
	}
	bool hookv8exports(HMODULE module) {
		ConsoleOutput("inter extra_v8orcef check");
		struct pairs_t {
			const BYTE* bytes;
			int size;
			int off;
		};
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		bool ok = false;


		struct libcefFunction { // argument indices start from 0 for SpecialHookMonoString, otherwise 1
			const char* functionName;
			size_t textIndex; // argument index
			short lengthIndex; // argument index
			unsigned long hookType; // HookParam type
			void(*text_fun)(uintptr_t stack, HookParam* hp, BYTE obsoleteAlwaysZero, uintptr_t* data, uintptr_t* split, DWORD* len); // HookParam::text_fun_t
		};

		HookParam hp = {};
		enum {
			r8=-0x24-40 , //0x4c
			rdx=-0x24

		};
		const libcefFunction funcs[] = {
			{"?WriteUtf8@String@v8@@QEBAHPEAVIsolate@2@PEADHPEAHH@Z",r8,0,USING_STRING | USING_UTF8 | NO_CONTEXT,NULL}, //ok
			{"?WriteUtf8@String@v8@@QEBAHPEADHPEAHH@Z",rdx,0,USING_STRING | USING_UTF8 | NO_CONTEXT,NULL}, //ok
			{"?WriteOneByte@String@v8@@QEBAHPEAVIsolate@2@PEAEHHH@Z",r8,0,USING_STRING | USING_UTF8 | NO_CONTEXT,NULL}, //ok
			{"?WriteOneByte@String@v8@@QEBAHPEAEHHH@Z",rdx,0,USING_STRING | USING_UTF8 | NO_CONTEXT,NULL}, //ok
			{"?Write@String@v8@@QEBAHPEAVIsolate@2@PEAGHHH@Z",r8,0,USING_STRING | USING_UNICODE | NO_CONTEXT,NULL}, //ok
			{"?Write@String@v8@@QEBAHPEAGHHH@Z",rdx,0,USING_STRING | USING_UNICODE | NO_CONTEXT,NULL}, //ok
			{"?NewFromUtf8@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PEAVIsolate@2@PEBDW4NewStringType@2@H@Z",r8,0,USING_STRING | USING_UTF8 | NO_CONTEXT,NULL}, //ok
			{"?NewFromTwoByte@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PEAVIsolate@2@PEBGW4NewStringType@2@H@Z",r8,0,USING_STRING | USING_UNICODE | NO_CONTEXT,NULL}, //ok
			{"?NewFromOneByte@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PEAVIsolate@2@PEBEW4NewStringType@2@H@Z",r8,0,USING_STRING | USING_UTF8 | NO_CONTEXT,NULL}, //ok
		};

		for (auto func : funcs) {
			if (FARPROC addr = ::GetProcAddress(module, func.functionName)) {
				hp.address = (uint64_t)addr ;
				hp.type = func.hookType;
				hp.offset = func.textIndex-4 ;
				hp.length_offset = func.lengthIndex * 4;
				hp.text_fun = func.text_fun;
				ConsoleOutput("vnreng: libcef: INSERT  %p", hp.address);
				NewHook(hp, "extra_v8exports");
				ok = true;
			}
		}


		if (!ok)ConsoleOutput("extra_v8exports not found");
		return ok;
	}

	bool checkv8orcef() {
		ConsoleOutput("checking v8cef");
		for (HMODULE module : { (HMODULE)processStartAddress, GetModuleHandleW(L"node.dll"), GetModuleHandleW(L"nw.dll") })
			if (GetProcAddress(module, "?Write@String@v8@@QEBAHPEAGHHH@Z")) {
				bool ok1 = hookv8addr(module);
				bool ok2= hookv8exports(module);
				if (ok1 || ok2)return true;
			}

		auto hm = GetModuleHandleW(L"libcef.dll");
		if (hm) {
			//todo
		}

		return false;
	}

	bool InsertArtemis64Hook()
	{
		const BYTE BYTES[] = {
			0x48,0x89,0x5C,0x24,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x60
			//__int64 __fastcall sub_14017A760(__int64 a1, char *a2, char **a3)
			//FLIP FLOP IO
		};
		auto addrs = Util::SearchMemory(BYTES, sizeof(BYTES), PAGE_EXECUTE_READ, processStartAddress, processStopAddress);
		for (auto addr : addrs) {
			char info[1000] = {};
			sprintf(info, "Textractor: InsertArtemis64Hook %08x", addr);
			ConsoleOutput(info);
			HookParam hp = {};
			hp.address = addr;
			hp.type = USING_UTF8 | USING_STRING;
			hp.offset = -0x24 - 4;//rdx
			NewHook(hp, "Artemis64");
			return true;
		}

		ConsoleOutput("Textractor: InsertArtemis64Hook failed");
		return false;
	}

	bool InsertArtemisHook() {
		//by Blu3train
		/*
		* Sample games:
		* https://vndb.org/v45247
		*/
		const BYTE bytes[] = {
			0xCC,                               // int 3
			0x40, 0x57,                         // push rdi          <- hook here
			0x48, 0x83, 0xEC, 0x40,             // sub rsp,40
			0x48, 0xC7, 0x44, 0x24, 0x30, XX4,  // mov qword ptr [rsp+30],FFFFFFFFFFFFFFFE
			0x48, 0x89, 0x5C, 0x24, 0x50        // mov [rsp+50],rbx
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = -0x44 - 4; //RDI
			hp.type = USING_STRING | USING_UTF8 | NO_CONTEXT;
			ConsoleOutput("vnreng: INSERT Artemis Hook ");
			NewHook(hp, "Artemis");
			return true;
		}

		// For newer Artemis x64 engine
		// By Chenx221
		/*
			Sample games :
			https://vndb.org/v54502
			https://vndb.org/v50081
			https://vndb.org/v33348
		*/
		const BYTE bytes2[] = {
			0xCC,                               // int3
			0x48, 0x89, 0x5C, 0x24, 0x08,       // mov qword ptr ss:[rsp+8],rbx
			0x48, 0x89, 0x74, 0x24, 0x10,       // mov qword ptr ss:[rsp+10],rsi
			0x57,                               // push rdi                     <- hook here
			0x48, 0x83, 0xEC, 0x30,             // sub rsp,30
			0x49, 0x8B, 0xD8                    // mov rbx,r8
		};
		for (auto addr : Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 0xB;
			hp.offset = pusha_rdi_off - 4; //RDI
			hp.type = USING_STRING | USING_UTF8; // | NO_CONTEXT;
			ConsoleOutput("vnreng: INSERT Artemis New Hook");
			NewHook(hp, "Artemis");
			return true;
		}

		ConsoleOutput("vnreng:Artemis: pattern not found");
		return false;
	}

	bool InsertArtemisExHook() {
		const BYTE bytes[] = {
			0xCC,                               // int 3
			0x48, 0x89, 0x5C, 0x24, 0x20,       // mov qword ptr ss:[rsp+20],rbx	// hook here RDX
			0x55,								// push rbp
			0x56,								// push rsi
			0x57,								// push rdi
			0x41, 0x54,							// push r12
			0x41, 0x55,							// push r13
			0x41, 0x56,							// push r14
			0x41, 0x57,							// push r15
			0x48, 0x83, 0xEC, 0x60				// sub rsp,60
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = -0x24 - 4; //RDI
			hp.type = USING_STRING | USING_UTF8;
			ConsoleOutput("vnreng: INSERT ArtemisEx Hook ");
			NewHook(hp, "ArtemisEx");
			return true;
		}

		// For newer Artemis x64 engine
		// By Chenx221
		//CC 48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8B EC
		const BYTE bytes2[] = {
			0xCC,                               // int3
			0x48, 0x89, 0x5C, 0x24, 0x20,		// mov qword ptr ss:[rsp+0x20], rbx // hook here RDX
			0x55,								// push rbp
			0x56,								// push rsi
			0x57,								// push rdi
			0x41, 0x54,							// push r12
			0x41, 0x55,							// push r13
			0x41, 0x56,							// push r14
			0x41, 0x57,							// push r15
			0x48, 0x8B, 0xEC					// mov rbp, rsp
		};
		for (auto addr : Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 0x1;
			hp.offset = pusha_rdx_off - 4; //RDI
			hp.type = USING_STRING | USING_UTF8; // | NO_CONTEXT;
			ConsoleOutput("vnreng: INSERT ArtemisEx New Hook ");
			NewHook(hp, "ArtemisEx");
			return true;
		}

		ConsoleOutput("vnreng:Artemis: pattern not found");
		return false;
	}

	bool InsertRenpy3Hook()
	{
		//by Blu3train
		/*
		* Sample games:
		* https://vndb.org/v45820
		* https://vndb.org/v26318
		* https://vndb.org/v39954
		* https://vndb.org/r110220
		* https://vndb.org/r114981
		* https://vndb.org/v33647
		* https://vndb.org/r73160
		* https://vndb.org/v44518
		* https://vndb.org/v31994
		* https://vndb.org/r115756
		*/
		const wchar_t* patterns[] = {
			L"python3.%d.dll",
			L"libpython3.%d.dll"
		};
		for (const auto& pattern : patterns)
		{
			for (int pythonMinorVersion = 0; pythonMinorVersion <= 9; ++pythonMinorVersion)
			{
				wchar_t name[64];
				swprintf(name, 64, pattern, pythonMinorVersion);
				if (HMODULE module = GetModuleHandleW(name))
				{
					wcscpy_s(spDefault.exportModule, name);
					HookParam hp = {};
					hp.address = (uintptr_t)GetProcAddress(module, "PyUnicode_Format");
					if (!hp.address)
					{
						ConsoleOutput("Textractor: Ren'py 3 failed: failed to find PyUnicode_Format");
						return false;
					}
					hp.offset = pusha_rcx_off - 4; // rcx
					hp.padding = 0x48;
					hp.length_offset = 0;
					hp.text_fun = [](uintptr_t rsp_base, HookParam* pHp, BYTE, uintptr_t* data, uintptr_t* split, DWORD* count)
					{
						uint64_t r8 = regof(r8, rsp_base);
						uint64_t r10 = regof(r10, rsp_base);
						uint64_t r11 = regof(r11, rsp_base);
						if (r10 == 0x03FF || r11 == 0x03FF || (r8 == r10 && r11 == 0x7F)) {
							uint64_t rcx = regof(rcx, rsp_base);
							BYTE unicode = !(*(BYTE*)(rcx + 0x20) & 0x40); // [rcx+0x20) bit 0x40 == 0
							if (unicode) {
								*data += 0x48; //padding
								*count = wcslen((wchar_t*)*data) * sizeof(wchar_t);
								return;
							}
						}
						*count = 0;
					};
					hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
					hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
					{
						auto text = reinterpret_cast<LPWSTR>(data);
						auto len =  static_cast<size_t>(*size);

						if (cpp_wcsnstr(text, L"%", len / sizeof(wchar_t)))
							return false;
						if (cpp_wcsnstr(text, L"{", len / sizeof(wchar_t))) {
							WideStringCharReplacer(text, &len, L"{i}", 3, L'\'');
							WideStringCharReplacer(text, &len, L"{/i}", 4, L'\'');
							WideStringFilterBetween(text, &len, L"{", 1, L"}", 1);
						}

						//CP_OEMCP -The current system OEM code page
						WideCharToMultiByte(CP_OEMCP, 0, text, -1, text_buffer, 0x1000, NULL, NULL);
						text_buffer_length = len / sizeof(wchar_t); // saved for not unicode hook
						*size = static_cast<DWORD>(len);
						return true;
					};
					NewHook(hp, "Ren'py 3 unicode");

					hp.address += 6;
					hp.padding = 0x30;
					hp.text_fun = [](uintptr_t rsp_base, HookParam* pHp, BYTE, uintptr_t* data, uintptr_t* split, DWORD* count)
					{
						uint64_t r8 = regof(r8, rsp_base);
						uint64_t r10 = regof(r10, rsp_base);
						uint64_t r11 = regof(r11, rsp_base);
						if (r10 == 0x03FF || r11 == 0x03FF || (r8 == r10 && r11 == 0x7F)) {
							uint64_t rcx = regof(rcx, rsp_base);
							BYTE unicode = !(*(BYTE*)(rcx + 0x20) & 0x40); // [rcx+0x20) bit 0x40 == 0

							*data += unicode ? 0x48 : 0x30; //padding
							*count = ::strlen((char*)*data);
							if (!cpp_strnstr((char*)*data, "%", *count)) // not garbage
								return;
						}
						*count = 0;
					};
					hp.type = USING_STRING | NO_CONTEXT;
					hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
					{
						auto text = reinterpret_cast<LPSTR>(data);
						size_t len = static_cast<size_t>(*size);

						if (text[0] != 0 && text[1] == 0) {
							// text from unicode hook
							len = text_buffer_length;
							::memmove(text, text_buffer, len);
						}
						if (cpp_strnstr(text, "%", len))
							return false;
						if (cpp_strnstr(text, "{", len)) {
							StringCharReplacer(text, &len, "{i}", 3, L'\'');
							StringCharReplacer(text, &len, "{/i}", 4, L'\'');
							StringFilterBetween(text, &len, "{", 1, "}", 1);
						}
						*size = static_cast<DWORD>(len);
						return true;
					};
					NewHook(hp, "Ren'py 3");

					return true;
				}
			}
			for (int pythonMinorVersion = 12; pythonMinorVersion <= 20; ++pythonMinorVersion)
			{
				wchar_t name[64];
				swprintf(name, 64, pattern, pythonMinorVersion);
				if (HMODULE module = GetModuleHandleW(name))
				{
					wcscpy_s(spDefault.exportModule, name);
					HookParam hp = {};
					hp.address = (uintptr_t)GetProcAddress(module, "PyUnicode_Format");
					if (!hp.address)
					{
						ConsoleOutput("Textractor: Ren'py 3 failed: failed to find PyUnicode_Format");
						return false;
					}
					hp.offset = pusha_rcx_off - 4; // rcx
					hp.padding = 0x38;
					hp.length_offset = 0;
					hp.text_fun = [](uintptr_t rsp_base, HookParam* pHp, BYTE, uintptr_t* data, uintptr_t* split, DWORD* count)
					{
						uint64_t r8 = regof(r8, rsp_base);
						uint64_t r10 = regof(r10, rsp_base);
						uint64_t r11 = regof(r11, rsp_base);
						// if (r10 == 0x03FF || r11 == 0x03FF || (r8 == r10 && r11 == 0x7F)) {
							uint64_t rcx = regof(rcx, rsp_base);
							BYTE unicode = !(*(BYTE*)(rcx + 0x20) & 0x40); // [rcx+0x20) bit 0x40 == 0
							if (unicode) {
								*data += 0x38; //padding
								*count = wcslen((wchar_t*)*data) * sizeof(wchar_t);
								return;
							}
						// }
						*count = 0;
					};
					hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
					hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
					{
						auto text = reinterpret_cast<LPWSTR>(data);
						auto len =  static_cast<size_t>(*size);

						if (cpp_wcsnstr(text, L"%", len / sizeof(wchar_t)))
							return false;
						if (cpp_wcsnstr(text, L"{", len / sizeof(wchar_t))) {
							WideStringCharReplacer(text, &len, L"{i}", 3, L'\'');
							WideStringCharReplacer(text, &len, L"{/i}", 4, L'\'');
							WideStringFilterBetween(text, &len, L"{", 1, L"}", 1);
						}

						//CP_OEMCP -The current system OEM code page
						WideCharToMultiByte(CP_OEMCP, 0, text, -1, text_buffer, 0x1000, NULL, NULL);
						text_buffer_length = len / sizeof(wchar_t); // saved for not unicode hook
						*size = static_cast<DWORD>(len);
						return true;
					};
					NewHook(hp, "Ren'py 3 unicode");

					hp.address += 6;
					hp.padding = 0x28;
					hp.text_fun = [](uintptr_t rsp_base, HookParam* pHp, BYTE, uintptr_t* data, uintptr_t* split, DWORD* count)
					{
						uint64_t r8 = regof(r8, rsp_base);
						uint64_t r10 = regof(r10, rsp_base);
						uint64_t r11 = regof(r11, rsp_base);
						// if (r10 == 0x03FF || r11 == 0x03FF || (r8 == r10 && r11 == 0x7F)) {
							uint64_t rcx = regof(rcx, rsp_base);
							BYTE unicode = !(*(BYTE*)(rcx + 0x20) & 0x40); // [rcx+0x20) bit 0x40 == 0

							*data += unicode ? 0x38 : 0x28; //padding
							*count = ::strlen((char*)*data);
							if (!cpp_strnstr((char*)*data, "%", *count)) // not garbage
								return;
						// }
						*count = 0;
					};
					hp.type = USING_STRING | NO_CONTEXT;
					hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
					{
						auto text = reinterpret_cast<LPSTR>(data);
						size_t len = static_cast<size_t>(*size);

						if (text[0] != 0 && text[1] == 0) {
							// text from unicode hook
							len = text_buffer_length;
							::memmove(text, text_buffer, len);
						}
						if (cpp_strnstr(text, "%", len))
							return false;
						if (cpp_strnstr(text, "{", len)) {
							StringCharReplacer(text, &len, "{i}", 3, L'\'');
							StringCharReplacer(text, &len, "{/i}", 4, L'\'');
							StringFilterBetween(text, &len, "{", 1, "}", 1);
						}
						*size = static_cast<DWORD>(len);
						return true;
					};
					NewHook(hp, "Ren'py 3");

					return true;
				}
			}
		}
		ConsoleOutput("Textractor: Ren'py 3 failed: failed to find python3X.dll");
		return false;
	}

	bool InsertRenpy3NewHook()
	{
		//by Chenx221
		/* Tested:
		* https://vndb.org/v50148 (renpy 8.3.3)
		*/
		const wchar_t* patterns[] = {
			L"python3.%d.dll",
			L"libpython3.%d.dll"
		};
		for (const auto& pattern : patterns)
		{
			for (int pythonMinorVersion = 0; pythonMinorVersion <= 9; ++pythonMinorVersion)
			{
				wchar_t name[64];
				swprintf(name, 64, pattern, pythonMinorVersion);
				if (HMODULE module = GetModuleHandleW(name))
				{
					wcscpy_s(spDefault.exportModule, name);
					HookParam hp = {};
					hp.address = (uintptr_t)GetProcAddress(module, "_PyUnicodeWriter_WriteStr");
					if (!hp.address)
					{
						ConsoleOutput("Textractor: Ren'py 3 New failed: failed to find _PyUnicodeWriter_WriteStr");
						return false;
					}
					hp.offset = pusha_rdi_off - 4; // rdi
					hp.length_offset = 0;
					hp.text_fun = [](uintptr_t rsp_base, HookParam* pHp, BYTE, uintptr_t* data, uintptr_t* split, DWORD* count)
					{
						uint64_t r11 = regof(r11, rsp_base);
						if (r11 == 0x7F) {
							*count = wcslen((wchar_t*)*data) * sizeof(wchar_t);
							return;
						}
						*count = 0;
					};
					hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
					NewHook(hp, "Ren'py 3 New unicode");
					// Q. Why is non-Unicode not supported?
					// A. Check _PyUnicodeWriter_WriteStr
					return true;
				}
			}
		}
		ConsoleOutput("Textractor: Ren'py 3 New failed: failed to find python3X.dll");
		return false;
	}

	bool InsertBGI64Hook()
	{
		bool found = false;
		const BYTE pattern[] = {
			0x48, 0x89, 0x54, 0x24, 0x10,   // mov qword ptr ss:[rsp+10], rdx // split
			0x48, 0x89, 0x4C, 0x24, 0x08,   // mov qword ptr ss:[rsp+8], rcx // char
			0x53,                           // push rbx // <-- hook here
			0x55,                           // push rbp
			0x57,                           // push rdi
			0x41, 0x55,                     // push r13
			0x41, 0x56,                     // push r14
			0x48, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00, // sub rsp, 100
			0x8B, 0xAC, 0x24, 0x50, 0x01, 0x00, 0x00  // mov ebp, dword ptr ss:[rsp+150]
		};


		for (auto addr : Util::SearchMemory(pattern, sizeof(pattern), PAGE_EXECUTE, processStartAddress, processStopAddress))
		{
			HookParam hp = {};
			hp.address = addr + 10;
			hp.offset = 8;
			hp.split = 0x10;
			hp.length_offset = 1;
			hp.type = USING_UNICODE | USING_SPLIT;
			hp.filter_fun = [](LPVOID data, DWORD *size, HookParam *, BYTE)
			{
				auto text = reinterpret_cast<LPWSTR>(data);
				auto len = reinterpret_cast<size_t *>(size);
				size_t wcharLen = *len / sizeof(wchar_t);

				if (wcharLen == 1 && text[0] == 0x3000)
					return false;
				return true;
			};
			ConsoleOutput("Textractor: INSERT BGI64");
			NewHook(hp, "BGI64");
			found = true;
		}

		const BYTE pattern2[] = {
			0x40, 0x53,						// push rbx							// RBP
			0x48, 0x8B, 0x44, 0x24, 0x38,	// mov rax,qword ptr ss:[rsp+38]
			0x45, 0x8B, 0xD1,				// mov r10d,r9d
			0x4C, 0x8B, 0x4C, 0x24, 0x30,	// mov r9,qword ptr ss:[rsp+30]
			0x45, 0x8B, 0xD8,				// mov r11d,r8d
			0x48, 0x8B, 0xCA,				// mov rcx,rdx
			0x48, 0x89, 0x44, 0x24, 0x30,	// mov qword ptr ss:[rsp+30],rax
			0x45, 0x8B, 0xC2,				// mov r8d,r10d
			0x41, 0x8B, 0xD3,				// mov edx,r11d
			0x5B							// pop rbx
		};

		for (auto addr : Util::SearchMemory(pattern2, sizeof(pattern2), PAGE_EXECUTE, processStartAddress, processStopAddress))
		{
			HookParam hp = {};
			hp.address = addr;
			hp.offset = pusha_rbp_off - 4;
			hp.type = USING_UTF8 | USING_STRING;
			hp.filter_fun = [](void* str, DWORD* size, HookParam* hp, BYTE index) { // REGEX Filter: <r.*?>|<\/r>
				if (*size < 1)
					return false;
				auto text = reinterpret_cast<char*>(str);
				size_t len = *size;

				std::regex pattern(R"(<[rc].*?>|<\/[rc]>|<FS.*?>)");
				RegexReplacer(text, &len, pattern, "");
				*size = len;
				return true;
			};
			ConsoleOutput("Textractor: INSERT BGI64Ex");
			NewHook(hp, "BGI64Ex");
			found = true;
		}
		if (!found) ConsoleOutput("Textractor:BGI64: pattern not found");
		return found;
	}

	int getGodoStringLength(uintptr_t stack, uintptr_t data) {
		int len = *(int*)(data - 4);
		len--;
		int checkLength = len > 0 && len < PIPE_BUFFER_SIZE ? len : 0;
		//检查是否为错误的unicode字符
		for (size_t i = 0; i < checkLength; i++)
		{
			if (*(WORD*)(data + i * 2) == 0x0)
				return 0;
		}
		return checkLength * 2;

	}
	//BY:IOV
	bool InsertGodotHook_X64() {
		bool flag = false;
		const BYTE bytes[] = { 0x8B,0x40,0xFC,0x83,0xF8,0x01,0x83,0xD0,0xFF,0x41,0x39,0xC6 };

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam myhp = {};
			myhp.address = addr;

			myhp.type = USING_STRING | USING_UNICODE | NO_CONTEXT; // /HQ 不使用上下文区分 把所有线程的文本都提取
			//myhp.padding = 0xc;//[esp+4]+padding
			// data_offset
			myhp.offset = -0xC-4;//RCX
			myhp.length_fun = getGodoStringLength;
			char nameForUser[HOOK_NAME_SIZE] = "RichTextLabel_add_text";
			NewHook(myhp, nameForUser);
			ConsoleOutput("Insert: Godot_add_text_X64 Hook ");
			flag = true;
		}

		// Godot 4.3 RichTextLabel_set_text
		// 56 53 48 83 EC ?? ?? ?? ?? 48 89 CB 48 89 D6 48 85 C0 74 ?? 83 78 ?? ?? 77 ?? 48 8B 93
		const BYTE bytes2[] = { 0x56,0x53,0x48,0x83,0xEC,XX4,0x48,0x89,0xCB,0x48,0x89,0xD6,0x48,0x85,0xC0,0x74,XX,0x83,0x78,XX2,0x77,XX,0x48,0x8B,0x93 };
		for (auto addr : Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam myhp = {};
			myhp.address = addr + 0x9;
			myhp.type = USING_STRING | USING_UNICODE | NO_CONTEXT | USING_SPLIT;
			myhp.offset = pusha_rax_off - 4;
			myhp.split = pusha_rcx_off - 4;
			myhp.null_length = 4; // 不推荐的做法 但又不是不能用 文本是utf-32
			char nameForUser[HOOK_NAME_SIZE] = "RichTextLabel_set_text";
			NewHook(myhp, nameForUser);
			ConsoleOutput("Insert: Godot_RichTextLabel_set_text_X64 Hook");
			flag = true;
		}

		// Godot 4.4.1 RichTextLabel_set_text
		// 48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? ?? ?? ?? 33 ED 48 8B D9 4D 85 C0
		// 等一个同版本的godot游戏出现

		// Godot 4.6 RichTextLabel_set_text
		// 53 48 83 EC ?? 49 89 D0 ?? ?? ?? 48 89 CB
		// 同上

		if (!flag)
			ConsoleOutput("vnreng:Godot_x64: pattern not found");
		return flag;
	}

	// TMPro_TMP_Text__set_text
	bool InsertUnityIl2TMPTextHook(HMODULE module) {
		const BYTE bytes[] = {
			0xCC, 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x80, 0xB9, 0xE8, 0x00, 0x00, 0x00, 0x00
		};
		bool flag = false;
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		for (auto addr: Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, minAddress, maxAddress)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.type = USING_STRING | USING_UNICODE;
			hp.offset = pusha_rdx_off - 4;
			hp.padding = 0x14;
			hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
			{
				auto text = static_cast<LPWSTR>(data);
				auto len =  static_cast<size_t>(*size);
				if (len == 0)
					return false;
				std::wregex pattern(LR"(<[^>]+?>)");
				RegexReplacerW(text, &len, pattern, L"");
				*size = static_cast<DWORD>(len);
				return true;
			};
			NewHook(hp, "TMPro_set_text");
			ConsoleOutput("Insert: Unity IL2cpp TMPro Text Hook");
			flag = true;
		}
		if (!flag) {
			// #2
			// 48 89 5C 24 ?? 57 48 83 EC ?? 80 B9 ?? ?? ?? ?? ?? 48 8B FA 48 8B D9 75 ?? 48 83 B9
			const BYTE bytes1[] = {
				0xCC, 0x48, 0x89, 0x5C, 0x24, XX, 0x57, 0x48, 0x83, 0xEC, XX, 0x80, 0xB9, XX4, XX, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x75, XX, 0x48, 0x83, 0xB9
			};
			for (auto addr: Util::SearchMemory(bytes1, sizeof(bytes1), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr + 1;
				hp.type = USING_STRING | USING_UNICODE;
				hp.offset = pusha_rdx_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					if (len == 0)
						return false;
					std::wregex pattern(LR"(<[^>]+?>)");
					RegexReplacerW(text, &len, pattern, L"");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "TMPro_set_text");
				ConsoleOutput("Insert: Unity IL2cpp TMPro Text Hook");
				flag = true;
			}
		}
		if (!flag)
			ConsoleOutput("Textractor:UnityIL2TMPTextHook: pattern not found");
		return flag;
	}
	// TMPro_TMP_Text__SetText
	bool InsertUnityIl2TMPSetTextHook(HMODULE module) {
		const BYTE bytes[] = {
			0xCC, 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x48, 0x85, 0xD2, 0x75
		};
		bool flag = false;
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		for (auto addr: Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, minAddress, maxAddress)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.type = USING_STRING | USING_UNICODE;
			hp.offset = pusha_rdx_off - 4;
			hp.padding = 0x14;
			hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
			{
				auto text = static_cast<LPWSTR>(data);
				auto len =  static_cast<size_t>(*size);
				if (len == 0)
					return false;
				std::wregex pattern(LR"(<[^>]+?>)");
				RegexReplacerW(text, &len, pattern, L"");
				*size = static_cast<DWORD>(len);
				return true;
			};
			NewHook(hp, "TMPro_SetText");
			ConsoleOutput("Insert: Unity IL2cpp TMPro SetText Hook");
			flag = true;
		}
		if (!flag) {
			// #2
			// 48 8B C4 48 89 58 ?? 48 89 68 ?? 48 89 70 ?? 57 41 54 41 55 41 56 41 57 48 83 EC ?? 33 FF
			const BYTE bytes1[] = {
				0xCC, 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, XX, 0x48, 0x89, 0x68, XX, 0x48, 0x89, 0x70, XX, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, XX, 0x33, 0xFF
			};
			for (auto addr: Util::SearchMemory(bytes1, sizeof(bytes1), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr + 1;
				hp.type = USING_STRING | USING_UNICODE;
				hp.offset = pusha_rdx_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					if (len == 0)
						return false;
					std::wregex pattern(LR"(<[^>]+?>)");
					RegexReplacerW(text, &len, pattern, L"");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "TMPro_SetText");
				ConsoleOutput("Insert: Unity IL2cpp TMPro SetText Hook");
				flag = true;
			}
		}
		if (!flag)
			ConsoleOutput("Textractor:UnityIL2TMPSetTextHook: pattern not found");
		return flag;
	}

	bool InsertUnityIL2TMPHook() {
		HMODULE module = GetModuleHandleW(L"GameAssembly.dll");
		bool b = InsertUnityIl2TMPTextHook(module);
		b|= InsertUnityIl2TMPSetTextHook(module);
		return b;
	}

	bool InsertUnityIL2UtageHook() {
		// Utage.TextData__ctor
		// Tested: twinkle_starknightsX.exe (ティンクルスターナイツX)
		// CC 48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 0F 29 74 24 ?? 48 8B F2 48 8B F9 80 3D
		const BYTE bytes[] = {
			0xCC, 0x48, 0x89, 0x5C, 0x24, XX, 0x48, 0x89, 0x74, 0x24, XX, 0x57, 0x48, 0x83, 0xEC, XX, 0x0F, 0x29, 0x74, 0x24, XX, 0x48, 0x8B, 0xF2, 0x48, 0x8B, 0xF9, 0x80, 0x3D
		};
		HMODULE module = GetModuleHandleW(L"GameAssembly.dll");
		bool flag = false;
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		for (auto addr: Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, minAddress, maxAddress)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.type = USING_STRING | USING_UNICODE;
			hp.offset = pusha_rdx_off - 4;
			hp.padding = 0x14;
			hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
			{
				auto text = static_cast<LPWSTR>(data);
				auto len =  static_cast<size_t>(*size);
				if (len == 0)
					return false;
				std::wregex pattern(LR"(<[^>]+?>)");
				RegexReplacerW(text, &len, pattern, L"");
				*size = static_cast<DWORD>(len);
				return true;
			};
			NewHook(hp, "Utage_TextData__ctor");
			ConsoleOutput("Insert: Unity IL2cpp Utage TextData Hook");
			flag = true;
		}
		if (flag) return flag;
		ConsoleOutput("Textractor:UnityIL2UtageHook: pattern not found");
		return false;
	}

	bool InsertUnityIl2GameSpHook() {
		HMODULE module = GetModuleHandleW(L"GameAssembly.dll");
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);

		// Mahou Shoujo no Majo Saiban (Steam)
		// https://vndb.org/v50283
		// Naninovel_UI_UITextPrinterPanel__FormatMessage
		if (Util::CheckFile(L"manosaba.exe") && Util::CheckFile(L"manosaba_Data")) {
			const BYTE bytes[] = {
				0x48, 0x89, 0x45, 0x10, 0x4C, 0x8D, 0x25, XX4, 0x44, 0x39, 0x35 //, XX4
			};
			for (auto addr: Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr;
				hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
				hp.offset = pusha_rax_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					std::wregex pattern(LR"(<[^>]+?>)");
					RegexReplacerW(text, &len, pattern, L"");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "Unity_IL2cpp_SP_manosaba");
				ConsoleOutput("Insert: Unity IL2cpp Game SP Hook (manosaba)");
				return true;
			}
		}

		// Donskoy.UI_MessageWindow.BeginMessage
		// みんなの育乳アカデミー ～桃色おっぱい研修録～ 体験版
		if (Util::CheckFile(L"ikunyu.exe") && Util::CheckFile(L"ikunyu_Data")) {
			const BYTE bytes2[] = {
				0x48, 0x89, 0x5C, 0x24, XX, 0x48, 0x89, 0x54, 0x24, XX, 0x57, 0x48, 0x83, 0xEC, XX, 0x48, 0x8B, 0xFA
			};
			for (auto addr: Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr;
				hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
				hp.offset = pusha_rdx_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					std::wregex pattern(LR"(\([/]?f[^)]*\)|<([^,]+),[^>]+>)");
					RegexReplacerW(text, &len, pattern, L"$1");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "Unity_IL2cpp_SP_Donskoy_UI_MessageWindow");
				ConsoleOutput("Insert: Unity IL2cpp Game SP Hook (Donskoy.UI_MessageWindow.BeginMessage)");
				return true;
			}
		}
		// 闇夜のPARENTE幻燐の姫将軍Remastered 体験版
		if ( Util::CheckFile(L"PARENTE_TRIAL.exe") && Util::CheckFile(L"PARENTE_TRIAL_Data") ||
			Util::CheckFile(L"PARENTE.exe") && Util::CheckFile(L"PARENTE_Data") ||
			Util::CheckFile(L"Archives\\ver00d0.arc") ) // 你小子不讲武德，怎么正式版改名叫START.exe了
			{
			const BYTE bytes2_1[] = {
				0x48, 0x89, 0x5C, 0x24, XX, 0x48, 0x89, 0x74, 0x24, XX, 0x48, 0x89, 0x54, 0x24, XX, 0x57, 0x48, 0x83, 0xEC, XX, 0x48, 0x8B, 0xF2
			};
			for (auto addr: Util::SearchMemory(bytes2_1, sizeof(bytes2_1), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr;
				hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
				hp.offset = pusha_rdx_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					std::wregex pattern(LR"(\([/]?f[^)]*\)|<([^,]+),[^>]+>)");
					RegexReplacerW(text, &len, pattern, L"$1");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "Unity_IL2cpp_SP_Donskoy_UI_MessageWindow");
				ConsoleOutput("Insert: Unity IL2cpp Game SP Hook (Donskoy.UI_MessageWindow.BeginMessage)");
				return true;
			}
		}

		// DMM.OLG.Unity.Extensions.Novel.EventMessageWindow.SetText
		// ガールズクリエイションR -少女藝術綺譚-
		// 48 89 5C 24 ?? 4C 89 44 24 ?? 56 48 83 EC ?? 80 3D ?? ?? ?? ?? ?? 48 8B F2 48 8B D9 75 ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B 44 24
		if (Util::CheckFile(L"GC.exe") && Util::CheckFile(L"GC_Data")) {
			const BYTE bytes3[] = {
				0x48, 0x89, 0x5C, 0x24, XX, 0x4C, 0x89, 0x44, 0x24, XX, 0x56, 0x48, 0x83, 0xEC, XX, 0x80, 0x3D, XX4, XX, 0x48, 0x8B, 0xF2, 0x48, 0x8B, 0xD9, 0x75, XX, 0x48, 0x8D, 0x0D, XX4, 0xE8, XX4, 0x48, 0x8D, 0x0D, XX4, 0xE8, XX4, 0x4C, 0x8B, 0x44, 0x24
			};
			for (auto addr: Util::SearchMemory(bytes3, sizeof(bytes3), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr;
				hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
				hp.offset = pusha_rdx_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					std::wregex pattern(LR"(<[^>]+?>)");
					RegexReplacerW(text, &len, pattern, L"");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "Unity_IL2cpp_SP_GC");
				ConsoleOutput("Insert: Unity IL2cpp Game SP Hook (GC)");
				return true;
			}
		}

		// Project::Novel::NovelText::Parse
		// ドットアビスX
		// 4C 89 44 24 ?? 48 89 54 24 ?? 55 53 56 57 41 54 41 55 41 57 48 8B EC 48 83 EC ??
		if (Util::CheckFile(L"ドットアビスX.exe") && Util::CheckFile(L"ドットアビスX_Data")) {
			const BYTE bytes4[] = {
				0x4C, 0x89, 0x44, 0x24, XX, 0x48, 0x89, 0x54, 0x24, XX, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x57, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC, XX
			};
			for (auto addr: Util::SearchMemory(bytes4, sizeof(bytes4), PAGE_EXECUTE, minAddress, maxAddress)) {
				HookParam hp = {};
				hp.address = addr;
				hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
				hp.offset = pusha_r8_off - 4;
				hp.padding = 0x14;
				hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
				{
					auto text = static_cast<LPWSTR>(data);
					auto len =  static_cast<size_t>(*size);
					std::wregex pattern(LR"(<[^>]+?>)");
					RegexReplacerW(text, &len, pattern, L"");
					*size = static_cast<DWORD>(len);
					return true;
				};
				NewHook(hp, "Unity_IL2cpp_SP_ABYSS");
				ConsoleOutput("Insert: Unity IL2cpp Game SP Hook (ABYSS)");
				return true;
			}
		}

		return false;
	}

	bool InsertSakanaGLHook() {
		//by Chenx221
		/*
		* 需要更多测试
		* Sample games:
		* https://vndb.org/v55020 猫猫旅行社 ももいろ町おこしプロジェクト！ // 国产游戏?
		*/
		const BYTE bytes[] = {
			0x4C, XX, XX, // mov r15,rax
			0x48, XX2, XX, // lea rsi,qword ptr ds:[rbx+30]
			0x85, 0xED, // test ebp,ebp					 <-- rsi
			0x0F, 0x8E, XX4 // jle sakanagl.7FFE8DAC711D
		};
		HMODULE module = GetModuleHandleW(L"sakanagl.dll");
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		for (auto addr: Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, minAddress, maxAddress)) {
			HookParam hp = {};
			hp.address = addr + 7;
			hp.offset = pusha_rsi_off - 4;
			hp.index = 0;
			hp.split = pusha_r12_off - 4;
			hp.type = USING_UTF8 | USING_STRING | USING_SPLIT | NO_CONTEXT;
			ConsoleOutput("vnreng: INSERT SakanaGL x64");
			NewHook(hp, "SakanaGL_x64");

			return true;
		}

		ConsoleOutput("vnreng:SakanaGL x64: pattern not found");
		return false;
	}

	bool InsertSakanaGL2Hook() {
		//by Chenx221
		/*
		* 需要更多测试
		* Sample games:
		* 悪役令嬢をわからせる！？体験版
		*/

		// 00007FFA1906FD8F | CC                  | int3                             |
		// 00007FFA1906FD90 | 48:895C24 08        | mov qword ptr ss:[rsp+8],rbx     |
		// 00007FFA1906FD95 | 48:896C24 10        | mov qword ptr ss:[rsp+10],rbp    |
		// 00007FFA1906FD9A | 48:897424 18        | mov qword ptr ss:[rsp+18],rsi    | rsi:sakanaKill+1D93F4
		// 00007FFA1906FD9F | 57                  | push rdi                         |
		// 00007FFA1906FDA0 | 48:83EC 20          | sub rsp,20                       |
		// 00007FFA1906FDA4 | 49:63F0             | movsxd rsi,r8d                   | rsi:sakanaKill+1D93F4
		// 00007FFA1906FDA7 | 48:8BF9             | mov rdi,rcx                      |
		// 00007FFA1906FDAA | 8B49 14             | mov ecx,dword ptr ds:[rcx+14]    |
		// 00007FFA1906FDAD | 48:8BEA             | mov rbp,rdx                      |

		const BYTE bytes[] = {
			0xCC,
			0x48,0x89,0x5C,0x24,0x08,
			0x48,0x89,0x6C,0x24,0x10,
			0x48,0x89,0x74,0x24,0x18,
			0x57,
			0x48,0x83,0xEC,0x20,
			0x49,0x63,0xF0,
			0x48,0x8B,0xF9,
			0x8B,0x49,0x14,
			0x48,0x8B,0xEA
		};
		HMODULE module = GetModuleHandleW(L"sakanagl.dll");
		auto [minAddress, maxAddress] = Util::QueryModuleLimits(module);
		for (auto addr: Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, minAddress, maxAddress)) {
			HookParam hp = {};
			hp.address = addr + 1;
			hp.offset = pusha_rdx_off - 4;
			hp.null_length = 4;
			hp.type = USING_UNICODE | USING_STRING;
			// 潜在问题 UTF-32
			ConsoleOutput("vnreng: INSERT SakanaGL_2 x64");
			NewHook(hp, "SakanaGL_2_x64");

			return true;
		}

		ConsoleOutput("vnreng:SakanaGL_2 x64: pattern not found");
		return false;
	}


	bool InsertLightVNHook() {
		// by Chenx221
		/*
		* Newer version
		* Tested on:
		* 17.0.1 ja_DEMO
		* 16.9.3 kr_DEMO
		* 16.9.2 ユニゾンコード Trial
		* Sample:
		* https://beta.chenx221.cyou/s/bfke6rEGM2tw4WK
		*/

		// 00007FF6A7CB0920 | 49:C7C1 FFFFFFFF         | mov r9,FFFFFFFFFFFFFFFF          |
		// 00007FF6A7CB0927 | E8 649CB8FF              | call lightapp.7FF6A783A590       |
		// 00007FF6A7CB092C | C785 5C010000 0C000000   | mov dword ptr ss:[rbp+15C],C     | hook here rdx 《--

		const BYTE bytes[] = { 0x49, 0xC7, 0xC1, XX4, 0xE8, XX4, 0xC7, 0x85, XX4, 0x0C, XX2, XX};
		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 12;
			hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
			hp.offset = pusha_rdx_off - 4;
			hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
			{
				auto text = reinterpret_cast<LPWSTR>(data);
				auto len =  static_cast<size_t>(*size);

				if (len == 0)
					return false;
				if (text[0] == L'\x2E')
					text[0] = L' '; //这对英语用户友好，但对其他语言用户而言是多余的
				if (cpp_wcsnstr(text, L"<", len/sizeof(wchar_t))) {
					WideStringFilterBetween(text, &len, L"<", 1, L">", 1); // remove <...>
				}
				WideStringFilter(text, &len, L"\\n", 2);
				WideStringFilter(text, &len, L"\\w", 2);
				*size = static_cast<DWORD>(len);
				return true;
			};
			NewHook(hp, "Light.vn");
			ConsoleOutput("Insert: Light.vn Hook");
			return true;
		}

		/*
		* Old version
		* Tested on:
		* 16.8.0-t10 プトリカ 1st.cut:The Reason She Must Perish https://vndb.org/v49261
		* 16.7.1 en_DEMO
		* Sample:
		* https://beta.chenx221.cyou/s/6NMyMRAze7wiNqK
		*/

		// 00007FF7DEF91499 | 48:83C1 02               | add rcx,2                        |
		// 00007FF7DEF9149D | EB E1                    | jmp putrika1st.7FF7DEF91480      |
		// 00007FF7DEF9149F | 49:63CD                  | movsxd rcx,r13d                  | hook here rax

		const BYTE bytes2[] = { 0x48, XX2, XX, 0xEB, XX, 0x49, 0x63, 0xCD};
		for (auto addr2 : Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr2 + 6;
			hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
			hp.offset = pusha_rax_off - 4;
			hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
			{
				auto text = reinterpret_cast<LPWSTR>(data);
				auto len =  static_cast<size_t>(*size);

				if (len == 0)
					return false;
				if (text[0] == L'\x2E')
					text[0] = L' ';
				if (cpp_wcsnstr(text, L"<", len/sizeof(wchar_t))) {
					WideStringFilterBetween(text, &len, L"<", 1, L">", 1); // remove <...>
				}
				WideStringFilter(text, &len, L"\\n", 2);
				WideStringFilter(text, &len, L"\\w", 2);
				*size = static_cast<DWORD>(len);
				return true;
			};
			NewHook(hp, "Light.vn");
			ConsoleOutput("Insert: Light.vn Hook");
			return true;
		}

		ConsoleOutput("vnreng:Light.vn: pattern not found");
		return false;
	}

	bool InsertFuzzHook() {
		// By Chenx221

		bool flag = false;
		// Tested:
		// https://vndb.org/v94 君が望む永遠 ～Enhanced Edition R～(体験版)
		// https://vndb.org/r128830 君が望む永遠 ～Enhanced Edition～ another episode collection+
		// https://vndb.org/r124569 君が望む永遠 ～Enhanced Edition～
		// https://vndb.org/r114285 マブラヴ オルタネイティヴ トータル・イクリプス 帝都燃ゆ
		const BYTE bytes[] = {
			0x48, 0x8B, 0xC8,             //mov rcx, rax
			0x48, 0x83, 0x78, 0x18, 0x10, //cmp qword ptr ds:[rax+18], 10
			0x72, 0x03,					  //jb kiminozet-win64vc14-release.7FF72482AD3A
			0x48, 0x8B, 0x08,             //mov rcx, qword ptr ds:[rax]
			0x48, 0x83, 0x78, 0x10, 0x05  //cmp qword ptr ds:[rax+10], 5 // <--
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 0xD;
			hp.offset = pusha_rcx_off - 4;
			hp.type = USING_STRING | USING_UTF8 | USING_SPLIT | NO_CONTEXT;
			hp.split = pusha_r15_off - 4;
			hp.filter_fun = [](void* str, DWORD* size, HookParam* hp, BYTE index) {
				auto text = reinterpret_cast<char*>(str);
				auto len =  static_cast<size_t>(*size);

				StringCharReplacer(text, &len, "\\n", 2, '\n');
				*size = static_cast<DWORD>(len);
				return true;
			};
			ConsoleOutput("vnreng: INSERT Fuzz Hook");
			NewHook(hp, "Fuzz");
			flag = true;
		}

		// Tested:
		// https://store.steampowered.com/app/1557480/Project_MIKHAIL_A_MuvLuv_War_Story/ Project MIKHAIL - A Muv-Luv War Story
		const BYTE bytes2[] = {
			0x48, 0x8D, 0x4D, 0x9F,	      //lea rcx, qword ptr ss:[rbp-61]
			0x48, 0x83, 0x7D, 0xB7, 0x10, //cmp qword ptr ss:[rbp-49], 10
			0x48, 0x0F, 0x43, 0x4D, 0x9F, //cmovae rcx, qword ptr ss:[rbp-61]
			0x48, 0x89, 0x44, 0x24, 0x20  //mov qword ptr ss:[rsp+20], rcx // <--
		};
		for (auto addr : Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, processStartAddress, processStopAddress)) {
			HookParam hp = {};
			hp.address = addr + 0xE;
			hp.offset = pusha_rcx_off - 4;
			hp.type = USING_STRING | USING_UTF8;
			hp.filter_fun = [](void* str, DWORD* size, HookParam* hp, BYTE index) {
				auto text = reinterpret_cast<char*>(str);
				auto len =  static_cast<size_t>(*size);

				StringCharReplacer(text, &len, "\\n", 2, '\n');
				*size = static_cast<DWORD>(len);
				return true;
			};
			ConsoleOutput("vnreng: INSERT Fuzz Hook");
			NewHook(hp, "Fuzz");
			flag = true;
		}

		// Tested:
		// https://vndb.org/r71433 マブラヴ アンリミテッド ザ・デイアフター episode:00 REMASTERED
		// https://vndb.org/r77581 マブラヴ アンリミテッド ザ・デイアフター episode:01 REMASTERED
		// https://vndb.org/r77582 マブラヴ アンリミテッド ザ・デイアフター episode:02 REMASTERED
		// https://vndb.org/r77583 マブラヴ アンリミテッド ザ・デイアフター episode:03 REMASTERED
		const BYTE bytes3[] = {
			0x48, 0x8D, 0x95, 0xF0, 0x00, 0x00, 0x00, //lea rdx, qword ptr ss:[rbp+F0]
			0x48, 0x8D, 0x8D, 0x30, 0x01, 0x00, 0x00, //lea rcx, qword ptr ss:[rbp+130]
			0xE8, XX4								  //call tda00-win64vc14-release.7FF72B36DC90 //<--
		};
		for (auto addr : Util::SearchMemory(bytes3, sizeof(bytes3), PAGE_EXECUTE, processStartAddress, processStopAddress)) {
			HookParam hp = {};
			hp.address = addr + 0xE;
			hp.offset = pusha_rdx_off - 4;
			hp.type = USING_STRING | USING_UTF8 | USING_SPLIT;
			hp.split = pusha_rdi_off - 4;
			hp.text_fun = [](uint64_t rsp_base, HookParam* pHp, BYTE, uint64_t* data, uintptr_t* split, DWORD* count) {
				uint64_t rdx = regof(rdx, rsp_base);
				*split = regof(rdi, rsp_base);
				uint64_t length = *(uint64_t*)(rdx + 0x10);
				if (length >= 0x10) {
					*data = *(uint64_t*)rdx;
				} else {
					*data = rdx;
				}
				*count = static_cast<DWORD>(length);
			};
			hp.filter_fun = [](void* str, DWORD* size, HookParam* hp, BYTE index) {
				auto text = reinterpret_cast<char*>(str);
				auto len =  static_cast<size_t>(*size);

				StringCharReplacer(text, &len, "\\n", 2, '\n');
				StringFilter(text, &len, "\\w", 2);
				StringFilter(text, &len, "\\p", 2);
				*size = static_cast<DWORD>(len);
				return true;
			};
			ConsoleOutput("Textractor: INSERT Fuzz Hook");
			NewHook(hp, "Fuzz");
			flag = true;
		}

		if(flag) return true;
		ConsoleOutput("Textractor:Fuzz: pattern not found");
		return false;
	}

	bool InsertMagesSPHook() {
		// By Chenx221

		// Tested:
		// https://vndb.org/v49686 岩倉アリア

		const BYTE bytes[] = {
			0x44, 0x03, 0xC8, 0x48, 0x8B, 0x0D
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr;
			hp.offset = pusha_rax_off - 4;
			hp.type = USING_UNICODE | HEX_DUMP | USING_SPLIT;
			hp.length_offset = 1;
			hp.split = pusha_rbp_off - 4;
			hp.text_fun = [](uint64_t rsp_base, HookParam* pHp, BYTE, uint64_t* data, uintptr_t* split, DWORD* count) {
				*split = rsp_base >> (regof(rbp, rsp_base) & 0x1ULL);
				*data = regof(rax, rsp_base);
				*count = 2;
			};
			ConsoleOutput("vnreng: INSERT MEGAS_SP(IWAKURA_ARIA) Hook");
			NewHook(hp, "MEGAS_SP");
			return true;
		}

		ConsoleOutput("Textractor:MEGAS_SP(IWAKURA_ARIA): pattern not found");
		return false;
	}

	bool InsertVisionairePlayerHook() {
		// Tested:
		// Adventures Of Willy D
		const BYTE bytes[] = {
			0x41, 0xB9, XX4, 0x4C, 0x8D, 0x85, XX4, 0xBA, XX4, 0x49, 0x8D, 0x4C, 0x24
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		for (auto addr : Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
			HookParam hp = {};
			hp.address = addr + 0xD;
			hp.offset = pusha_r8_off - 4;
			hp.type = USING_UNICODE | USING_STRING;
			hp.text_fun = [](uint64_t rsp_base, HookParam* pHp, BYTE, uint64_t* data, uintptr_t* split, DWORD* count) {
				uint64_t r8 = regof(r8, rsp_base);
				uint64_t length = *(uint64_t*)(r8 + 0x10);
				uint64_t capacity = *(uint64_t*)(r8 + 0x18);
				if (capacity >= 8) {
					*data = *(uint64_t*)r8;
				} else {
					*data = r8;
				}
				*count = static_cast<DWORD>(length*2);
			};
			hp.filter_fun = [](LPVOID data, DWORD* size, HookParam*, BYTE)
			{
				auto text = reinterpret_cast<LPWSTR>(data);
				auto len =  static_cast<size_t>(*size);

				if (len == 0)
					return false;
				WideStringCharReplacer(text,&len,L"<br/>",5,L'\n');
				WideStringFilter(text, &len, L"<p>", 3);

				std::wregex pattern(LR"(<[^>]+>)");
				RegexReplacerW(text, &len, pattern, L"");
				*size = static_cast<DWORD>(len);
				return true;
			};
			ConsoleOutput("vnreng: INSERT Visionaire Player Hook");
			NewHook(hp, "VisionairePlayer");
			return true;
		}

		ConsoleOutput("Textractor:Visionaire Player: pattern not found");
		return false;
	}

	bool InsertWillPlus64Hook() {
		// By Chenx221

		bool flag = false, gFlag = false;
		// Tested:
		// https://vndb.org/v61221 OfficeLove -クールな先輩彼女と秘密のイキ抜き- TRIAL EDITION

		// WillPlus6Hook 64bit Port
		// 2.1.0.0
		// 48 8b c4 55 53 56 57 41 54 41 55 41 56 41 57 48 8d 6c 24 ?? 48 81 EC ?? ?? ?? ?? 0f 29 70 ?? 0f 29 78 ?? 44 0f 29 40 ?? 48 8b 05 ?? ?? ?? ?? 48 33 c4 48 89 45 ?? 4d 8b e1
		const BYTE bytes1_1[] = {
			0x48, 0x8B, 0xC4, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, XX, 0x48, 0x81, 0xEC, XX4, 0x0F, 0x29, 0x70, XX, 0x0F, 0x29, 0x78, XX, 0x44, 0x0F, 0x29, 0x40, XX, 0x48, 0x8B, 0x05, XX4, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x45, XX, 0x4D, 0x8B, 0xE1
		};

		// 有閑夫人倶楽部 -体験版-
		// 2.1.1.0
		// 48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 4D 8B E1
		const BYTE bytes1_2[] = {
			0x48, 0x8B, 0xC4, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xA8, XX4, 0x48, 0x81, 0xEC, XX4, 0x0F, 0x29, 0x70, XX, 0x0F, 0x29, 0x78, XX, 0x44, 0x0F, 0x29, 0x40, XX, 0x48, 0x8B, 0x05, XX4, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x45, XX, 0x4D, 0x8B, 0xE1
		};

		// 虜ノ桜 ～美しく咲き、淫らに散らされる処女たち～ 体験版
		// 2.1.2.0
		// 48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 4D 8B E9
		const BYTE bytes1_3[] = {
			0x48, 0x8B, 0xC4, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, XX, 0x48, 0x81, 0xEC, XX4, 0x0F, 0x29, 0x70, XX, 0x0F, 0x29, 0x78, XX, 0x44, 0x0F, 0x29, 0x40, XX, 0x48, 0x8B, 0x05, XX4, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x45, XX, 0x4D, 0x8B, 0xE9
		};

		ULONG64 range = min(processStopAddress - processStartAddress, X64_MAX_REL_ADDR);
		auto doHook1 = [&](const BYTE* pattern, size_t patSize) {
			for (auto addr : Util::SearchMemory(pattern, patSize, PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
				HookParam hp = {};
				hp.address = addr;
				hp.offset = pusha_rcx_off - 4;
				hp.type = USING_STRING | USING_UNICODE | NO_CONTEXT;
				hp.filter_fun = [](void* str, DWORD* size, HookParam* hp, BYTE index) {
					auto text = reinterpret_cast<LPWSTR>(str);
					auto len = reinterpret_cast<size_t *>(size);
					if (*text == L'/' || *text == L'|')
						return false;
					WideStringCharReplacer(text, len, L" \\n", 3, L' ');
					WideStringCharReplacer(text, len, L"\\n", 2, L' ');
					WideStringCharReplacer(text, len, L"\\d", 2, L'\"');
					WideStringFilter(text, len, L"%LF", 3);
					WideStringFilter(text, len, L"%LC", 3);
					WideStringFilter(text, len, L"%K", 2);
					WideStringFilter(text, len, L"%P", 2);
					WideStringFilter(text, len, L"%XS", 5);
					WideStringFilter(text, len, L"%XE", 3);
					return true;
				};
				hp.text_fun = [](uint64_t rsp_base, HookParam* pHp, BYTE, uint64_t* data, uintptr_t* split, DWORD* count) {
					uint64_t rcx = regof(rcx, rsp_base);
					uint64_t length = *(uint64_t*)(rcx + 0x10);
					uint64_t capacity = *(uint64_t*)(rcx + 0x18);
					if (capacity >= 8) {
						*data = *(uint64_t*)rcx;
					} else {
						*data = rcx;
					}
					*count = static_cast<DWORD>(length*2);
				};
				ConsoleOutput("Textractor: INSERT WillPlus64_1 Hook");
				NewHook(hp, "WillPlus64_1");
				flag = true;
			}
		};
		doHook1(bytes1_1, sizeof(bytes1_1));
		if (!flag) {
			doHook1(bytes1_2, sizeof(bytes1_2));
			if (!flag) {
				doHook1(bytes1_3, sizeof(bytes1_3));
			}
		}

		gFlag |= flag;
		if(!flag) {
			ConsoleOutput("Textractor:WillPlus64_1: pattern not found");
		}
		flag = false;

		// WillPlus7Hook 64bit Port
		const BYTE bytes2_1[] = {
			0x40, 0x56,				// push rsi	// hook here
			0x57,					// push rdi
			0x41, 0x56,				// push r14
			0x48, 0x81, 0xEC, XX4,	// sub rsp,130
			0x48, 0x83, 0x7A, XX2,	// cmp qword ptr ds:[rdx+18],7
			0x49, 0x8B, 0xF8		// mov rdi,r8
		};
		// 虜ノ桜 ～美しく咲き、淫らに散らされる処女たち～ 体験版
		// 2.1.2.0
		// 40 55 56 57 48 81 EC ?? ?? ?? ?? 48 83 7A
		const BYTE bytes2_2[] = {
			0x40, 0x55, 0x56, 0x57, 0x48, 0x81, 0xEC, XX4, 0x48, 0x83, 0x7A
		};

		auto doHook2 = [&](const BYTE* pattern, size_t patSize) {
			for (auto addr : Util::SearchMemory(pattern, patSize, PAGE_EXECUTE, processStartAddress, processStartAddress + range)) {
				HookParam hp = {};
				hp.address = addr;
				hp.offset = pusha_rcx_off - 4;
				hp.type = USING_STRING | USING_UNICODE;
				hp.text_fun = [](uint64_t rsp_base, HookParam* pHp, BYTE, uint64_t* data, uintptr_t* split, DWORD* count) {
					uint64_t rcx = regof(rcx, rsp_base);
					uint64_t length = *(uint64_t*)(rcx + 0x10);
					uint64_t capacity = *(uint64_t*)(rcx + 0x18);
					if (capacity >= 8) {
						*data = *(uint64_t*)rcx;
					} else {
						*data = rcx;
					}
					*count = static_cast<DWORD>(length*2);
				};
				ConsoleOutput("Textractor: INSERT WillPlus64_2 Hook");
				NewHook(hp, "WillPlus64_2");
				flag = true;
			}
		};
		doHook2(bytes2_1, sizeof(bytes2_1));
		if (!flag) {
			doHook2(bytes2_2, sizeof(bytes2_2));
		}

		gFlag |= flag;
		if(!flag) {
			ConsoleOutput("Textractor:WillPlus64_2: pattern not found");
		}

		return gFlag;
	}

	bool InsertBakinPlayerHook() {
		ConsoleOutput("Textractor: Initializing BakinPlayer .NET hook");

		wchar_t modulePath[MAX_PATH];
		GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		std::wstring helperPath = modulePath;
		size_t pos = helperPath.find_last_of(L"\\/");
		if (pos != std::wstring::npos) {
			helperPath = helperPath.substr(0, pos + 1);
		}
		helperPath += L"CLRHelper.dll";
		if (!CLRHelper::ExtractHelperAssembly(helperPath.c_str())) {
			return false;
		}

		bool flag = false;

		void* methodAddress = nullptr;

		bool success = CLRHelper::GetDotNetMethodAddress(
		   L"bakinengine",
		   L"Yukar.Engine.LayoutMenuController",
		   L"ShowMessage",
		   4,
		   &methodAddress
		);

		if (!success || !methodAddress) {
			ConsoleOutput("Textractor: Failed to get BakinPlayer ShowMessage method address");
		} else {
			ConsoleOutput("Textractor: Found ShowMessage method at address: 0x%p", methodAddress);
			HookParam hp = {};
			hp.address = reinterpret_cast<uint64_t>(methodAddress);
			hp.offset = pusha_rdx_off - 4;
			hp.type = USING_STRING | USING_UNICODE;
			hp.padding = 0xC;
			ConsoleOutput("Textractor: INSERT BakinPlayer_ShowMessage Hook");
			NewHook(hp, "BakinPlayer ShowMessage");
			flag = true;
		}

		methodAddress = nullptr;
		success = CLRHelper::GetDotNetMethodAddress(
		   L"bakinengine",
		   L"Yukar.Engine.LayoutMenuController",
		   L"ShowDialogue",
		   4,
		   &methodAddress
		);
		if (!success || !methodAddress) {
			ConsoleOutput("Textractor: Failed to get BakinPlayer ShowDialogue method address");
		} else {
			ConsoleOutput("Textractor: Found ShowDialogue method at address: 0x%p", methodAddress);
			HookParam hp = {};
			hp.address = reinterpret_cast<uint64_t>(methodAddress);
			hp.offset = pusha_rdx_off - 4;
			hp.type = USING_STRING | USING_UNICODE;
			hp.padding = 0xC;
			ConsoleOutput("Textractor: INSERT BakinPlayer_ShowDialogue Hook");
			NewHook(hp, "BakinPlayer ShowDialogue");
			flag = true;
		}

		return flag;
	}

	bool InsertV8HookHelper() {
		HMODULE module = GetModuleHandleW(L"version.dll");
		HookParam hp = {};
		hp.address = reinterpret_cast<uintptr_t>(GetProcAddress(module, "hookme"));
		hp.offset = pusha_rcx_off - 4;
		hp.split = pusha_rdx_off - 4;
		hp.type = USING_STRING | USING_UTF8 | NO_CONTEXT | USING_SPLIT;
		ConsoleOutput("Textractor: INSERT V8Helper Hook");
		NewHook(hp, "V8Helper Hook");
		return true;
	}

	bool UnsafeDetermineEngineType()
	{
		if (Util::CheckFile(L"PPSSPP*.exe") && FindPPSSPP()) return true;

		if (Util::CheckFile(L"*.pck") || Util::CheckFile(L"libgodotsteam*.dll") || Util::SearchResourceString(L"Godot Engine")) {
			return InsertGodotHook_X64();
		}

		if (Util::CheckFile(L"erc_nospfx.dll") || Util::CheckFile(L"erc.dll")) {
			return InsertFuzzHook();
		}

		if (Util::CheckFile(L"Rio.arc") && Util::CheckFile(L"Chip*.arc")) {
			return InsertWillPlus64Hook();
		}

		if (Util::CheckFile(L"nw.dll") || Util::CheckFile(L"v8_context_snapshot.bin")) {
			if (Util::IsHookedDllLoaded(L"version.dll", "hookme")) {
				InsertV8HookHelper();
			} else {
				ConsoleOutput("Textractor: V8Hook Helper(version.dll) not found, skipping V8Helper Hook");
				ConsoleOutput("You can download it from: https://github.com/Chenx221/v8hook");
			}
		}

		if (checkv8orcef())return true;

		for (const wchar_t* moduleName : { (const wchar_t*)NULL, L"node.dll", L"nw.dll" }) if (InsertV8Hook(GetModuleHandleW(moduleName))) return true;

		if (GetModuleHandleW(L"GameAssembly.dll")) // TODO: is there a way to autofind hook?
		{
			ConsoleOutput("Textractor: Precompiled Unity found (searching for hooks should work)");
			InsertUnityIL2TMPHook();
			InsertUnityIL2UtageHook();
			InsertUnityIl2GameSpHook();
			wcscpy_s(spDefault.boundaryModule, L"GameAssembly.dll");
			spDefault.padding = 20;
			return true;
		}

		if (Util::CheckFile(L"files/*.PAK")) {
			InsertLucaSystemHook();
			return true;
		}

		if (Util::CheckFile(L"*.pfs")) {
			InsertArtemis64Hook();
			InsertArtemisHook();
			InsertArtemisExHook();
			return true;
		}

	    if (Util::CheckFile(L"*.vis") && Util::CheckFile(L"config.ini")) {
	        InsertVisionairePlayerHook();
	    	return true;
	    }

		if (Util::CheckFile(L"bgi.*") || Util::CheckFile(L"sysgrp.arc")) {
			InsertBGI64Hook();
			return true;
		}

		if (Util::CheckFile(L"*.py")) {
			InsertRenpyHook();
			InsertRenpy3Hook();
			InsertRenpy3NewHook();
			return true;
		}

		if (Util::CheckFile(L"sakanagl.dll")) {
			bool status = InsertSakanaGLHook();
			status |= InsertSakanaGL2Hook();
			return status;
		}

		if (Util::CheckFile(L"bakinengine.dll")) {
			return InsertBakinPlayerHook();
		}

		// 也许有更好的检测方法
		if (Util::CheckFile(L"LightTests.exe")|Util::CheckFile(L"BugTrap.dll")) {
			if (InsertLightVNHook())
				return true;
		}

		for (const wchar_t* monoName : { L"mono.dll", L"mono-2.0-bdwgc.dll" }) if (HMODULE module = GetModuleHandleW(monoName)) if (InsertMonoHooks(module)) return true;

		if (Util::CheckFile(L"Data/system.mpk")) {
			ConsoleOutput("Textractor: MAGES engine detected. To correctly obtain the text, please prepare the SavedReplacements.txt replacement table and enable the Replacer extension.");
			if (Util::CheckFile(L"IWAKURA_ARIA.exe")) {
				InsertMagesSPHook();
			}
			return true;
		}

		if (Util::CheckFile(L"*.xp3") || Util::SearchResourceString(L"TVP(KIRIKIRI) Z ")) { // TVP(KIRIKIRI) Z CORE
			if (InsertKiriKiriZHook())
				return true;
		}

		for (std::wstring DXVersion : { L"d3dx9", L"d3dx10" })
			if (HMODULE module = GetModuleHandleW(DXVersion.c_str())) PcHooks::hookD3DXFunctions(module);
			else for (int i = 0; i < 50; ++i)
				if (HMODULE module = GetModuleHandleW((DXVersion + L"_" + std::to_wstring(i)).c_str())) PcHooks::hookD3DXFunctions(module);

		PcHooks::hookGDIFunctions();
		PcHooks::hookGDIPlusFunctions();
		return false;
	}
}
