#ifndef lock_client_h
#define lock_client_h

#include <string>
#include "lock_protocol.h"
#include "rsm_client.h"

class lock_client {

	protected:
		rsm_client *cl;

	public:
		lock_client(std::string d);
		virtual ~lock_client() {};

		virtual lock_protocol::status acquire(lock_protocol::lockid_t);
		virtual lock_protocol::status release(lock_protocol::lockid_t);
		virtual lock_protocol::status stat(lock_protocol::lockid_t);
};

#endif 
