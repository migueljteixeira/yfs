// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <vector>
#include <stdio.h>

class lock_server {

	protected:
		int nacquire;
		lock_protocol::lockid_t clientID;
		
		pthread_mutex_t global_mutex;

		struct lockid_info {
			enum lock_status { LOCKED, FREE } status;
			pthread_mutex_t *mutex;
			pthread_cond_t *wait;
		};

		std::map<lock_protocol::lockid_t, lock_server::lockid_info*> locks;

	public:
		lock_server();
		~lock_server() {};
		
		lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

};

#endif
