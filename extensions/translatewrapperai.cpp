#include "qtcommon.h"
#include "extension.h"
#include "translatewrapperai.h"
#include "blockmarkup.h"
#include <concurrent_priority_queue.h>
#include <deque>
#include <fstream>
#include <QComboBox>
#include <QDoubleSpinBox>

extern const char* NATIVE_LANGUAGE;
extern const char* TRANSLATE_TO;
extern const char* TRANSLATE_FROM;
extern const char* TRANSLATE_SELECTED_THREAD_ONLY;
extern const char* RATE_LIMIT_ALL_THREADS;
extern const char* RATE_LIMIT_SELECTED_THREAD;
extern const char* USE_TRANS_CACHE;
extern const char* FILTER_GARBAGE;
extern const char* MAX_TRANSLATIONS_IN_TIMESPAN;
extern const char* TIMESPAN;
extern const char* MAX_SENTENCE_SIZE;
extern const char* API_KEY;
extern const char* INCLUDE_PREVIOUS_CONTEXT;
extern const char* AI_TEMPERATURE;
extern const char* AI_SYSTEM_PROMPT;
extern const char* AI_API_PATH;
extern const char* AI_API_HOST;
extern const char* AI_MODEL;
extern const char* AI_PROVIDER;
extern const char* CUSTOM_TRANSLATE_TO;
extern const char* CUSTOM_TRANSLATE_FROM;
extern const char* AI_THINKING;
extern const char* AI_REASONING_EFFORT;
extern const wchar_t* SENTENCE_TOO_LARGE_TO_TRANS;
extern const wchar_t* TRANSLATION_ERROR;
extern const wchar_t* TOO_MANY_TRANS_REQUESTS;

extern const char* TRANSLATION_PROVIDER;
extern const char* GET_API_KEY_FROM;
extern const QStringList languagesTo, languagesFrom;
extern const QStringList aiProviders, aiModels;
extern const QStringList reasoningEfforts;
extern const std::unordered_map<std::wstring, std::wstring> providerApiHosts;
extern const std::unordered_map<std::wstring, std::wstring> providerApiPaths;
extern const wchar_t* AI_DEFAULT_PROVIDER;
extern const wchar_t* AI_DEFAULT_MODEL;
extern const wchar_t* AI_DEFAULT_API_HOST;
extern const wchar_t* AI_DEFAULT_API_PATH;
extern const wchar_t* AI_DEFAULT_SYSTEM_PROMPT;
extern const double AI_DEFAULT_TEMPERATURE;
extern bool translateSelectedOnly, useRateLimiter, rateLimitSelected, useCache, useFilter;
extern bool includePreviousContext;
extern bool enableThinking;
extern int tokenCount, rateLimitTimespan, maxSentenceSize;
std::pair<bool, std::wstring> Translate(const std::wstring& text, TranslationParam tlp);

constexpr auto KEY_TRANSLATE_TO = u8"Translate to";
constexpr auto KEY_TRANSLATE_FROM = u8"Translate from";
constexpr auto KEY_TRANSLATE_SELECTED_THREAD_ONLY = u8"Translate selected text thread only";
constexpr auto KEY_RATE_LIMIT_ALL_THREADS = u8"Use rate limiter";
constexpr auto KEY_RATE_LIMIT_SELECTED_THREAD = u8"Rate limit selected text thread";
constexpr auto KEY_USE_TRANS_CACHE = u8"Use translation cache";
constexpr auto KEY_FILTER_GARBAGE = u8"Filter garbage characters";
constexpr auto KEY_MAX_TRANSLATIONS_IN_TIMESPAN = u8"Max translation requests in timespan";
constexpr auto KEY_TIMESPAN = u8"Timespan (ms)";
constexpr auto KEY_MAX_SENTENCE_SIZE = u8"Max sentence size";
constexpr auto KEY_API_KEY = u8"API key";
constexpr auto KEY_AI_PROVIDER = u8"AI provider";
constexpr auto KEY_AI_MODEL = u8"AI model";
constexpr auto KEY_AI_API_HOST = u8"AI API host";
constexpr auto KEY_AI_API_PATH = u8"AI API path";
constexpr auto KEY_AI_SYSTEM_PROMPT = u8"AI system prompt";
constexpr auto KEY_AI_TEMPERATURE = u8"AI temperature";
constexpr auto KEY_CUSTOM_TRANSLATE_TO   = u8"Custom translate to";
constexpr auto KEY_CUSTOM_TRANSLATE_FROM = u8"Custom translate from";
constexpr auto KEY_INCLUDE_PREVIOUS_CONTEXT = u8"Include previous sentences";
constexpr auto KEY_AI_THINKING = u8"AI thinking mode";
constexpr auto KEY_AI_REASONING_EFFORT = u8"AI reasoning effort";

