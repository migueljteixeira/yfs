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
	pthread_mutex_init(&global_mutex, NULL);
	rs = rsm;
	rs->set_state_transfer(this);
}

lock_protocol::status lock_server::stat(unsigned int id, lock_protocol::lockid_t lid, int &r) {
	// if im not primary, cant talk with clients
	if(! rs->amiprimary())
		return lock_protocol::RPCERR;

	lock_protocol::status ret = lock_protocol::OK;
	r = nacquire;
	
	return ret;
}

lock_protocol::status lock_server::acquire(unsigned int id, lock_protocol::lockid_t lid, int &r) {
	// if im not primary, cant talk with clients
	if(! rs->amiprimary()) {
		r = lock_protocol::RPCERR;
		return lock_protocol::RPCERR;
	}

	printf("------ lock do: %llu -- %d \n", lid, id);

	// lockid does not exist, we have to create a new one
	pthread_mutex_lock( &global_mutex );

		if( locks.find(lid) == locks.end() ) {
			// create new lockid entry
			lockid_info *new_lockid = new lockid_info;
			new_lockid->id = 0;
			new_lockid->status = lockid_info::FREE;

			pthread_mutex_t *mutex = new pthread_mutex_t;
			pthread_mutex_init(mutex, NULL);
			new_lockid->mutex = mutex;

			// insert entry into map
			locks.insert( std::pair<lock_protocol::lockid_t, lockid_info *>(lid, new_lockid) );
		}

	pthread_mutex_unlock( &global_mutex );

	lockid_info *lockid_info_ptr = locks.find(lid)->second;
	pthread_mutex_lock (lockid_info_ptr->mutex);

		// if its already locked be this client, return OK
		if(lockid_info_ptr->id == id) {
			printf("------ lock do: %llu equal -- %d \n", lid, id);

			pthread_mutex_unlock (lockid_info_ptr->mutex);
			return lock_protocol::OK;
		}

		// if its locked we tell the client to try again
		if(lockid_info_ptr->status == lockid_info::LOCKED) {
			printf("------ lock do: %llu retry -- %d \n", lid, id);

			pthread_mutex_unlock (lockid_info_ptr->mutex);
			return lock_protocol::RETRY;
		}

		printf("------ lock do: %llu ok -- %d \n", lid, id);
	
		// otherwise lock it
		lockid_info_ptr->id = id;
		lockid_info_ptr->status = lockid_info::LOCKED;

	pthread_mutex_unlock (lockid_info_ptr->mutex);

	return lock_protocol::OK;
}

lock_protocol::status lock_server::release(unsigned int id, lock_protocol::lockid_t lid, int &r) {
	// if im not primary, cant talk with clients
	if(!rs->amiprimary())
		return lock_protocol::RPCERR;

	printf("------ unlock do: %llu -- %d \n", lid, id);
	
	lockid_info *lock_info = locks.find(lid)->second;
	pthread_mutex_lock (lock_info->mutex);

		// if its already locked by another client, tell it to try again
		if(lock_info->id != id) {
			printf("------ unlock do: %llu equal -- %d \n", lid, id);

			pthread_mutex_unlock (lock_info->mutex);
			return lock_protocol::RETRY;
		}

		lock_info->id = 0;
		lock_info->status = lockid_info::FREE;

	pthread_mutex_unlock (lock_info->mutex);

	printf("------ unlock do: %llu terminou -- %d \n", lid, id);
	
	return lock_protocol::OK;
}

marshall &
operator<<(marshall &m, lockid_info::lock_status &status) {
	m << status;
	return m;
}

unmarshall &
operator>>(unmarshall &u, lockid_info::lock_status &status) {
	u >> status;
	return u;
}

std::string 
lock_server::marshal_state() {

  // lock any needed mutexes
  pthread_mutex_lock( &global_mutex );
  
  marshall rep;
  rep << locks.size();
  std::map<lock_protocol::lockid_t, lockid_info*>::iterator iter_lock;
  for (iter_lock = locks.begin(); iter_lock != locks.end(); iter_lock++) {
    lock_protocol::lockid_t lid = iter_lock->first;
    lockid_info* lid_info = locks[lid];
    rep << lid;
    rep << lid_info->status;
    rep << lid_info->id;
  }
  
  // unlock any mutexes
  pthread_mutex_unlock( &global_mutex );
  
  return rep.str();

}

void 
lock_server::unmarshal_state(std::string state) {

  // lock any needed mutexes
  pthread_mutex_lock( &global_mutex );
  
  unmarshall rep(state);
  unsigned int locks_size;
  rep >> locks_size;
  for (unsigned int i = 0; i < locks_size; i++) {
    lock_protocol::lockid_t lid;
    rep >> lid;
    
    // create new lockid entry
	lockid_info *new_lockid = new lockid_info;
	rep >> new_lockid->status;
	rep >> new_lockid->id;

	pthread_mutex_t *mutex = new pthread_mutex_t;
	pthread_mutex_init(mutex, NULL);
	new_lockid->mutex = mutex;

	// insert entry into map
	locks.insert( std::pair<lock_protocol::lockid_t, lockid_info *>(lid, new_lockid) );
  }
  
  // unlock any mutexes
  pthread_mutex_unlock( &global_mutex );
}
