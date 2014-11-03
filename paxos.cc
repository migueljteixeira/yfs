#include "paxos.h"
#include "handle.h"
// #include <signal.h>
#include <stdio.h>

// This module implements the proposer and acceptor of the Paxos
// distributed algorithm as described by Lamport's "Paxos Made
// Simple".  To kick off an instance of Paxos, the caller supplies a
// list of nodes, a proposed value, and invokes the proposer.  If the
// majority of the nodes agree on the proposed value after running
// this instance of Paxos, the acceptor invokes the upcall
// paxos_commit to inform higher layers of the agreed value for this
// instance.


bool
operator> (const prop_t &a, const prop_t &b)
{
  return (a.n > b.n || (a.n == b.n && a.m > b.m));
}

bool
operator>= (const prop_t &a, const prop_t &b)
{
  return (a.n > b.n || (a.n == b.n && a.m >= b.m));
}

std::string
print_members(const std::vector<std::string> &nodes)
{
  std::string s;
  s.clear();
  for (unsigned i = 0; i < nodes.size(); i++) {
    s += nodes[i];
    if (i < (nodes.size()-1))
      s += ",";
  }
  return s;
}

bool isamember(std::string m, const std::vector<std::string> &nodes)
{
  for (unsigned i = 0; i < nodes.size(); i++) {
    if (nodes[i] == m) return 1;
  }
  return 0;
}

bool
proposer::isrunning()
{
  bool r;
  assert(pthread_mutex_lock(&pxs_mutex)==0);
  r = !stable;
  assert(pthread_mutex_unlock(&pxs_mutex)==0);
  return r;
}

// check if the servers in l2 contains a majority of servers in l1
bool
proposer::majority(const std::vector<std::string> &l1, 
		const std::vector<std::string> &l2)
{
  unsigned n = 0;

  for (unsigned i = 0; i < l1.size(); i++) {
    if (isamember(l1[i], l2))
      n++;
  }
  return n >= (l1.size() >> 1) + 1;
}

proposer::proposer(class paxos_change *_cfg, class acceptor *_acceptor, 
		   std::string _me)
  : cfg(_cfg), acc (_acceptor), me (_me), break1 (false), break2 (false), 
    stable (true)
{
  assert (pthread_mutex_init(&pxs_mutex, NULL) == 0);

}

void
proposer::setn()
{
  my_n.n = acc->get_n_h().n + 1 > my_n.n + 1 ? acc->get_n_h().n + 1 : my_n.n + 1;
}

bool
proposer::run(int instance, std::vector<std::string> newnodes, std::string newv)
{
	std::vector<std::string> accepts;
	std::vector<std::string> nodes1;
	std::string v;
	bool r = false;

	pthread_mutex_lock(&pxs_mutex);
	printf("start: initiate paxos for %s w. i=%d v=%s stable=%d\n",
	print_members(newnodes).c_str(), instance, newv.c_str(), stable);
	if (!stable) {  // already running proposer?
		printf("proposer::run: already running\n");
		pthread_mutex_unlock(&pxs_mutex);
		return false;
	}
	
	// paxos is running
	stable = false;

	setn(); // update the last proposal number (this proposal)
	my_n.m = me; // who sent the proposal (since im proposing, it's me)
	
	c_nodes = newnodes; // set current nodes known by me
	c_v = newv; // set new value we would like to propose

	if (prepare(instance, accepts, c_nodes, v)) {

		if (majority(c_nodes, accepts)) {
			printf("paxos::manager: received a majority of prepare responses\n");

			if (v.size() == 0) {
				v = c_v;
			}

			breakpoint1();

			nodes1 = accepts;
			accepts.clear();
			accept(instance, accepts, nodes1, v);

			if (majority(c_nodes, accepts)) {
				printf("paxos::manager: received a majority of accept responses\n");

				breakpoint2();

				decide(instance, accepts, v);
				r = true;
			} else {
				printf("paxos::manager: no majority of accept responses\n");
			}

		} else {
			printf("paxos::manager: no majority of prepare responses\n");
		}

	} else {
		printf("paxos::manager: prepare is rejected %d\n", stable);
	}
	
	stable = true; // paxos has terminated
	pthread_mutex_unlock(&pxs_mutex);
	return r;
}