enum class Language;
extern Language CURRENT_LANGUAGE;
extern void Localize();

QFormLayout* display;
Settings settings;

namespace
{
	Synchronized<TranslationParam> tlp;
	Synchronized<std::unordered_map<std::wstring, std::wstring>> translationCache;
	Synchronized<std::unordered_map<int64_t, std::deque<std::wstring>>> previousSentencesByThread;
	bool initializingWindow = false;

	std::wstring SanitizeFilePart(std::wstring value)
	{
		if (value.empty()) value = L"default";
		for (auto& ch : value)
			if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
				ch = L'_';
		return value;
	}

	std::wstring CacheFile()
	{
		auto snapshot = tlp.Copy();
		return FormatString(
			L"%S Cache (%s-%s-%s-%s).txt",
			TRANSLATION_PROVIDER,
			SanitizeFilePart(snapshot.provider),
			SanitizeFilePart(snapshot.translateFrom),
			SanitizeFilePart(snapshot.translateTo),
			SanitizeFilePart(snapshot.model)
		);
	}
	void SaveCache()
	{
		std::wstring allTranslations(L"\xfeff");
		for (const auto& [sentence, translation] : translationCache.Acquire().contents)
			allTranslations.append(L"|SENTENCE|").append(sentence).append(L"|TRANSLATION|").append(translation).append(L"|END|\r\n");
		std::ofstream(CacheFile(), std::ios::binary | std::ios::trunc).write((const char*)allTranslations.c_str(), allTranslations.size() * sizeof(wchar_t));
	}
	void LoadCache()
	{
		translationCache->clear();
		std::ifstream stream(CacheFile(), std::ios::binary);
		BlockMarkupIterator savedTranslations(stream, Array<std::wstring_view>{ L"|SENTENCE|", L"|TRANSLATION|" });
		auto translationCache = ::translationCache.Acquire();
		while (auto read = savedTranslations.Next())
		{
			auto& [sentence, translation] = read.value();
			translationCache->try_emplace(std::move(sentence), std::move(translation));
		}
	}

	std::pair<std::wstring, std::wstring> GetPreviousSentences(int64_t threadId)
	{
		auto all = previousSentencesByThread.Acquire();
		auto it = all->find(threadId);
		if (it == all->end()) return {};
		auto& history = it->second;
		std::pair<std::wstring, std::wstring> out;
		if (history.size() >= 1) out.first = history.at(history.size() - 1);    // previousSentence1
		if (history.size() >= 2) out.second = history.at(history.size() - 2);   // previousSentence2
		return out;
	}

	void PushPreviousSentence(int64_t threadId, const std::wstring& sentence)
	{
		auto all = previousSentencesByThread.Acquire();
		auto& history = all->operator[](threadId);
		history.push_back(sentence);
		while (history.size() > 2) history.pop_front();
	}
}

