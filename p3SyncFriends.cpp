#include "p3SyncFriends.h"

#include <sstream>

#include <rsitems/rsconfigitems.h>
#include <retroshare/rsidentity.h>
#ifndef USE_GXSTRANS
#include <retroshare/rsmsgs.h>
#endif
#include <util/rstime.h>

/*******************************************
 * STATICS
 *******************************************/
static const std::string SF_CONFIG_SEND_FROM		= "SF_CONFIG_SEND_FROM";
static const std::string SF_CONFIG_SEND_TO			= "SF_CONFIG_SEND_TO";
static const std::string SF_CONFIG_ACCEPT_FROM		= "SF_CONFIG_ACCEPT_FROM";
static const std::string SF_CONFIG_GROUPS			= "SF_CONFIG_GROUPS";
static const std::string SF_CONFIG_TICKET			= "SF_CONFIG_TICKET";
static const std::string SF_CONFIG_ACTIVE			= "SF_CONFIG_ACTIVE";
static const std::string SF_CONFIG_REMOVE_REQUEST	= "SF_CONFIG_REMOVE_REQUEST";
static const std::string SF_CONFIG_REMOVE_ACTIVE	= "SF_CONFIG_REMOVE_ACTIVE";

static const std::string SF_ROOT_ITEM_NAME	= "SyncFriends";

/*******************************************
 * SETTINGS
 *******************************************/
// the mail system does not play well with fast message action
// --> sleep longer for polling
// SF_SLEEP_IN_SEC * SF_POLL_MSG_COUNTER = message poll frequency
static const uint32_t	SF_SLEEP_IN_SEC		= 1;
static const uint8_t	SF_POLL_MSG_COUNTER	= 15;

static const std::string SF_MSG_TITLE = "%SyncFriends%";

/*
 * Design concept:
 * Sender side:
 *		When a taks is added, create a ticket and send out messages.
 *		Ticket is used to keep track of pending tasks.
 *			--> statefull
 *		Wait for reply containing (optional) data and return value.
 *
 * Receiver side:
 *		When receiving a request, process it immediately and send (optional) data and return value.
 *			--> stateless
 *
 * Mails are used for communication.
 *	- RS takes care of everything.
 *	- Works based on GXS ids.
 *	- Independen from distance and asynchronous
 *
 */

p3SyncFriends::p3SyncFriends() :
    RsTickingThread (), p3Config (),
    _doCleanUp(false),
    _doCleanUpAll(false),
    _counter(0),
    _mutex("SyncFriends")
{
	_config.loaded = false;
	_config.active = false;
#ifdef USE_GXSTRANS
	_gxsTrans = new gxsTransHandler();
#endif
	start("SyncFriends");
}

RsSerialiser *p3SyncFriends::setupSerialiser()
{
	RsSerialiser* rsSerialiser = new RsSerialiser();
	rsSerialiser->addSerialType(new RsGeneralConfigSerialiser());

	return rsSerialiser;
}

RsTlvKeyValue push_str_val(const std::string& key, const std::string &value)
{
	RsTlvKeyValue kv;
	kv.key = key;
	kv.value = value;

	return kv;
}

#define fromBool(name, val) vitem->tlvkvs.pairs.push_back(push_str_val(name, val ? "1" : "0"))
#define toBool(name, val) else if(kit->key == name) val = kit->value == "1"

bool p3SyncFriends::saveList(bool &cleanup, std::list<RsItem *> &lst)
{
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "saveList: saving...");

	cleanup = true;

	RsConfigKeyValueSet *vitem = new RsConfigKeyValueSet;

	fromBool(SF_CONFIG_GROUPS, _config.includeGroups);
	vitem->tlvkvs.pairs.push_back(push_str_val(SF_CONFIG_SEND_FROM, _config.sendFrom.toStdString()));
	for (auto it = _config.sendTo.begin(); it != _config.sendTo.end(); ++it)
		vitem->tlvkvs.pairs.push_back(push_str_val(SF_CONFIG_SEND_TO, it->toStdString()));
	for (auto it = _tickets.begin(); it != _tickets.end(); ++it)
		vitem->tlvkvs.pairs.push_back(push_str_val(SF_CONFIG_TICKET, syncFriends::ticketToString(*it)));
	for (auto it = _config.acceptFrom.begin(); it != _config.acceptFrom.end(); ++it)
		vitem->tlvkvs.pairs.push_back(push_str_val(SF_CONFIG_ACCEPT_FROM, it->toStdString()));
	fromBool(SF_CONFIG_ACTIVE,			_config.active);
	fromBool(SF_CONFIG_REMOVE_ACTIVE,	_config.removeFriendsActive);
	fromBool(SF_CONFIG_REMOVE_REQUEST,	_config.removeFriendsRequest);
	lst.push_back(vitem);

	return true;
}

