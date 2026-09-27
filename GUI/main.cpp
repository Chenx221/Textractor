#include "mainwindow.h"
#include "module.h"
#include <winhttp.h>
#include <shellapi.h>
#include "../extensions/network.h"
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QTranslator>

extern const wchar_t* UPDATE_AVAILABLE;

namespace
{
	constexpr auto CONFIG_LANGUAGE = u8"Language";

	QString QtLocaleFromLanguageIndex(int languageIndex)
	{
		switch (languageIndex)
		{
			case 1: return u8"es";
			case 2: return u8"zh_CN";
			case 3: return u8"ru";
			case 5: return u8"it";
			case 6: return u8"pt_BR";
			case 8: return u8"ko";
			case 9: return u8"fr";
			case 10: return u8"tr";
			case 11: return u8"zh_TW";
			case 12: return u8"ja";
			case 13: return u8"de";
			case 0:
			default: return u8"en";
		}
	}

	bool TryLoadQtCatalog(QTranslator& translator, const QString& baseName, const QString& localeName, const QStringList& searchPaths)
	{
		const QString languageOnly = localeName.section(u8'_', 0, 0);
		for (const auto& path : searchPaths)
		{
			if (translator.load(QLocale(localeName), baseName, u8"_", path)) return true;
			if (languageOnly != localeName && translator.load(QLocale(languageOnly), baseName, u8"_", path)) return true;
		}
		return false;
	}

	void InstallQtTranslations()
	{
		Settings settings;
		const QString localeName = QtLocaleFromLanguageIndex(settings.value(CONFIG_LANGUAGE, 0).toInt());

		const QString appDir = QCoreApplication::applicationDirPath();
		const QStringList searchPaths{
			appDir + u8"/translations",
			appDir,
			QLibraryInfo::location(QLibraryInfo::TranslationsPath)
		};

		static QTranslator qtBaseTranslator;
		if (TryLoadQtCatalog(qtBaseTranslator, u8"qtbase", localeName, searchPaths)) QCoreApplication::installTranslator(&qtBaseTranslator);

		static QTranslator qtTranslator;
		if (TryLoadQtCatalog(qtTranslator, u8"qt", localeName, searchPaths)) QCoreApplication::installTranslator(&qtTranslator);
	}
	constexpr auto UPDATE_REPO = L"Chenx221/Textractor";
	constexpr DWORD UPDATE_CHECK_TIMEOUT_MS = 5000;

	bool IsVersionDate(const std::wstring& text)
	{
		return text.size() >= 6 && text.substr(0, 6).find_first_not_of(L"0123456789") == std::wstring::npos;
	}

	void CheckForUpdates()
	{
		const std::wstring version = StringToWideString(VERSION);
		if (!IsVersionDate(version)) return;
		const std::wstring flavor = version.substr(std::min<size_t>(6, version.size()));
		const std::wstring agent = L"Textractor/" + version;
		const std::wstring path = L"/repos/" + std::wstring(UPDATE_REPO) + L"/releases";

		HttpRequest httpRequest{
			agent.c_str(),
			L"api.github.com",
			L"GET",
			path.c_str(),
			"",
			nullptr,
			INTERNET_DEFAULT_HTTPS_PORT,
			nullptr,
			WINHTTP_FLAG_SECURE,
			nullptr,
			nullptr,
			UPDATE_CHECK_TIMEOUT_MS
		};
		if (!httpRequest || httpRequest.statusCode != 200) return;

		auto releases = JSON::Parse(httpRequest.response);
		if (!releases.IsArray()) return;

		const std::wstring ownDate = version.substr(0, 6);
		std::wstring newestDate, newestUrl;
		for (int i = 0; i < static_cast<int>(releases.Size()); ++i)
		{
			const auto& release = releases[i];
			if (auto draft = release[L"draft"].Boolean(); draft && *draft) continue;
			if (auto prerelease = release[L"prerelease"].Boolean(); prerelease && *prerelease) continue;

			auto tag = release[L"tag_name"].String();
			if (!tag) continue;
			std::wstring name = *tag;
			if (!name.empty() && (name[0] == L'v' || name[0] == L'V')) name.erase(0, 1);
			if (!IsVersionDate(name)) continue;

			const std::wstring releaseDate = name.substr(0, 6);
			if (releaseDate <= ownDate || (!newestDate.empty() && releaseDate <= newestDate)) continue;

			const std::wstring assetName = L"Textractor_" + releaseDate + flavor + L".7z";
			auto assets = release[L"assets"].Array();
			if (!assets) continue;
			for (const auto& asset : *assets)
			{
				auto fileName = asset[L"name"].String();
				if (!fileName || *fileName != assetName) continue;
				if (auto url = asset[L"browser_download_url"].String()) { newestDate = releaseDate; newestUrl = *url; }
				break;
			}
		}
		if (newestUrl.empty()) return;

		const std::wstring message = FormatString(UPDATE_AVAILABLE, (L"v" + newestDate + flavor).c_str(), version.c_str());
		if (MessageBoxW(NULL, message.c_str(), L"Textractor", MB_OK) == IDOK)
			ShellExecuteW(NULL, L"open", newestUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}
}

int main(int argc, char *argv[])
{
	QDir::setCurrent(QFileInfo(S(GetModuleFilename().value())).absolutePath());

	const bool checkUpdate = Settings().value(KEY_CHECK_UPDATE, true).toBool();

	std::thread([checkUpdate]
	{
		if (!checkUpdate || !*VERSION) return;
		CheckForUpdates();
	}).detach();

	QApplication app(argc, argv);
	InstallQtTranslations();
	app.setFont(QFont("MS Shell Dlg 2", 10));
	return MainWindow().show(), app.exec();
}