class Window : public QDialog, Localizer
{
public:
	Window() : QDialog(nullptr, Qt::WindowMinMaxButtonsHint)
	{
		initializingWindow = true;
		QString iniPath = QCoreApplication::applicationDirPath() + "/Textractor.ini";
		QSettings settings0(iniPath, QSettings::IniFormat);
		int langId = settings0.value("Language", 0).toInt();
		CURRENT_LANGUAGE = static_cast<Language>(langId);
		Localize();

		display = new QFormLayout(this);
		settings.beginGroup(TRANSLATION_PROVIDER);

		auto translateToCombo = new QComboBox(this);
		translateToCombo->addItems(languagesTo);
		int i = -1;
		if (settings.contains(KEY_TRANSLATE_TO)) i = translateToCombo->findText(settings.value(KEY_TRANSLATE_TO).toString());
		if (i < 0) i = translateToCombo->findText(NATIVE_LANGUAGE, Qt::MatchStartsWith);
		if (i < 0) i = translateToCombo->findText("English", Qt::MatchStartsWith);
		translateToCombo->setCurrentIndex(i);
		SaveTranslateTo(translateToCombo->currentText());
		display->addRow(TRANSLATE_TO, translateToCombo);
		connect(translateToCombo, &QComboBox::currentTextChanged, this, &Window::SaveTranslateTo);

		// Custom target language input (shown only when "Custom..." is selected)
		customToEdit = new QLineEdit(settings.value(KEY_CUSTOM_TRANSLATE_TO, "").toString(), this);
		customToEdit->setPlaceholderText("e.g. French, German, Spanish...");
		customToEdit->setVisible(translateToCombo->currentText() == "Custom...");
		if (translateToCombo->currentText() == "Custom...")
			tlp->translateTo = S(customToEdit->text());
		display->addRow(CUSTOM_TRANSLATE_TO, customToEdit);
		SetCustomRowVisible(customToEdit, translateToCombo->currentText() == "Custom...");
		connect(customToEdit, &QLineEdit::editingFinished, this, [this](){
			SaveCustomTranslateTo(customToEdit->text());
		});

		auto translateFromCombo = new QComboBox(this);
		translateFromCombo->addItem("?");
		translateFromCombo->addItems(languagesFrom);
		i = -1;
		if (settings.contains(KEY_TRANSLATE_FROM)) i = translateFromCombo->findText(settings.value(KEY_TRANSLATE_FROM).toString());
		if (i < 0) i = 0;
		translateFromCombo->setCurrentIndex(i);
		SaveTranslateFrom(translateFromCombo->currentText());
		display->addRow(TRANSLATE_FROM, translateFromCombo);
		connect(translateFromCombo, &QComboBox::currentTextChanged, this, &Window::SaveTranslateFrom);

		// Custom source language input (shown only when "Custom..." is selected)
		customFromEdit = new QLineEdit(settings.value(KEY_CUSTOM_TRANSLATE_FROM, "").toString(), this);
		customFromEdit->setPlaceholderText("e.g. French, German, Spanish...");
		customFromEdit->setVisible(translateFromCombo->currentText() == "Custom...");
		if (translateFromCombo->currentText() == "Custom...")
			tlp->translateFrom = S(customFromEdit->text());
		display->addRow(CUSTOM_TRANSLATE_FROM, customFromEdit);
		SetCustomRowVisible(customFromEdit, translateFromCombo->currentText() == "Custom...");
		connect(customFromEdit, &QLineEdit::editingFinished, this, [this](){
			SaveCustomTranslateFrom(customFromEdit->text());
		});

		providerCombo = new QComboBox(this);
		providerCombo->addItems(aiProviders);
		i = providerCombo->findText(settings.value(KEY_AI_PROVIDER, QString::fromWCharArray(AI_DEFAULT_PROVIDER)).toString());
		if (i < 0) i = providerCombo->findText(QString::fromWCharArray(AI_DEFAULT_PROVIDER));
		if (i < 0) i = 0;
		providerCombo->setCurrentIndex(i);
		SaveProvider(providerCombo->currentText());
		display->addRow(AI_PROVIDER, providerCombo);
		connect(providerCombo, &QComboBox::currentTextChanged, this, &Window::SaveProvider);

		modelCombo = new QComboBox(this);
		modelCombo->setEditable(true);
		modelCombo->addItems(aiModels);
		i = modelCombo->findText(settings.value(KEY_AI_MODEL, QString::fromWCharArray(AI_DEFAULT_MODEL)).toString());
		if (i >= 0) modelCombo->setCurrentIndex(i);
		else modelCombo->setEditText(settings.value(KEY_AI_MODEL, QString::fromWCharArray(AI_DEFAULT_MODEL)).toString());
		SaveModel(modelCombo->currentText());
		display->addRow(AI_MODEL, modelCombo);
		connect(modelCombo, &QComboBox::currentTextChanged, this, &Window::SaveModel);

		apiHostEdit = new QLineEdit(settings.value(KEY_AI_API_HOST, QString::fromWCharArray(AI_DEFAULT_API_HOST)).toString(), this);
		SaveApiHost(apiHostEdit->text());
		connect(apiHostEdit, &QLineEdit::textChanged, this, &Window::SaveApiHost);
		display->addRow(AI_API_HOST, apiHostEdit);

		apiPathEdit = new QLineEdit(settings.value(KEY_AI_API_PATH, QString::fromWCharArray(AI_DEFAULT_API_PATH)).toString(), this);
		SaveApiPath(apiPathEdit->text());
		connect(apiPathEdit, &QLineEdit::textChanged, this, &Window::SaveApiPath);
		display->addRow(AI_API_PATH, apiPathEdit);

		std::wstring currentProvider = tlp.Copy().provider;
		UpdateProviderDependentUi(currentProvider);

		auto promptEdit = new QLineEdit(settings.value(KEY_AI_SYSTEM_PROMPT, QString::fromWCharArray(AI_DEFAULT_SYSTEM_PROMPT)).toString(), this);
		SaveSystemPrompt(promptEdit->text());
		connect(promptEdit, &QLineEdit::textChanged, this, &Window::SaveSystemPrompt);
		display->addRow(AI_SYSTEM_PROMPT, promptEdit);

		auto temperatureSpin = new QDoubleSpinBox(this);
		temperatureSpin->setRange(0.0, 2.0);
		temperatureSpin->setSingleStep(0.1);
		temperatureSpin->setDecimals(2);
		double temperature = settings.value(KEY_AI_TEMPERATURE, AI_DEFAULT_TEMPERATURE).toDouble();
		temperatureSpin->setValue(temperature);
		SaveTemperature(temperature);
		connect(temperatureSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &Window::SaveTemperature);
		display->addRow(AI_TEMPERATURE, temperatureSpin);

		auto thinkingCheckBox = new QCheckBox(this);
		enableThinking = settings.value(KEY_AI_THINKING, enableThinking).toBool();
		thinkingCheckBox->setChecked(enableThinking);
		display->addRow(AI_THINKING, thinkingCheckBox);

		auto reasoningEffortCombo = new QComboBox(this);
		reasoningEffortCombo->addItems(reasoningEfforts);
		QString savedEffort = settings.value(KEY_AI_REASONING_EFFORT, "high").toString();
		int effortIdx = reasoningEffortCombo->findText(savedEffort);
		if (effortIdx >= 0) reasoningEffortCombo->setCurrentIndex(effortIdx);
		reasoningEffortCombo->setEnabled(enableThinking);
		tlp->reasoningEffort = S(reasoningEffortCombo->currentText());
		display->addRow(AI_REASONING_EFFORT, reasoningEffortCombo);

		connect(thinkingCheckBox, &QCheckBox::toggled, this, [this, reasoningEffortCombo](bool checked) {
			settings.setValue(KEY_AI_THINKING, enableThinking = checked);
			tlp->enableThinking = checked;
			reasoningEffortCombo->setEnabled(checked);
		});
		connect(reasoningEffortCombo, &QComboBox::currentTextChanged, this, [this](const QString& effort) {
			settings.setValue(KEY_AI_REASONING_EFFORT, S(tlp->reasoningEffort = S(effort)));
		});

		for (auto [value, label, keylabel] : Array<bool&, const char*, const char*>{
			{ includePreviousContext, INCLUDE_PREVIOUS_CONTEXT, KEY_INCLUDE_PREVIOUS_CONTEXT },
			{ translateSelectedOnly, TRANSLATE_SELECTED_THREAD_ONLY, KEY_TRANSLATE_SELECTED_THREAD_ONLY },
			{ useRateLimiter, RATE_LIMIT_ALL_THREADS, KEY_RATE_LIMIT_ALL_THREADS },
			{ rateLimitSelected, RATE_LIMIT_SELECTED_THREAD, KEY_RATE_LIMIT_SELECTED_THREAD },
			{ useCache, USE_TRANS_CACHE, KEY_USE_TRANS_CACHE },
			{ useFilter, FILTER_GARBAGE, KEY_FILTER_GARBAGE }
		})
		{
			value = settings.value(keylabel, value).toBool();
			auto checkBox = new QCheckBox(this);
			checkBox->setChecked(value);
			display->addRow(label, checkBox);
			connect(checkBox, &QCheckBox::clicked, [keylabel, &value](bool checked) { settings.setValue(keylabel, value = checked); });
		}
		for (auto [value, label, keylabel] : Array<int&, const char*, const char*>{
			{ tokenCount, MAX_TRANSLATIONS_IN_TIMESPAN, KEY_MAX_TRANSLATIONS_IN_TIMESPAN },
			{ rateLimitTimespan, TIMESPAN, KEY_TIMESPAN },
			{ maxSentenceSize, MAX_SENTENCE_SIZE, KEY_MAX_SENTENCE_SIZE },
		})
		{
			value = settings.value(keylabel, value).toInt();
			auto spinBox = new QSpinBox(this);
			spinBox->setRange(0, INT_MAX);
			spinBox->setValue(value);
			display->addRow(label, spinBox);
			connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), [keylabel, &value](int newValue) { settings.setValue(keylabel, value = newValue); });
		}
		if (GET_API_KEY_FROM)
		{
			auto keyEdit = new QLineEdit(settings.value(KEY_API_KEY).toString(), this);
			tlp->authKey = S(keyEdit->text());
			QObject::connect(keyEdit, &QLineEdit::textChanged, [](QString key) { settings.setValue(KEY_API_KEY, S(tlp->authKey = S(key))); });
			auto keyLabel = new QLabel(QString("<a href=\"%1\">%2</a>").arg(GET_API_KEY_FROM, API_KEY), this);
			keyLabel->setOpenExternalLinks(true);
			display->addRow(keyLabel, keyEdit);
		}

		setWindowTitle(TRANSLATION_PROVIDER);
		initializingWindow = false;
		LoadCache();
		QMetaObject::invokeMethod(this, &QWidget::show, Qt::QueuedConnection);
	}

	~Window()
	{
		SaveCache();
	}

