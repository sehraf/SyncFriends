#include "gxsTransHandler.h"

//#include <serialiser/rsserializer.h>

static const std::string SF_ROOT_ITEM_NAME	= "SyncFriends";

// echo 'SyncFriends' | xxd -p | head -c 4
static const uint16_t serviceId = 0x5379;

gxsTransHandler::gxsTransHandler() :
    _mutex("gxsTransHanlder")
{
	dynamic_cast<p3GxsTrans*>(rsGxsTrans)->registerGxsTransClient(GxsTransSubServices::UNKNOWN, dynamic_cast<GxsTransClient*>(this));
}

bool gxsTransHandler::popMessage(gxsTransHandler::messageContainer &msg)
{
	if (_messages.empty())
		return false;

	{
		RS_STACK_MUTEX(_mutex);
		msg = _messages.front();
		_messages.pop();
	}

	return true;
}

void gxsTransHandler::sendMessage(const syncFriends::ticket &ticket, const syncFriends::message &msg)
{
	RsGxsTransId id;

	RsGenericSerializer::SerializeContext ctx;

	// estimate size and allocate space
	RsTypeSerializer::serial_process(RsGenericSerializer::SIZE_ESTIMATE, ctx, const_cast<syncFriends::message&>(msg), SF_ROOT_ITEM_NAME);
	ctx.mData = static_cast<uint8_t*>(rs_malloc(ctx.mOffset));
	ctx.mSize = ctx.mOffset;
	ctx.mOffset = 0;

	RsTypeSerializer::serial_process(RsGenericSerializer::SERIALIZE, ctx, const_cast<syncFriends::message&>(msg), SF_ROOT_ITEM_NAME);

	for (auto to = ticket.targetIds.begin(); to != ticket.targetIds.end(); ++to)
		dynamic_cast<p3GxsTrans*>(rsGxsTrans)->sendData(id, GxsTransSubServices::UNKNOWN, ticket.fromId, *to, ctx.mData, ctx.mSize);
}

bool gxsTransHandler::receiveGxsTransMail(const RsGxsId &authorId, const RsGxsId &recipientId, const uint8_t *data, uint32_t dataSize)
{
	gxsTransHandler::messageContainer m;

	m.from = authorId;
	m.to = recipientId;

	// deserialize payload
	syncFriends::message message;
	RsGenericSerializer::SerializeContext ctx;
	ctx.mData = const_cast<unsigned char *>(data);
	ctx.mSize = dataSize;
	RsTypeSerializer::serial_process(RsGenericSerializer::DESERIALIZE, ctx, message, SF_ROOT_ITEM_NAME);

	if (ctx.mOk) {
		m.message = message;

		{
			RS_STACK_MUTEX(_mutex);
			_messages.push(m);
		}

		return true;
	}

	return false;
}

bool gxsTransHandler::notifyGxsTransSendStatus(RsGxsTransId mailId, GxsTransSendStatus status)
{
	(void) mailId;
	(void) status;
	return true;
}
