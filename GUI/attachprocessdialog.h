#pragma once

#include "qtcommon.h"
#include "ui_attachprocessdialog.h"
#include <QStandardItemModel>
#include <functional>

struct ProcessInfo
{
	DWORD processId;
	QString processName;
	QString windowTitle;
	bool is64Bit;
	bool isRenderer; // Chromium/NW.js child process: holds the page, has no window
	QIcon icon;

	ProcessInfo();

	ProcessInfo(DWORD pid, QString name, QString title, bool is64, HICON ic, bool renderer = false);
};

class AttachProcessDialog : public QDialog
{
public:
    explicit AttachProcessDialog(QWidget* parent, std::vector<ProcessInfo> processInfoList,
		std::function<std::vector<ProcessInfo>()> refreshCallback = nullptr);
    DWORD SelectedProcessId();

private:
	void populateProcessList();
	void applyFilters();

    Ui::AttachProcessDialog ui;
    QStandardItemModel model;
    std::vector<ProcessInfo> processInfoList;
	std::function<std::vector<ProcessInfo>()> refreshCallback;
	bool isCurrentProcess64Bit;
};