private:
	QLineEdit* customToEdit   = nullptr;
	QLineEdit* customFromEdit = nullptr;
	QComboBox* providerCombo = nullptr;
	QComboBox* modelCombo = nullptr;
	QLineEdit* apiHostEdit = nullptr;
	QLineEdit* apiPathEdit = nullptr;

	// Toggle the visibility of a custom-language row in the form layout
	void SetCustomRowVisible(QLineEdit* edit, bool visible)
	{
		if (!edit) return;
		edit->setVisible(visible);
		if (auto* lbl = display->labelForField(edit))
			lbl->setVisible(visible);
	}

	void UpdateProviderDependentUi(const std::wstring& provider)
	{
		const bool editable = true;
		if (apiHostEdit) apiHostEdit->setReadOnly(!editable);
		if (apiPathEdit) apiPathEdit->setReadOnly(!editable);

		if (auto hostIt = providerApiHosts.find(provider); hostIt != providerApiHosts.end() && !hostIt->second.empty())
		{
			if (apiHostEdit) apiHostEdit->setText(S(hostIt->second));
			else tlp->apiHost = hostIt->second;
		}
		if (auto pathIt = providerApiPaths.find(provider); pathIt != providerApiPaths.end() && !pathIt->second.empty())
		{
			if (apiPathEdit) apiPathEdit->setText(S(pathIt->second));
			else tlp->apiPath = pathIt->second;
		}
	}

	void SaveTranslateTo(QString language)
	{
		SetCustomRowVisible(customToEdit, language == "Custom...");
		if (initializingWindow)
		{
			settings.setValue(KEY_TRANSLATE_TO, language);
			tlp->translateTo = (language == "Custom..." && customToEdit) ? S(customToEdit->text()) : S(language);
			return;
		}
		SaveCache();
		settings.setValue(KEY_TRANSLATE_TO, language);
		tlp->translateTo = (language == "Custom..." && customToEdit) ? S(customToEdit->text()) : S(language);
		LoadCache();
	}
	void SaveTranslateFrom(QString language)
	{
		SetCustomRowVisible(customFromEdit, language == "Custom...");
		if (initializingWindow)
		{
			settings.setValue(KEY_TRANSLATE_FROM, language);
			tlp->translateFrom = (language == "Custom..." && customFromEdit) ? S(customFromEdit->text()) : S(language);
			return;
		}
		SaveCache();
		settings.setValue(KEY_TRANSLATE_FROM, language);
		tlp->translateFrom = (language == "Custom..." && customFromEdit) ? S(customFromEdit->text()) : S(language);
		LoadCache();
	}
	void SaveCustomTranslateTo(QString customLang)
	{
		settings.setValue(KEY_CUSTOM_TRANSLATE_TO, customLang);
		if (!initializingWindow)
		{
			SaveCache();
			tlp->translateTo = S(customLang);
			LoadCache();
		}
		else
		{
			tlp->translateTo = S(customLang);
		}
	}
	void SaveCustomTranslateFrom(QString customLang)
	{
		settings.setValue(KEY_CUSTOM_TRANSLATE_FROM, customLang);
		if (!initializingWindow)
		{
			SaveCache();
			tlp->translateFrom = S(customLang);
			LoadCache();
		}
		else
		{
			tlp->translateFrom = S(customLang);
		}
	}
	void SaveProvider(QString provider)
	{
		const std::wstring providerW = S(provider);
		if (initializingWindow)
		{
			settings.setValue(KEY_AI_PROVIDER, S(tlp->provider = providerW));
			UpdateProviderDependentUi(providerW);
			return;
		}
		SaveCache();
		settings.setValue(KEY_AI_PROVIDER, S(tlp->provider = providerW));
		UpdateProviderDependentUi(providerW);
		LoadCache();
	}
	void SaveModel(QString model)
	{
		if (initializingWindow)
		{
			settings.setValue(KEY_AI_MODEL, S(tlp->model = S(model)));
			return;
		}
		SaveCache();
		settings.setValue(KEY_AI_MODEL, S(tlp->model = S(model)));
		LoadCache();
	}
	void SaveApiHost(QString host)
	{
		settings.setValue(KEY_AI_API_HOST, S(tlp->apiHost = S(host)));
	}
	void SaveApiPath(QString path)
	{
		settings.setValue(KEY_AI_API_PATH, S(tlp->apiPath = S(path)));
	}
	void SaveSystemPrompt(QString prompt)
	{
		settings.setValue(KEY_AI_SYSTEM_PROMPT, S(tlp->systemPrompt = S(prompt)));
	}
	void SaveTemperature(double temperature)
	{
		settings.setValue(KEY_AI_TEMPERATURE, tlp->temperature = temperature);
	}
};

