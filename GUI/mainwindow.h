#pragma once

#include "qtcommon.h"

class MainWindow : public QMainWindow
{
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

protected:
	void changeEvent(QEvent* event) override;

private:
	void closeEvent(QCloseEvent*);
};
