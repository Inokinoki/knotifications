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


#include <QTemporaryDir>

#include <QLoggingCategory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QGuiApplication>

#include <snoretoastactions.h>

static NotifyBySnore *s_instance = nullptr;

// !DOCUMENT THIS! apps must have shortcut appID same as app->applicationName()
NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    s_instance = this;
    app = QCoreApplication::instance();
    iconDir = new QTemporaryDir();
    server = new QLocalServer();
    server->listen(app->applicationName());

    QObject::connect(server, &QLocalServer::newConnection, server, [this]() {
        auto sock = server->nextPendingConnection();
        sock->waitForReadyRead();
        const QByteArray rawData = sock->readAll();
        const QString data =
                    QString::fromWCharArray(reinterpret_cast<const wchar_t *>(rawData.constData()),
                                        rawData.size() / sizeof(wchar_t));
        QMap<QString, QString> map;
        for (const auto &str : data.split(QStringLiteral(";"))) {
            const auto index = str.indexOf(QStringLiteral("="));
            map[str.mid(0, index)] = str.mid(index + 1);
        }
        const auto action = map[QStringLiteral("action")];
        const auto ID = map[QStringLiteral("notificationId")].toInt();

        const auto snoreAction = SnoreToastActions::getAction(action.toStdWString());
        qDebug() << "THE ID IS : " << QString::number(ID) << "AND THE INTERACTION WAS : " << QString::number(static_cast<int>(snoreAction)) <<" : "<< action;
        const auto button = map[QStringLiteral("button")];
        if(!action.compare(QStringLiteral("buttonClicked"))){
            qDebug() << "AND THE BUTTON CLICKED IS : " << button;
            QStringList s = m_notifications.value(ID)->actions();
            int action_no = s.indexOf(button) + 1;
            NotifyBySnore::notificationActionInvoked(ID, action_no); 
        }
});
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
    QProcess *proc = new QProcess();

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
    arguments << QStringLiteral("-pipename") << server->fullServerName();
    if (!notification->actions().isEmpty()){
        arguments << QStringLiteral("-b") << notification->actions().join(QStringLiteral(";"));
    }
    m_notifications.insert(notification->id(), notification); // I don't need to store whole 
    proc->start(program, arguments);

}

void NotifyBySnore::close(KNotification* notification)
{
    const auto it = m_notifications.find(notification->id());
    if (it == m_notifications.end()) {
        return;
    }

    qDebug() << "SnoreToast closing notification by ID: "<< notification->id();

    QProcess *proc = new QProcess();
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
