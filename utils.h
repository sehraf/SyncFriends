#ifndef UTILS_H
#define UTILS_H

#include <map>
#include <vector>

#include <serialiser/rsserializer.h>
#include <retroshare/rspeers.h>
#include <util/rsdebug.h>

#ifdef DEBUG
static struct RsLog::logInfo syncFriendsLogInfo = {RsLog::Debug_All, "SyncFriends"};
#else
static struct RsLog::logInfo syncFriendsLogInfo = {RsLog::Default, "SyncFriends"};
#endif

namespace syncFriends {

/*******************************************
 * virtualFriendList stuff
 *******************************************/
struct location : RsSerializable {
	std::string cert;
	std::string	name;
	RsPeerId	id;
	ServicePermissionFlags	flags;

	// RsSerializable interface
public:
	void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
		RS_SERIAL_PROCESS(cert);
		RS_SERIAL_PROCESS(name);
		RS_SERIAL_PROCESS(id);
		RS_SERIAL_PROCESS(flags);
	}
};

struct friendEntry : RsSerializable {
	std::string				name;
	std::vector<location>	locations;

	// RsSerializable interface
public:
	void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
		RS_SERIAL_PROCESS(name);
		RS_SERIAL_PROCESS(locations);
	}
};

struct virtualFriendList : RsSerializable {
	std::map<RsPgpId, friendEntry>	peers;
	std::vector<RsGroupInfo>		groups;

	void clear() {
		peers.clear();
		groups.clear();
	}

	// RsSerializable interface
public:
	void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
		RS_SERIAL_PROCESS(peers);
		RS_SERIAL_PROCESS(groups);
	}
};

/*******************************************
 * p3SyncFriends stuff
 *******************************************/
enum class actions : uint8_t {
	synchronize = 0,
	send,
	request
};

enum class results : uint8_t {
	unset = 0,
	ok,
	error,
	disabled
};

struct controls : RsSerializable {
	/**
	 * @brief request send peer list back
	 */
	bool	requestFriendList;

	/**
	 * @brief addFriends add peers that are in the supplied list
	 */
	bool	addFriends;

	/**
	 * @brief removeFriends remove peers that are not in the supplied list
	 */
	bool	removeFriends;

	/**
	 * @brief groups inlcude groups when sending the list back
	 */
	bool	includeGroups;

	// RsSerializable interface
public:
	void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
		RS_SERIAL_PROCESS(requestFriendList);
		RS_SERIAL_PROCESS(addFriends);
		RS_SERIAL_PROCESS(removeFriends);
		RS_SERIAL_PROCESS(includeGroups);
	}
};

struct config {
	bool					loaded;
	RsGxsId					sendFrom;
	std::vector<RsGxsId>	sendTo;
	std::vector<RsGxsId>	acceptFrom;
	bool					includeGroups;
	bool					removeFriendsRequest;	// used to signal the other node that it should remove friends
	bool					removeFriendsActive;	// used to allow the own node to remove friends (when requested)
	bool					active;
};

struct ticket : RsSerializable {
	enum class states : uint8_t {
		init = 0,
		sending,
		sent,
		receivedAnswer,
		finish
	};

	struct answers : RsSerializable {
		virtualFriendList	answer;
		RsGxsId				from;

		// RsSerializable interface
	public:
		void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
			RS_SERIAL_PROCESS(answer);
			RS_SERIAL_PROCESS(from);
		}
	};

	uint32_t	id;
	actions		action;

	RsGxsId					fromId;
	std::vector<RsGxsId>	targetIds;
	std::vector<answers>	answers;

	states				state;
	controls			control;

	// RsSerializable interface
public:
	void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
		RS_SERIAL_PROCESS(id);
		RS_SERIAL_PROCESS(action);
		RS_SERIAL_PROCESS(fromId);
		RS_SERIAL_PROCESS(targetIds);
		RS_SERIAL_PROCESS(answers);
		RS_SERIAL_PROCESS(state);
		RS_SERIAL_PROCESS(control);
	}
};

struct message : RsSerializable {
	// header
	uint32_t	id;
	controls	control;
	results		result;

	// paylod
	virtualFriendList	vfl;

	// RsSerializable interface
public:
	void serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext &ctx) {
		RS_SERIAL_PROCESS(id);
		RS_SERIAL_PROCESS(control);
		RS_SERIAL_PROCESS(result);
		RS_SERIAL_PROCESS(vfl);
	}
};

/*******************************************
 * functions
 *******************************************/

virtualFriendList	rsToVfl(const bool includeGroups);
bool				vflToRs(const virtualFriendList &vfl, bool remove);

std::string actionToString(const syncFriends::actions a);
std::string stateToString(const syncFriends::ticket::states s);
std::string resultToString(const syncFriends::results r);

std::string ticketToString(const syncFriends::ticket &t);
bool		stringToTicket(const std::string &s, syncFriends::ticket &t);

}

#endif // UTILS_H
