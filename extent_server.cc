#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
	printf("Creating root directory\n");
	int r;
	if(put(0x00000001, 0, "", false, r) != extent_protocol::OK) {
		printf("Couldn't create root directory\n");
		exit(0);
	}
}


int extent_server::put(extent_protocol::extentid_t id, int offset, std::string file, int update, int &r)
{
	extent_t ex;
	
	// check if the element already exists
	if(extent_map.count(id) > 0 && update)
		ex = extent_map[id];
	// element exists but we are trying to create it again
	else if(extent_map.count(id) > 0 && !update)
		return extent_protocol::IOERR;	

	// if the offset is negative we will replace all content
	if(offset < 0)
		ex.buf = file;

	// if the offset is bigger than the string itself, we need to resize it
	else if((unsigned)offset >= ex.buf.size()) {
		std::string str = std::string(offset + file.size(), '\0');
        str.replace(0, ex.buf.size(), ex.buf);
		str.replace(offset, file.size(), file);
        ex.buf = str;
	}
	else
		ex.buf.replace(offset, file.size(), file);

	// initialize extent
	ex.attr.size = ex.buf.size();
	if(!update) ex.attr.atime = time(NULL);
	ex.attr.mtime = time(NULL);
	ex.attr.ctime = time(NULL);

	// store extent in extent_map
	extent_map[id] = ex;

	return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// remove extent
	extent_map.erase(id);

	return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, unsigned int offset, unsigned int len, std::string &file)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	// if its a file, we truncate the string if needed
	if(len > 0) {
		if(offset >= extent_map[id].attr.size)
			file = "";
		else {
			file = extent_map[id].buf.substr(offset, len);
		}
	}
	// its a directory
	else {
		file = extent_map[id].buf;
	}

	// update access time with relatime
	// only update atime if it's lower than
	// mtime, ctime or older than 24h
	unsigned int current_time = time(NULL);
	if(extent_map[id].attr.atime < extent_map[id].attr.mtime
			|| extent_map[id].attr.atime < extent_map[id].attr.ctime
			|| extent_map[id].attr.atime < (current_time - 24*60*60)) {
		extent_map[id].attr.atime = current_time;
	}

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

int extent_server::setattr(extent_protocol::extentid_t id, extent_protocol::attr a, int &)
{
	// check if extent exists
	if(extent_map.find(id) == extent_map.end()) {
		return extent_protocol::NOENT;
	}

	unsigned int old_size = extent_map[id].attr.size;
	unsigned int new_size = a.size;
	
	// if file size changed, we have to adapt the string
	if(old_size > new_size) {
		extent_map[id].buf = extent_map[id].buf.substr(0, new_size);
	}
	else if(old_size < new_size) {
		extent_map[id].buf.resize(new_size);
	}

	// set extent attributes
	extent_map[id].attr = a;

	// update mtime
	extent_map[id].attr.mtime = time(NULL);

	return extent_protocol::OK;
}

