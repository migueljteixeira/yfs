// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#undef max
#include <algorithm>

lock_server::lock_server():nacquire (0) {
	pthread_mutex_init(&globalMutex, NULL);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
	lock_protocol::status ret = lock_protocol::OK;
	printf("stat request from clt %d\n", clt);
	r = nacquire;
	
	return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {
	printf("acquire %llu\n", lid);

	// thread to handle client
	pthread_create(&threads[lid], NULL, acquireHandle, lid);

	return lock_protocol::OK;
}

void lock_server::acquireHandle(lock_protocol::lockid_t lid) {
	pthread_mutex_lock (&globalMutex);

	// client_id does not exist	
	if( clients.find(lid) == clients.end() ) {
		printf("acquireHandle if %llu\n", lid);

		// create new client entry
		lock_server::client *new_client;
		new_client->status = client::FREE;

		pthread_mutex_t mutex;
		pthread_mutex_init(&mutex, NULL);
		new_client->mutex = &mutex;

		// insert into map
		//clients.insert ( std::pair<lock_protocol::lockid_t, lock_server::client*>(lid, &new_client) );
	}

	/*while( clients.find(id)->second->status == client::LOCKED ) {
		
	}*/

	pthread_mutex_unlock (&globalMutex);

	pthread_mutex_t *clientMutex = clients.find(lid)->second->mutex;

	// lock clientID
	assert( pthread_mutex_lock (clientMutex) == 0 );
	printf("locked! %llu\n", lid);
}




lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
	pthread_mutex_t *clientMutex = clients.find(lid)->second->mutex;

	// unlock clientID
	assert( pthread_mutex_unlock (clientMutex) == 0);
	printf("unlocked! %llu\n", lid);
	
	return lock_protocol::OK;
}

