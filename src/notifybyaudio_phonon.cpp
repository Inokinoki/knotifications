/* This file is part of the KDE libraries
   Copyright (C) 2014-2015 by Martin Klapetek <mklapetek@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "notifybyaudio_phonon.h"
#include "debug_p.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QString>

#include "knotifyconfig.h"
#include "knotification.h"

#include <phonon/mediaobject.h>
#include <phonon/mediasource.h>
#include <phonon/audiooutput.h>

NotifyByAudio::NotifyByAudio(QObject *parent)
    : KNotificationPlugin(parent),
      m_audioOutput(nullptr)
{
}

NotifyByAudio::~NotifyByAudio()
{
    qDeleteAll(m_reusablePhonons);
    delete m_audioOutput;
}

void NotifyByAudio::notify(KNotification *notification, KNotifyConfig *config)
{
    if (!m_audioOutput) {
        m_audioOutput = new Phonon::AudioOutput(Phonon::NotificationCategory, this);
    }
    const QString soundFilename = config->readEntry(QStringLiteral("Sound"));
    if (soundFilename.isEmpty()) {
        qCWarning(LOG_KNOTIFICATIONS) << "Audio notification requested, but no sound file provided in notifyrc file, aborting audio notification";

        finish(notification);
        return;
    }

    QUrl soundURL;
    const auto dataLocations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dataLocation : dataLocations) {
        soundURL = QUrl::fromUserInput(soundFilename,
                                       dataLocation + QStringLiteral("/sounds"),
                                       QUrl::AssumeLocalFile);
        if (soundURL.isLocalFile() && QFile::exists(soundURL.toLocalFile())) {
            break;
        } else if (!soundURL.isLocalFile() && soundURL.isValid()) {
            break;
        }
        soundURL.clear();
    }
    if (soundURL.isEmpty()) {
        qCWarning(LOG_KNOTIFICATIONS) << "Audio notification requested, but sound file from notifyrc file was not found, aborting audio notification";
        finish(notification);
        return;
    }

    Phonon::MediaObject *m;

    if (m_reusablePhonons.isEmpty()) {
        m = new Phonon::MediaObject(this);
        connect(m, &Phonon::MediaObject::finished, this, &NotifyByAudio::onAudioFinished);
        connect(m, &Phonon::MediaObject::stateChanged, this, &NotifyByAudio::stateChanged);
        Phonon::createPath(m, m_audioOutput);
    } else {
        m = m_reusablePhonons.takeFirst();
    }

    m->setCurrentSource(soundURL);
    m->play();

    if (notification->flags() & KNotification::LoopSound) {
        // Enqueuing essentially prevents the subsystem pipeline from partial teardown
        // which is the most desired thing in terms of load and delay between loop cycles.
        // All of this is timing dependent, which is why we want at least one source queued;
        // in reality the shorter the source the more sources we want to be queued to prevent
        // the MO from running out of sources.
        // Point being that all phonon signals are forcefully queued (because qthread has problems detecting !pthread threads),
        // so when you get for example the aboutToFinish signal the MO might already have stopped playing.
        //
        // And so we queue it three times at least; doesn't cost anything and keeps us safe.

        m->enqueue(soundURL);
        m->enqueue(soundURL);
        m->enqueue(soundURL);

        connect(m, &Phonon::MediaObject::currentSourceChanged,
                this, &NotifyByAudio::onAudioSourceChanged);
    }

    Q_ASSERT(!m_notifications.value(m));
    m_notifications.insert(m, notification);
}

void NotifyByAudio::stateChanged(Phonon::State newState, Phonon::State oldState)
{
    qCDebug(LOG_KNOTIFICATIONS) << "Changing audio state from" << oldState << "to" << newState;
}

void NotifyByAudio::close(KNotification *notification)
{
    Phonon::MediaObject *m = m_notifications.key(notification);

    if (!m) {
        return;
    }

    m->stop();
    finishNotification(notification, m);
}

void NotifyByAudio::onAudioFinished()
{
    Phonon::MediaObject *m = qobject_cast<Phonon::MediaObject*>(sender());

    if (!m) {
        return;
    }

    KNotification *notification = m_notifications.value(m, nullptr);

    if (!notification) {
        // This means that close was called already so there's nothing else to do.
        // Ideally we should not be getting here if close has already been called
        // since stopping a mediaobject means it won't emit finished() *BUT*
        // since the finished signal is a queued connection in phonon it can happen
        // that the playing had already finished and we just had not got the signal yet
        return;
    }

    //if the sound is short enough, we can't guarantee new sounds are
    //enqueued before finished is emitted.
    //so to make sure we are looping restart it when the sound finished
    if (notification && (notification->flags() & KNotification::LoopSound)) {
        m->play();
        return;
    }

    finishNotification(notification, m);
}

void NotifyByAudio::finishNotification(KNotification *notification, Phonon::MediaObject *m)
{
    m_notifications.remove(m);

    if (notification) {
        finish(notification);
    }

    disconnect(m, &Phonon::MediaObject::currentSourceChanged,
               this, &NotifyByAudio::onAudioSourceChanged);

    m_reusablePhonons.append(m);
}

void NotifyByAudio::onAudioSourceChanged(const Phonon::MediaSource &source)
{
    Phonon::MediaObject *m = qobject_cast<Phonon::MediaObject*>(sender());

    if (!m) {
        return;
    }

    m->enqueue(source);
}

