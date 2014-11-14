#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#undef max
#include <algorithm>

lock_server::lock_server(rsm *rsm)
 :	nacquire (0)
{
	//pthread_mutex_init(&global_mutex, NULL);
	status = FREE;
	rs = rsm;
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
	lock_protocol::status ret = lock_protocol::OK;
	r = nacquire;
	
	return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {

	/*// lockid does not exist, we have to create a new one
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

		// block current thread
		assert( pthread_cond_wait(lockid_info_ptr->wait, lockid_info_ptr->mutex) == 0 );
	}*/

	if(! rs->amiprimary())
		return lock_protocol::IOERR;

	if(status == LOCKED)
		return lock_protocol::RETRY;

	status = LOCKED;

	//assert( pthread_mutex_unlock (lockid_info_ptr->mutex) == 0 );

	return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
	//lock_server::lockid_info *lock_info = locks.find(lid)->second;

	//assert( pthread_mutex_lock (lock_info->mutex) == 0 );

	if(! rs->amiprimary())
		return lock_protocol::IOERR;

	status = FREE;

	//assert( pthread_mutex_unlock (lock_info->mutex) == 0 );

	// unblock threads blocked on the condition variable
	//assert( pthread_cond_signal(lock_info->wait) == 0 );
	
	return lock_protocol::OK;
}

