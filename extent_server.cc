// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
	extent_t ex;

	// initialize extent
	ex.buf = buf;
	ex.attr.size = buf.length();
	ex.attr.atime = time(NULL);
	ex.attr.mtime = time(NULL);
	ex.attr.ctime = time(NULL);

	// store extent in extent_map
	extent_map[id] = ex;
printf("extent: %llu\n", id);

	return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// get extent
	extent_t *ex = &extent_map[id];

	// update access time with relatime
	// only update atime if it's lower than
	// mtime, ctime or older than 24h
	unsigned int current_time = time(NULL);
	if(ex->attr.atime < ex->attr.mtime
			|| ex->attr.atime < ex->attr.ctime
			|| ex->attr.atime < (current_time - 24*60*60)) {
		ex->attr.atime = current_time;
	}

	// get buf
	buf = ex->buf;

	return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// get extent attributes
	a = extent_map[id].attr;

	return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// delete extent
	extent_map.erase(id);

	return extent_protocol::OK;
}

