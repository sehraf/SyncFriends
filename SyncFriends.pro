!include("../Common/retroshare_plugin.pri")::error( "Could not include file ../Common/retroshare_plugin.pri" )

CONFIG += qt resources uic qrc

greaterThan(QT_MAJOR_VERSION, 4) {
        # Qt 5
        QT += widgets
}

#DEFINES += DEBUG            # more logging + less random ticket IDs
#DEFINES += DEBUG_DELETE     # don't delete mails
#DEFINES += DEBUG_DRY_RUN    # don't change friend list

# don't use the following!
#DEFINES += USE_GXSTRANS     # don't piggyback on mails, use it directly

# when rapidjson is mainstream on all distribs, we will not need the sources anymore
# in the meantime, they are part of the RS directory so that it is always possible to find them
INCLUDEPATH += ../../rapidjson-1.1.0

HEADERS += \
    SyncFriends.h \
    SyncFriendsWidget.h \
    p3SyncFriends.h \
    utils.h \
    gxsTransHandler.h

SOURCES += \
    SyncFriends.cpp \
    SyncFriendsWidget.cpp \
    p3SyncFriends.cpp \
    utils.cpp \
    gxsTransHandler.cpp

FORMS += \
    SyncFriendsWidget.ui

RESOURCES += \
    SyncFriendsImages.qrc
