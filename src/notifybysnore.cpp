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


static NotifyBySnore *s_instance = nullptr;
// RUN THIS BEFORE TRYING OR THE NOTIFS WON'T SHOW!
// .\SnoreToast.exe -install "KDE Connect" "C:\CraftRoot\bin\kdeconnectd.exe" "org.kde.connect"

//gotta get more inspirations from notifybyportal, eg: PixMap rendering in notifs
// installing the Shortcut

NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    s_instance = this;
    program = QStringLiteral("SnoreToast.exe");
    // TODO expose the function for Win32 based apps
// installShortcut(QStringLiteral("KDE Connect"),QString(QDir::currentPath()+QStringLiteral("/kdeconnectd.exe")), QStringLiteral("org.kde.connect"));
    // Sleep(uint(4000));

}

NotifyBySnore::~NotifyBySnore()
{
    s_instance = nullptr;
}

void NotifyBySnore::installShortcut(QString& appName, QString& appLocation, QString& appID)
{
    myProcess = new QProcess();
    myProcess->start(program, arguments);
    QStringList arguments;
// -install <name> <application> <appID>| Creates a shortcut <name> in the start menu which point to the executable <application>, appID used for the notifications.
    arguments << QStringLiteral("-install") << appName << appLocation << appID;

    if(myProcess->waitForStarted(5000))
    {   
        // qDebug() << out_string.toStdString().c_str();
        qDebug() << "SnoreToast called for Shortcut installation.";
    }
    else
    {
        qDebug() << "SnoreToast did not start in time for Notif-Show";
    }
}

void NotifyBySnore::notify(KNotification *notification, KNotifyConfig *config)
{

    myProcess = new QProcess();
    QStringList arguments;
    
    // image loading now! we read from the pixmap sent through KDE Connect 
    // and save it to a file temporarily, then delete the icon once the notif is rendered

    if (!notification->pixmap().isNull()) {
        QFile file( notification->eventId() );
        file.open(QIODevice::WriteOnly);
        notification->pixmap().save(&file, "PNG");
        file.close();
    }

    arguments << QStringLiteral("-t") << notification->title();
    arguments << QStringLiteral("-m") << notification->text();
    arguments << QStringLiteral("-p") <<  notification->eventId() ;
    arguments << QStringLiteral("-appID") << QStringLiteral("org.kde.connect");

    myProcess->start(program, arguments);
    if(myProcess->waitForStarted(5000))
    {
        Sleep(1000);
        qDebug() << "SnoreToast called for Notif-show.";
        QFile rem_file( notification->eventId() );
        rem_file.remove();
    }
    else
    {
        qDebug() << "SnoreToast did not start in time for Notif-Show";
    }

}
/* 
basically, we have every notifiaction packaged as a JSON-ish object. 
Need to look into what's this "id" thingy.
Then we also have a config 
    Not sure what's that, but we surely get this from notifs in KDEConnect.
Here in this plugin we gotta define our own 
    notify
    close
    notificationFinished
    notificationActionInvoked

aaaand we should be done :)
*/


void NotifyBySnore::close(KNotification* notification)
{
    // KNotificationPlugin::close(notification);
}

void NotifyBySnore::update(KNotification *notification, KNotifyConfig *config)
{
}

void NotifyBySnore::notificationActionInvoked(int id, int action)
{
    qCDebug(LOG_KNOTIFICATIONS) << id << action;
    emit actionInvoked(id, action);
}
