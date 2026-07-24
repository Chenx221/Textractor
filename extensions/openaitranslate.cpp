#include "qtcommon.h"
#include "translatewrapperai.h"
#include "network.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern const wchar_t* TRANSLATION_ERROR;
extern const wchar_t* TRANSLATION_JSON_PARSE_ERROR;

const char* TRANSLATION_PROVIDER = "OpenAI AI Translate";
const char* GET_API_KEY_FROM = "https://platform.openai.com/api-keys";

extern const QStringList aiProviders{
	"OpenAI", // 我的试用额度过期了，所以理论支持:(
	"OpenRouter",
	"Tencent Cloud CN",
	"Aliyun CN",
	"Ollama",
	"Custom"
};

extern const QStringList aiModels{
	"gpt-5.4-mini",
	// 自己填
};

const wchar_t* AI_DEFAULT_PROVIDER = L"OpenAI";
const wchar_t* AI_DEFAULT_MODEL = L"gpt-5.4-mini";
const wchar_t* AI_DEFAULT_API_HOST = L"api.openai.com";
const wchar_t* AI_DEFAULT_API_PATH = L"/v1/chat/completions";
const wchar_t* AI_DEFAULT_SYSTEM_PROMPT = L"You are a professional translator. Translate faithfully and keep original line breaks. If previous-context sentences are provided, use them only for disambiguation and output only the translation of the current paragraphs.";
extern const double AI_DEFAULT_TEMPERATURE = 0.3;

extern const QStringList languagesTo{
	"Simplified Chinese",
	"Traditional Chinese",
	"English",
	"Japanese",
	"Korean",
	"Custom..."
}, languagesFrom = languagesTo;

extern const std::unordered_map<std::wstring, std::wstring> codes{
	{{L"Simplified Chinese"}, {L"zh"}},
	{{L"Traditional Chinese"}, {L"zh-tw"}},
	{{L"English"}, {L"en"}},
	{{L"Japanese"}, {L"ja"}},
	{{L"Korean"}, {L"ko"}},
	{{L"Custom..."}, {L"custom"}},
	{{L"?"}, {L"auto"}}
};

extern const std::unordered_map<std::wstring, std::wstring> providerApiHosts{
	{{L"OpenAI"}, {L"api.openai.com"}},
	{{L"OpenRouter"}, {L"openrouter.ai"}},
	{{L"Tencent Cloud CN"}, {L"tokenhub.tencentmaas.com"}},
	{{L"Aliyun CN"}, {L"dashscope.aliyuncs.com"}},
	{{L"Ollama"}, {L"http://127.0.0.1:11434"}},
	{{L"Custom"}, {L""}}
};

extern const std::unordered_map<std::wstring, std::wstring> providerApiPaths{
	{{L"OpenAI"}, {L"/v1/chat/completions"}},
	{{L"OpenRouter"}, {L"/api/v1/chat/completions"}},
	{{L"Tencent Cloud CN"}, {L"/v1/chat/completions"}},
	{{L"Aliyun CN"}, {L"/compatible-mode/v1/chat/completions"}},
	{{L"Ollama"}, {L"/v1/chat/completions"}},
	{{L"Custom"}, {L""}}
};

bool translateSelectedOnly = true, useRateLimiter = true, rateLimitSelected = false, useCache = true, useFilter = true;
bool includePreviousContext = false, enableThinking = false;

extern const QStringList reasoningEfforts{};
int tokenCount = 20, rateLimitTimespan = 1000, maxSentenceSize = 2000;

namespace
{
	constexpr wchar_t CUSTOM_PROVIDER[] = L"Custom";

	struct ParsedUrl
	{
		std::wstring host;
		DWORD port;
		bool isSecure;
	};

	std::wstring NormalizePath(std::wstring path)
	{
		Trim(path);
		if (path.empty()) return AI_DEFAULT_API_PATH;
		if (path[0] != L'/') path.insert(path.begin(), L'/');
		return path;
	}

	ParsedUrl ParseHostUrl(std::wstring url)
	{
		Trim(url);
		if (url.empty()) return { AI_DEFAULT_API_HOST, INTERNET_DEFAULT_HTTPS_PORT, true };

		bool isSecure = true;

		// 检查并移除协议前缀
		if (url.rfind(L"https://", 0) == 0)
		{
			url = url.substr(8);
			isSecure = true;
		}
		else if (url.rfind(L"http://", 0) == 0)
		{
			url = url.substr(7);
			isSecure = false;
		}

		// 移除路径部分
		if (auto pos = url.find(L'/'); pos != std::wstring::npos)
			url = url.substr(0, pos);

		// 提取端口
		DWORD port = isSecure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
		if (auto pos = url.find(L':'); pos != std::wstring::npos)
		{
			try
			{
				port = std::stoul(url.substr(pos + 1));
				url = url.substr(0, pos);
			}
			catch (...) {}
		}

		if (url.empty()) url = AI_DEFAULT_API_HOST;

		return { url, port, isSecure };
	}

