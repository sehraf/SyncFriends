#ifndef P3SYNCFRIENDS_H
#define P3SYNCFRIENDS_H

#include <plugins/rspqiservice.h>

#include <queue>
#ifdef USE_GXSTRANS
#include "gxsTransHandler.h"
#endif
#include "utils.h"

class p3SyncFriends : public RsTickingThread, public p3Config
{
public:
	p3SyncFriends();

	// p3Config interface
protected:
	RsSerialiser *setupSerialiser();
	bool saveList(bool &cleanup, std::list<RsItem *> &lst);
	bool loadList(std::list<RsItem *> &load);

	// RsTickingThread interface
public:
	void data_tick();

public:
	void addTicket(const syncFriends::actions a);

private:
	void processTickets();
	void pollMessages();
#ifdef USE_GXSTRANS
	bool processMsg(const gxsTransHandler::messageContainer &msg);
#else
	bool processMsg(const std::string &id);
#endif
	bool checkPermission(const RsGxsId &sender, const bool hasResult, const uint32_t ticketId);

	syncFriends::message composeMessage(const syncFriends::ticket &t);
	syncFriends::message composeAcknowledgement(const syncFriends::ticket &t, const syncFriends::results result);

	void sendMessage(const syncFriends::ticket &ticket, const bool isAck = false, const syncFriends::results result = syncFriends::results::unset);
	void processAnswers(syncFriends::ticket *ticket);

	uint32_t getNextId();	

public:
	std::list<syncFriends::ticket> getTickets() const;
	void doCleanUp(const bool all = false);
	syncFriends::config getConfig() const;
	void setConfig(const syncFriends::config &config);

private:
	std::list<syncFriends::ticket>	_tickets;
	struct syncFriends::config		_config;

#ifdef USE_GXSTRANS
	gxsTransHandler	*_gxsTrans;
#endif

	bool	_doCleanUp;
	bool	_doCleanUpAll;

	uint8_t	_counter;	// used for polling messages less often than other actions

	RsMutex	_mutex;		// only used to protect _tasks and _doCleanUp, everything else is only access by own thread
};

#endif // P3SYNCFRIENDS_H
