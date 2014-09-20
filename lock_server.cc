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
	pthread_cond_init (&wait_condition, NULL);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
	lock_protocol::status ret = lock_protocol::OK;
	printf("stat request from clt %d\n", clt);
	r = nacquire;
	
	return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {
	printf("acquire %llu\n", lid);

	struct lock_server::thread_params *readParams = (struct lock_server::thread_params *)malloc(sizeof(*readParams));
	readParams->clt = clt;
	readParams->lid = lid;
	readParams->context = this;

	// thread to handle client
	// TODO: change thread pool
	pthread_create(&threads[lid], NULL, &lock_server::acquireHandler, readParams);

	// suspend thread until it terminates
	pthread_join(threads[lid], NULL);

	return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
	lock_server::lockid_info *lock_info = locks.find(lid)->second;

	assert( pthread_mutex_lock (lock_info->mutex) == 0 );
	lock_info->status = lockid_info::FREE;

	assert( pthread_mutex_unlock (lock_info->mutex) == 0 );
	printf("thread RELEASED\n");

	// unblock threads blocked on the condition variable
	assert( pthread_cond_signal(&wait_condition) == 0 );
	
	printf("signal sent! %llu\n", lid);
	
	return lock_protocol::OK;
}

