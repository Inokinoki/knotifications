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
#include <QIcon>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QLoggingCategory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QGuiApplication>
#include <QCryptographicHash>

#include <snoretoastactions.h>

/**
 * Be sure to have a shortcut installed in Windows Start Menu by SnoreToast
 * The syntax is -
 * ./SnoreToast.exe -install <absolute\address\of\shortcut> <absolute\address\to\app.exe> <appID>
 * 
 * appID : use as-is from your app's QCoreApplication::applicationName() when installing the shortcut.
 * NOTE: Install the shortcut in Windows Start Menu.
 */

NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    server.listen(QString::fromStdString(QCryptographicHash::hash(qApp->applicationDirPath().toUtf8(), \
                    QCryptographicHash::Md5	).toHex().toStdString()).left(5)); 
                    // ^ can be increased from 5 to N for lesser collisions

    QObject::connect(&server, &QLocalServer::newConnection, &server, [this]() {
        auto sock = server.nextPendingConnection();
        sock->waitForReadyRead();
        const QByteArray rawData = sock->readAll();
        sock->close();
        const QString data =
                    QString::fromWCharArray(reinterpret_cast<const wchar_t *>(rawData.constData()),
                                        rawData.size() / sizeof(wchar_t));
        QMap<QString, QString> map;
        for (const auto &str : data.split(QStringLiteral(";"))) {
            const auto index = str.indexOf(QStringLiteral("="));
            map.insert(str.mid(0, index), str.mid(index + 1));
        }
        const auto action = map[QStringLiteral("action")];
        const auto id = map[QStringLiteral("notificationId")].toInt();
        KNotification *notification = nullptr;
        const auto it = m_notifications.find(id);
        if (it != m_notifications.end()) {
            notification = it.value();
        }
        const auto snoreAction = SnoreToastActions::getAction(action.toStdWString());
        qCDebug(LOG_KNOTIFICATIONS) << "The notification ID is : " << id;
        switch (snoreAction) {
        case SnoreToastActions::Actions::Clicked :
            qCDebug(LOG_KNOTIFICATIONS) << " User clicked on the toast.";
            if (notification) {
                close(notification);
            }
            break;
        case SnoreToastActions::Actions::Hidden :
            qCDebug(LOG_KNOTIFICATIONS) << "The toast got hidden.";
            break;
        case SnoreToastActions::Actions::Dismissed :
            qCDebug(LOG_KNOTIFICATIONS) << "User dismissed the toast.";
            break;
        case SnoreToastActions::Actions::Timedout :
            qCDebug(LOG_KNOTIFICATIONS) << "The toast timed out.";
            break;
        case SnoreToastActions::Actions::ButtonClicked :{
            qCDebug(LOG_KNOTIFICATIONS) << " User clicked a button on the toast.";
            const auto button = map[QStringLiteral("button")];
            QStringList s = m_notifications.value(id)->actions();
            int actionNum = s.indexOf(button) + 1; // QStringList starts with index 0 but not actions
            emit actionInvoked(id, actionNum);
            break;}
        case SnoreToastActions::Actions::TextEntered :
            qCDebug(LOG_KNOTIFICATIONS) << " User entered some text in the toast.";
            break;
        default:
            qCDebug(LOG_KNOTIFICATIONS) << "Unexpected behaviour with the toast.";
            if (notification) {
                close(notification);
            }
            break;
        }
    });
}

NotifyBySnore::~NotifyBySnore()
{
    server.close();
    iconDir.remove();
}

void NotifyBySnore::notify(KNotification *notification, KNotifyConfig *config)
{
    if (m_notifications.constFind(notification->id()) != m_notifications.constEnd()) {
            qCDebug(LOG_KNOTIFICATIONS) << "Duplicate notification with ID: " << notification->id() << " ignored.";
            return;
        }
    QProcess *proc = new QProcess();
    QStringList arguments;
    QString iconPath;

    arguments << QStringLiteral("-t");
    if (!notification->title().isEmpty()) {
        arguments << notification->title();
    }
    else {
        arguments << qApp->applicationDisplayName();
    }
    arguments << QStringLiteral("-m") << notification->text();
    if (!notification->pixmap().isNull()) {
        iconPath = iconDir.path() + QStringLiteral("/") 
                    + QString::number(notification->id()) + QStringLiteral(".png");
        notification->pixmap().save(iconPath, "PNG");
        arguments << QStringLiteral("-p") <<  iconPath;
    }
    arguments << QStringLiteral("-appID") << qApp->applicationName()
            << QStringLiteral("-id") << QString::number(notification->id())
            << QStringLiteral("-pipename") << server.fullServerName();

    if (!notification->actions().isEmpty()) {
        arguments << QStringLiteral("-b") << notification->actions().join(QStringLiteral(";"));
    }
    qCDebug(LOG_KNOTIFICATIONS) << arguments;

    m_notifications.insert(notification->id(), notification);
    proc->start(program, arguments);

   connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus exitStatus){ 
                proc->deleteLater();
    });
}

void NotifyBySnore::close(KNotification *notification)
{
    const auto it = m_notifications.find(notification->id());
    if (it == m_notifications.end()) {
        return;
    }

    qCDebug(LOG_KNOTIFICATIONS) << "SnoreToast closing notification with ID: " << notification->id();

    QProcess *proc = new QProcess();
    QStringList arguments;
    arguments << QStringLiteral("-close") << QString::number(notification->id())
            << QStringLiteral("-appID") << qApp->applicationName();
    proc->start(program, arguments);
    if (it.value()) {
        finish(it.value());
    }
    m_notifications.erase(it);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
    [=](int exitCode, QProcess::ExitStatus exitStatus){ 
        proc->deleteLater();
        delete proc;
    });
}

void NotifyBySnore::update(KNotification *notification, KNotifyConfig *config)
{
    close(notification);
    notify(notification, config);
}
