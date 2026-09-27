#include "mainwindow.h"
#include "module.h"
#include <winhttp.h>
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
}

int main(int argc, char *argv[])
{
	QDir::setCurrent(QFileInfo(S(GetModuleFilename().value())).absolutePath());

	const bool checkUpdate = Settings().value(KEY_CHECK_UPDATE, true).toBool();

	std::thread([checkUpdate]
	{
		if (!checkUpdate || !*VERSION) return;
		using InternetHandle = AutoHandle<Functor<WinHttpCloseHandle>>;
		// Queries GitHub releases API https://developer.github.com/v3/repos/releases/ and checks the last release tag to check if it's the same
		if (InternetHandle internet = WinHttpOpen(L"Mozilla/5.0 Textractor", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0))
			if (InternetHandle connection = WinHttpConnect(internet, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0))
				if (InternetHandle request = WinHttpOpenRequest(connection, L"GET", L"/repos/Chenx221/Textractor/releases", NULL, NULL, NULL, WINHTTP_FLAG_SECURE))
					if (WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, NULL))
					{
						char buffer[1000] = {};
						WinHttpReceiveResponse(request, NULL);
						WinHttpReadData(request, buffer, 1000, DUMMY);
						const char* newestTag = strstr(buffer, "/tag/");
						const char* myVersion = strstr(buffer, VERSION);
						if (newestTag && (!myVersion || abs(static_cast<int>(newestTag - myVersion)) > 10)) TEXTRACTOR_MESSAGE(UPDATE_AVAILABLE);
					}
	}).detach();

	QApplication app(argc, argv);
	InstallQtTranslations();
	app.setFont(QFont("MS Shell Dlg 2", 10));
	return MainWindow().show(), app.exec();
}
