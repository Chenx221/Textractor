#pragma once

struct TranslationParam
{
	std::wstring translateTo, translateFrom, authKey;
	std::wstring provider, model, apiHost, apiPath, systemPrompt;
	double temperature;
	bool includePreviousContext = false;
	bool enableThinking = false;
	std::wstring reasoningEffort = L"high";
	std::wstring previousSentence1;
	std::wstring previousSentence2;
};

