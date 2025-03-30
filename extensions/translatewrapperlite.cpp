#include "qtcommon.h"
#include "extension.h"
#include "blockmarkup.h"
#include "translatewrapperlite.h"
#include <concurrent_priority_queue.h>
#include <fstream>
#include <QComboBox>

#include "../texthook/main.h"

extern const char *NATIVE_LANGUAGE;
extern const char *TRANSLATE_TO;
extern const char *TRANSLATE_FROM;
extern const char *TRANSLATE_SELECTED_THREAD_ONLY;
extern const char *RATE_LIMIT_ALL_THREADS;
extern const char *RATE_LIMIT_SELECTED_THREAD;
extern const char *USE_TRANS_CACHE;
extern const char *FILTER_GARBAGE;
extern const char *MAX_TRANSLATIONS_IN_TIMESPAN;
extern const char *TIMESPAN;
extern const char *MAX_SENTENCE_SIZE;
extern const wchar_t *SENTENCE_TOO_LARGE_TO_TRANS;
extern const wchar_t *TRANSLATION_ERROR;
extern const wchar_t *TOO_MANY_TRANS_REQUESTS;

extern const char *TRANSLATION_PROVIDER;
extern const QStringList languagesTo, languagesFrom;
extern bool translateSelectedOnly, useRateLimiter, rateLimitSelected, useCache, useFilter;
extern int tokenCount, rateLimitTimespan, maxSentenceSize, port;

std::pair<bool, std::wstring> Translate(const std::wstring &text, TranslationParam tlp);

QFormLayout *display;
Settings settings;

namespace {
    Synchronized<TranslationParam> tlp;
    Synchronized<std::unordered_map<std::wstring, std::wstring> > translationCache;

    std::string CacheFile() {
        return FormatString("%s Cache (%S).txt", TRANSLATION_PROVIDER, tlp->translateTo);
    }

    void SaveCache() {
        std::wstring allTranslations(L"\xfeff");
        for (const auto &[sentence, translation]: translationCache.Acquire().contents)
            allTranslations.append(L"|SENTENCE|").append(sentence).append(L"|TRANSLATION|").append(translation).append(
                L"|END|\r\n");
        std::ofstream(CacheFile(), std::ios::binary | std::ios::trunc).write(
            (const char *) allTranslations.c_str(), allTranslations.size() * sizeof(wchar_t));
    }

    void LoadCache() {
        translationCache->clear();
        std::ifstream stream(CacheFile(), std::ios::binary);
        BlockMarkupIterator savedTranslations(stream, Array<std::wstring_view>{L"|SENTENCE|", L"|TRANSLATION|"});
        auto translationCache = ::translationCache.Acquire();
        while (auto read = savedTranslations.Next()) {
            auto &[sentence, translation] = read.value();
            translationCache->try_emplace(std::move(sentence), std::move(translation));
        }
    }
}

class Window : public QDialog, Localizer {
public:
    Window() : QDialog(nullptr, Qt::WindowMinMaxButtonsHint) {
        display = new QFormLayout(this);

        settings.beginGroup(TRANSLATION_PROVIDER);

        auto infoLabel = new QLabel("Q: How do I set the source language, target language, and API key?\nA: You need to modify the corresponding translate API's .env file.", this);
        display->addRow(infoLabel);

        auto translateToCombo = new QComboBox(this);
        translateToCombo->addItems(languagesTo);
        int i = -1;
        if (settings.contains(TRANSLATE_TO)) i = translateToCombo->findText(settings.value(TRANSLATE_TO).toString());
        if (i < 0) i = 0;
        translateToCombo->setCurrentIndex(i);
        SaveTranslateTo(translateToCombo->currentText());
        display->addRow(TRANSLATE_TO, translateToCombo);
        connect(translateToCombo, &QComboBox::currentTextChanged, this, &Window::SaveTranslateTo);

        auto translateFromCombo = new QComboBox(this);
        translateFromCombo->addItems(languagesFrom);
        i = -1;
        if (settings.contains(TRANSLATE_FROM)) i = translateFromCombo->findText(
                                                   settings.value(TRANSLATE_FROM).toString());
        if (i < 0) i = 0;
        translateFromCombo->setCurrentIndex(i);
        SaveTranslateFrom(translateFromCombo->currentText());
        display->addRow(TRANSLATE_FROM, translateFromCombo);
        connect(translateFromCombo, &QComboBox::currentTextChanged, this, &Window::SaveTranslateFrom);

        for (auto [value, label]: Array<bool &, const char *>{
                 {translateSelectedOnly, TRANSLATE_SELECTED_THREAD_ONLY},
                 {useRateLimiter, RATE_LIMIT_ALL_THREADS},
                 {rateLimitSelected, RATE_LIMIT_SELECTED_THREAD},
                 {useCache, USE_TRANS_CACHE},
                 {useFilter, FILTER_GARBAGE}
             }) {
            value = settings.value(label, value).toBool();
            auto checkBox = new QCheckBox(this);
            checkBox->setChecked(value);
            display->addRow(label, checkBox);
            connect(checkBox, &QCheckBox::clicked, [label, &value](bool checked) {
                settings.setValue(label, value = checked);
            });
        }

        for (auto [value, label]: Array<int &, const char *>{
                 {tokenCount, MAX_TRANSLATIONS_IN_TIMESPAN},
                 {rateLimitTimespan, TIMESPAN},
                 {maxSentenceSize, MAX_SENTENCE_SIZE},
                 {port, "Port"}
             }) {
            value = settings.value(label, value).toInt();
            auto spinBox = new QSpinBox(this);
            spinBox->setRange(0, INT_MAX);
            if (label == "Port") spinBox->setRange(1, 65535);
            spinBox->setValue(value);
            display->addRow(label, spinBox);
            connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), [label, &value](int newValue) {
                settings.setValue(label, value = newValue);
            });
        }

        setWindowTitle(TRANSLATION_PROVIDER);
        QMetaObject::invokeMethod(this, &QWidget::show, Qt::QueuedConnection);
    }

    ~Window() {
        SaveCache();
    }

