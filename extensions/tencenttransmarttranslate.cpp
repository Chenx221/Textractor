#include "qtcommon.h"
#include "translatewrapperlite2.h"
#include "network.h"

extern const wchar_t *TRANSLATION_ERROR;

const char *TRANSLATION_PROVIDER = "Tencent transmart Translate";

extern const QStringList languagesTo
        {
            "arabic",
            "german",
            "russian",
            "french",
            "korean",
            "portuguese",
            "japanese",
            "thai",
            "turkish",
            "spanish",
            "italian",
            "english",
            "vietnamese",
            "cantonese",
            "chinese",
        }, languagesFrom = languagesTo;
extern const std::unordered_map<std::wstring, std::wstring> codes
{
    {{L"arabic"}, {L"ar"}},
    {{L"german"}, {L"de"}},
    {{L"russian"}, {L"ru"}},
    {{L"french"}, {L"fr"}},
    {{L"korean"}, {L"ko"}},
    {{L"portuguese"}, {L"pt"}},
    {{L"japanese"}, {L"ja"}},
    {{L"thai"}, {L"th"}},
    {{L"turkish"}, {L"tr"}},
    {{L"spanish"}, {L"es"}},
    {{L"italian"}, {L"it"}},
    {{L"english"}, {L"en"}},
    {{L"vietnamese"}, {L"vi"}},
    {{L"cantonese"}, {L"ct"}},
    {{L"chinese"}, {L"zh"}},
    {{L"?"}, {L"auto"}}
};

bool translateSelectedOnly = false, useRateLimiter = true, rateLimitSelected = false, useCache = true, useFilter = true;
int tokenCount = 30, rateLimitTimespan = 60000, maxSentenceSize = 5000;

void uuid(char *uuid) {
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid[i] = '-';
        } else {
            uuid[i] = hex[rand() % 16];
        }
    }
    uuid[36] = '\0';
}

long long current_utc_milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
            .count();
}

std::pair<bool, std::wstring> Translate(const std::wstring &text, TranslationParam tlp) {
    char uuidStr[37];
    uuid(uuidStr);
    std::string clientKey = "browser-chrome-134.0.0-Windows_10-" +
                            std::string(uuidStr) + "-" +
                            std::to_string(current_utc_milliseconds());
    if (clientKey.length() > 100) clientKey = clientKey.substr(0, 100);

    if (HttpRequest httpRequest{
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36 Textractor",
        L"transmart.qq.com",
        L"POST",
        L"/api/imt",
        FormatString(
            R"({"header":{"fn":"auto_translation","session":"","client_key":"%s","user":""},"type":"plain","model_category":"normal","text_domain":"","source":{"lang":"%S","text_list":["%s"]},"target":{"lang":"%S"}}
)", clientKey, codes.at(tlp.translateFrom),
            JSON::Escape(WideStringToString(std::regex_replace(text, std::wregex(L"\u200B"), L""))),
            codes.at(tlp.translateTo)),
        L"Content-Type: application/json\r\naccept: application/json"
    }) {
        if (auto translation = Copy(JSON::Parse(httpRequest.response)[L"auto_translation"][0].String()))
            return {true, HTML::Unescape(translation.value())};
        return {false, FormatString(L"%s: %s", TRANSLATION_ERROR, httpRequest.response)};
    } else return {false, FormatString(L"%s (code=%u)", TRANSLATION_ERROR, httpRequest.errorCode)};
}
