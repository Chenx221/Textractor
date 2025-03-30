#include "qtcommon.h"
#include "network.h"
#include "translatewrapperlite.h"

extern const wchar_t *TRANSLATION_ERROR;

const char *TRANSLATION_PROVIDER = "Translate Bridge";

extern const QStringList languagesTo
        {
            "?"
        }, languagesFrom = languagesTo;
extern const std::unordered_map<std::wstring, std::wstring> codes
{
        {{L"?"}, {L"auto"}}
};

bool translateSelectedOnly = false, useRateLimiter = true, rateLimitSelected = false, useCache = false, useFilter = true;
int tokenCount = 30, rateLimitTimespan = 60000, maxSentenceSize = 1000, port = 3005;

std::pair<bool, std::wstring> Translate(const std::wstring &text, TranslationParam tlp) {
    HttpRequest httpRequest{
        L"Mozilla/5.0 Textractor",
        L"localhost",
        L"POST",
        L"/translate",
        FormatString(R"({"text": "%s"})", JSON::Escape(WideStringToString(text))),
        L"Content-Type: application/json",
        DWORD(port),
        NULL,
        WINHTTP_FLAG_ESCAPE_DISABLE
    };

    if (httpRequest) {
        if (auto translation = Copy(JSON::Parse(httpRequest.response)[L"translation"].String()))
            return {true, translation.value()};
        return {false, FormatString(L"%s: %s", TRANSLATION_ERROR, httpRequest.response)};
    }
    else {
        return {false, FormatString(L"%s (code=%u)", TRANSLATION_ERROR, httpRequest.errorCode)};
    }
}
