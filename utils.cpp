#include "utils.h"

#include <sstream>

#include <retroshare-gui/RsAutoUpdatePage.h>
#include <gui/RetroShareLink.h>

static const std::string SF_CONFIG_TICKET_ID		= "id";
static const std::string SF_CONFIG_TICKET_STATE		= "state";
static const std::string SF_CONFIG_TICKET_ACTION	= "action";
static const std::string SF_CONFIG_TICKET_FROM_ID	= "fromId";
static const std::string SF_CONFIG_TICKET_TARGET_ID	= "targetId";

static const std::string SF_CONTROLS				= "controls";
static const std::string SF_CONTROLS_GROUPS			= "groups";
static const std::string SF_CONTROLS_REMOVE_FRIEDNS	= "removeFriends";
static const std::string SF_CONTROLS_RERQUEST		= "request";

static const std::string SF_CONFIG_ROOT_ITEM_NAME	= "ticket";

namespace syncFriends {

std::string actionToString(const syncFriends::actions a)
{
	switch (a) {
	case syncFriends::actions::synchronize:
		return "synchronize";
	case syncFriends::actions::send:
		return "send";
	case syncFriends::actions::request:
		return "request";
	}
	return "invalid";
}

std::string stateToString(const syncFriends::ticket::states s)
{
	switch (s) {
	case syncFriends::ticket::states::init:
		return "init";
	case syncFriends::ticket::states::sending:
		return "sending";
	case syncFriends::ticket::states::sent:
		return "sent";
	case syncFriends::ticket::states::receivedAnswer:
		return "receivedAnswer";
	case syncFriends::ticket::states::finish:
		return "finish";
	}
	return "invalid";
}

std::string resultToString(const syncFriends::results r)
{
	switch (r) {
	case syncFriends::results::unset:
		return "unset";
	case syncFriends::results::ok:
		return "ok";
	case syncFriends::results::disabled:
		return "disabled";
	case syncFriends::results::error:
		return "error";
	}
	return "invalid";
}

std::string ticketToString(const syncFriends::ticket &t)
{
	RsGenericSerializer::SerializeContext cTicket;	
	RsTypeSerializer::serial_process(RsGenericSerializer::TO_JSON, cTicket, const_cast<syncFriends::ticket&>(t), SF_CONFIG_ROOT_ITEM_NAME);

	RsJson &jTicket(cTicket.mJson);
	std::stringstream ss;
	ss << jTicket;

	return ss.str();
}

bool stringToTicket(const std::string &s, syncFriends::ticket &t)
{
	RsGenericSerializer::SerializeContext cTicket;

	RsJson &jTicket(cTicket.mJson);
	jTicket.Parse(s.c_str(), s.size());
	RsTypeSerializer::serial_process(RsGenericSerializer::FROM_JSON, cTicket, t, SF_CONFIG_ROOT_ITEM_NAME);

	return cTicket.mOk;
}


/*
 * copied from retroshare-gui/src/gui/common/FriendList.cpp
 */

bool getGroupIdByName(const std::string &name, RsNodeGroupId &id)
{
	std::list<RsGroupInfo> grpList;
	if(!rsPeers->getGroupInfoList(grpList))
		return false;

	for (auto grp = grpList.begin(); grp != grpList.end(); ++grp) {
		if(grp->name == name) {
			id = grp->id;
			return true;
		}
	}

	return false;
}

bool getOrCreateGroup(const std::string &name, const uint &flag, RsNodeGroupId &id)
{
	if(getGroupIdByName(name, id))
		return true;

	// -> create one
	RsGroupInfo grp;
	grp.id.clear(); // RS will generate an ID
	grp.name = name;
	grp.flag = flag;

	if(!rsPeers->addGroup(grp))
		return false;

	// try again
	return getGroupIdByName(name, id);
}

syncFriends::virtualFriendList rsToVfl(const bool includeGroups)
{
	syncFriends::virtualFriendList vfl;
	vfl.clear();

	std::list<RsPgpId> gpgIDs;
	rsPeers->getGPGAcceptedList(gpgIDs);
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::rsToVfl(): found " + std::to_string(gpgIDs.size()) + " PGP IDs");

	RsPeerDetails detailPGP;
	for(auto gpgID = gpgIDs.begin(); gpgID !=  gpgIDs.end(); gpgID++)	{
		syncFriends::friendEntry fe;

		rsPeers->getGPGDetails(*gpgID, detailPGP);
		fe.name = detailPGP.name;

		std::list<RsPeerId> sslIDs;
		if (!rsPeers->getAssociatedSSLIds(*gpgID, sslIDs)) {
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::rsToVfl(): no associated locations found -> skipping (" + gpgID->toStdString() + ")");
			continue;
		}

		for(auto sslID = sslIDs.begin(); sslID !=  sslIDs.end(); sslID++) {
			syncFriends::location l;

			RsPeerDetails detailSSL;
			if (!rsPeers->getPeerDetails(*sslID, detailSSL))
				continue;

			std::string certificate = rsPeers->GetRetroshareInvite(detailSSL.id, true);
			// remove \n from certificate
			certificate.erase(std::remove(certificate.begin(), certificate.end(), '\n'), certificate.end());

			l.cert = certificate;
			l.name = detailSSL.location;
			l.flags = detailSSL.service_perm_flags;
			l.id = detailSSL.id;

			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::rsToVfl(): adding location '" + l.name + "' (" + l.id.toStdString() + ")");
			fe.locations.push_back(l);
		}

		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::rsToVfl(): adding peer '" + fe.name + "' with " + std::to_string(fe.locations.size()) + " locations");
		vfl.peers[detailPGP.gpg_id] = fe;
	}

	if (!includeGroups)
		return vfl;

	std::list<RsGroupInfo> groups;
	rsPeers->getGroupInfoList(groups);
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::rsToVfl(): found " + std::to_string(groups.size()) + " groups IDs");

	for(std::list<RsGroupInfo>::iterator group = groups.begin(); group !=  groups.end(); group++)	{
		// skip groups without peers
		if(group->peerIds.empty())
			continue;

		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::rsToVfl(): adding group '" + group->name + "' with " + std::to_string(group->peerIds.size()) + " peers");
		vfl.groups.push_back(*group);
	}

	return vfl;
}

bool addVflToRs(const virtualFriendList &vfl)
{
	bool errorPeers = false;
	bool errorGroups = false;

	std::string error_string;

	RsPeerDetails rsPeerDetails;
	RsPeerId rsPeerID;
	RsPgpId rsPgpID;

	RsNodeGroupId groupId;

	// lock all events for faster processing
	RsAutoUpdatePage::lockAllEvents();

	// pgp IDs
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::addVflToRs(): found " + std::to_string(vfl.peers.size()) + " PGP IDs");

	// for each pgpID
	for(auto pgpID = vfl.peers.begin(); pgpID != vfl.peers.end(); ++pgpID) {
		// no location known, just try to accept the ID when known
		if (pgpID->second.locations.empty()) {
			rsPeerID.clear();
			rsPeers->addFriend(rsPeerID, pgpID->first);
			continue;
		}

		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::addVflToRs(): found " + std::to_string(pgpID->second.locations.size()) + " location(s)");

		// for each sslID
		for(auto location = pgpID->second.locations.begin(); location != pgpID->second.locations.end(); ++location) {
			rsPeerID.clear();
			rsPgpID.clear();

			// load everything needed from the pubkey string
			if (location->cert.empty())
				continue;
#ifndef DEBUG_DRY_RUN
			rsPeers->acceptInvite(location->cert, location->flags);
#endif
		}
	}

	// groups
	rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::addVflToRs(): found " + std::to_string(vfl.groups.size()) + " group(s)");

	// for each group
	for(auto group = vfl.groups.begin(); group != vfl.groups.end(); ++group) {
		// check name
		if (group->name.empty() || group->peerIds.empty())
			continue;

		rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::addVflToRs(): found " + std::to_string(group->peerIds.size()) + " PGP IDs for group '" + group->name + "'");

#ifndef DEBUG_DRY_RUN
		if(getOrCreateGroup(group->name, group->flag, groupId)) {
			// group id found!

			for(auto pgpID = group->peerIds.begin(); pgpID != group->peerIds.end(); ++pgpID) {
				// add pgp id to group
				rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::addVflToRs(): adding " + pgpID->toStdString() + " to groups");

				if(pgpID->isNull() || !rsPeers->assignPeerToGroup(groupId, *pgpID, true)) {
					errorGroups = true;
					std::cerr << "SyncFriends::vflToRs(): failed to add '" << rsPeers->getGPGName(*pgpID) << "'' to group '" << group->name << "'" << std::endl;
					rslog(RsLog::Warning, &syncFriendsLogInfo, "syncFriends::addVflToRs(): failed to add '" + rsPeers->getGPGName(*pgpID) + "'' to group '" + group->name + "'");
				}
			}
		} else {
			errorGroups = true;
			rslog(RsLog::Warning, &syncFriendsLogInfo, "syncFriends::addVflToRs(): failed to find/create group '" + group->name + "'");
		}
#endif
	}

	// unlock events
	RsAutoUpdatePage::unlockAllEvents();

	return !(errorPeers || errorGroups);
}

void removeNotInVfl(const virtualFriendList &vfl)
{
	std::list<RsPeerId> peerList;
	rsPeers->getFriendList(peerList);

	for (auto sslId = peerList.begin(); sslId != peerList.end(); ++sslId) {
		RsPeerDetails details;
		rsPeers->getPeerDetails(*sslId, details);
		RsPgpId &pgpId = details.gpg_id;

		if (vfl.peers.find(pgpId) == vfl.peers.end()) {
			rslog(RsLog::Debug_Basic, &syncFriendsLogInfo, "syncFriends::removeNotInVfl(): peer '" + details.name + "' not found in virtualFriendList, deleting");
			rsPeers->removeFriend(pgpId);
		}
	}
}

bool vflToRs(const virtualFriendList &vfl, bool remove)
{
	bool ret = addVflToRs(vfl);

	if (remove)
		removeNotInVfl(vfl);

	return ret;
}

}
