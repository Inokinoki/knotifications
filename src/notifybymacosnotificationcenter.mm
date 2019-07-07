#include "notifybymacosnotificationcenter.h"
#include "knotification.h"
#include "knotifyconfig.h"
#include "debug_p.h"

#include <QIcon>
#include <QLoggingCategory>
#include <QDebug>
#include <QtMac>

#include <Foundation/Foundation.h>
#import <objc/runtime.h>

class MacOSNotificationCenterPrivate
{
public:
    MacOSNotificationCenterPrivate();
    ~MacOSNotificationCenterPrivate();

    void insertKNotification(int internalId, KNotification *notification);
    KNotification *getKNotification(int internalId);
    const KNotification *getKNotification(int internalId) const;
    KNotification *takeKNotification(int internalId);
    void removeKNotification(int internalId);

    int generateInternalId() { return m_internalCounter++; }
private:
    id m_delegate;

    int m_internalCounter;
    QMap<int, KNotification*> m_notifications;
};

static MacOSNotificationCenterPrivate macosNotificationCenterPrivate;

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
    KNotification *originNotification;
    NSLog(@"User clicked on notification %d, internal Id: %d", [notification.userInfo[@"id"] intValue], [notification.userInfo[@"internalId"] intValue]);
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
            NSLog(@"AdditionalAction clicked %@", notification.additionalActivationAction.identifier);
            originNotification = macosNotificationCenterPrivate.getKNotification([notification.userInfo[@"internalId"] intValue]);
            if (!originNotification) break;
            emit originNotification->activate([notification.additionalActivationAction.identifier intValue] + 1);
            break;
        default:
            NSLog(@"Other clicked");
            break;
    }
}
@end

MacOSNotificationCenterPrivate::MacOSNotificationCenterPrivate()
{
    // Set delegate
    m_delegate = [MacOSNotificationCenterDelegate alloc];
    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:m_delegate];

    // Init internal notification counter
    m_internalCounter = 0;
}

MacOSNotificationCenterPrivate::~MacOSNotificationCenterPrivate()
{
    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate: nil];
    [m_delegate release];

    // Try to finish all KNotification
    for (KNotification *notification : m_notifications.values()) {
        notification->deref();
    }

    // Try to finish all NSNotification
    NSArray<NSUserNotification *> *deliveredNotifications = [NSUserNotificationCenter defaultUserNotificationCenter].deliveredNotifications;
    for (NSUserNotification *deliveredNotification in deliveredNotifications) {
        // Remove NSNotification in notification center
        [[NSUserNotificationCenter defaultUserNotificationCenter] removeDeliveredNotification: deliveredNotification];
    }
}

void MacOSNotificationCenterPrivate::insertKNotification(int internalId, KNotification *notification)
{
    if (!notification) return;

    m_notifications.insert(internalId, notification);
}

KNotification *MacOSNotificationCenterPrivate::getKNotification(int internalId)
{
    return m_notifications[internalId];
}

const KNotification *MacOSNotificationCenterPrivate::getKNotification(int internalId) const
{
    return m_notifications[internalId];
}

KNotification *MacOSNotificationCenterPrivate::takeKNotification(int internalId)
{
    return m_notifications.take(internalId);
}

void MacOSNotificationCenterPrivate::removeKNotification(int internalId)
{
    m_notifications.remove(internalId);
}

NotifyByMacOSNotificationCenter::NotifyByMacOSNotificationCenter (QObject* parent) 
    : KNotificationPlugin(parent)
{
    qCDebug(LOG_KNOTIFICATIONS) << "Knotification macos backend created";
}

NotifyByMacOSNotificationCenter::~NotifyByMacOSNotificationCenter () 
{
    qCDebug(LOG_KNOTIFICATIONS) << "Knotification macos backend deleted";
}

void NotifyByMacOSNotificationCenter::notify(KNotification *notification, KNotifyConfig *config)
{
    qCDebug(LOG_KNOTIFICATIONS) <<  "Test notification " << notification->id();

    int internalId = macosNotificationCenterPrivate.generateInternalId();
    NSUserNotification *osxNotification = [[[NSUserNotification alloc] init] autorelease];
    NSString *notificationId = [NSString stringWithFormat: @"%d", notification->id()],
             *internalNotificationId = [NSString stringWithFormat: @"%d", internalId];

    CFStringRef cfTitle = notification->title().toCFString(),
                cfText = notification->text().toCFString();

    osxNotification.title = [NSString stringWithString: (NSString *)cfTitle];
    osxNotification.userInfo = [NSDictionary dictionaryWithObjectsAndKeys: notificationId, @"id",
        internalNotificationId, @"internalId", nil];
    osxNotification.informativeText = [NSString stringWithString: (NSString *)cfText];
    osxNotification.contentImage = QtMac::toNSImage(notification->pixmap());

    NSLog(@"Action size %d", notification->actions().length());
    NSLog(@"Default action: %s", notification->defaultAction().toStdString().c_str());

    if (notification->actions().isEmpty()) {
        // Remove all buttons
        osxNotification.hasReplyButton = false;
        osxNotification.hasActionButton = false;
    } else {
        osxNotification.hasActionButton = true;
        // Workaround: this "API" will cause refuse from Apple
        [osxNotification setValue:[NSNumber numberWithBool:YES] forKey: @"_alwaysShowAlternateActionMenu"];

        // Construct a list for all actions
        NSMutableArray<NSUserNotificationAction*> *actions = [[NSMutableArray alloc] init];
        for (int index = 0; index < notification->actions().length(); index++) {
            NSUserNotificationAction *action =
                [NSUserNotificationAction actionWithIdentifier: [NSString stringWithFormat:@"%d", index] 
                                            title: notification->actions().at(index).toNSString()];
            [actions addObject: action];
        }
        osxNotification.additionalActions = actions;
    }

    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification: osxNotification];

    macosNotificationCenterPrivate.insertKNotification(internalId, notification);

    CFRelease(cfTitle);
    CFRelease(cfText);
}

void NotifyByMacOSNotificationCenter::close(KNotification *notification)
{
    qCDebug(LOG_KNOTIFICATIONS) << "Test remove notification " << notification->id();

    NSArray<NSUserNotification *> *deliveredNotifications = [NSUserNotificationCenter defaultUserNotificationCenter].deliveredNotifications;
    for (NSUserNotification *deliveredNotification in deliveredNotifications) {
        if ([deliveredNotification.userInfo[@"id"] intValue] == notification->id()) {
            NSLog(@"Remove notification %d %d", [deliveredNotification.userInfo[@"id"] intValue], [deliveredNotification.userInfo[@"internalId"] intValue]);
            // Remove KNotification in mapping
            int internalId = [deliveredNotification.userInfo[@"id"] intValue];
            macosNotificationCenterPrivate.removeKNotification(internalId);

            // Remove NSNotification in notification center
            [[NSUserNotificationCenter defaultUserNotificationCenter] removeDeliveredNotification: deliveredNotification];
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
