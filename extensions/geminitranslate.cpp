#include "qtcommon.h"
#include "translatewrapperai.h"
#include "network.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern const wchar_t* TRANSLATION_ERROR;
extern const wchar_t* TRANSLATION_JSON_PARSE_ERROR;

const char* TRANSLATION_PROVIDER = "Google Gemini AI Translate";
const char* GET_API_KEY_FROM = "https://aistudio.google.com/app/apikey";

extern const QStringList aiProviders{
	"Google Gemini"
};

// https://ai.google.dev/gemini-api/docs/models
extern const QStringList aiModels{
	"gemini-3.1-pro-preview",
	"gemini-3.1-flash-lite-preview",
	"gemini-3-flash-preview",
	"gemini-2.5-pro",
	"gemini-2.5-flash",
	"gemini-2.5-flash-lite"
};

const wchar_t* AI_DEFAULT_PROVIDER = L"Google Gemini";
const wchar_t* AI_DEFAULT_MODEL = L"gemini-3-flash-preview";
const wchar_t* AI_DEFAULT_API_HOST = L"generativelanguage.googleapis.com";
const wchar_t* AI_DEFAULT_API_PATH = L"/v1beta/models/";
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
	{{L"Gemini"}, {L"generativelanguage.googleapis.com"}}
};

extern const std::unordered_map<std::wstring, std::wstring> providerApiPaths{
	{{L"Gemini"}, {L"/v1beta/models/"}}
};

bool translateSelectedOnly = true, useRateLimiter = true, rateLimitSelected = false, useCache = true, useFilter = true;
bool includePreviousContext = false, enableThinking = false;

extern const QStringList reasoningEfforts{};
int tokenCount = 20, rateLimitTimespan = 1000, maxSentenceSize = 2000;

namespace
{
	std::wstring NormalizePath(std::wstring path)
	{
		Trim(path);
		if (path.empty()) return AI_DEFAULT_API_PATH;
		if (path[0] != L'/') path.insert(path.begin(), L'/');
		// Ensure path ends with / for Gemini (since we append model name)
		if (!path.empty() && path.back() != L'/') path.push_back(L'/');
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

	void ApplyProviderDefaults(TranslationParam& tlp)
	{
		if (tlp.provider.empty()) tlp.provider = AI_DEFAULT_PROVIDER;
		if (tlp.model.empty()) tlp.model = AI_DEFAULT_MODEL;
		if (tlp.apiHost.empty()) tlp.apiHost = AI_DEFAULT_API_HOST;
		if (tlp.apiPath.empty()) tlp.apiPath = AI_DEFAULT_API_PATH;
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
	tlp.apiHost = NormalizeHost(tlp.apiHost);
	tlp.apiPath = NormalizePath(tlp.apiPath);

	// Build Gemini API request body according to the spec
	json requestBody{
		{"contents", json::array({
			{
				{"role", "user"},
				{"parts", json::array({
					{{"text", BuildUserPrompt(text, tlp)}}
				})}
			}
		})},
		{"system_instruction", {
			{"parts", json::array({
				{{"text", WideStringToString(tlp.systemPrompt)}}
			})}
		}},
		{"generationConfig", {
			{"temperature", tlp.temperature > 0 ? tlp.temperature : AI_DEFAULT_TEMPERATURE},
			{"topP", 0.95},
			{"topK", 40},
			{"maxOutputTokens", 2048},
			{"responseMimeType", "text/plain"}
		}},
		{"safetySettings", json::array({
			{{"category", "HARM_CATEGORY_HARASSMENT"}, {"threshold", "BLOCK_NONE"}},
			{{"category", "HARM_CATEGORY_HATE_SPEECH"}, {"threshold", "BLOCK_NONE"}},
			{{"category", "HARM_CATEGORY_SEXUALLY_EXPLICIT"}, {"threshold", "BLOCK_NONE"}},
			{{"category", "HARM_CATEGORY_DANGEROUS_CONTENT"}, {"threshold", "BLOCK_NONE"}}
		})}
	};

	// Gemini uses API key in URL path, not in headers
	// Path format: /v1beta/models/{model}:generateContent?key=API_KEY
	std::wstring fullPath = tlp.apiPath + tlp.model + L":generateContent?key=" + tlp.authKey;

	HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		tlp.apiHost.c_str(),
		L"POST",
		fullPath.c_str(),
		requestBody.dump(),
		L"Content-Type: application/json",
		INTERNET_DEFAULT_HTTPS_PORT,
		nullptr,
		WINHTTP_FLAG_SECURE | WINHTTP_FLAG_ESCAPE_DISABLE
	};

	if (httpRequest)
	{
		try
		{
			auto parsed = json::parse(WideStringToString(httpRequest.response));

			// Handle error response
			if (parsed.contains("error") && parsed["error"].is_object())
			{
				std::string errorMessage = "unknown error";
				if (parsed["error"].contains("message") && parsed["error"]["message"].is_string())
					errorMessage = parsed["error"]["message"].get<std::string>();
				return { false, FormatString(L"%s: %s", TRANSLATION_ERROR, StringToWideString(errorMessage)) };
			}

			// Handle successful response - Gemini structure: candidates[0].content.parts[0].text
			if (parsed.contains("candidates") && parsed["candidates"].is_array() && !parsed["candidates"].empty())
			{
				auto& candidate = parsed["candidates"][0];
				if (candidate.contains("content") && candidate["content"].is_object())
				{
					auto& content = candidate["content"];
					if (content.contains("parts") && content["parts"].is_array() && !content["parts"].empty())
					{
						auto& part = content["parts"][0];
						if (part.contains("text") && part["text"].is_string())
						{
							return { true, StringToWideString(part["text"].get<std::string>()) };
						}
					}
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