bool p3SyncFriends::loadList(std::list<RsItem *> &load)
{
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "loadList: loading...");

	for(auto item = load.begin(); item != load.end(); ++item) {
		RsConfigKeyValueSet *vitem = dynamic_cast<RsConfigKeyValueSet*>(*item);
		if(vitem != nullptr) {
			for(auto kit = vitem->tlvkvs.pairs.begin(); kit != vitem->tlvkvs.pairs.end(); ++kit) {
				if(kit->key ==      SF_CONFIG_SEND_FROM)
					_config.sendFrom = RsGxsId(kit->value);
				toBool(SF_CONFIG_GROUPS,			_config.includeGroups);
				toBool(SF_CONFIG_ACTIVE,			_config.active);
				toBool(SF_CONFIG_REMOVE_ACTIVE,		_config.removeFriendsActive);
				toBool(SF_CONFIG_REMOVE_REQUEST,	_config.removeFriendsRequest);
				else if(kit->key == SF_CONFIG_SEND_TO)
					_config.sendTo.push_back(RsGxsId(kit->value));
				else if(kit->key == SF_CONFIG_ACCEPT_FROM)
					_config.acceptFrom.push_back(RsGxsId(kit->value));
				else if(kit->key == SF_CONFIG_TICKET) {
					syncFriends::ticket t;
					if (syncFriends::stringToTicket(kit->value, t))
						_tickets.push_back(t);
				}
				else
					rslog(RsLog::Warning, &syncFriendsLogInfo, "loadList: unknown key: " + kit->key);
			}
		}

		delete vitem;
	}

	_config.loaded = true;
	return true;
}

#undef fromBool
#undef toBool

void p3SyncFriends::data_tick()
{
	processTickets();
	if (++_counter >= SF_POLL_MSG_COUNTER) {
		pollMessages();
		_counter = 0;
	}

	rstime::rs_usleep(SF_SLEEP_IN_SEC * 1000 * 1000);
}

void p3SyncFriends::addTicket(const syncFriends::actions a)
{
	// crerate ticket
	syncFriends::ticket ticket;

	// get everything from config, i.e. the values from the gui
	ticket.action = a;
	ticket.fromId = _config.sendFrom;
	ticket.targetIds.clear();

	ticket.control.includeGroups = _config.includeGroups;
	ticket.control.addFriends = true;
	ticket.control.removeFriends = _config.removeFriendsRequest;
	ticket.control.requestFriendList = !(a == syncFriends::actions::send);

	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "addTask: got new task");

	// verify validity of gxs IDs
	RsIdentityDetails details;
	for (auto it = _config.sendTo.begin(); it != _config.sendTo.end(); ++it) {
		if (!rsIdentity->getIdDetails(*it, details)) {
			rslog(RsLog::Warning, &syncFriendsLogInfo, "addTask: dropping unknown ID " + it->toStdString());
		} else {
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "addTask: known ID " + it->toStdString());
			ticket.targetIds.push_back(*it);
		}
	}

	// just in case
	if (ticket.targetIds.empty()) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "addTask: no gxs IDs left to talk to");
		return;
	}

	ticket.state = syncFriends::ticket::states::init;
	ticket.id = getNextId();
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "addTask: created ticket with id " + std::to_string(ticket.id));

	{
		RS_STACK_MUTEX(_mutex);
		_tickets.push_back(ticket);
	}

	IndicateConfigChanged();
}

