// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"

class extent_client {
	private:
		rpcc *cl;

	public:
		extent_client(std::string dst);

		extent_protocol::status get(extent_protocol::extentid_t eid, unsigned int, unsigned int, std::string &file);
		extent_protocol::status put(extent_protocol::extentid_t eid, int, std::string buf, int update);
		extent_protocol::status remove(extent_protocol::extentid_t eid);
		extent_protocol::status getattr(extent_protocol::extentid_t eid, extent_protocol::attr &);
		extent_protocol::status setattr(extent_protocol::extentid_t eid, extent_protocol::attr);
};

#endif 