	void ApplyProviderDefaults(TranslationParam& tlp)
	{
		if (tlp.provider.empty()) tlp.provider = AI_DEFAULT_PROVIDER;

		if (tlp.provider == CUSTOM_PROVIDER) return;

		if (auto it = providerApiHosts.find(tlp.provider); it != providerApiHosts.end())
		{
			if (!it->second.empty() && tlp.apiHost.empty())
				tlp.apiHost = it->second;
		}
		else if (tlp.apiHost.empty())
		{
			tlp.apiHost = AI_DEFAULT_API_HOST;
		}

		if (auto it = providerApiPaths.find(tlp.provider); it != providerApiPaths.end())
		{
			if (!it->second.empty() && tlp.apiPath.empty())
				tlp.apiPath = it->second;
		}
		else if (tlp.apiPath.empty())
		{
			tlp.apiPath = AI_DEFAULT_API_PATH;
		}

		if (tlp.model.empty()) tlp.model = AI_DEFAULT_MODEL;
	}

	std::string BuildUserPrompt(const std::wstring& text, const TranslationParam& tlp)
	{
		std::wstring prompt;
		if (tlp.translateFrom == L"?")
			prompt = FormatString(L"Translate the CURRENT paragraphs to %s. Return only the translated CURRENT paragraphs.", tlp.translateTo);
		else
			prompt = FormatString(L"Translate the CURRENT paragraphs from %s to %s. Return only the translated CURRENT paragraphs.", tlp.translateFrom, tlp.translateTo);

		if (tlp.includePreviousContext)
		{
			prompt += L"\nUse PREVIOUS sentences for context only. NEVER translate or include PREVIOUS sentences in the output.";
			if (!tlp.previousSentence2.empty()) prompt += FormatString(L"\nPREVIOUS_2: %s", tlp.previousSentence2);
			if (!tlp.previousSentence1.empty()) prompt += FormatString(L"\nPREVIOUS_1: %s", tlp.previousSentence1);
		}

		prompt += FormatString(L"\nCURRENT: %s", text);
		return WideStringToString(prompt);
	}
}

std::pair<bool, std::wstring> Translate(const std::wstring& text, TranslationParam tlp)
{
	if (tlp.authKey.empty())
		return { false, FormatString(L"%s, API key is required.", TRANSLATION_ERROR) };

	ApplyProviderDefaults(tlp);
	if (tlp.systemPrompt.empty()) tlp.systemPrompt = AI_DEFAULT_SYSTEM_PROMPT;

	auto parsedUrl = ParseHostUrl(tlp.apiHost);
	tlp.apiPath = NormalizePath(tlp.apiPath);

	json requestBody{
		{"model", WideStringToString(tlp.model)},
		{"temperature", tlp.temperature > 0 ? tlp.temperature : AI_DEFAULT_TEMPERATURE},
		{"messages", json::array({
			{{"role", "system"}, {"content", WideStringToString(tlp.systemPrompt)}},
			{{"role", "user"}, {"content", BuildUserPrompt(text, tlp)}}
		})}
	};

	DWORD requestFlags = WINHTTP_FLAG_ESCAPE_DISABLE;
	if (parsedUrl.isSecure)
		requestFlags |= WINHTTP_FLAG_SECURE;

	HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		parsedUrl.host.c_str(),
		L"POST",
		tlp.apiPath.c_str(),
		requestBody.dump(),
		FormatString(L"Content-Type: application/json\r\nAuthorization: Bearer %s", tlp.authKey).c_str(),
		parsedUrl.port,
		nullptr,
		requestFlags
	};

	if (httpRequest)
	{
		try
		{
			auto parsed = json::parse(WideStringToString(httpRequest.response));

			if (parsed.contains("error") && parsed["error"].is_object())
			{
				std::string errorMessage = "unknown error";
				if (parsed["error"].contains("message") && parsed["error"]["message"].is_string())
					errorMessage = parsed["error"]["message"].get<std::string>();
				return { false, FormatString(L"%s: %s", TRANSLATION_ERROR, StringToWideString(errorMessage)) };
			}

			if (parsed.contains("choices") && parsed["choices"].is_array() && !parsed["choices"].empty())
			{
				auto& choice = parsed["choices"][0];
				if (choice.contains("message") && choice["message"].is_object() &&
					choice["message"].contains("content") && choice["message"]["content"].is_string())
				{
					return { true, StringToWideString(choice["message"]["content"].get<std::string>()) };
				}
			}

			return { false, FormatString(L"%s: %s", TRANSLATION_ERROR, httpRequest.response) };
		}
		catch (const json::exception& e)
		{
			return { false, FormatString(L"%s: %s", TRANSLATION_JSON_PARSE_ERROR, StringToWideString(e.what())) };
		}
	}

	return { false, FormatString(L"%s (code=%u)", TRANSLATION_ERROR, httpRequest.errorCode) };
}

