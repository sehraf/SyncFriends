#ifndef SYNCFRIENDSWIDGET_H
#define SYNCFRIENDSWIDGET_H

#include <retroshare-gui/mainpage.h>
//#include <gui/common/FriendSelectionWidget.h>

#include "p3SyncFriends.h"

class p3SyncFriends;

namespace Ui {
class SyncFriendsWidget;
}

class SyncFriendsWidget : public MainPage
{
	Q_OBJECT

public:
	explicit SyncFriendsWidget(QWidget *parent = nullptr);
	~SyncFriendsWidget();

	void setService(p3SyncFriends *service);

private slots:
	void on_pb_sync_clicked();
	void on_pb_send_clicked();
	void on_pb_request_clicked();
	void on_pb_sendToAdd_clicked();
	void on_pb_sendToRemove_clicked();

	void updateTickets();
	void loadConfig();
public slots:
	void storeConfig();	

private slots:
	void on_load_clicked();
	void on_save_clicked();
	void on_cb_active_stateChanged(int arg1);
	void on_pb_clearFinish_clicked();
	void on_pb_acceptFromAdd_clicked();
	void on_pb_acceptFromRemove_clicked();
	void on_pb_dropAllTickets_clicked();
	void on_cb_groups_stateChanged(int arg1);

	void on_cb_removeFriendsRequest_stateChanged(int arg1);

	void on_cb_removeFriendsActive_stateChanged(int arg1);

private:

	Ui::SyncFriendsWidget *ui;

	p3SyncFriends	*_service;
};

#endif // SYNCFRIENDSWIDGET_H
