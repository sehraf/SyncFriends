#include "SyncFriends.h"

#include <retroshare/rsidentity.h>
#include <retroshare/rsmsgs.h>
#include <retroshare/rspeers.h>

#include "SyncFriendsWidget.h"
#include "p3SyncFriends.h"

/*
 * Credits:
 * <div>Icons made by <a href="https://www.flaticon.com/authors/itim2101" title="itim2101">itim2101</a> from <a href="https://www.flaticon.com/" title="Flaticon">www.flaticon.com</a> is licensed by <a href="http://creativecommons.org/licenses/by/3.0/" title="Creative Commons BY 3.0" target="_blank">CC 3.0 BY</a></div>
 * Icons made by itim2101 from www.flaticon.com is licensed by CC 3.0 BY
 */
#define ICON_LINK ":/images/sync.svg"

extern "C" {
#ifdef WIN32
    __declspec(dllexport)
#endif
	RsPlugin *RETROSHARE_PLUGIN_provide()
	{
		return new SyncFriends();
	}

	// This symbol contains the svn revision number grabbed from the executable.
	// It will be tested by RS to load the plugin automatically, since it is safe to load plugins
	// with same revision numbers, assuming that the revision numbers are up-to-date.
	//
#ifdef WIN32
	__declspec(dllexport)
#endif
	uint32_t RETROSHARE_PLUGIN_revision = 1;

	// This symbol contains the svn revision number grabbed from the executable.
	// It will be tested by RS to load the plugin automatically, since it is safe to load plugins
	// with same revision numbers, assuming that the revision numbers are up-to-date.
	//
#ifdef WIN32
	__declspec(dllexport)
#endif
	uint32_t RETROSHARE_PLUGIN_api = RS_PLUGIN_API_VERSION ;
}

SyncFriends::SyncFriends() :
    _service(nullptr), _icon(nullptr), _mainpage(nullptr)
{

}

p3Config *SyncFriends::p3_config() const
{
	return _service;
}

void SyncFriends::stop()
{
	_service->fullstop();
}

std::string SyncFriends::configurationFileName() const
{
	return "SyncFriends.cfg";
}

MainPage *SyncFriends::qt_page() const
{
	if (_mainpage == nullptr)
		_mainpage = new SyncFriendsWidget();

	return _mainpage;
}

QIcon *SyncFriends::qt_icon() const
{
	if(_icon == nullptr) {
		Q_INIT_RESOURCE(SyncFriendsImages);
		_icon = new QIcon(ICON_LINK);
	}

	return _icon;
}

std::string SyncFriends::getShortPluginDescription() const
{
//	return QApplication::translate("Lua4RS", "This plugin provides Lua scripting capabilities to RetroShare.").toUtf8().constData();
	return "Synchonize your friendslist between nodes. Based on mails to GXS IDs";
}

std::string SyncFriends::getPluginName() const
{
//	return QApplication::translate("Lua4RS", "Lua4RS").toUtf8().constData();
	return "SyncFriends";
}

void SyncFriends::getPluginVersion(int &major, int &minor, int &build, int &svn_rev) const
{
	major = RS_MAJOR_VERSION;
	minor = RS_MINOR_VERSION;
	build = RS_MINI_VERSION;
	svn_rev = 0;
}

void SyncFriends::setInterfaces(RsPlugInInterfaces &/*interfaces*/)
{
	// use this call to figure out when retroshare core is setup and ready

	if (_service == nullptr)
		_service = new p3SyncFriends();

	dynamic_cast<SyncFriendsWidget*>(qt_page())->setService(_service);
}

void SyncFriends::setPlugInHandler(RsPluginHandler *pgHandler)
{
	(void) pgHandler;
}
