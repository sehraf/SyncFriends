#ifndef GXSTRANSHANDLER_H
#define GXSTRANSHANDLER_H

#include <queue>

#include <gxstrans/p3gxstrans.h>

#include "utils.h"

class gxsTransHandler : public GxsTransClient
{
public:
	gxsTransHandler();

	struct messageContainer {
		RsGxsId from;
		RsGxsId to;
		syncFriends::message message;
	};

	bool popMessage(gxsTransHandler::messageContainer &msg);
	void sendMessage(const syncFriends::ticket &ticket, const syncFriends::message &msg);

	// GxsTransClient interface
protected:
	bool receiveGxsTransMail(const RsGxsId &authorId, const RsGxsId &recipientId, const uint8_t *data, uint32_t dataSize);
	bool notifyGxsTransSendStatus(RsGxsTransId mailId, GxsTransSendStatus status);

private:
	std::queue<gxsTransHandler::messageContainer> _messages;
	RsMutex	_mutex;
};

#endif // GXSTRANSHANDLER_H
