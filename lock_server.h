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
#define NUM_THREADS 10

class lock_server {

	protected:
		int nacquire;
		lock_protocol::lockid_t clientID;
		
		pthread_t threads[NUM_THREADS];
		pthread_mutex_t global_mutex;
		pthread_cond_t wait_condition;

		struct lockid_info {
			enum lock_status { LOCKED, FREE };
			lock_status status;
			pthread_mutex_t *mutex;
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
			if( locks->find(*lid) == locks->end() ) {
				pthread_mutex_lock( &(params->context->global_mutex) );

				// create new lockid entry
				lock_server::lockid_info *new_lockid = (lock_server::lockid_info *)malloc(sizeof(*new_lockid));
				new_lockid->status = lockid_info::FREE;

				pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(*mutex));
				pthread_mutex_init(mutex, NULL);
				new_lockid->mutex = mutex;

				// insert entry into map
				locks->insert( std::pair<lock_protocol::lockid_t, lock_server::lockid_info *>(*lid, new_lockid) );

				pthread_mutex_unlock( &(params->context->global_mutex) );
			}

			assert( pthread_mutex_lock (locks->find(*lid)->second->mutex) == 0 );

			// while lockid is locked, wait for it to be unlocked
			while( locks->find(*lid)->second->status == lockid_info::LOCKED ) {
				printf("thread blocked! %llu\n", *lid);

				// block current thread
				assert( pthread_cond_wait( &(params->context->wait_condition), locks->find(*lid)->second->mutex) == 0 );
			}
			printf("thread may proceed! %llu\n", *lid);

			locks->find(*lid)->second->status = lockid_info::LOCKED;

			assert( pthread_mutex_unlock (locks->find(*lid)->second->mutex) == 0 );
			printf("thread LOCKED\n");

			// clears memory
			free(context);

			pthread_exit(NULL);
		};

};

#endif
