/*
    Copyright (C) 2019 Piyush Aggarwal <piyushaggarwal002@gmail.com>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "notifybysnore.h"
#include "knotification.h"
#include "knotifyconfig.h"
#include "debug_p.h"

#include <Windows.h>

#include <QBuffer>
#include <QDebug>
#include <QIcon>
#include <QProcess>
#include <QString>
#include <QDir>
#include <QTemporaryFile>
#include <QLoggingCategory>
// Q_LOGGING_CATEGORY(SNORETOAST, "SNORETOAST")
// #include <QLocalServer>
// #include <QLocalSocket>
#include <QDebug>
#include <QProcess>
#include <QTimer>

#include <iostream>

#include "snoretoastactions.h"

static NotifyBySnore *s_instance = nullptr;

// RUN THIS BEFORE TRYING OR THE NOTIFS WON'T SHOW!
// .\SnoreToast.exe -install "KDE Connect" "C:\CraftRoot\bin\kdeconnectd.exe" "org.kde.connect"

//gotta get more inspirations from notifybyportal
// installing the Shortcut


NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    s_instance = this;
    program = QStringLiteral("SnoreToast.exe");
    // Sleep(uint(4000));

}

NotifyBySnore::~NotifyBySnore()
{
    s_instance = nullptr;
}

void NotifyBySnore::notify(KNotification *notification, KNotifyConfig *config)
{
    if (m_notifications.find(notification->id()) != m_notifications.end() || notification->id() == -1) {
            qDebug() << "AHAA ! Duplicate for ID: " << notification->id() << " caught!";
            return;
        }

    proc = new QProcess();
    QTemporaryFile iconFile;
    QStringList arguments;

    // we read from the pixmap sent through KDE Connect 
    // and save it to a file temporarily, then delete the icon once the notif is rendered

    if (!notification->pixmap().isNull()) {
        if (iconFile.open()) {
            notification->pixmap().save(&iconFile, "PNG");
        }
    }

    arguments << QStringLiteral("-t") << notification->title();
    arguments << QStringLiteral("-m") << notification->text()+QString::number(notification->id());
    arguments << QStringLiteral("-p") <<  iconFile.fileName() ;
    arguments << QStringLiteral("-appID") << QStringLiteral("org.kde.connect");
    arguments << QStringLiteral("-id") << QString::number(notification->id());
    // arguments << QStringLiteral("-b") << QStringLiteral("Close;Button2");
    arguments << QStringLiteral("-w");

    m_notifications.insert(notification->id(), notification);

    proc->start(program, arguments);

    Sleep(5000); // !HACK! for the notifications to pick up the image icon

    if(proc->waitForStarted(5000))
    {
        qDebug() << "SnoreToast called for Notif-show by ID: "<< notification->id();
        arguments.clear();
    }
    else
    {
        qDebug() << "SnoreToast did not start in time for Notif-Show";
    }
}

/* 
Then we also have a config 
    Not sure what's that, but we surely get this from notifs in KDEConnect.
Here in this plugin we gotta define our own 
    notificationFinished
    notificationActionInvoked

aaaand we should be done :)
*/

// LIMITATION : notifs won't get closed auto-ly if you open Action Center, read them, and then close it back.
void NotifyBySnore::close(KNotification* notification)
{
    const auto it = m_notifications.find(notification->id());
    if (it == m_notifications.end()) {
        return;
    }

    qDebug() << "SnoreToast called for closing notif by ID: "<< notification->id();

    proc = new QProcess();
    QStringList arguments;
    arguments << QStringLiteral("-close") << QString::number(notification->id());
    proc->start(program, arguments);
    arguments.clear();

    m_notifications.erase(it);
    if (it.value()) {
        finish(it.value());
    }
}

void NotifyBySnore::update(KNotification *notification, KNotifyConfig *config)
{
    close(notification);
    notify(notification, config);
}

void NotifyBySnore::notificationActionInvoked(int id, int action)
{
    qCDebug(LOG_KNOTIFICATIONS) << id << action;
    emit actionInvoked(id, action);
}