void p3SyncFriends::processTickets()
{
	std::vector<std::list<syncFriends::ticket>::iterator> toDelet;

	if (_doCleanUp && _doCleanUpAll) {
		_tickets.clear();
		IndicateConfigChanged();
	}

	for (auto ticket = _tickets.begin(); ticket != _tickets.end(); ++ticket) {
		switch (ticket->state) {
		case syncFriends::ticket::states::init:
			// send message
			ticket->state = syncFriends::ticket::states::sending;
			break;
		case syncFriends::ticket::states::sending:
			// send message
			sendMessage(*ticket);
			ticket->state = syncFriends::ticket::states::sent;
			break;
		case syncFriends::ticket::states::sent:
			// wait for acknowledgement
			break;
		case syncFriends::ticket::states::receivedAnswer:
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processTasks: received answer (ticket id: " + std::to_string(ticket->id) + ") with " + std::to_string(ticket->targetIds.size()) + " gxs id(s) left");

			processAnswers(&(*ticket));

			// done yet?
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processTickets: ending with " + std::to_string(ticket->targetIds.size()) + " gxs id(s) left");
			if (ticket->targetIds.empty())
				ticket->state = syncFriends::ticket::states::finish;
			else
				ticket->state = syncFriends::ticket::states::sent;

			IndicateConfigChanged();
			break;
		case syncFriends::ticket::states::finish:
			if (_doCleanUp)
				toDelet.push_back(ticket);
			break;
		}
	}

	// reset _doCleanUp
	{
		RS_STACK_MUTEX(_mutex);
		if (_doCleanUp) {
			_doCleanUp = false;
			_doCleanUpAll = false;
		}
	}

	// we only add entries when _doCleanUp was true before
	if (!toDelet.empty()) {
		for (auto it = toDelet.begin(); it != toDelet.end(); ++it)
			_tickets.erase(*it);
		IndicateConfigChanged();
	}

}

/**
 * @brief p3SyncFriends::pollMessages
 * Poll messages and check for SyncFriend specific titel. When an unread message is found, mark it as read and process it.
 */
void p3SyncFriends::pollMessages()
{
#ifdef USE_GXSTRANS
	gxsTransHandler::messageContainer msg;
	while (_gxsTrans->popMessage(msg)) {
		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "pollMessages: reading message ...");
		processMsg(msg);
	}
#else
	std::list<Rs::Msgs::MsgInfoSummary> msgs;
	rsMsgs->getMessageSummaries(msgs);

	std::vector<std::string> toDelete;
	for (auto msg = msgs.begin(); msg != msgs.end(); ++msg) {
		// only read new ones
		if (!(msg->msgflags & (RS_MSG_NEW | RS_MSG_UNREAD_BY_USER)))
			continue;

		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "pollMessages: reading message (id: " + msg->msgId + " title: " + msg->title + ")");
		if (msg->title.compare(0, SF_MSG_TITLE.length(), SF_MSG_TITLE))
			continue;

		// mark read to ensure that it gets only processed once (when the title matches)
		rsMsgs->MessageRead(msg->msgId, false);

		if (processMsg(msg->msgId))
			toDelete.push_back(msg->msgId);
	}

#ifndef DEBUG_DELETE
	for (auto it = toDelete.begin(); it != toDelete.end(); ++it)
		rsMsgs->MessageDelete(*it);
#endif
#endif
}

#ifdef USE_GXSTRANS
bool p3SyncFriends::processMsg(const gxsTransHandler::messageContainer &msg)
#else
bool p3SyncFriends::processMsg(const std::string &id)
#endif
{
#ifdef USE_GXSTRANS
	RsGxsId gxsIdSender = msg.from;
#else
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: processing msg id: " + id);
	Rs::Msgs::MessageInfo infoIn;
	rsMsgs->getMessage(id, infoIn);

	RsGxsId gxsIdSender = infoIn.rsgxsid_srcId;
#endif

	// temporary local task
	syncFriends::ticket localTicket;

#ifdef USE_GXSTRANS
	// abuse t.fromId for storing the sender ID
	localTicket.fromId = msg.to;
#else
	// find our id (abuse t.fromId for storing the ID)
	// technically this should be impossible to receive a message where we don't know the recevier because we are the receiver ... nevertheless just in case
	for (auto it = infoIn.rsgxsid_msgto.begin(); it != infoIn.rsgxsid_msgto.end(); ++it) {
		if (rsIdentity->isOwnId(*it)) {
			localTicket.fromId = *it;
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: found gxs id: " + localTicket.fromId.toStdString());
		}
	}
#endif

	// start parsing	
#ifdef USE_GXSTRANS
	const syncFriends::message &message = msg.message;
#else
	syncFriends::message message;

	RsGenericSerializer::SerializeContext cReq;
	RsJson& jReq(cReq.mJson);
	jReq.Parse(infoIn.msg.c_str(), infoIn.msg.size());
	RsTypeSerializer::serial_process(RsGenericSerializer::FROM_JSON, cReq, message, SF_ROOT_ITEM_NAME);

	// early check
	if (!cReq.mOk) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "processMsg: failed to parse json");
		return false;
	}
