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

#include <QBuffer>
#include <QIcon>
#include <QLoggingCategory>
#include <QLocalSocket>
#include <QGuiApplication>
#include <QSourceLocation>

#include <snoretoastactions.h>

/*
 * On Windows a shortcut to your app is needed to be installed in the Start Menu 
 * (and subsequently, registered with the OS) in order to show notifications.
 * Since KNotifications is a library, an app using it can't (feasibly) be properly
 * registered with the OS. It is possible we could come up with some complicated solution
 * which would require every KNotification-using app to do some special and probably
 * difficult to understand change to support Windows. Or we can have SnoreToast.exe
 * take care of all that nonsense for us.
 * Note that, up to this point, there have been no special
 * KNotifications changes to the generic application codebase to make this work,
 * just some tweaks to the Craft blueprint and packaging script
 * to pull in SnoreToast and trigger shortcut building respectively.
 * Be sure to have a shortcut installed in Windows Start Menu by SnoreToast.
 * 
 * So the location doesn't matter, but it's only possible to register the internal COM server in an executable.
 * We could make it a static lib and link it in all KDE applications,
 * but to make the action center integration work, we would need to also compile a class
 * into the executable using a compile time uuid.
 *
 * The used header is meant to help with parsing the response.
 * The cmake target for LibSnoreToast is a INTERFACE lib, it only provides the include path.
 *
 *
 * Trigger the shortcut installation during the installation of your app; syntax for shortcut installation is -
 * ./SnoreToast.exe -install <absolute\address\of\shortcut> <absolute\address\to\app.exe> <appID>
 * 
 * appID: use as-is from your app's QCoreApplication::applicationName() when installing the shortcut.
 * NOTE: Install the shortcut in Windows Start Menu folder.
 * For example, check out Craft Blueprint for Quassel-IRC or KDE Connect.
*/

NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    server.listen(QString::number(qHash(qApp->applicationDirPath()))); 
    QObject::connect(&server, &QLocalServer::newConnection, &server, [this]() {
        auto sock = server.nextPendingConnection();
        sock->waitForReadyRead();
        const QByteArray rawData = sock->readAll();
        sock->deleteLater();
        sock->close();
        const QString data =
                    QString::fromWCharArray(reinterpret_cast<const wchar_t *>(rawData.constData()),
                                        rawData.size() / sizeof(wchar_t));
        QMap<QString, QStringRef> map;
        const auto parts = data.splitRef(QLatin1Char(';'));
        for (auto &str : parts) {
            const auto index = str.indexOf(QLatin1Char('='));
            map.insert(str.toString().mid(0, index), str.mid(index + 1));
        }
        const auto action = map[QStringLiteral("action")].toString();
        const auto id = map[QStringLiteral("notificationId")].toString().toInt();
        KNotification *notification;
        const auto it = m_notifications.constFind(id);
        if (it != m_notifications.constEnd()) {
            notification = it.value();
        }
        const auto snoreAction = SnoreToastActions::getAction(action.toStdWString());
        qCDebug(LOG_KNOTIFICATIONS) << "The notification ID is : " << id;
        switch (snoreAction) {
        case SnoreToastActions::Actions::Clicked:
            qCDebug(LOG_KNOTIFICATIONS) << " User clicked on the toast.";
            if (notification) {
                close(notification);
            }
            break;
        case SnoreToastActions::Actions::Hidden:
            qCDebug(LOG_KNOTIFICATIONS) << "The toast got hidden.";
            break;
        case SnoreToastActions::Actions::Dismissed:
            qCDebug(LOG_KNOTIFICATIONS) << "User dismissed the toast.";
            break;
        case SnoreToastActions::Actions::Timedout:
            qCDebug(LOG_KNOTIFICATIONS) << "The toast timed out.";
            break;
        case SnoreToastActions::Actions::ButtonClicked:{
            qCDebug(LOG_KNOTIFICATIONS) << " User clicked a button on the toast.";
            const auto button = map[QStringLiteral("button")].toString();
            QStringList s = m_notifications.value(id)->actions();
            int actionNum = s.indexOf(button) + 1; // QStringList starts with index 0 but not actions
            emit actionInvoked(id, actionNum);
            break;}
        case SnoreToastActions::Actions::TextEntered:
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
}

void NotifyBySnore::notify(KNotification *notification, KNotifyConfig *config)
{
    if (m_notifications.constFind(notification->id()) != m_notifications.constEnd()) {
            qCDebug(LOG_KNOTIFICATIONS) << "Duplicate notification with ID: " << notification->id() << " ignored.";
            return;
        }
    QProcess *proc = new QProcess();
    QStringList arguments;

    arguments << QStringLiteral("-t");
    if (!notification->title().isEmpty()) {
        arguments << notification->title();
    } else {
        arguments << qApp->applicationDisplayName();
    }
    arguments << QStringLiteral("-m") << notification->text();
    if (!notification->pixmap().isNull()) {
        auto iconPath = QString(iconDir.path() + QLatin1Char('/') 
                    + QString::number(notification->id()) + QStringLiteral(".png"));
        notification->pixmap().save(iconPath, "PNG");
        arguments << QStringLiteral("-p") << iconPath;
    }
    arguments << QStringLiteral("-appID") << qApp->applicationName()
            << QStringLiteral("-id") << QString::number(notification->id())
            << QStringLiteral("-pipename") << server.fullServerName();

    if (!notification->actions().isEmpty()) {
        arguments << QStringLiteral("-b") << notification->actions().join(QLatin1Char(';'));
    }
    qCDebug(LOG_KNOTIFICATIONS) << arguments;

    proc->start(program, arguments);
    
    if(!proc->waitForStarted()){
        m_notifications.insert(notification->id(), notification);
        finish(notification);
    }
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus exitStatus){ 
                proc->deleteLater();
                if(exitStatus == QProcess::NormalExit){
                } else{
                    qDebug() << "SnoreToast crashed.";
                    close(notification);
                }
    });
}

void NotifyBySnore::close(KNotification *notification)
{
    const auto it = m_notifications.constFind(notification->id());
    if (it == m_notifications.constEnd()) {
        return;
    }

    qCDebug(LOG_KNOTIFICATIONS) << "SnoreToast closing notification with ID: " << notification->id();

    QStringList arguments;
    arguments << QStringLiteral("-close") << QString::number(notification->id())
            << QStringLiteral("-appID") << qApp->applicationName();
    QProcess::startDetached(program, arguments);
    if (it.value()) {
        finish(it.value());
    }
    m_notifications.erase(it);
}

void NotifyBySnore::update(KNotification *notification, KNotifyConfig *config)
{
    close(notification);
    notify(notification, config);
}
