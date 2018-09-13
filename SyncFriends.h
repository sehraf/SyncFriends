#ifndef SYNCFRIENDS_H
#define SYNCFRIENDS_H

#include <retroshare/rsplugin.h>

class p3SyncFriends;

class SyncFriends : public RsPlugin
{
public:
	SyncFriends();

	// RsPlugin interface
public:
	p3Config	*p3_config() const;
	void		stop();
	std::string configurationFileName() const;
	MainPage	*qt_page() const;
	QIcon		*qt_icon() const;
	std::string	getShortPluginDescription() const;
	std::string	getPluginName() const;
	void		getPluginVersion(int &major, int &minor, int &build, int &svn_rev) const;
	void		setInterfaces(RsPlugInInterfaces &);
	void		setPlugInHandler(RsPluginHandler *pgHandler);

private:
	p3SyncFriends	*_service;

	mutable QIcon		*_icon;
	mutable MainPage	*_mainpage;
};

#endif // SYNCFRIENDS_H
