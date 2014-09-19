// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <vector>
#define NUM_THREADS 10

class lock_server {

	protected:
		int nacquire;
		lock_protocol::lockid_t clientID;
		
		pthread_t threads[NUM_THREADS];
		pthread_mutex_t globalMutex;

		struct client {
			enum lock_status { LOCKED, FREE };
			lock_status status;
			pthread_mutex_t* mutex;
		};

		std::map<lock_protocol::lockid_t, lock_server::client*> clients;

	public:
		lock_server();
		~lock_server() {};
		
		lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

	private:
		void acquireHandle(lock_protocol::lockid_t lid);
};

#endif 







