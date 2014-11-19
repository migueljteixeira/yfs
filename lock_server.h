#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include "rsm.h"

struct lockid_info {
	enum lock_status { LOCKED, FREE } status;
	pthread_mutex_t *mutex;
	unsigned int id;
};

class lock_server : public rsm_state_transfer {

	public:
		lock_server(rsm *rsm);
		~lock_server() {};
		
		lock_protocol::status acquire(unsigned int id, lock_protocol::lockid_t lid, int &);
		lock_protocol::status release(unsigned int id, lock_protocol::lockid_t lid, int &);
		lock_protocol::status stat(unsigned int id, lock_protocol::lockid_t lid, int &);
		
		virtual std::string marshal_state();
		virtual void unmarshal_state(std::string);

	protected:
		int nacquire;
		
		rsm *rs;
		pthread_mutex_t global_mutex;
		std::map<lock_protocol::lockid_t, lockid_info*> locks;
};

#endif
