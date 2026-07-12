#include "attachprocessdialog.h"
// #include <QtWinExtras/QtWin>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPixmap>

extern const char* SELECT_PROCESS;
extern const char* ATTACH_INFO;
extern const char* REFRESH;
extern const char* HIDE_INCOMPATIBLE_ARCHITECTURE;
extern const char* HIDE_NO_TITLE_PROCESS;

ProcessInfo::ProcessInfo() : processId(0), is64Bit(false) {}

ProcessInfo::ProcessInfo(DWORD pid, QString name, QString title, bool is64, HICON ic)
	: processId(pid)
	, processName(std::move(name))
	, windowTitle(std::move(title))
	, is64Bit(is64)
{
	if (ic) {
		icon = QIcon(QPixmap::fromImage(QImage::fromHICON(ic)));
		DestroyIcon(ic);
	} else {
		QPixmap transparent(16, 16);
		transparent.fill(Qt::transparent);
		icon = QIcon(transparent);
	}
}

AttachProcessDialog::AttachProcessDialog(QWidget* parent, std::vector<ProcessInfo> processInfoList,
	std::function<std::vector<ProcessInfo>()> refreshCallback) :
	QDialog(parent, Qt::WindowCloseButtonHint),
	model(this),
	processInfoList(processInfoList),
	refreshCallback(refreshCallback)
{
#ifdef _WIN64
	isCurrentProcess64Bit = true;
#else
	isCurrentProcess64Bit = false;
#endif

	ui.setupUi(this);
	setWindowTitle(SELECT_PROCESS);
	ui.label->setText(ATTACH_INFO);
	ui.refreshButton->setText(REFRESH);
	ui.filterArchitectureCheckbox->setText(HIDE_INCOMPATIBLE_ARCHITECTURE);
	ui.filterNoTitleCheckbox->setText(HIDE_NO_TITLE_PROCESS);
	ui.processList->setModel(&model);

	populateProcessList();

	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(ui.filterArchitectureCheckbox, &QCheckBox::stateChanged, [this](int) {
		applyFilters();
	});

	connect(ui.filterNoTitleCheckbox, &QCheckBox::stateChanged, [this](int) {
		applyFilters();
	});

	connect(ui.refreshButton, &QPushButton::clicked, [this]() {
		if (this->refreshCallback) {
			this->processInfoList = this->refreshCallback();
			populateProcessList();
			applyFilters();
		}
	});

	connect(ui.processList, &QListView::clicked, [this](QModelIndex index)
	{
		QVariant data = index.data(Qt::UserRole);
		if (data.isValid()) {
			ui.processEdit->setText(QString::number(data.toUInt()));
		}
	});
	connect(ui.processList, &QListView::doubleClicked, this, &QDialog::accept);
	connect(ui.processEdit, &QLineEdit::textEdited, [this](QString) {
		applyFilters();
	});
	connect(ui.processEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
}

DWORD AttachProcessDialog::SelectedProcessId()
{
	QString pidText = ui.processEdit->text().trimmed();
	if (pidText.isEmpty()) return 0;

	bool ok = false;
	DWORD pid = 0;

	if (pidText.startsWith("0x", Qt::CaseInsensitive)) {
		pid = pidText.toULong(&ok, 16);
		if (ok) return pid;
	}

	pid = pidText.toULong(&ok, 10);
	if (ok) return pid;

	pid = pidText.toULong(&ok, 16);
	if (ok) return pid;

	return 0;
}

void AttachProcessDialog::populateProcessList()
{
	ui.processList->setUpdatesEnabled(false);
	model.removeRows(0, model.rowCount());

	for (const auto& info : processInfoList)
	{
		QString platform = info.is64Bit ? "x64" : "x86";
		QString titlePart = info.windowTitle.isEmpty() ? "" : QString(" - %1").arg(info.windowTitle);

		QString displayText = QString("%1 [%2]%3 (PID: %4)")
			.arg(info.processName)
			.arg(platform)
			.arg(titlePart)
			.arg(info.processId);

		auto item = new QStandardItem(info.icon, displayText);
		item->setData(static_cast<unsigned int>(info.processId), Qt::UserRole);
		item->setData(info.is64Bit, Qt::UserRole + 1);
		item->setData(info.windowTitle, Qt::UserRole + 2);
		item->setEditable(false);
		model.appendRow(item);
	}

	ui.processList->setUpdatesEnabled(true);
}

void AttachProcessDialog::applyFilters()
{
	QString filter = ui.processEdit->text().trimmed();
	bool filterArchitecture = ui.filterArchitectureCheckbox->isChecked();
	bool filterNoTitle = ui.filterNoTitleCheckbox->isChecked();

	for (int i = 0; i < model.rowCount(); ++i)
	{
		auto item = model.item(i);
		QString displayText = item->text();
		DWORD pid = item->data(Qt::UserRole).toUInt();
		bool is64Bit = item->data(Qt::UserRole + 1).toBool();
		QString windowTitle = item->data(Qt::UserRole + 2).toString();

		bool matchesFilter = filter.isEmpty() ||
							displayText.contains(filter, Qt::CaseInsensitive) ||
							QString::number(pid, 16).contains(filter, Qt::CaseInsensitive) ||
							QString::number(pid, 10).contains(filter, Qt::CaseInsensitive);

		bool matchesArchitecture = !filterArchitecture || (is64Bit == isCurrentProcess64Bit);
		bool matchesTitle = !filterNoTitle || !windowTitle.isEmpty();

		ui.processList->setRowHidden(i, !(matchesFilter && matchesArchitecture && matchesTitle));
	}
}

