#ifndef NOTIFYBYMACOSNOTIFICATIONCENTER_H
#define NOTIFYBYMACOSNOTIFICATIONCENTER_H

#include "knotificationplugin.h"

class MacOSNotificationCenterPrivate;

class NotifyByMacOSNotificationCenter : public KNotificationPlugin
{
    Q_OBJECT

public:
    NotifyByMacOSNotificationCenter(QObject* parent);
    ~NotifyByMacOSNotificationCenter();

    QString optionName() override { return QStringLiteral("Popup"); }
    void notify(KNotification *notification, KNotifyConfig *config) override;
    void update(KNotification *notification, KNotifyConfig *config) override;
    void close(KNotification *notification) override;
//private:
//    MacOSNotificationCenterPrivate *m_macosNotificationCenter;
};

#endif // NOTIFYBYMACOSNOTIFICATIONCENTER_H
