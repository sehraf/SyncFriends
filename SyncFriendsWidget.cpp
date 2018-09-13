#include "SyncFriendsWidget.h"
#include "ui_SyncFriendsWidget.h"

#include <QTimer>
#include <QStringList>
#include <QStringListModel>
#include <QAbstractItemView>

#include "utils.h"

SyncFriendsWidget::SyncFriendsWidget(QWidget *parent) :
    MainPage(parent),
    ui(new Ui::SyncFriendsWidget),
    _service(nullptr)
{
	ui->setupUi(this);

	QTimer *timer = new QTimer(this);
	QApplication::connect(timer, SIGNAL(timeout()), this, SLOT(updateTickets()));
	timer->start(500);
#ifdef VIRTUAL_FL
	ui->cb_removeFriendsActive->setEnabled(true);
	ui->cb_removeFriendsRequest->setEnabled(true);
#endif
}

SyncFriendsWidget::~SyncFriendsWidget()
{
	delete ui;
}

void SyncFriendsWidget::on_pb_sync_clicked()
{
	// save last config
	emit storeConfig();
	_service->addTicket(syncFriends::actions::synchronize);
}

void SyncFriendsWidget::on_pb_send_clicked()
{
	// save last config
	emit storeConfig();
	_service->addTicket(syncFriends::actions::send);
}

void SyncFriendsWidget::on_pb_request_clicked()
{
	// save last config
	emit storeConfig();
	_service->addTicket(syncFriends::actions::request);
}

void SyncFriendsWidget::setService(p3SyncFriends *service)
{
	_service = service;

	// load config
	QTimer::singleShot(500, this, SLOT(loadConfig()));
}

void SyncFriendsWidget::updateTickets()
{
	ui->lw_tickets->clear();

	std::list<syncFriends::ticket> ts = _service->getTickets();
	if (ts.empty())
		return;

	char c = '\t';
	for (auto ticket = ts.begin(); ticket != ts.end(); ++ticket) {
		std::string row;
		row += std::to_string(ticket->id) + c;
		row += syncFriends::stateToString(ticket->state) + c;
		row += syncFriends::actionToString(ticket->action) + c;
		row += std::to_string(ticket->targetIds.size()) + c;
		ui->lw_tickets->addItem(QString::fromStdString(row));
	}
	ui->lw_tickets->sortItems();
}

void SyncFriendsWidget::loadConfig()
{
	syncFriends::config c =_service->getConfig();
	if (!c.loaded) {
		QTimer::singleShot(500, this, SLOT(loadConfig()));
		return;
	}

	if (c.sendFrom.isNull())
		ui->le_sendFrom->clear();
	else
		ui->le_sendFrom->setText(QString::fromStdString(c.sendFrom.toStdString()));

	for (auto it = c.sendTo.begin(); it != c.sendTo.end(); ++it)
		ui->lw_sendTo->addItem(QString::fromStdString(it->toStdString()));

	for (auto it = c.acceptFrom.begin(); it != c.acceptFrom.end(); ++it)
		ui->lw_acceptFrom->addItem(QString::fromStdString(it->toStdString()));

	ui->cb_groups->setChecked(c.includeGroups);
	ui->cb_removeFriendsRequest->setChecked(c.removeFriendsRequest);
	ui->cb_active->setChecked(c.active);
	ui->cb_removeFriendsActive->setChecked(c.removeFriendsActive);
}

void SyncFriendsWidget::storeConfig()
{
	syncFriends::config c =_service->getConfig();

	c.sendFrom = RsGxsId(ui->le_sendFrom->text().toStdString());

	c.sendTo.clear();
	for (int i = 0; i < ui->lw_sendTo->count(); ++i)
		c.sendTo.push_back(RsGxsId(ui->lw_sendTo->item(i)->data(0).toString().toStdString()));

	c.acceptFrom.clear();
	for (int i = 0; i < ui->lw_acceptFrom->count(); ++i)
		c.acceptFrom.push_back(RsGxsId(ui->lw_acceptFrom->item(i)->data(0).toString().toStdString()));

	c.includeGroups = ui->cb_groups->isChecked();
	c.removeFriendsRequest = ui->cb_removeFriendsRequest->isChecked();
	c.active = ui->cb_active->isChecked();
	c.removeFriendsActive = ui->cb_removeFriendsActive->isChecked();

	_service->setConfig(c);
}

void SyncFriendsWidget::on_pb_sendToAdd_clicked()
{
	RsGxsId id = RsGxsId(ui->le_sendTo->text().toStdString());
	if (id.isNull())
		// todo add message
		return;

	ui->lw_sendTo->addItem(QString::fromStdString(id.toStdString()));
	ui->lw_sendTo->sortItems();

	ui->le_sendTo->clear();
}

void SyncFriendsWidget::on_pb_sendToRemove_clicked()
{
	qDeleteAll(ui->lw_sendTo->selectedItems());
}

void SyncFriendsWidget::on_pb_acceptFromAdd_clicked()
{
	RsGxsId id = RsGxsId(ui->le_acceptFrom->text().toStdString());
	if (id.isNull())
		// todo add message
		return;

	ui->lw_acceptFrom->addItem(QString::fromStdString(id.toStdString()));
	ui->lw_acceptFrom->sortItems();

	ui->le_acceptFrom->clear();
}

void SyncFriendsWidget::on_pb_acceptFromRemove_clicked()
{
	qDeleteAll(ui->lw_acceptFrom->selectedItems());
}

void SyncFriendsWidget::on_load_clicked()
{
	emit loadConfig();
}

void SyncFriendsWidget::on_save_clicked()
{
	emit storeConfig();
}

void SyncFriendsWidget::on_pb_clearFinish_clicked()
{
	_service->doCleanUp();
}

void SyncFriendsWidget::on_pb_dropAllTickets_clicked()
{
	_service->doCleanUp(true);
}

void SyncFriendsWidget::on_cb_active_stateChanged(int /*arg1*/)
{
	syncFriends::config c =_service->getConfig();
	c.active = ui->cb_active->isChecked();
	_service->setConfig(c);
}

void SyncFriendsWidget::on_cb_groups_stateChanged(int /*arg1*/)
{
	syncFriends::config c =_service->getConfig();
	c.includeGroups = ui->cb_groups->isChecked();
	_service->setConfig(c);
}

void SyncFriendsWidget::on_cb_removeFriendsRequest_stateChanged(int /*arg1*/)
{
	syncFriends::config c =_service->getConfig();
	c.removeFriendsRequest = ui->cb_removeFriendsRequest->isChecked();
	_service->setConfig(c);
}

void SyncFriendsWidget::on_cb_removeFriendsActive_stateChanged(int /*arg1*/)
{
	syncFriends::config c =_service->getConfig();
	c.removeFriendsActive = ui->cb_removeFriendsActive->isChecked();
	_service->setConfig(c);
}
