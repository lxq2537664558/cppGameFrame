
#include <mysdk/protocol/kabu/codec/KaBuCodec.h>

#include <mysdk/base/Logging.h>

static uint16 GetHeight16(const int32 t)
{
	return static_cast<uint16>((t >> 16) & 0x0000FFFF);
}

static uint16 GetLow16(const int32 t)
{
	return static_cast<uint16>(t & 0x0000FFFF);
}


#define PACKET_MAGIC_D ('S' << 8 | 'D') //'DS'
#define PACKET_MAGIC_C ('S' << 8 | 'C') //'CS'

void KaBuCodec::onMessage(mysdk::net::TcpConnection* pCon,
									mysdk::net::Buffer* buf,
									mysdk::Timestamp receiveTime)
{
    LOG_TRACE << "KaBuCodec::onMessage - bytes: " << buf->readableBytes();
	while (buf->readableBytes() >= kMinMessageLen + kHeaderLen)
	{
		int32 tmp = buf->peekInt32();
		uint16 magic = GetLow16(tmp);
		uint16 len = GetHeight16(tmp);
		if (magic == PACKET_MAGIC_C)
		{
			errorCallback_(pCon, buf, receiveTime, kUnSupportError);
		}
		else if (len > kMaxMessageLen)
		{
			errorCallback_(pCon, buf, receiveTime, kInvalidLength);
		}
		else if (buf->readableBytes() >= static_cast<size_t>(len + kHeaderLen))
		{
			magic = buf->readInt16();
			len = buf->readInt16();
			uint32 op = buf->readInt32();
			uint32 param = buf->readInt32();

			char msgBuf[512];
			snprintf(msgBuf, sizeof(msgBuf), "len: %d, param: %d, hexop: 0x%X", len, param, op);
			LOG_TRACE << "KaBuCodec::onMessage - msgBuf[ " << msgBuf << " ]";

			PacketBase pb(magic, len, op, param, buf->peek(), len);
		    messageCallback_(pCon, pb, receiveTime);
		    buf->retrieve(len);
		}
		else
		{
			break;
		}
	}
}

void KaBuCodec::send(mysdk::net::TcpConnection* pCon, PacketBase& pb)
{
	if (!pCon) return;

	// 这个地方 等有时间要在回来优化哦
#if 1
	if (pb.prependableBytes() >= kHeaderLen)
	{
		int16 len = pb.getContentLen();
		int32 op = pb.getOP();
		int32 param = pb.getParam();

		Buffer buf;
		buf.appendInt16(PACKET_MAGIC_D);
		buf.appendInt16(len);
		buf.appendInt32(op);
		buf.appendInt32(param);
		pb.prepend(buf);
	}
#else
	char buf[16];
	int32 tmp = ( (len << 16) & PACKET_MAGIC_D); //(PACKET_MAGIC_D << 16 & len);
	int32 headlen = snprintf(buf, sizeof(buf) - 1, "%d%d%d", tmp, op, param);
	pb.prepend(buf, headlen);
#endif

	pCon->send((pb.getBufferContent()));
}

namespace
{
  const string kNoErrorStr = "NoError";
  const string kInvalidLengthStr = "InvalidLength";
  const string kParseErrorStr = "ParseError";
  const string kUnknownErrorStr = "UnknownError";
  const string kUnSupportErrorStr = "UnSupportError";
}

const string& KaBuCodec::errorCodeToString(ErrorCode errorCode)
{
  switch (errorCode)
  {
   case kNoError:
	   return kNoErrorStr;
   case kInvalidLength:
	   return kInvalidLengthStr;
   case kParseError:
	   return kParseErrorStr;
   case kUnSupportError:
	   return kUnSupportErrorStr;
   default:
	   return kUnknownErrorStr;
  }
}

void KaBuCodec::defaultErrorCallback(mysdk::net::TcpConnection* pCon,
															mysdk::net::Buffer* buf,
															mysdk::Timestamp timestamp,
															ErrorCode errorCode)
{
	  LOG_ERROR << "KaBuCodec::defaultErrorCallback - " << errorCodeToString(errorCode);
	  if (pCon && pCon->connected())
	  {
		  pCon->shutdown();
	  }
}