bool
proposer::prepare(unsigned instance, std::vector<std::string> &accepts, 
         std::vector<std::string> nodes,
         std::string &v)
{
	// sets number of the highest proposal accepted to the minimum
	// it will be updated in the next loop
	prop_t highest_n_a = {0, std::string()};

	// send prepare RPCs to nodes and collect responses
	for (unsigned i = 0; i < nodes.size(); i++) {

		// instantiates a RPC client for this node
		handle m(nodes[i]);
		rpcc *cl = m.get_rpcc();

		// something went wrong
		if(!cl) {
			printf("proposer::prepare: rejected\n");
			continue;
		}
			
		paxos_protocol::preparearg arg;
		paxos_protocol::prepareres res;

		// sets prepare arguments
		arg.instance = instance;
		arg.n = my_n; // the current proposal

		int ret = cl->call(paxos_protocol::preparereq, me, arg, res, rpcc::to(1000));
		if(ret != paxos_protocol::OK) {
			printf("proposer::prepare: failed\n");
			continue;
		}
			
		// if one of the nodes replies with an oldinstance it means
		// this propose is old so we'll update this node with the value
		// for the proposed instance
		if(res.oldinstance) {
			acc->commit(instance, res.v_a);
            stable = true;

            return false;
		}

		// fill in accepts with set of nodes that accepted the proposal,
		// set v to the v_a with the highest n_a, and return true
		else if(res.accept) {

			// add node to the list
            accepts.push_back(nodes[i]);

			if(res.n_a > highest_n_a) {
				v = res.v_a;
				highest_n_a = res.n_a;
			}
		}

		else return false;
	}

	return true;
}


void
proposer::accept(unsigned instance, std::vector<std::string> &accepts,
        std::vector<std::string> nodes, std::string v)
{
	// send accept RPCs to nodes and collect nodes that accepted
	for (unsigned i = 0; i < nodes.size(); i++) {

		// instantiates a RPC client for this node
		handle m(nodes[i]);
		rpcc *cl = m.get_rpcc();

		// something went wrong
		if(!cl) {
			printf("proposer::accept: rejected\n");
			continue;
		}

		paxos_protocol::acceptarg arg;
		int res;

		arg.instance = instance;
		arg.n = my_n; // the current proposal
		arg.v = v; // the highest proposal accepted so far by all nodes

		int ret = cl->call(paxos_protocol::acceptreq, me, arg, res, rpcc::to(1000));
		if(ret != paxos_protocol::OK) {
			printf("proposer::accept: failed\n");
			continue;
		}

		// this node has accepted the proposal, add it to the list
		if(res)
            accepts.push_back(nodes[i]);
		else
			printf("proposer::accept: rejected\n");
	}
}

void
proposer::decide(unsigned instance, std::vector<std::string> nodes, 
	      std::string v)
{
	// send decide RPCs to the nodes that have already accepted the proposal
	for (unsigned i = 0; i < nodes.size(); i++) {

		// instantiates a RPC client for this node
		handle m(nodes[i]);
		rpcc *cl = m.get_rpcc();

		// something went wrong
		if(!cl) {
			printf("proposer::decide: rejected\n");
			continue;
		}

		paxos_protocol::decidearg arg;
		int res;

	    arg.instance = instance;
	    arg.v = v; // the highest proposal accepted so far by all nodes

		int ret = cl->call(paxos_protocol::decidereq, me, arg, res, rpcc::to(1000));
		if(ret != paxos_protocol::OK) {
			printf("proposer::decide: failed\n");
			continue;
		}
	}
}

