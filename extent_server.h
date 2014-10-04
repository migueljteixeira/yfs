// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

struct extent_t {
	std::string buf;
	extent_protocol::attr attr;
};

class extent_server {

	private:
		std::map<extent_protocol::extentid_t, extent_t> extent_map;

	public:
		extent_server();

		int put(extent_protocol::extentid_t id, int offset, std::string, int &);
		int get(extent_protocol::extentid_t id, std::string &);
		int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
		int remove(extent_protocol::extentid_t id, int &);
};

#endif 







