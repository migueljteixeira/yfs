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
	pthread_mutex_init(&global_mutex, NULL);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
	lock_protocol::status ret = lock_protocol::OK;
	printf("stat request from clt %d\n", clt);
	r = nacquire;
	
	return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {
	printf("%d acquire %llu\n", clt, lid);

	// lockid does not exist, we have to create a new one
	pthread_mutex_lock( &global_mutex );
	if( locks.find(lid) == locks.end() ) {
		// create new lockid entry
		lock_server::lockid_info *new_lockid = (lock_server::lockid_info *)malloc(sizeof(*new_lockid));
		new_lockid->status = lockid_info::FREE;

		pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(*mutex));
		pthread_mutex_init(mutex, NULL);
		new_lockid->mutex = mutex;

		pthread_cond_t *wait = (pthread_cond_t *)malloc(sizeof(*wait));
		pthread_cond_init(wait, NULL);
		new_lockid->wait = wait;

		// insert entry into map
		locks.insert( std::pair<lock_protocol::lockid_t, lock_server::lockid_info *>(lid, new_lockid) );
	}
	pthread_mutex_unlock( &global_mutex );

	lock_server::lockid_info *lockid_info_ptr = locks.find(lid)->second;
	assert( pthread_mutex_lock (lockid_info_ptr->mutex) == 0 );

	// while lockid is locked, wait for it to be unlocked
	while( lockid_info_ptr->status == lockid_info::LOCKED ) {
		printf("%d blocked for lid %llu\n", clt, lid);

		// block current thread
		assert( pthread_cond_wait(lockid_info_ptr->wait, lockid_info_ptr->mutex) == 0 );
	}

	printf("%d can proceed for lid %llu\n", clt, lid);
	lockid_info_ptr->status = lockid_info::LOCKED;
	printf("lid %llu locked\n", lid);

	assert( pthread_mutex_unlock (lockid_info_ptr->mutex) == 0 );

	return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
	printf("%d release %llu\n", clt, lid);

	lock_server::lockid_info *lock_info = locks.find(lid)->second;

	assert( pthread_mutex_lock (lock_info->mutex) == 0 );

	lock_info->status = lockid_info::FREE;
	printf("%d released %llu\n", clt, lid);

	assert( pthread_mutex_unlock (lock_info->mutex) == 0 );

	// unblock threads blocked on the condition variable
	assert( pthread_cond_signal(lock_info->wait) == 0 );
	
	printf("signal sent! %llu\n", lid);
	
	return lock_protocol::OK;
}

