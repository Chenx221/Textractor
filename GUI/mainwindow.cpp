#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "defs.h"
#include "module.h"
#include "extenwindow.h"
#include "../host/host.h"
#include "../host/hookcode.h"
#include "attachprocessdialog.h"
#include <shellapi.h>
#include <process.h>
#include <qcryptographichash.h>
#include <QRegularExpression>
#include <QStringListModel>
#include <QScrollBar>
#include <QMenu>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QHash>
#include <QStyle>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include "../extensions/network.h"
#include <algorithm>
#include <QPainter>
#include <QSvgRenderer>

extern const char* ATTACH;
extern const char* LAUNCH;
extern const char* CONFIG;
extern const char* DETACH;
extern const char* FORGET;
extern const char* RESTART_AS_ADMIN;
extern const char* ADD_HOOK;
extern const char* REMOVE_HOOKS;
extern const char* SAVE_HOOKS;
extern const char* SEARCH_FOR_HOOKS;
extern const char* SETTINGS;
extern const char* EXTENSIONS;
extern const char* FONT;
extern const char* CLEAR_CURRENT_THREAD;
extern const char* SEARCH_GOOGLE;
extern const char* SELECT_PROCESS;
extern const char* SELECT_PROCESS_INFO;
extern const char* FROM_COMPUTER;
extern const char* PROCESSES;
extern const char* CODE_INFODUMP;
extern const char* FAILED_TO_CREATE_CONFIG_FILE;
extern const char* HOOK_SEARCH_UNSTABLE_WARNING;
extern const char* HOOK_SEARCH_STARTING_VIEW_CONSOLE;
extern const char* SEARCH_CJK;
extern const char* SEARCH_PATTERN;
extern const char* SEARCH_DURATION;
extern const char* SEARCH_MODULE;
extern const char* PATTERN_OFFSET;
extern const char* MIN_ADDRESS;
extern const char* MAX_ADDRESS;
extern const char* STRING_OFFSET;
extern const char* MAX_HOOK_SEARCH_RECORDS;
extern const char* HOOK_SEARCH_FILTER;
extern const char* SEARCH_FOR_TEXT;
extern const char* TEXT;
extern const char* CODEPAGE;
extern const char* START_HOOK_SEARCH;
extern const char* SAVE_SEARCH_RESULTS;
extern const char* TEXT_FILES;
extern const char* DOUBLE_CLICK_TO_REMOVE_HOOK;
extern const char* SAVE_SETTINGS;
extern const char* USE_JP_LOCALE;
extern const char* FILTER_REPETITION;
extern const char* AUTO_ATTACH;
extern const char* ATTACH_SAVED_ONLY;
extern const char* SHOW_SYSTEM_PROCESSES;
extern const char* DEFAULT_CODEPAGE;
extern const char* FLUSH_DELAY;
extern const char* FLUSH_DELAY_SPACING;
extern const char* FLUSH_DELAY_SPACING_OFF;
extern const char* FLUSH_DELAY_SPACING_SPACE;
extern const char* FLUSH_DELAY_SPACING_NEWLINE;
extern const char* MAX_BUFFER_SIZE;
extern const char* MAX_HISTORY_SIZE;
extern const char* LIMIT_STRING_LENGTH;
extern const char* CONFIG_JP_LOCALE;
extern const char* CHECK_UPDATE;
extern const wchar_t* ABOUT;
extern const wchar_t* CL_OPTIONS;
extern const wchar_t* LAUNCH_FAILED;
extern const wchar_t* INVALID_CODE;
extern const wchar_t* WINHTTP_TIMEOUT;
extern const wchar_t* WINHTTP_NAME_NOT_RESOLVED;
extern const wchar_t* WINHTTP_CANNOT_CONNECT;
extern const wchar_t* WINHTTP_CONNECTION_ERROR;
extern const wchar_t* WINHTTP_SECURE_FAILURE;
extern const wchar_t* WINHTTP_INVALID_SERVER_RESPONSE;
extern const wchar_t* WINHTTP_OPERATION_CANCELLED;
extern const wchar_t* UNKNOWN_ERROR;
extern const wchar_t* CHECKING_UPDATE;
extern const wchar_t* OPEN_TEXTHOOK_FAILED;
extern const wchar_t* CHECK_UPDATE_FAILED;
extern const wchar_t* CHECK_UPDATE_FAILED_CODE;
extern const wchar_t* CHECK_UPDATE_FAILED_HTTP;
extern const wchar_t* CHECK_UPDATE_FAILED_EMPTY_RESPONSE;
extern const wchar_t* CHECK_UPDATE_FAILED_MALFORMED_RESPONSE;
extern const wchar_t* CHECK_UPDATE_FAILED_UNKNOWN;
extern const wchar_t* CHECK_UPDATE_FAILED_UNEXPECTED_RESPONSE;
extern const wchar_t* TEXTHOOK_UPDATE_AVAILABLE;
extern const wchar_t* TEXTHOOK_IS_LATEST;

// Language configuration
extern const char* LANGUAGE_SETTING;
extern const char* LANGUAGE_RESTART_NOTICE;
enum class Language;
extern Language CURRENT_LANGUAGE;
extern const char* LANGUAGE_NAMES[];
extern const int LANGUAGE_COUNT;
extern void Localize();

namespace
{
	constexpr auto HOOK_SAVE_FILE = u8"SavedHooks.txt";
	constexpr auto GAME_SAVE_FILE = u8"SavedGames.txt";
	constexpr auto CONFIG_LANGUAGE = u8"Language";
	constexpr auto KEY_FONT = u8"Font";
	constexpr auto KEY_FILTER_REPETITION = u8"Filter repetition";
	constexpr auto KEY_AUTO_ATTACH = u8"Auto attach";
	constexpr auto KEY_ATTACH_SAVED_ONLY = u8"Auto attach (saved only)";
	constexpr auto KEY_CHECK_UPDATE = u8"Check for texthook updates on startup";
	constexpr auto KEY_SHOW_SYSTEM_PROCESSES = u8"Show system processes";
	constexpr auto KEY_FLUSH_DELAY_SPACING = u8"Flush delay string spacing";
	constexpr auto KEY_MAX_BUFFER_SIZE = u8"Max buffer size";
	constexpr auto KEY_FLUSH_DELAY = u8"Flush delay";
	constexpr auto KEY_MAX_HISTORY_SIZE = u8"Max history size";
	constexpr auto KEY_LIMIT_STRING_LENGTH = u8"Limit string length (0=Off)";
	constexpr auto KEY_DEFAULT_CODEPAGE = u8"Default codepage";
	constexpr auto KEY_CONFIG_JP_LOCALE = u8"Launch with JP locale";


	enum LaunchWithJapaneseLocale { PROMPT, ALWAYS, NEVER };

