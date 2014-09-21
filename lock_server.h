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
		
		std::map<int, pthread_t*> threads;
		pthread_mutex_t global_mutex;

		struct lockid_info {
			enum lock_status { LOCKED, FREE } status;
			pthread_mutex_t *mutex;
			pthread_cond_t *wait;
		};

		std::map<lock_protocol::lockid_t, lock_server::lockid_info*> locks;

		struct thread_params {
			int clt;
			lock_protocol::lockid_t lid;
			lock_server *context;
		};

	public:
		lock_server();
		~lock_server() {};
		
		lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

		static void *acquireHandler(void *context) {
			struct lock_server::thread_params *params = (struct lock_server::thread_params *)context;
			
			lock_protocol::lockid_t *lid = &(params->lid);
			std::map<lock_protocol::lockid_t, lock_server::lockid_info*> *locks = &(params->context->locks);

			// lockid does not exist, we have to create a new one
			pthread_mutex_lock( &(params->context->global_mutex) );
			if( locks->find(*lid) == locks->end() ) {
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
				locks->insert( std::pair<lock_protocol::lockid_t, lock_server::lockid_info *>(*lid, new_lockid) );
			}
			pthread_mutex_unlock( &(params->context->global_mutex) );

			lock_server::lockid_info *lockid_info_ptr = locks->find(*lid)->second;
			assert( pthread_mutex_lock (lockid_info_ptr->mutex) == 0 );

			// while lockid is locked, wait for it to be unlocked
			while( lockid_info_ptr->status == lockid_info::LOCKED ) {
				printf("%d blocked for lid %llu\n", params->clt, *lid);

				// block current thread
				assert( pthread_cond_wait(lockid_info_ptr->wait, lockid_info_ptr->mutex) == 0 );
			}

			printf("%d can proceed for lid %llu\n", params->clt, *lid);
			lockid_info_ptr->status = lockid_info::LOCKED;
			printf("lid %llu locked\n", *lid);

			assert( pthread_mutex_unlock (lockid_info_ptr->mutex) == 0 );

			// clears memory
			free(context);

			pthread_exit(NULL);
		};

};

#endif
