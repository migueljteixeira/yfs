// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
	cl = new rpcc(dstsock);
	if (cl->bind() != 0) {
		printf("extent_client: bind failed\n");
	}
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, unsigned int offset, unsigned int len, std::string &file)
{
	extent_protocol::status ret = extent_protocol::OK;
	ret = cl->call(extent_protocol::get, eid, offset, len, file);
	return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, int offset, std::string buf, int update)
{
	extent_protocol::status ret = extent_protocol::OK;
	int r;
	ret = cl->call(extent_protocol::put, eid, offset, buf, update, r);
	return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
	extent_protocol::status ret = extent_protocol::OK;
	int r;
	ret = cl->call(extent_protocol::remove, eid, r);
	return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &a)
{
	extent_protocol::status ret = extent_protocol::OK;
	ret = cl->call(extent_protocol::getattr, eid, a);
	return ret;
}

extent_protocol::status
extent_client::setattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr a)
{
	extent_protocol::status ret = extent_protocol::OK;
	int r;
	ret = cl->call(extent_protocol::setattr, eid, a, r);
	return ret;
}