Window& GetWindow()
{
	static Window window;
	return window;
}

static Window& window = GetWindow();

std::vector<std::wstring> splitWString(const std::wstring& sentence, const std::wstring& delimiter) {
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

bool ProcessSentence(std::wstring& sentence, SentenceInfo sentenceInfo) {
	if (sentenceInfo["text number"] == 0) return false; // not processing the sentence from console
	const int64_t threadId = sentenceInfo["text number"];

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
		concurrency::concurrent_priority_queue<DWORD64, std::greater<DWORD64>> tokens;
	} rateLimiter;

	bool cache = false;
	std::wstring translation;
	std::vector<std::wstring> parts = splitWString(sentence, L"\x200b \n");
	std::wstring& originalSentence = parts[0];

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
		{
			auto requestParam = tlp.Copy();
			requestParam.enableThinking = enableThinking;
			if (includePreviousContext)
			{
				auto [prev1, prev2] = GetPreviousSentences(threadId);
				requestParam.includePreviousContext = true;
				requestParam.previousSentence1 = std::move(prev1);
				requestParam.previousSentence2 = std::move(prev2);
			}
			std::tie(cache, translation) = Translate(originalSentence, requestParam);
		}
		else translation = TOO_MANY_TRANS_REQUESTS;
	if (cache) translationCache->operator[](originalSentence) = translation;

	if (useFilter) Trim(translation);
	for (int i = 0; i < translation.size(); ++i) if (translation[i] == '\r' && translation[i + 1] == '\n')
		translation[i] = 0x200b; // for some reason \r appears as newline - no need to double
	if (translation.empty()) translation = TRANSLATION_ERROR;

	sentence = originalSentence;
	for (size_t i = 1; i < parts.size(); ++i) {
		sentence += L"\x200b \n" + parts[i];
	}
	sentence += L"\x200b \n" + translation;
	PushPreviousSentence(threadId, originalSentence);
	return true;
}

extern const std::unordered_map<std::wstring, std::wstring> codes;
TEST(
	{
		for (auto languages : { languagesFrom, languagesTo }) for (auto language : languages)
			assert(codes.count(S(language)));
		assert(codes.count(L"?"));
	}
);

