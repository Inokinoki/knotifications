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
#include <QTemporaryDir>

#include <QLoggingCategory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QGuiApplication>

#include <snoretoastactions.h>

static NotifyBySnore *s_instance = nullptr;

// !IMPORTANT! apps must have shortcut appID same as app->applicationName()

NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    s_instance = this;
    app = QCoreApplication::instance();
    iconDir = new QTemporaryDir();
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
    QFile file(iconDir->path() + QString::number(notification->id()));
    if (!notification->pixmap().isNull()) {
            notification->pixmap().save(&file, "PNG");
}
 
    arguments << QStringLiteral("-t") << notification->title();
    arguments << QStringLiteral("-m") << notification->text();
    arguments << QStringLiteral("-p") <<  file.fileName();
    arguments << QStringLiteral("-appID") << app->applicationName(); 
    arguments << QStringLiteral("-id") << QString::number(notification->id());
    arguments << QStringLiteral("-w");
    m_notifications.insert(notification->id(), notification);
    proc->start(program, arguments);
    if(proc->waitForStarted(1000))
    {
        qDebug() << "SnoreToast displaying notification by ID: "<< notification->id();
    }
    else
    {
        qDebug() << "SnoreToast did not start in time for Notif-Show";
    }
}

void NotifyBySnore::close(KNotification* notification)
{
    const auto it = m_notifications.find(notification->id());
    if (it == m_notifications.end()) {
        return;
    }

    qDebug() << "SnoreToast closing notification by ID: "<< notification->id();

    proc = new QProcess();
    QStringList arguments;
    arguments << QStringLiteral("-close") << QString::number(notification->id())
              << QStringLiteral("-appID") << app->applicationName();
;
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
    emit actionInvoked(id, action);
}