#endif

	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: controls (groups:" + std::to_string(message.control.includeGroups) + " removefriends:" + std::to_string(message.control.removeFriends) + " request:" + std::to_string(message.control.requestFriendList) + ") result " + resultToString(message.result));

	localTicket.control = message.control;
	localTicket.id = message.id;

	// check for result
	bool hasResult = message.result != syncFriends::results::unset;

	// before doing anything check if any action is allowed
	if (!checkPermission(gxsIdSender, hasResult, localTicket.id)) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "processMsg: permission check failed (from " + localTicket.fromId.toStdString() + " with " + (hasResult ? "" : "no") + " result and ticket id " + std::to_string(localTicket.id) + ")");
		return false;
	}

	// only process when active (or has result)
	if (!_config.active && !hasResult) {
		// bail out early
		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: not active -> bailing out");		

		// localTicket.fromId is set to the ID that received the message and thus the new sender ID
		localTicket.targetIds.push_back(gxsIdSender);
		sendMessage(localTicket, true, syncFriends::results::disabled);
		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: sent ack");

		return true;
	}

	// check for self-messaging
	// When a message has no result we must answer it and not process any possible related ticket.
	// (When syncing with the own node, one will hit this case where there is a message without a result but with a matching ticket.)
	if (hasResult) {
		// search ticket
		for (auto it = _tickets.begin(); it != _tickets.end(); ++it) {
			if (it->id == localTicket.id) {
				rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: found ticket");

				syncFriends::ticket &ticket = *it;
				struct syncFriends::ticket::answers a;

				// found ticket, store msg for later processing
				a.answer = message.vfl;
				a.from = gxsIdSender;
				ticket.answers.push_back(a);
				if (ticket.state == syncFriends::ticket::states::sent)
					ticket.state = syncFriends::ticket::states::receivedAnswer;

				return true;
			}
		}
	}

	// Catch orphaned message
	// When an acknowledgement is receivied but no ticket is found we have to drop it and not answer it!
	if (hasResult) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "processMsg: found orphaned message");
		return true;
	}

	// overwrite removeFriends
	localTicket.control.removeFriends &= _config.removeFriendsActive;

	// did we found our ID?
	bool success = !localTicket.fromId.isNull();

	// process - can be empty
	if (!message.vfl.peers.empty())
		success &= syncFriends::vflToRs(message.vfl, localTicket.control.removeFriends);

	// send acknowledgement
	// localTicket.fromId is set to the ID that received the message and thus the new sender ID
	localTicket.targetIds.push_back(gxsIdSender);
	sendMessage(localTicket, true, success ? syncFriends::results::ok : syncFriends::results::error);
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processMsg: sent ack");

	return true;
}

/**
 * @brief p3SyncFriends::checkPermission check if a reuqest is allowed to be executed.
 * @param sender
 * @param hasResult
 * @param ticketId
 * @return
 * There are two positive scenarios:
 * 1) The sender is issuing a request and is in the white list, so we will do what he wants (as long as enabled in the gui, e.g. delete friends).
 * 2) The sender is responding to a request we issued before, he might not be in the white list but we must process the answer if the sender was targetted by the matching ticket.
 * In all other cases the permission check fails.
 */
bool p3SyncFriends::checkPermission(const RsGxsId &sender, const bool hasResult, const uint32_t ticketId)
{
	syncFriends::ticket t;
	bool foundTicket = false;
	bool whiteList = false;

	if (sender.isNull()) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "checkPermission: failed, message has no sender");
		return false;
	}

	// white list check
	for (auto id = _config.acceptFrom.begin(); id != _config.acceptFrom.end(); ++id)
		if (sender == *id)
			whiteList = true;
	if (!whiteList)
		rslog(RsLog::Warning, &syncFriendsLogInfo, "checkPermission: sender is not white listed (from: " + sender.toStdString() + ")");

	// fast check
	if (whiteList) {
		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "checkPermission: pass, sender is in white list");
		return true;
	}

	// when there is no result, it is a request that must be on the white list
	if (!hasResult && !whiteList) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "checkPermission: failed, sender is not in white list but the message is also not a repsonse");
		return false;
	}

	// valid from here on: !whiteList && hasResult
	// we must find the ticket and check the target ids
	for (auto it = _tickets.begin(); it != _tickets.end(); ++it) {
		if (it->id == ticketId) {
			t = *it;
			foundTicket = true;
			break;
		}
	}
	// existing ticket?
	if (!foundTicket) {
		rslog(RsLog::Warning, &syncFriendsLogInfo, "checkPermission: failed, no ticket found (but required)");
		return false;
	}

	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "checkPermission: found ticket");

	for (auto it = t.targetIds.begin(); it != t.targetIds.end(); ++it) {
		if (sender == *it) {
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "checkPermission: pass, sender is in target id list");
			return true;
		}
	}

	rslog(RsLog::Warning, &syncFriendsLogInfo, "checkPermission: failed");
	return false;
}

