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

static NotifyBySnore *s_instance = nullptr;

NotifyBySnore::NotifyBySnore(QObject* parent) :
    KNotificationPlugin(parent)
{
    s_instance = this;
}

NotifyBySnore::~NotifyBySnore()
{
    s_instance = nullptr;
}

QString NotifyBySnore::optionName()
{
    return QStringLiteral("Toast");
}

void NotifyBySnore::notify(KNotification *notification, KNotifyConfig *config)
{

}
/* 
basically, we have every notifiaction packaged as a JSON-ish object. 
Need to look into what's this "id" thingy.
Then we also have a config 
    Not sure what's that, but we sure get this from notifs in KDEConnect.
Here in this plugin we gotta define our own 
    notify
    close
    notificationFinished
    notificationActionInvoked

aaaand we should be done :)
*/
void NotifyBySnore::notifyDeferred(KNotification* notification)
{

}

void NotifyBySnore::close(KNotification* notification)
{
    // KNotificationPlugin::close(notification);
}

void NotifyBySnore::notificationFinished(int id)
{
}

void NotifyBySnore::notificationActionInvoked(int id, int action)
{
    qCDebug(LOG_KNOTIFICATIONS) << id << action;
    emit actionInvoked(id, action);
}