private:
    void SaveTranslateTo(QString language) {
        SaveCache();
        settings.setValue(TRANSLATE_TO, S(tlp->translateTo = S(language)));
        LoadCache();
    }

    void SaveTranslateFrom(QString language) {
        settings.setValue(TRANSLATE_FROM, S(tlp->translateFrom = S(language)));
    }

} window;

std::vector<std::wstring> splitWString(const std::wstring &sentence, const std::wstring &delimiter) {
    std::vector<std::wstring> result;
    size_t start = 0;
    size_t end = sentence.find(delimiter);

    while (end != std::wstring::npos) {
        result.push_back(sentence.substr(start, end - start));
        start = end + delimiter.length();
        end = sentence.find(delimiter, start);
    }

    result.push_back(sentence.substr(start));
    return result;
}

bool ProcessSentence(std::wstring &sentence, SentenceInfo sentenceInfo) {
    if (sentenceInfo["text number"] == 0) return false; // not processing the sentence from console

    static class {
    public:
        bool Request() {
            DWORD64 current = GetTickCount64(), token;
            while (tokens.try_pop(token))
                if (token > current - rateLimitTimespan) {
                    tokens.push(token); // popped one too many
                    break;
                }
            bool available = tokens.size() < tokenCount;
            if (available) tokens.push(current);
            return available;
        }

    private:
        concurrency::concurrent_priority_queue<DWORD64, std::greater<DWORD64> > tokens;
    } rateLimiter;

    bool cache = false;
    std::wstring translation;
    std::vector<std::wstring> parts = splitWString(sentence, L"\x200b \n");
    std::wstring &originalSentence = parts[0];

    if (useFilter) {
        Trim(originalSentence);
        originalSentence.erase(std::remove_if(originalSentence.begin(), originalSentence.end(),
                                              [](wchar_t ch) { return ch < ' ' && ch != '\n'; }), originalSentence.end());
    }
    if (originalSentence.empty()) return true;
    if (originalSentence.size() > maxSentenceSize) translation = SENTENCE_TOO_LARGE_TO_TRANS;
    if (useCache) {
        auto translationCache = ::translationCache.Acquire();
        if (auto it = translationCache->find(originalSentence); it != translationCache->end()) translation = it->second;
    }
    if (translation.empty() && (!translateSelectedOnly || sentenceInfo["current select"]))
        if (rateLimiter.Request() || !useRateLimiter || (!rateLimitSelected && sentenceInfo["current select"]))
            std::tie(cache, translation) = Translate(originalSentence, tlp.Copy());
        else translation = TOO_MANY_TRANS_REQUESTS;
    if (cache) translationCache->operator[](originalSentence) = translation;

    // if (useFilter) Trim(translation);
    for (int i = 0; i < translation.size(); ++i) if (translation[i] == '\r' && translation[i + 1] == '\n')
        translation[i] = 0x200b; // for some reason \r appears as newline - no need to double
    if (translation.empty()) translation = TRANSLATION_ERROR;

    sentence = originalSentence;
    for (size_t i = 1; i < parts.size(); ++i) {
        sentence += L"\x200b \n" + parts[i];
    }
    sentence += L"\x200b \n" + translation;
    return true;
}

extern const std::unordered_map<std::wstring, std::wstring> codes;
TEST(
    {
        // assert(Translate(L"こんにちは",{ L"?", L"?"}).second.find(L"ello") == 1 || strstr(TRANSLATION_PROVIDER, "DevTools"));
    }
);

