#include "notifybymacosnotificationcenter.h"
#include "knotification.h"
#include "knotifyconfig.h"
#include "debug_p.h"

#include <QBuffer>
#include <QIcon>
#include <QLoggingCategory>
#include <QCoreApplication>
#include <QDebug>

#include <Foundation/Foundation.h>
#import <objc/runtime.h>

// class MacOSNotificationCenterPrivate
// {
// public:
//     MacOSNotificationCenterPrivate();
//     ~MacOSNotificationCenterPrivate();

//     //void addNotification(NSUserNotification *notification);
//     //void removeNotification();
// private:
//     QMap<int, NSUserNotification*> m_notifications;
//     friend class NotifyByMacOSNotificationCenter;
// };
//
// static MacOSNotificationCenterPrivate macOSNotificationCenterPrivate;
id m_delegate;

@interface MacOSNotificationCenterDelegate : NSObject<NSUserNotificationCenterDelegate> {}
@end

@implementation MacOSNotificationCenterDelegate
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center shouldPresentNotification:(NSUserNotification *)notification 
{
    return YES;
}

- (void)userNotificationCenter:(NSUserNotificationCenter *)center didDeliverNotification:(NSUserNotification *)notification
{
    NSLog(@"Send notification %d", [notification.userInfo[@"id"] intValue]);
}

- (void) userNotificationCenter:(NSUserNotificationCenter *)center didActivateNotification:(NSUserNotification *)notification
{
    // TO-DO: actions
    NSLog(@"User clicked on notification %d", [notification.userInfo[@"id"] intValue]);
    switch (notification.activationType) {
        case NSUserNotificationActivationTypeReplied:
            NSLog(@"Replied clicked");
            break;
        case NSUserNotificationActivationTypeContentsClicked:
            NSLog(@"Contents clicked");
            break;
        case NSUserNotificationActivationTypeActionButtonClicked:
            NSLog(@"ActionButton clicked");
            break;
        case NSUserNotificationActivationTypeAdditionalActionClicked:
            NSLog(@"AdditionalAction clicked");
            break;
        default:
            NSLog(@"Other clicked");
            break;
    }
}
@end

// MacOSNotificationCenterPrivate::MacOSNotificationCenterPrivate()
// {
//     // Set delegate
//     m_delegate = [MacOSNotificationCenterDelegate alloc];
//     [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:m_delegate];
// }

// MacOSNotificationCenterPrivate::~MacOSNotificationCenterPrivate()
// {
//     [m_delegate release];
// }

NotifyByMacOSNotificationCenter::NotifyByMacOSNotificationCenter (QObject* parent) 
    : KNotificationPlugin(parent)
{
    qCDebug(LOG_KNOTIFICATIONS) << "Knotification macos backend created";
    m_delegate = [MacOSNotificationCenterDelegate alloc];
    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:m_delegate];
    //m_macosNotificationCenter = new MacOSNotificationCenterPrivate();
}

NotifyByMacOSNotificationCenter::~NotifyByMacOSNotificationCenter () 
{
    qCDebug(LOG_KNOTIFICATIONS) << "Knotification macos backend deleted";
    [m_delegate release];
    // if (m_macosNotificationCenter != nullptr) {
    //     delete m_macosNotificationCenter;
    //     m_macosNotificationCenter = nullptr;
    // }
}

void NotifyByMacOSNotificationCenter::notify(KNotification *notification, KNotifyConfig *config)
{
    qCDebug(LOG_KNOTIFICATIONS) <<  "Test notification " << notification->id();
    NSUserNotification *osxNotification = [[[NSUserNotification alloc] init] autorelease];
    NSString *notificationId = [NSString stringWithFormat:@"%d", notification->id()];

    CFStringRef cfTitle = notification->title().toCFString(),
                cfText = notification->text().toCFString();

    osxNotification.title = [NSString stringWithString: (NSString *)cfTitle];
    osxNotification.userInfo = [NSDictionary dictionaryWithObjectsAndKeys:notificationId, @"id", nil];
    osxNotification.informativeText = [NSString stringWithString: (NSString *)cfText];

    // m_macosNotificationCenter->m_notifications.insert(notification->id(), osxNotification);

    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification: osxNotification];

    CFRelease(cfTitle);
    CFRelease(cfText);
}

void NotifyByMacOSNotificationCenter::close(KNotification *notification)
{
    qCDebug(LOG_KNOTIFICATIONS) << "Test remove notification " << notification->id();

    NSArray<NSUserNotification *> *deliveredNotifications = [NSUserNotificationCenter defaultUserNotificationCenter].deliveredNotifications;
    for (NSUserNotification *deliveredNotification in deliveredNotifications) {
        if ([deliveredNotification.userInfo[@"id"] intValue] == notification->id()) {
            qCDebug(LOG_KNOTIFICATIONS) <<  "Try to remove notification " << notification->id();
            [[NSUserNotificationCenter defaultUserNotificationCenter] removeDeliveredNotification: deliveredNotification];
            qCDebug(LOG_KNOTIFICATIONS) <<  "Removed notification " << notification->id();
            finish(notification);
            return;
        }
    }
    qCDebug(LOG_KNOTIFICATIONS) <<  "Notification " << notification->id() << " not found";
}

void NotifyByMacOSNotificationCenter::update(KNotification *notification, KNotifyConfig *config)
{
    close(notification);
    notify(notification, config);
}
