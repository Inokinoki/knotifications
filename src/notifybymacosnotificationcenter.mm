#include "notifybymacosnotificationcenter.h"
#include "knotification.h"
#include "knotifyconfig.h"
#include "debug_p.h"

#include <QIcon>
#include <QLoggingCategory>
#include <QDebug>

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

KNotification *MacOSNotificationCenterPrivate::getKNotification(int internalId)
{
    return m_notifications.take(internalId);
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
    // osxNotification.contentImage = [NSImage contentsOfURL: [NSURL string: notification->iconName().toStdString().c_str()]];

    NSLog(@"Action size %d", notification->actions().length());
    NSLog(@"Default action: %s", notification->defaultAction().toStdString().c_str());

    if (!notification->actions().isEmpty()) {
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