acceptor::acceptor(class paxos_change *_cfg, bool _first, std::string _me, 
	     std::string _value)
  : cfg(_cfg), me (_me), instance_h(0)
{
	assert (pthread_mutex_init(&pxs_mutex, NULL) == 0);

	n_h.n = 0;
	n_h.m = me;
	n_a.n = 0;
	n_a.m = me;
	v_a.clear();

	l = new log (this, me);

	if (instance_h == 0 && _first) {
		values[1] = _value;

		// logs view 1 to disk
		l->loginstance(1, _value);
		instance_h = 1;
	}

	pxs = new rpcs(atoi(_me.c_str()));
	pxs->reg(paxos_protocol::preparereq, this, &acceptor::preparereq);
	pxs->reg(paxos_protocol::acceptreq, this, &acceptor::acceptreq);
	pxs->reg(paxos_protocol::decidereq, this, &acceptor::decidereq);
}

paxos_protocol::status
acceptor::preparereq(std::string src, paxos_protocol::preparearg a,
    paxos_protocol::prepareres &r)
{
	// the proposal is older than the instance we're on
	// send back to the proposer the value that was agreed on that instance
	if(a.instance <= instance_h) {
		r.oldinstance = 1; // true
		r.accept = 0; // false
		r.n_a = n_a;
		r.v_a = values[a.instance];
	}
	// it's a new proposal, higher than we've seen so far
	else if(a.n > n_h) {
		n_h = a.n;

		// informs the proposer of the highest accept proposal
		// we've seen so far (and its value)
		r.oldinstance = 0; // false
		r.accept = 1; // true
		r.n_a = n_a;
		r.v_a = v_a;

		// it's a higher proposal than we've seen, so we log it
		l->loghigh(n_h);
	}
	// this shouldn't happen
	else {
		r.oldinstance = 0; // false
		r.accept = 0; // false
	}

	return paxos_protocol::OK;
}

paxos_protocol::status
acceptor::acceptreq(std::string src, paxos_protocol::acceptarg a, int &r)
{
	// the proposal is equal (in case it comes from the previous proposal)
	// or higher than we've seen so far, so we accept it
	if(a.n >= n_h) {
		n_a = a.n;
		v_a = a.v;
		r = 1; // true

		// since we accepted the proposal, we log it
		l->logprop(n_a, v_a);
	}
	else {
		r = 0; // false
	}

	return paxos_protocol::OK;
}

paxos_protocol::status
acceptor::decidereq(std::string src, paxos_protocol::decidearg a, int &r)
{
	// the proposal has been decided, we finally update the
	// instance and value (this requires locking)
	if (a.instance > instance_h) {
		commit(a.instance, a.v);
	}

	return paxos_protocol::OK;
}

void
acceptor::commit_wo(unsigned instance, std::string value)
{
  //assume pxs_mutex is held
  printf("acceptor::commit: instance=%d has v= %s\n", instance, value.c_str());
  if (instance > instance_h) {
    printf("commit: highestaccepteinstance = %d\n", instance);
    values[instance] = value;
    l->loginstance(instance, value);
    instance_h = instance;
    n_h.n = 0;
    n_h.m = me;
    n_a.n = 0;
    n_a.m = me;
    v_a.clear();
    if (cfg) {
      pthread_mutex_unlock(&pxs_mutex);
      cfg->paxos_commit(instance, value);
      pthread_mutex_lock(&pxs_mutex);
    }
  }
}

void
acceptor::commit(unsigned instance, std::string value)
{
  pthread_mutex_lock(&pxs_mutex);
  commit_wo(instance, value);
  pthread_mutex_unlock(&pxs_mutex);
}

std::string
acceptor::dump()
{
  return l->dump();
}

void
acceptor::restore(std::string s)
{
  l->restore(s);
  l->logread();
}



// For testing purposes

// Call this from your code between phases prepare and accept of proposer
void
proposer::breakpoint1()
{
  if (break1) {
    printf("Dying at breakpoint 1!\n");
    exit(1);
  }
}

// Call this from your code between phases accept and decide of proposer
void
proposer::breakpoint2()
{
  if (break2) {
    printf("Dying at breakpoint 2!\n");
    exit(1);
  }
}

void
proposer::breakpoint(int b)
{
  if (b == 3) {
    printf("Proposer: breakpoint 1\n");
    break1 = true;
  } else if (b == 4) {
    printf("Proposer: breakpoint 2\n");
    break2 = true;
  }
}
