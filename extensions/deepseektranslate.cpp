#include "qtcommon.h"
#include "translatewrapperai.h"
#include "network.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern const wchar_t* TRANSLATION_ERROR;
extern const wchar_t* TRANSLATION_JSON_PARSE_ERROR;

const char* TRANSLATION_PROVIDER = "DeepSeek AI Translate";
const char* GET_API_KEY_FROM = "https://platform.deepseek.com/api_keys";

extern const QStringList aiProviders{
	"DeepSeek"
};

extern const QStringList aiModels{
	"deepseek-v4-flash",
	"deepseek-v4-pro"
};

extern const QStringList reasoningEfforts{
	"high",
	"max"
};

const wchar_t* AI_DEFAULT_PROVIDER = L"DeepSeek";
const wchar_t* AI_DEFAULT_MODEL = L"deepseek-v4-flash";
const wchar_t* AI_DEFAULT_API_HOST = L"api.deepseek.com";
const wchar_t* AI_DEFAULT_API_PATH = L"/chat/completions";
const wchar_t* AI_DEFAULT_SYSTEM_PROMPT = L"You are a professional translator. Translate faithfully and keep original line breaks. If previous-context sentences are provided, use them only for disambiguation and output only the translation of the current paragraphs.";
extern const double AI_DEFAULT_TEMPERATURE = 0.3;

extern const QStringList languagesTo
{
	"Simplified Chinese",
	"Traditional Chinese",
	"English",
	"Japanese",
	"Korean",
	"Custom..."
}, languagesFrom = languagesTo;

extern const std::unordered_map<std::wstring, std::wstring> codes
{
	{{L"Simplified Chinese"}, {L"zh"}},
	{{L"Traditional Chinese"}, {L"zh-tw"}},
	{{L"English"}, {L"en"}},
	{{L"Japanese"}, {L"ja"}},
	{{L"Korean"}, {L"ko"}},
	{{L"Custom..."}, {L"custom"}},
	{{L"?"}, {L"auto"}}
};

extern const std::unordered_map<std::wstring, std::wstring> providerApiHosts{
	{{L"DeepSeek"}, {L"api.deepseek.com"}}
};

extern const std::unordered_map<std::wstring, std::wstring> providerApiPaths{
	{{L"DeepSeek"}, {L"/chat/completions"}}
};

bool translateSelectedOnly = true, useRateLimiter = true, rateLimitSelected = false, useCache = true, useFilter = true;
bool includePreviousContext = false, enableThinking = false;
int tokenCount = 20, rateLimitTimespan = 1000, maxSentenceSize = 2000;

namespace
{
	std::wstring NormalizePath(std::wstring path)
	{
		Trim(path);
		if (path.empty()) return AI_DEFAULT_API_PATH;
		if (path[0] != L'/') path.insert(path.begin(), L'/');
		return path;
	}

	std::wstring NormalizeHost(std::wstring host)
	{
		Trim(host);
		if (host.empty()) return AI_DEFAULT_API_HOST;
		if (host.rfind(L"https://", 0) == 0) host = host.substr(8);
		if (host.rfind(L"http://", 0) == 0) host = host.substr(7);
		if (auto pos = host.find(L'/'); pos != std::wstring::npos) host = host.substr(0, pos);
		return host;
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

	if (tlp.model.empty()) tlp.model = AI_DEFAULT_MODEL;
	if (tlp.systemPrompt.empty()) tlp.systemPrompt = AI_DEFAULT_SYSTEM_PROMPT;
	tlp.apiHost = NormalizeHost(tlp.apiHost);
	tlp.apiPath = NormalizePath(tlp.apiPath);

	json requestBody{
		{"model", WideStringToString(tlp.model)},
		{"messages", json::array({
			{{"role", "system"}, {"content", WideStringToString(tlp.systemPrompt)}},
			{{"role", "user"}, {"content", BuildUserPrompt(text, tlp)}}
		})}
	};

	if (tlp.enableThinking)
	{
		requestBody["thinking"] = {{"type", "enabled"}};
		if (!tlp.reasoningEffort.empty())
			requestBody["reasoning_effort"] = WideStringToString(tlp.reasoningEffort);
	}
	else
	{
		requestBody["thinking"] = {{"type", "disabled"}};
		requestBody["temperature"] = tlp.temperature > 0 ? tlp.temperature : AI_DEFAULT_TEMPERATURE;
	}

	HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		tlp.apiHost.c_str(),
		L"POST",
		tlp.apiPath.c_str(),
		requestBody.dump(),
		FormatString(L"Content-Type: application/json\r\nAuthorization: Bearer %s", tlp.authKey).c_str(),
		INTERNET_DEFAULT_HTTPS_PORT,
		nullptr,
		WINHTTP_FLAG_SECURE | WINHTTP_FLAG_ESCAPE_DISABLE
	};

	if (httpRequest) {
		try {
			auto parsed = json::parse(WideStringToString(httpRequest.response));

			if (parsed.contains("error") && parsed["error"].is_object()) {
				std::string errorMessage = "unknown error";
				if (parsed["error"].contains("message") && parsed["error"]["message"].is_string())
					errorMessage = parsed["error"]["message"].get<std::string>();
				return { false, FormatString(L"%s: %s", TRANSLATION_ERROR, StringToWideString(errorMessage)) };
			}

			if (parsed.contains("choices") && parsed["choices"].is_array() && !parsed["choices"].empty()) {
				auto& choice = parsed["choices"][0];
				if (choice.contains("message") && choice["message"].is_object() &&
					choice["message"].contains("content") && choice["message"]["content"].is_string()) {
					return { true, StringToWideString(choice["message"]["content"].get<std::string>()) };
				}
			}

			return { false, FormatString(L"%s: %s", TRANSLATION_ERROR, httpRequest.response) };
		}
		catch (const json::exception& e) {
			return { false, FormatString(L"%s: %s", TRANSLATION_JSON_PARSE_ERROR, StringToWideString(e.what())) };
		}
	}

	return { false, FormatString(L"%s (code=%u)", TRANSLATION_ERROR, httpRequest.errorCode) };
}



