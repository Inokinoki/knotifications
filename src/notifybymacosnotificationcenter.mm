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

// @interface KNotificationWrapper : NSObject
// @property KNotification *notification;
// - (id) initWithKNotification: (KNotification *) knotification;
// @end

// @implementation KNotificationWrapper
// - (id) initWithKNotification: (KNotification *) knotification {
//     _notification = knotification;
//     return self;
// }
// @end

class MacOSNotificationCenterPrivate
{
public:
    MacOSNotificationCenterPrivate();
    ~MacOSNotificationCenterPrivate();

    void insertKNotification(int internalId, KNotification *notification);
    KNotification *getKNotification(int internalId);
    const KNotification *getKNotification(int internalId) const;

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
    // TO-DO: actions
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
            // notificationWrapper = notification.userInfo[@"originNotification"];
            // emit notificationWrapper.notification->activate([notification.additionalActivationAction.identifier intValue]);
            // emit [notification.additionalActivationAction.identifier intValue];
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

NotifyByMacOSNotificationCenter::NotifyByMacOSNotificationCenter (QObject* parent) 
    : KNotificationPlugin(parent)
{
    qCDebug(LOG_KNOTIFICATIONS) << "Knotification macos backend created";
    //m_delegate = [MacOSNotificationCenterDelegate alloc];
    //[[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:m_delegate];
    //m_macosNotificationCenter = new MacOSNotificationCenterPrivate();
}

NotifyByMacOSNotificationCenter::~NotifyByMacOSNotificationCenter () 
{
    qCDebug(LOG_KNOTIFICATIONS) << "Knotification macos backend deleted";
    // [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:nil];
    // [m_delegate release];
    //if (m_macosNotificationCenter != nullptr) {
    //    delete m_macosNotificationCenter;
    //    m_macosNotificationCenter = nullptr;
    //}
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

    //id value = ;
    //[osxNotification setValue:value forKey: @"_alwaysShowAlternateActionMenu"];

    //osxNotification.hasReplyButton = true;
    if (!notification->actions().isEmpty()) {
        osxNotification.hasActionButton = true;
        // Workaround: this "API" will cause refuse from Apple
        [osxNotification setValue:[NSNumber numberWithBool:YES] forKey: @"_alwaysShowAlternateActionMenu"];

        // if (notification->actions().length() == 1) {
        //     // This will be autoreleased, set by Qt
        //     osxNotification.additionalActivationAction = 
        //         [NSUserNotificationAction actionWithIdentifier: @"-1" title: notification->actions().at(0).toNSString()];
        // } else {
            // Allocate action list
            NSMutableArray<NSUserNotificationAction*> *actions = [[NSMutableArray alloc] init];
            for (int index = 0; index < notification->actions().length(); index++) {
                NSUserNotificationAction *action =
                    [NSUserNotificationAction actionWithIdentifier: [NSString stringWithFormat:@"%d", index] 
                                              title: notification->actions().at(index).toNSString()];
                [actions addObject: action];
            }
            osxNotification.additionalActions = actions;
        //}
    }
    // osxNotification.hasActionButton = true;
    // NSMutableArray<NSUserNotificationAction*> *actions = [[NSMutableArray alloc] init];
    // NSUserNotificationAction *action1 = [NSUserNotificationAction actionWithIdentifier: @"0" title: @"Action 1"];
    // NSUserNotificationAction *action2 = [NSUserNotificationAction actionWithIdentifier: @"1" title: @"Action 2"];
    // NSUserNotificationAction *action3 = [NSUserNotificationAction actionWithIdentifier: @"2" title: @"Action 3"];
    // [actions addObject: action1];
    // [actions addObject: action2];
    // [actions addObject: action3];
    
    // osxNotification.additionalActions = actions;

    // m_macosNotificationCenter->m_notifications.insert(notification->id(), osxNotification);

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
