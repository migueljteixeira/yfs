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
	extent_t *ex;

	// if extent exists we don't need to allocate new space
	if(extent_map.find(id) == extent_map.end())
		ex = (extent_t*)malloc(sizeof(extent_t));
	else
		ex = extent_map[id];

	// initialize extent
	ex->buf = buf;
	ex->attr.atime = time(NULL);
	ex->attr.mtime = time(NULL);
	ex->attr.ctime = time(NULL);
	ex->attr.size = buf.length();

	// store extent in extent_map
	extent_map[id] = ex;
	
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// get extent
	extent_t *ex = extent_map[id];

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
	a = extent_map[id]->attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// hold extent pointer
	extent_t *ex = extent_map[id];

	// delete extent
	extent_map.erase(id);

	// free memory
	free(ex);

  return extent_protocol::OK;
}

