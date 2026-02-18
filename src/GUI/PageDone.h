/*
Copyright (c) 2012-2020 Maarten Baert <maarten-baert@hotmail.com>

This file is part of SimpleScreenRecorder.

SimpleScreenRecorder is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

SimpleScreenRecorder is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SimpleScreenRecorder.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "Global.h"

#include <QListWidget>

class MainWindow;

class PageDone : public QWidget {
	Q_OBJECT

private:
	MainWindow *m_main_window;

	QListWidget *m_listwidget_recordings;
	QStringList m_recording_files;
	QPushButton *m_button_play;

public:
	PageDone(MainWindow* main_window);

	void AddRecording(const QString& file);

public slots:
	void OnOpenFolder();
	void OnPlayRecording();
	void OnRecordingDoubleClicked(QListWidgetItem* item);

};