	Ui::MainWindow ui;
	std::atomic<DWORD> selectedProcessId = 0;
	ExtenWindow* extenWindow = nullptr;
	std::unordered_set<DWORD> alreadyAttached;
	bool autoAttach = false, autoAttachSavedOnly = true, checkUpdate = false;
	bool showSystemProcesses = false;
	uint64_t savedThreadCtx = 0, savedThreadCtx2 = 0;
	wchar_t savedThreadCode[1000] = {};
	TextThread* current = nullptr;
	MainWindow* This = nullptr;
	bool refreshingThemeStyle = false;

	void FindHooks();
	void RefreshIcons();

	void RefreshThemeStyle()
	{
		if (!This || refreshingThemeStyle) return;
		refreshingThemeStyle = true;

		// Force QSS-based controls to rebuild after system palette/theme switches.
		const QString styleSheet = This->styleSheet();
		This->setStyleSheet(QString());
		This->setStyleSheet(styleSheet);

		for (QWidget* widget : This->findChildren<QWidget*>())
		{
			if (QStyle* style = widget->style())
			{
				style->unpolish(widget);
				style->polish(widget);
			}
			widget->update();
		}

		RefreshIcons();

		This->update();
		refreshingThemeStyle = false;
	}

	bool IsRunningAsAdmin()
	{
		BOOL isAdmin = FALSE;
		PSID administratorsGroup = nullptr;
		SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

		if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup))
		{
			CheckTokenMembership(nullptr, administratorsGroup, &isAdmin);
			FreeSid(administratorsGroup);
		}
		return isAdmin != FALSE;
	}

	void RestartAsAdmin()
	{
		wchar_t exePath[MAX_PATH];
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);

		SHELLEXECUTEINFOW sei = { sizeof(sei) };
		sei.lpVerb = L"runas";
		sei.lpFile = exePath;
		sei.hwnd = nullptr;
		sei.nShow = SW_NORMAL;

		if (ShellExecuteExW(&sei))
		{
			QApplication::quit();
		}
	}

	QString TextThreadString(TextThread& thread)
	{
		return QString("%1:%2:%3:%4:%5: %6").arg(
			QString::number(thread.handle, 16),
			QString::number(thread.tp.processId, 16),
			QString::number(thread.tp.addr, 16),
			QString::number(thread.tp.ctx, 16),
			QString::number(thread.tp.ctx2, 16)
		).toUpper().arg(S(thread.name));
	}

	ThreadParam ParseTextThreadString(QString ttString)
	{
		auto threadParam = ttString.split(":");
		return { threadParam[1].toUInt(nullptr, 16), threadParam[2].toULongLong(nullptr, 16), threadParam[3].toULongLong(nullptr, 16), threadParam[4].toULongLong(nullptr, 16) };
	}

	std::array<InfoForExtension, 20> GetSentenceInfo(TextThread& thread)
	{
		void (*AddText)(int64_t, const wchar_t*) = [](int64_t number, const wchar_t* text)
		{
			QMetaObject::invokeMethod(This, [number, text = std::wstring(text)] { if (TextThread* thread = Host::GetThread(number)) thread->Push(text.c_str()); });
		};
		void (*AddSentence)(int64_t, const wchar_t*) = [](int64_t number, const wchar_t* sentence)
		{
			// pointer from Host::GetThread may not stay valid unless on main thread
			QMetaObject::invokeMethod(This, [number, sentence = std::wstring(sentence)] { if (TextThread* thread = Host::GetThread(number)) thread->AddSentence(sentence); });
		};
		DWORD (*GetSelectedProcessId)() = [] { return selectedProcessId.load(); };

		return
		{ {
		{ "current select", &thread == current },
		{ "text number", thread.handle },
		{ "process id", thread.tp.processId },
		{ "hook address", (int64_t)thread.tp.addr },
		{ "text handle", thread.handle },
		{ "text name", (int64_t)thread.name.c_str() },
		{ "add sentence", (int64_t)AddSentence },
		{ "add text", (int64_t)AddText },
		{ "get selected process id", (int64_t)GetSelectedProcessId },
		{ "void (*AddSentence)(int64_t number, const wchar_t* sentence)", (int64_t)AddSentence },
		{ "void (*AddText)(int64_t number, const wchar_t* text)", (int64_t)AddText },
		{ "DWORD (*GetSelectedProcessId)()", (int64_t)GetSelectedProcessId },
		{ nullptr, 0 } // nullptr marks end of info array
		} };
	}

	void AttachSavedProcesses()
	{
		std::unordered_set<std::wstring> attachTargets;
		if (autoAttach)
			for (auto process : QString(QTextFile(GAME_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", Qt::SkipEmptyParts))
				attachTargets.insert(S(process));
		if (autoAttachSavedOnly)
			for (auto process : QString(QTextFile(HOOK_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", Qt::SkipEmptyParts))
				attachTargets.insert(S(process.split(" , ")[0]));

		if (!attachTargets.empty())
			for (auto [processId, processName] : GetAllProcesses())
				if (processName && attachTargets.count(processName.value()) > 0 && alreadyAttached.count(processId) == 0) Host::InjectProcess(processId);
	}

	std::optional<std::wstring> UserSelectedProcess()
	{
		QStringList savedProcesses = QString::fromUtf8(QTextFile(GAME_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", Qt::SkipEmptyParts);
		std::reverse(savedProcesses.begin(), savedProcesses.end());
		savedProcesses.removeDuplicates();
		if (savedProcesses.isEmpty()) {
			savedProcesses.append(FROM_COMPUTER);
		} else {
			if (savedProcesses.size() >= 1)
				savedProcesses.insert(1, FROM_COMPUTER);
			else
				savedProcesses.append(FROM_COMPUTER);
		}
		QString process = QInputDialog::getItem(This, SELECT_PROCESS, SELECT_PROCESS_INFO, savedProcesses, 0, true, &ok, Qt::WindowCloseButtonHint);
		if (process == FROM_COMPUTER) process = QDir::toNativeSeparators(QFileDialog::getOpenFileName(This, SELECT_PROCESS, "/", PROCESSES));
		if (ok && process.contains('\\')) return S(process);
		return {};
	}

	void ViewThread(int index)
	{
		ui.ttCombo->setCurrentIndex(index);
		ui.textOutput->setPlainText(sanitize(S((current = &Host::GetThread(ParseTextThreadString(ui.ttCombo->itemText(index))))->storage->c_str())));
		ui.textOutput->moveCursor(QTextCursor::End);
	}

	void AttachProcess()
	{
		auto getProcessList = []() -> std::vector<ProcessInfo> {
			std::vector<ProcessInfo> processInfoList;
			for (auto [processId, processName] : GetAllProcesses())
			{
				if (processName && (showSystemProcesses || processName->find(L":\\Windows\\") == std::string::npos))
				{
					QString fileName = QFileInfo(S(processName.value())).fileName();
					HICON hIcon = nullptr;
					ExtractIconExW(processName->c_str(), 0, &hIcon, nullptr, 1);

					auto windowTitle = GetProcessMainWindowTitle(processId);
					bool is64Bit = IsProcess64Bit(processId);

					processInfoList.emplace_back(
						processId,
						fileName,
						windowTitle ? S(windowTitle.value()) : QString(),
						is64Bit,
						hIcon
					);
				}
			}
			std::sort(processInfoList.begin(), processInfoList.end(), [](const auto& one, const auto& two)
			{
				return QString::compare(one.processName, two.processName, Qt::CaseInsensitive) < 0;
			});
			return processInfoList;
		};

		AttachProcessDialog attachProcessDialog(This, getProcessList(), getProcessList);
		if (attachProcessDialog.exec())
		{
			DWORD processId = attachProcessDialog.SelectedProcessId();
			if (processId) Host::InjectProcess(processId);
		}
	}

	void LaunchProcess()
	{
		std::wstring process;
		if (auto selected = UserSelectedProcess()) process = selected.value();
		else return;
		std::wstring path = std::wstring(process).erase(process.rfind(L'\\'));

		PROCESS_INFORMATION info = {};
		auto useLocale = Settings().value(KEY_CONFIG_JP_LOCALE, PROMPT).toInt();
		if (!x64 && (useLocale == ALWAYS || (useLocale == PROMPT && QMessageBox::question(This, SELECT_PROCESS, USE_JP_LOCALE) == QMessageBox::Yes)))
		{
			if (HMODULE localeEmulator = LoadLibraryW(L"LoaderDll"))
			{
				// https://github.com/xupefei/Locale-Emulator/blob/aa99dec3b25708e676c90acf5fed9beaac319160/LEProc/LoaderWrapper.cs#L252
				struct
				{
					ULONG AnsiCodePage = SHIFT_JIS;
					ULONG OemCodePage = SHIFT_JIS;
					ULONG LocaleID = LANG_JAPANESE;
					ULONG DefaultCharset = SHIFTJIS_CHARSET;
					ULONG HookUiLanguageApi = FALSE;
					WCHAR DefaultFaceName[LF_FACESIZE] = {};
					TIME_ZONE_INFORMATION Timezone;
					ULONG64 Unused = 0;
				} LEB;
				GetTimeZoneInformation(&LEB.Timezone);
				((LONG(__stdcall*)(decltype(&LEB), LPCWSTR appName, LPWSTR commandLine, LPCWSTR currentDir, void*, void*, PROCESS_INFORMATION*, void*, void*, void*, void*))
					GetProcAddress(localeEmulator, "LeCreateProcess"))(&LEB, process.c_str(), NULL, path.c_str(), NULL, NULL, &info, NULL, NULL, NULL, NULL);
			}
		}
		if (info.hProcess == NULL)
		{
			STARTUPINFOW DUMMY = { sizeof(DUMMY) };
			CreateProcessW(process.c_str(), NULL, nullptr, nullptr, FALSE, 0, nullptr, path.c_str(), &DUMMY, &info);
		}
		if (info.hProcess == NULL) return Host::AddConsoleOutput(LAUNCH_FAILED);
		Host::InjectProcess(info.dwProcessId);
		CloseHandle(info.hProcess);
		CloseHandle(info.hThread);
	}

	void ConfigureProcess()
	{
		if (auto processName = GetModuleFilename(selectedProcessId)) if (int last = processName->rfind(L'\\') + 1)
		{
			std::wstring configFile = std::wstring(processName.value()).replace(last, std::string::npos, GAME_CONFIG_FILE);
			if (!std::filesystem::exists(configFile)) QTextFile(S(configFile), QFile::WriteOnly).write("see https://github.com/Artikash/Textractor/wiki/Game-configuration-file");
			if (std::filesystem::exists(configFile)) _wspawnlp(_P_DETACH, L"notepad", L"notepad", configFile.c_str(), NULL);
			else QMessageBox::critical(This, CONFIG, QString(FAILED_TO_CREATE_CONFIG_FILE).arg(S(configFile)));
		}
	}

	void DetachProcess()
	{
		try { Host::DetachProcess(selectedProcessId); }
		catch (std::out_of_range) {}
	}

	void ForgetProcess()
	{
		auto processName = GetModuleFilename(selectedProcessId);
		if (!processName) processName = UserSelectedProcess();
		DetachProcess();
		if (!processName) return;
		for (auto file : { GAME_SAVE_FILE, HOOK_SAVE_FILE })
		{
			QStringList lines = QString::fromUtf8(QTextFile(file, QIODevice::ReadOnly).readAll()).split("\n", Qt::SkipEmptyParts);
			lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const QString& line) { return line.contains(S(processName.value())); }), lines.end());
			QTextFile(file, QIODevice::WriteOnly | QIODevice::Truncate).write(lines.join("\n").append("\n").toUtf8());
		}
	}

	void AddHook(QString hook)
	{
		if (QString hookCode = QInputDialog::getText(This, ADD_HOOK, CODE_INFODUMP, QLineEdit::Normal, hook, &ok, Qt::WindowCloseButtonHint); ok)
			if (hookCode.startsWith("S") || hookCode.startsWith("/S")) FindHooks(); // backwards compatibility for old hook search UX
			else if (auto hp = HookCode::Parse(S(hookCode))) try { Host::InsertHook(selectedProcessId, hp.value()); } catch (std::out_of_range) {}
			else Host::AddConsoleOutput(INVALID_CODE);
	}

	void AddHook()
	{
		AddHook("");
	}

	void RemoveHooks()
	{
		DWORD processId = selectedProcessId;
		std::unordered_map<uint64_t, HookParam> hooks;
		for (int i = 0; i < ui.ttCombo->count(); ++i)
		{
			ThreadParam tp = ParseTextThreadString(ui.ttCombo->itemText(i));
			if (tp.processId == selectedProcessId) hooks[tp.addr] = Host::GetThread(tp).hp;
		}
		auto hookList = new QListWidget(This);
		hookList->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
		hookList->setAttribute(Qt::WA_DeleteOnClose);
		hookList->setMinimumSize({ 300, 50 });
		hookList->setWindowTitle(DOUBLE_CLICK_TO_REMOVE_HOOK);
		for (auto [address, hp] : hooks) new QListWidgetItem(QString(hp.name) + "@" + QString::number(address, 16), hookList);
		QObject::connect(hookList, &QListWidget::itemDoubleClicked, [processId, hookList](QListWidgetItem* item)
		{
			try
			{
				Host::RemoveHook(processId, item->text().split("@")[1].toULongLong(nullptr, 16));
				delete item;
			}
			catch (std::out_of_range) { hookList->close(); }
		});
		hookList->show();
	}

	void SaveHooks()
	{
		auto processName = GetModuleFilename(selectedProcessId);
		if (!processName) return;
		QHash<uint64_t, QString> hookCodes;
		for (int i = 0; i < ui.ttCombo->count(); ++i)
		{
			ThreadParam tp = ParseTextThreadString(ui.ttCombo->itemText(i));
			if (tp.processId == selectedProcessId)
			{
				HookParam hp = Host::GetThread(tp).hp;
				if (!(hp.type & HOOK_ENGINE)) hookCodes[tp.addr] = S(HookCode::Generate(hp, tp.processId));
			}
		}
		auto hookInfo = QStringList() << S(processName.value()) << hookCodes.values();
		ThreadParam tp = current->tp;
		if (tp.processId == selectedProcessId) hookInfo << QString("|%1:%2:%3").arg(tp.ctx).arg(tp.ctx2).arg(S(HookCode::Generate(current->hp, tp.processId)));
		QTextFile(HOOK_SAVE_FILE, QIODevice::WriteOnly | QIODevice::Append).write((hookInfo.join(" , ") + "\n").toUtf8());
	}

	void FindHooks()
	{
		QMessageBox::information(This, SEARCH_FOR_HOOKS, HOOK_SEARCH_UNSTABLE_WARNING);

		DWORD processId = selectedProcessId;
		SearchParam sp = {};
		sp.codepage = Host::defaultCodepage;
		bool searchForText = false, customSettings = false;
		QRegularExpression filter(".", QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::DotMatchesEverythingOption);

		QDialog dialog(This, Qt::WindowCloseButtonHint);
		QFormLayout layout(&dialog);
		QCheckBox asianCheck(&dialog);
		layout.addRow(SEARCH_CJK, &asianCheck);
		QDialogButtonBox confirm(QDialogButtonBox::Ok | QDialogButtonBox::Help | QDialogButtonBox::Retry, &dialog);
		layout.addRow(&confirm);
		confirm.button(QDialogButtonBox::Ok)->setText(START_HOOK_SEARCH);
		confirm.button(QDialogButtonBox::Retry)->setText(SEARCH_FOR_TEXT);
		confirm.button(QDialogButtonBox::Help)->setText(SETTINGS);
		QObject::connect(&confirm, &QDialogButtonBox::clicked, [&](QAbstractButton* button)
		{
			if (button == confirm.button(QDialogButtonBox::Retry)) searchForText = true;
			if (button == confirm.button(QDialogButtonBox::Help)) customSettings = true;
			dialog.accept();
		});
		dialog.setWindowTitle(SEARCH_FOR_HOOKS);
		if (!dialog.exec()) return;

		if (searchForText)
		{
			QDialog dialog(This, Qt::WindowCloseButtonHint);
			QFormLayout layout(&dialog);
			QLineEdit textEdit(&dialog);
			layout.addRow(TEXT, &textEdit);
			QSpinBox codepageSpin(&dialog);
			codepageSpin.setMaximum(INT_MAX);
			codepageSpin.setValue(sp.codepage);
			layout.addRow(CODEPAGE, &codepageSpin);
			QDialogButtonBox confirm(QDialogButtonBox::Ok);
			QObject::connect(&confirm, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
			layout.addRow(&confirm);
			if (!dialog.exec()) return;
			wcsncpy_s(sp.text, S(textEdit.text()).c_str(), PATTERN_SIZE - 1);
			try
			{
				Host::FindHooks(selectedProcessId, sp);
				ViewThread(0);
			} catch (std::out_of_range) {}
			return;
		}

		filter.setPattern(asianCheck.isChecked() ? "[\\x{3000}-\\x{a000}]{4,}" : "[\\x{0020}-\\x{1000}]{4,}");
		if (customSettings)
		{
			QDialog dialog(This, Qt::WindowCloseButtonHint);
			QFormLayout layout(&dialog);
			QLineEdit patternEdit(x64 ? "CC CC 48 89" : "55 8B EC", &dialog);
			assert(QByteArray::fromHex(patternEdit.text().toUtf8()) == QByteArray((const char*)sp.pattern, sp.length));
			layout.addRow(SEARCH_PATTERN, &patternEdit);
			for (auto [value, label] : Array<int&, const char*>{
				{ sp.searchTime, SEARCH_DURATION },
				{ sp.offset, PATTERN_OFFSET },
				{ sp.maxRecords, MAX_HOOK_SEARCH_RECORDS },
				{ sp.codepage, CODEPAGE },
			})
			{
				auto spinBox = new QSpinBox(&dialog);
				spinBox->setMaximum(INT_MAX);
				spinBox->setValue(value);
				layout.addRow(label, spinBox);
				QObject::connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), [&value](int newValue) { value = newValue; });
			}
			QLineEdit boundEdit(QFileInfo(S(GetModuleFilename(selectedProcessId).value_or(L""))).fileName(), &dialog);
			layout.addRow(SEARCH_MODULE, &boundEdit);
			for (auto [value, label] : Array<uintptr_t&, const char*>{
				{ sp.minAddress, MIN_ADDRESS },
				{ sp.maxAddress, MAX_ADDRESS },
				{ sp.padding, STRING_OFFSET },
			})
			{
				auto edit = new QLineEdit(QString::number(value, 16), &dialog);
				layout.addRow(label, edit);
				QObject::connect(edit, &QLineEdit::textEdited, [&value](QString text) { if (uintptr_t newValue = text.toULongLong(&ok, 16); ok) value = newValue; });
			}
			QLineEdit filterEdit(filter.pattern(), &dialog);
			layout.addRow(HOOK_SEARCH_FILTER, &filterEdit);
			QPushButton startButton(START_HOOK_SEARCH, &dialog);
			layout.addWidget(&startButton);
			QObject::connect(&startButton, &QPushButton::clicked, &dialog, &QDialog::accept);
			if (!dialog.exec()) return;
			if (patternEdit.text().contains('.'))
			{
				wcsncpy_s(sp.exportModule, S(patternEdit.text()).c_str(), MAX_MODULE_SIZE - 1);
				sp.length = 1;
			}
			else
			{
				QByteArray pattern = QByteArray::fromHex(patternEdit.text().replace("??", QString::number(XX, 16)).toUtf8());
				memcpy(sp.pattern, pattern.data(), sp.length = (std::min)(static_cast<int>(pattern.size()), static_cast<int>(PATTERN_SIZE)));
			}
			wcsncpy_s(sp.boundaryModule, S(boundEdit.text()).c_str(), MAX_MODULE_SIZE - 1);
			filter.setPattern(filterEdit.text());
			if (!filter.isValid()) filter.setPattern(".");
		}
		else
		{
			sp.length = 0; // use default
		}
		filter.optimize();

		auto hooks = std::make_shared<QStringList>();
		try
		{
			Host::FindHooks(processId, sp,
				[hooks, filter](HookParam hp, std::wstring text) { if (filter.match(S(text)).hasMatch()) *hooks << sanitize(S(HookCode::Generate(hp) + L" => " + text)); });
		}
		catch (std::out_of_range) { return; }
		ViewThread(0);
		std::thread([hooks]
		{
			for (int lastSize = 0; hooks->size() == 0 || hooks->size() != lastSize; Sleep(2000)) lastSize = hooks->size();

			QString saveFileName;
			QMetaObject::invokeMethod(This, [&]
			{
				auto hookList = new QListView(This);
				hookList->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
				hookList->setAttribute(Qt::WA_DeleteOnClose);
				hookList->resize({ 750, 300 });
				hookList->setWindowTitle(SEARCH_FOR_HOOKS);
				if (hooks->size() > 5'000)
				{
					hookList->setUniformItemSizes(true); // they aren't actually uniform, but this improves performance
					hooks->push_back(QString(2000, '-')); // dumb hack: with uniform item sizes, the last item is assumed to be the largest
				}
				hookList->setModel(new QStringListModel(*hooks, hookList));
				QObject::connect(hookList, &QListView::clicked, [](QModelIndex i) { AddHook(i.data().toString().split(" => ")[0]); });
				hookList->show();

				saveFileName = QFileDialog::getSaveFileName(This, SAVE_SEARCH_RESULTS, "./results.txt", TEXT_FILES);
			}, Qt::BlockingQueuedConnection);
			if (!saveFileName.isEmpty())
			{
				QTextFile saveFile(saveFileName, QIODevice::WriteOnly | QIODevice::Truncate);
				for (auto hook = hooks->cbegin(); hook != hooks->cend(); ++hook) saveFile.write(hook->toUtf8().append('\n')); // QStringList::begin() makes a copy
			}
			hooks->clear();
		}).detach();
		QMessageBox::information(This, SEARCH_FOR_HOOKS, HOOK_SEARCH_STARTING_VIEW_CONSOLE);
	}

	void OpenSettings()
	{
		QDialog dialog(This, Qt::WindowCloseButtonHint);
		Settings settings(&dialog);
		QFormLayout layout(&dialog);
		QPushButton saveButton(SAVE_SETTINGS, &dialog);
		for (auto [value, label, keylabel] : Array<bool&, const char*, const char*>{
			{ TextThread::filterRepetition, FILTER_REPETITION, KEY_FILTER_REPETITION },
			{ autoAttach, AUTO_ATTACH, KEY_AUTO_ATTACH },
			{ autoAttachSavedOnly, ATTACH_SAVED_ONLY, KEY_ATTACH_SAVED_ONLY },
			{checkUpdate,CHECK_UPDATE, KEY_CHECK_UPDATE},
			{ showSystemProcesses, SHOW_SYSTEM_PROCESSES, KEY_SHOW_SYSTEM_PROCESSES },
		})
		{
			auto checkBox = new QCheckBox(&dialog);
			checkBox->setChecked(value);
			layout.addRow(label, checkBox);
			QObject::connect(&saveButton, &QPushButton::clicked, [checkBox, keylabel, &settings, &value] { settings.setValue(keylabel, value = checkBox->isChecked()); });
		}
		for (auto [value, label, keylabel] : Array<int&, const char*, const char*>{
			{ TextThread::maxBufferSize, MAX_BUFFER_SIZE, KEY_MAX_BUFFER_SIZE },
			{ TextThread::flushDelay, FLUSH_DELAY, KEY_FLUSH_DELAY },
			{ TextThread::maxHistorySize, MAX_HISTORY_SIZE, KEY_MAX_HISTORY_SIZE },
			{ TextThread::limitStringLength, LIMIT_STRING_LENGTH, KEY_LIMIT_STRING_LENGTH },
			{ Host::defaultCodepage, DEFAULT_CODEPAGE, KEY_DEFAULT_CODEPAGE },
		})
		{
			auto spinBox = new QSpinBox(&dialog);
			spinBox->setMaximum(INT_MAX);
			spinBox->setValue(value);
			layout.addRow(label, spinBox);
			QObject::connect(&saveButton, &QPushButton::clicked, [spinBox, keylabel, &settings, &value] { settings.setValue(keylabel, value = spinBox->value()); });
		}
		QComboBox spacingCombo(&dialog);
		spacingCombo.addItems({ FLUSH_DELAY_SPACING_OFF, FLUSH_DELAY_SPACING_SPACE, FLUSH_DELAY_SPACING_NEWLINE });
		spacingCombo.setCurrentIndex(TextThread::flushDelaySpacing);
		layout.addRow(FLUSH_DELAY_SPACING, &spacingCombo);
		QObject::connect(&saveButton, &QPushButton::clicked, [&spacingCombo, &settings] { settings.setValue(KEY_FLUSH_DELAY_SPACING, TextThread::flushDelaySpacing = spacingCombo.currentIndex()); });

		QComboBox languageCombo(&dialog);
		for (int i = 0; i < LANGUAGE_COUNT; ++i)
		{
			languageCombo.addItem(LANGUAGE_NAMES[i]);
		}
		languageCombo.setCurrentIndex(settings.value(CONFIG_LANGUAGE, 0).toInt());
		layout.addRow(LANGUAGE_SETTING, &languageCombo);
		QComboBox localeCombo(&dialog);
		assert(PROMPT == 0 && ALWAYS == 1 && NEVER == 2);
		localeCombo.addItems({ { "Prompt", "Always", "Never" } });
		localeCombo.setCurrentIndex(settings.value(KEY_CONFIG_JP_LOCALE, PROMPT).toInt());
		layout.addRow(CONFIG_JP_LOCALE, &localeCombo);
		QObject::connect(&localeCombo, qOverload<int>(&QComboBox::activated), [&settings](int i) { settings.setValue(KEY_CONFIG_JP_LOCALE, i); });
		layout.addWidget(&saveButton);
		QObject::connect(&saveButton, &QPushButton::clicked, [&dialog, &settings, &languageCombo]
		{
			int currentLanguage = settings.value(CONFIG_LANGUAGE, 0).toInt();
			int selectedLanguage = languageCombo.currentIndex();
			settings.setValue(CONFIG_LANGUAGE, selectedLanguage);

			if (currentLanguage != selectedLanguage)
			{
				QMessageBox::information(This, SETTINGS, LANGUAGE_RESTART_NOTICE);
			}
			dialog.accept();
		});
		dialog.setWindowTitle(SETTINGS);
		dialog.exec();
	}

	void Extensions()
	{
		extenWindow->activateWindow();
		extenWindow->showNormal();
	}

	void SetOutputFont(QString fontString)
	{
		QFont font = ui.textOutput->font();
		font.fromString(fontString);
		font.setStyleStrategy(QFont::NoFontMerging);
		ui.textOutput->setFont(font);
		Settings().setValue(KEY_FONT, font.toString());
	}

	void ProcessConnected(DWORD processId)
	{
		alreadyAttached.insert(processId);

		QString process = S(GetModuleFilename(processId).value_or(L"???"));
		QMetaObject::invokeMethod(This, [process, processId]
		{
			ui.processCombo->addItem(QString::number(processId, 16).toUpper() + ": " + QFileInfo(process).fileName());
		});
		if (process == "???") return;

		// This does add (potentially tons of) duplicates to the file, but as long as I don't perform Ω(N^2) operations it shouldn't be an issue
		QTextFile(GAME_SAVE_FILE, QIODevice::WriteOnly | QIODevice::Append).write((process + "\n").toUtf8());

		QStringList allProcesses = QString(QTextFile(HOOK_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", Qt::SkipEmptyParts);
		auto hookList = std::find_if(allProcesses.rbegin(), allProcesses.rend(), [&](QString hookList) { return hookList.contains(process); });
		if (hookList != allProcesses.rend())
			for (auto hookInfo : hookList->split(" , "))
				if (auto hp = HookCode::Parse(S(hookInfo))) Host::InsertHook(processId, hp.value());
				else swscanf_s(S(hookInfo).c_str(), L"|%I64d:%I64d:%[^\n]", &savedThreadCtx, &savedThreadCtx2, savedThreadCode, (unsigned)std::size(savedThreadCode));
	}

	void ProcessDisconnected(DWORD processId)
	{
		QMetaObject::invokeMethod(This, [processId]
		{
			ui.processCombo->removeItem(ui.processCombo->findText(QString::number(processId, 16).toUpper() + ":", Qt::MatchStartsWith));
		}, Qt::BlockingQueuedConnection);
	}

	void ThreadAdded(TextThread& thread)
	{
		std::wstring threadCode = HookCode::Generate(thread.hp, thread.tp.processId);
		bool savedMatch = (savedThreadCtx & 0xFFFF) == (thread.tp.ctx & 0xFFFF) && savedThreadCtx2 == thread.tp.ctx2 && savedThreadCode == threadCode;
		if (savedMatch)
		{
			savedThreadCtx = savedThreadCtx2 = savedThreadCode[0] = 0;
			current = &thread;
		}
		QMetaObject::invokeMethod(This, [savedMatch, ttString = TextThreadString(thread) + S(FormatString(L" (%s)", threadCode))]
		{
			ui.ttCombo->addItem(ttString);
			if (savedMatch) ViewThread(ui.ttCombo->count() - 1);
		});
	}

	void ThreadRemoved(TextThread& thread)
	{
		QMetaObject::invokeMethod(This, [ttString = TextThreadString(thread)]
		{
			int threadIndex = ui.ttCombo->findText(ttString, Qt::MatchStartsWith);
			if (threadIndex == ui.ttCombo->currentIndex())	ViewThread(0);
			ui.ttCombo->removeItem(threadIndex);
		}, Qt::BlockingQueuedConnection);
	}

	bool SentenceReceived(TextThread& thread, std::wstring& sentence)
	{
		for (int i = 0; i < sentence.size(); ++i) if (sentence[i] == '\r' && sentence[i + 1] == '\n') sentence[i] = 0x200b; // for some reason \r appears as newline - no need to double
		if (!DispatchSentenceToExtensions(sentence, GetSentenceInfo(thread).data())) return false;
		sentence += L'\n';
		if (&thread == current) QMetaObject::invokeMethod(This, [sentence = S(sentence)]() mutable
		{
			sanitize(sentence);
			auto scrollbar = ui.textOutput->verticalScrollBar();
			bool atBottom = scrollbar->value() + 3 > scrollbar->maximum() || (double)scrollbar->value() / scrollbar->maximum() > 0.975; // arbitrary
			QTextCursor cursor(ui.textOutput->document());
			cursor.movePosition(QTextCursor::End);
			cursor.insertText(sentence);
			if (atBottom) scrollbar->setValue(scrollbar->maximum());
		});
		return true;
	}

	void OutputContextMenu(QPoint point)
	{
		std::unique_ptr<QMenu> menu(ui.textOutput->createStandardContextMenu());
		menu->addAction(FONT, [] { if (QString font = QFontDialog::getFont(&ok, ui.textOutput->font(), This, FONT).toString(); ok) SetOutputFont(font); });
		menu->addAction(CLEAR_CURRENT_THREAD, [] {
			if (current) {
				current->storage.Acquire().contents.clear();
				ui.textOutput->clear();
			}
		});
		menu->addAction(SEARCH_GOOGLE, [] {
			QString text = ui.textOutput->textCursor().selectedText().trimmed();
			if (!text.isEmpty())
				QDesktopServices::openUrl(QUrl("https://www.google.com/search?q=" + text));
		});
		menu->exec(ui.textOutput->mapToGlobal(point));
	}

	void CopyUnlessMouseDown()
	{
		if (!(QApplication::mouseButtons() & Qt::LeftButton)) ui.textOutput->copy();
	}

	std::wstring DescribeWinHttpError(DWORD errorCode)
	{
		if (errorCode == ERROR_WINHTTP_TIMEOUT) return WINHTTP_TIMEOUT;
		if (errorCode == ERROR_WINHTTP_NAME_NOT_RESOLVED) return WINHTTP_NAME_NOT_RESOLVED;
		if (errorCode == ERROR_WINHTTP_CANNOT_CONNECT) return WINHTTP_CANNOT_CONNECT;
		if (errorCode == ERROR_WINHTTP_CONNECTION_ERROR) return WINHTTP_CONNECTION_ERROR;
		if (errorCode == ERROR_WINHTTP_SECURE_FAILURE) return WINHTTP_SECURE_FAILURE;
		if (errorCode == ERROR_WINHTTP_INVALID_SERVER_RESPONSE) return WINHTTP_INVALID_SERVER_RESPONSE;
		if (errorCode == ERROR_WINHTTP_OPERATION_CANCELLED) return WINHTTP_OPERATION_CANCELLED;

		LPWSTR systemMessage = nullptr;
		DWORD messageLength = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			errorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&systemMessage,
			0,
			nullptr
		);
		if (!messageLength || !systemMessage) return UNKNOWN_ERROR;

		std::wstring message(systemMessage, messageLength);
		LocalFree(systemMessage);
		while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) message.pop_back();
		return message;
	}

	void CheckForUpdates()
	{
		constexpr DWORD UPDATE_CHECK_TIMEOUT_MS = 5000;
		QString dllPath = QCoreApplication::applicationDirPath() + "/texthook.dll";
		QFile file(dllPath);

		Host::AddConsoleOutput(CHECKING_UPDATE);

		if (!file.open(QIODevice::ReadOnly)) {
			Host::AddConsoleOutput(OPEN_TEXTHOOK_FAILED);
			return;
		}

		QByteArray fileData = file.readAll();
		QByteArray hash = QCryptographicHash::hash(fileData, QCryptographicHash::Sha256);
		QString sha256 = hash.toHex();

		if (HttpRequest httpRequest{
			L"Textractor",
			L"api.iloli.one",
			L"GET",
			FormatString(L"/checkUpdate?sha256=%S", sha256.toStdString()).c_str(),
			"",
			nullptr,
			INTERNET_DEFAULT_HTTPS_PORT,
			nullptr,
			WINHTTP_FLAG_SECURE | WINHTTP_FLAG_ESCAPE_DISABLE,
			nullptr,
			nullptr,
			UPDATE_CHECK_TIMEOUT_MS
		}) {
			if (httpRequest.statusCode != 200) {
				Host::AddConsoleOutput(FormatString(
					CHECK_UPDATE_FAILED_HTTP,
					httpRequest.statusCode
				));
				return;
			}

			if (httpRequest.response.empty()) {
				Host::AddConsoleOutput(CHECK_UPDATE_FAILED_EMPTY_RESPONSE);
				return;
			}

			auto response = JSON::Parse(httpRequest.response);
			if (!response.IsObject()) {
				Host::AddConsoleOutput(CHECK_UPDATE_FAILED_MALFORMED_RESPONSE);
				return;
			}

			if (response[L"error"]) {
				if (auto errorMessage = response[L"error"].String())
					Host::AddConsoleOutput(FormatString(CHECK_UPDATE_FAILED, errorMessage->c_str()));
				else
					Host::AddConsoleOutput(CHECK_UPDATE_FAILED_UNKNOWN);
				return;
			}
			if (response[L"update"].Boolean()) {
				if ( *response[L"update"].Boolean()) {
					Host::AddConsoleOutput(FormatString(TEXTHOOK_UPDATE_AVAILABLE,
													response[L"current_release_date"].String()->c_str(), response[L"release_date"].String()->c_str(),
													response[L"download_url"].String()->c_str()));
				}
				else {
					Host::AddConsoleOutput(TEXTHOOK_IS_LATEST);
				}
			} else
				Host::AddConsoleOutput(CHECK_UPDATE_FAILED_UNEXPECTED_RESPONSE);
		} else {
			Host::AddConsoleOutput(FormatString(CHECK_UPDATE_FAILED_CODE,
				DescribeWinHttpError(httpRequest.errorCode).c_str(), httpRequest.errorCode));
		}
	}

	enum class ButtonAction
	{
		Attach, Launch, Config, Detach, Forget,
		AddHook, RemoveHooks, SaveHooks, SearchHooks,
		Settings, Extensions, RestartAdmin
	};

	static QIcon LoadSvgIcon(const char* resourceName, int size = 24)
	{
		const QString resPath = QStringLiteral(":/icons/") + resourceName;
		QFile file(resPath);
		if (!file.open(QIODevice::ReadOnly)) return {};

		QByteArray svgData = file.readAll();
		file.close();

		const QColor fg = QApplication::palette().color(QPalette::WindowText);
		svgData.replace("currentColor", fg.name().toLatin1());

		QSvgRenderer renderer(svgData);
		QPixmap pix(size, size);
		pix.fill(Qt::transparent);
		QPainter p(&pix);
		p.setRenderHint(QPainter::Antialiasing);
		renderer.render(&p, QRectF(0, 0, size, size));
		p.end();

		return QIcon(pix);
	}

	const char* IconResourceName(ButtonAction action)
	{
		static constexpr const char* resource[] = {
			"attach", "launch", "configure", "detach", "forget",
			"add", "remove", "save", "search",
			"settings", "extension", "admin"
		};
		const int idx = static_cast<int>(action);
		if (idx < 0 || idx >= static_cast<int>(std::size(resource))) return "";
		return resource[idx];
	}

	static QIcon MakeIcon(ButtonAction action)
	{
		return LoadSvgIcon(IconResourceName(action));
	}

	std::vector<QPushButton*> sidebarButtons;
	bool sidebarCollapsed = false;

	void ToggleSidebar()
	{
		sidebarCollapsed = !sidebarCollapsed;
		const int iconOnlyWidth = 36;
		for (auto* btn : sidebarButtons)
		{
			if (!btn) continue;
			if (sidebarCollapsed)
			{
				btn->setFixedWidth(iconOnlyWidth);
				btn->setText(QString());
			}
			else
			{
				btn->setMinimumWidth(0);
				btn->setMaximumWidth(16777215);
				btn->setText(btn->property("fullText").toString());
			}
		}
		ui.processCombo->setVisible(!sidebarCollapsed);

		if (auto* collapseBtn = This->findChild<QPushButton*>("collapseBtn"))
		{
			if (sidebarCollapsed)
			{
				collapseBtn->setIcon(LoadSvgIcon("expand", 20));
				collapseBtn->setProperty("iconName", "expand");
				collapseBtn->setToolTip("Expand");
			}
			else
			{
				collapseBtn->setIcon(LoadSvgIcon("collapse", 20));
				collapseBtn->setProperty("iconName", "collapse");
				collapseBtn->setToolTip("Collapse");
			}
		}
	}

	void RefreshIcons()
	{
		for (auto* btn : sidebarButtons)
		{
			if (!btn) continue;
			if (auto iconName = btn->property("iconName"); iconName.isValid())
				btn->setIcon(LoadSvgIcon(iconName.toString().toLatin1().data(), 20));
		}
	}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	This = this;
	ui.setupUi(this);

	Settings settings;
	int languageIndex = settings.value(CONFIG_LANGUAGE, 0).toInt();
	if (languageIndex >= 0 && languageIndex < LANGUAGE_COUNT)
	{
		CURRENT_LANGUAGE = static_cast<Language>(languageIndex);
		Localize();
	}

	extenWindow = new ExtenWindow(this);
	sidebarButtons.clear();
	for (auto [text, icon, slot] : Array<const char*, ButtonAction, void(&)()>{
		{ ATTACH, ButtonAction::Attach, AttachProcess },
		{ LAUNCH, ButtonAction::Launch, LaunchProcess },
		{ CONFIG, ButtonAction::Config, ConfigureProcess },
		{ DETACH, ButtonAction::Detach, DetachProcess },
		{ FORGET, ButtonAction::Forget, ForgetProcess },
		{ ADD_HOOK, ButtonAction::AddHook, AddHook },
		{ REMOVE_HOOKS, ButtonAction::RemoveHooks, RemoveHooks },
		{ SAVE_HOOKS, ButtonAction::SaveHooks, SaveHooks },
		{ SEARCH_FOR_HOOKS, ButtonAction::SearchHooks, FindHooks },
		{ SETTINGS, ButtonAction::Settings, OpenSettings },
		{ EXTENSIONS, ButtonAction::Extensions, Extensions }
	})
	{
		auto button = new QPushButton(text, ui.processFrame);
		button->setIcon(MakeIcon(icon));
		button->setIconSize({ 20, 20 });
		button->setToolTip(text);
		button->setProperty("fullText", text);
		button->setProperty("iconName", IconResourceName(icon));
		sidebarButtons.push_back(button);
		connect(button, &QPushButton::clicked, slot);
		ui.processLayout->addWidget(button);
	}

	{
		auto restartButton = new QPushButton(RESTART_AS_ADMIN, ui.processFrame);
		restartButton->setIcon(MakeIcon(ButtonAction::RestartAdmin));
		restartButton->setIconSize({ 20, 20 });
		restartButton->setToolTip(RESTART_AS_ADMIN);
		restartButton->setProperty("fullText", QString(RESTART_AS_ADMIN));
		restartButton->setProperty("iconName", IconResourceName(ButtonAction::RestartAdmin));
		restartButton->setEnabled(!IsRunningAsAdmin());
		sidebarButtons.push_back(restartButton);
		connect(restartButton, &QPushButton::clicked, RestartAsAdmin);
		ui.processLayout->addWidget(restartButton);
	}

	{
		auto collapseBtn = new QPushButton("Collapse", ui.processFrame);
		collapseBtn->setObjectName("collapseBtn");
		collapseBtn->setIcon(LoadSvgIcon("collapse", 20));
		collapseBtn->setIconSize({ 20, 20 });
		collapseBtn->setToolTip("Collapse");
		collapseBtn->setProperty("fullText", "Collapse");
		collapseBtn->setProperty("iconName", "collapse");
		sidebarButtons.push_back(collapseBtn);
		QObject::connect(collapseBtn, &QPushButton::clicked, [] { ToggleSidebar(); });
		ui.processLayout->addWidget(collapseBtn);
	}

	ui.processLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

	connect(ui.processCombo, qOverload<int>(&QComboBox::currentIndexChanged), [] { selectedProcessId = ui.processCombo->currentText().split(":")[0].toULong(nullptr, 16); });
	connect(ui.ttCombo, qOverload<int>(&QComboBox::activated), this, ViewThread);
	connect(ui.textOutput, &QPlainTextEdit::selectionChanged, this, CopyUnlessMouseDown);
	connect(ui.textOutput, &QPlainTextEdit::customContextMenuRequested, this, OutputContextMenu);
	connect(qApp, &QGuiApplication::paletteChanged, this, [](const QPalette&)
	{
		QTimer::singleShot(0, [] { RefreshThemeStyle(); });
	});

	if (settings.contains(WINDOW) && QApplication::screenAt(settings.value(WINDOW).toRect().center())) setGeometry(settings.value(WINDOW).toRect());
	SetOutputFont(settings.value(KEY_FONT, ui.textOutput->font().toString()).toString());
	TextThread::filterRepetition = settings.value(KEY_FILTER_REPETITION, TextThread::filterRepetition).toBool();
	autoAttach = settings.value(KEY_AUTO_ATTACH, autoAttach).toBool();
	autoAttachSavedOnly = settings.value(KEY_ATTACH_SAVED_ONLY, autoAttachSavedOnly).toBool();
	checkUpdate = settings.value(KEY_CHECK_UPDATE, checkUpdate).toBool();
	showSystemProcesses = settings.value(KEY_SHOW_SYSTEM_PROCESSES, showSystemProcesses).toBool();
	TextThread::flushDelaySpacing = settings.value(KEY_FLUSH_DELAY_SPACING, TextThread::flushDelaySpacing).toInt();
	TextThread::flushDelay = settings.value(KEY_FLUSH_DELAY, TextThread::flushDelay).toInt();
	TextThread::maxBufferSize = settings.value(KEY_MAX_BUFFER_SIZE, TextThread::maxBufferSize).toInt();
	TextThread::maxHistorySize = settings.value(KEY_MAX_HISTORY_SIZE, TextThread::maxHistorySize).toInt();
	TextThread::limitStringLength = settings.value(KEY_LIMIT_STRING_LENGTH, TextThread::limitStringLength).toInt();
	Host::defaultCodepage = settings.value(KEY_DEFAULT_CODEPAGE, Host::defaultCodepage).toInt();

	Host::Start(ProcessConnected, ProcessDisconnected, ThreadAdded, ThreadRemoved, SentenceReceived);
	current = &Host::GetThread(Host::console);
	Host::AddConsoleOutput(ABOUT);
	if (checkUpdate) CheckForUpdates();

	AttachConsole(ATTACH_PARENT_PROCESS);
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), CL_OPTIONS, wcslen(CL_OPTIONS), DUMMY, NULL);
	auto processes = GetAllProcesses();
	int argc;
	std::unique_ptr<LPWSTR[], Functor<LocalFree>> argv(CommandLineToArgvW(GetCommandLineW(), &argc));
	for (int i = 0; i < argc; ++i)
		if (std::wstring arg = argv[i]; arg[0] == L'/' || arg[0] == L'-')
			if (arg[1] == L'p' || arg[1] == L'P')
				if (DWORD processId = wcstoul(arg.substr(2).c_str(), nullptr, 0)) Host::InjectProcess(processId);
				else for (auto [processId, processName] : processes)
					if (processName.value_or(L"").find(L"\\" + arg.substr(2)) != std::string::npos) Host::InjectProcess(processId);

	std::thread([] { for (; ; Sleep(10000)) AttachSavedProcesses(); }).detach();
}

MainWindow::~MainWindow()
{
	DetachProcess(); //force detach process
	Settings().setValue(WINDOW, geometry());
	CleanupExtensions();
	SetErrorMode(SEM_NOGPFAULTERRORBOX);
	ExitProcess(0);
}

void MainWindow::closeEvent(QCloseEvent*)
{
	QApplication::quit(); // Need to do this to kill any windows that might've been made by extensions
}

void MainWindow::changeEvent(QEvent* event)
{
	QMainWindow::changeEvent(event);

	switch (event->type())
	{
	case QEvent::ApplicationPaletteChange:
	case QEvent::PaletteChange:
	case QEvent::ThemeChange:
		QTimer::singleShot(0, [] { RefreshThemeStyle(); });
		break;
	default:
		break;
	}
}