syncFriends::message p3SyncFriends::composeMessage(const syncFriends::ticket &t)
{
	syncFriends::message msg;

	msg.control = t.control;
	msg.id = t.id;
	msg.result = syncFriends::results::unset;
	if (t.action != syncFriends::actions::request)
		msg.vfl = syncFriends::rsToVfl(t.control.includeGroups);
	else
		msg.vfl.clear();

	return msg;
}

syncFriends::message p3SyncFriends::composeAcknowledgement(const syncFriends::ticket &t, const syncFriends::results result)
{
	syncFriends::message msg;

	msg.control = t.control;
	msg.id = t.id;
	msg.result = result;
	if (_config.active && t.control.requestFriendList)
		msg.vfl = syncFriends::rsToVfl(t.control.includeGroups);
	else
		msg.vfl.clear();

	return msg;
}

void p3SyncFriends::sendMessage(const syncFriends::ticket &ticket, const bool isAck, const syncFriends::results result)
{
	// generatre message
	syncFriends::message msg;
	if (isAck)
		msg = composeAcknowledgement(ticket, result);
	else
		msg = composeMessage(ticket);
#ifdef USE_GXSTRANS
	_gxsTrans->sendMessage(ticket, msg);
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "sendMessage: sent message (ticket id: " + std::to_string(ticket.id) + ")");
#else

	// serialize
	RsGenericSerializer::SerializeContext cMsg;
	RsTypeSerializer::serial_process(RsGenericSerializer::TO_JSON, cMsg, msg, SF_ROOT_ITEM_NAME);

	// convert to std::string
	RsJson& jAns(cMsg.mJson);
	std::stringstream ss;
	ss << jAns;

	Rs::Msgs::MessageInfo info;
	info.rsgxsid_msgto.insert(ticket.targetIds.begin(), ticket.targetIds.end());
	info.rsgxsid_srcId = ticket.fromId;
	info.title = SF_MSG_TITLE;
	info.msg = ss.str();

	rsMsgs->MessageSend(info);
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "sendMessage: sent message (id: " + info.msgId + " ticket id: " + std::to_string(ticket.id) + ")");
#endif
}

void p3SyncFriends::processAnswers(syncFriends::ticket *ticket)
{
	// go through answers
	for (auto answer = ticket->answers.begin(); answer != ticket->answers.end(); ++answer) {
		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processAnswers: processing answer from gxs id " + answer->from.toStdString());

		// search gxs ID
		auto it = ticket->targetIds.begin();
		for (; it != ticket->targetIds.end(); ++it)
			if (*it == answer->from)
				break;

		if (it == ticket->targetIds.end()) {
			// message from an ID that is not in the list --> ignore
			rslog(RsLog::Warning, &syncFriendsLogInfo, "processAnswers: received msg from gxs id not in sendTo list");
			continue;
		}

		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "processAnswers: found gxs id in sendTo list (" + answer->from.toStdString() + ")");
		ticket->targetIds.erase(it);

		// process data (when required
		if (ticket->action != syncFriends::actions::send) {
			syncFriends::vflToRs(answer->answer, ticket->control.removeFriends && _config.removeFriendsActive);
		}
	}

	// clean up
	ticket->answers.clear();
}

uint32_t p3SyncFriends::getNextId()
{
#ifdef DEBUG
	// predictable IDs
	static uint32_t id = RSRandom::random_u32();
	return id++;
#else
	return RSRandom::random_u32();
#endif

}

std::list<syncFriends::ticket> p3SyncFriends::getTickets() const
{
	return _tickets;
}

void p3SyncFriends::doCleanUp(const bool all)
{
	RS_STACK_MUTEX(_mutex);
	_doCleanUp = true;
	_doCleanUpAll = all;
}

syncFriends::config p3SyncFriends::getConfig() const
{
	return _config;
}

void p3SyncFriends::setConfig(const syncFriends::config &config)
{
	_config = config;
	IndicateConfigChanged();
}

