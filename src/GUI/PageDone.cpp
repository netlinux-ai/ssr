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

#include "PageDone.h"

#include "Icons.h"
#include "MainWindow.h"
#include "PageOutput.h"

PageDone::PageDone(MainWindow* main_window)
	: QWidget(main_window->centralWidget()) {

	m_main_window = main_window;

	QLabel *label_done = new QLabel(tr("The recording has been saved. You can edit the video now, or re-encode it with better settings to "
									   "make the file smaller (the default settings are optimized for quality and speed, not file size)."), this);
	label_done->setWordWrap(true);

	QLabel *label_recordings = new QLabel(tr("Recordings:"), this);

	m_listwidget_recordings = new QListWidget(this);
	m_listwidget_recordings->setSelectionMode(QAbstractItemView::SingleSelection);
	connect(m_listwidget_recordings, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnRecordingDoubleClicked(QListWidgetItem*)));

	m_button_play = new QPushButton(QIcon::fromTheme("media-playback-start"), tr("Play"), this);
	m_button_play->setEnabled(false);
	connect(m_button_play, SIGNAL(clicked()), this, SLOT(OnPlayRecording()));
	connect(m_listwidget_recordings, &QListWidget::currentRowChanged, this, [this](int row) {
		m_button_play->setEnabled(row >= 0);
	});

	QPushButton *button_open_folder = new QPushButton(g_icon_folder_open, tr("Open folder"), this);
	connect(button_open_folder, SIGNAL(clicked()), this, SLOT(OnOpenFolder()));

	QPushButton *button_back_start = new QPushButton(g_icon_go_home, tr("Back to the start screen"), this);
	connect(button_back_start, SIGNAL(clicked()), m_main_window, SLOT(GoPageStart()));

	QPushButton *button_back_record = new QPushButton(g_icon_go_previous, tr("Back to recording"), this);
	connect(button_back_record, SIGNAL(clicked()), m_main_window, SLOT(GoPageRecord()));

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(label_done);
	layout->addWidget(label_recordings);
	layout->addWidget(m_listwidget_recordings, 1);
	{
		QHBoxLayout *layout2 = new QHBoxLayout();
		layout->addLayout(layout2);
		layout2->addWidget(m_button_play);
		layout2->addWidget(button_open_folder);
		layout2->addStretch();
	}
	{
		QHBoxLayout *layout2 = new QHBoxLayout();
		layout->addLayout(layout2);
		layout2->addWidget(button_back_start);
		layout2->addStretch();
		layout2->addWidget(button_back_record);
	}

}

void PageDone::AddRecording(const QString& file) {
	if(file.isEmpty())
		return;
	if(m_recording_files.contains(file))
		return;
	m_recording_files.append(file);
	QFileInfo fi(file);
	QListWidgetItem *item = new QListWidgetItem(fi.fileName(), m_listwidget_recordings);
	item->setData(Qt::UserRole, file);
	item->setToolTip(file);
	m_listwidget_recordings->setCurrentItem(item);
}

void PageDone::OnPlayRecording() {
	QListWidgetItem *item = m_listwidget_recordings->currentItem();
	if(item == NULL)
		return;
	QString file = item->data(Qt::UserRole).toString();
	QDesktopServices::openUrl(QUrl::fromLocalFile(file));
}

void PageDone::OnRecordingDoubleClicked(QListWidgetItem* item) {
	if(item == NULL)
		return;
	QString file = item->data(Qt::UserRole).toString();
	QDesktopServices::openUrl(QUrl::fromLocalFile(file));
}

void PageDone::OnOpenFolder() {
	QListWidgetItem *item = m_listwidget_recordings->currentItem();
	QString file;
	if(item != NULL) {
		file = item->data(Qt::UserRole).toString();
	} else {
		file = m_main_window->GetPageOutput()->GetFile();
	}
	QFileInfo fi(file);
	QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}
