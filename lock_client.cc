// RPC stubs for clients to talk to lock_server

#include "lock_client.h"
#include <arpa/inet.h>

#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>

lock_client::lock_client(std::string dst)
{
	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);

	cl = new rsm_client(dst.c_str());
}

int lock_client::stat(lock_protocol::lockid_t lid) {
	int r;
	int ret = cl->call(lock_protocol::stat, cl->id, lid, r);

	assert (ret == lock_protocol::OK);
	
	return r;
}

lock_protocol::status lock_client::acquire(lock_protocol::lockid_t lid) {
	int r;
	int ret = cl->call(lock_protocol::acquire, cl->id, lid, r);
	
	while(ret == lock_protocol::RETRY) {
		printf("lock_client::acquire: busy\n");
		
		usleep(50 * 1000); // 50 ms
		ret = cl->call(lock_protocol::acquire, cl->id, lid, r);
	}

	return ret;
}

lock_protocol::status lock_client::release(lock_protocol::lockid_t lid) {
	int r;
	int ret = cl->call(lock_protocol::release, cl->id, lid, r);
	
	return ret;
}

