#include "rpc.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include "lock_server.h"
#include "paxos.h"
#include "rsm.h"

#include "jsl_log.h"

// Main loop of lock_server

int
main(int argc, char *argv[])
{
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  srandom(getpid());

  if(argc != 3){
    fprintf(stderr, "Usage: %s [master:]port [me:]port\n", argv[0]);
    exit(1);
  }

  //jsl_set_debug(2);
  // Comment out the next line to switch between the ordinary lock
  // server and the RSM.  In Lab 5, we disable the lock server and
  // implement Paxos.  In Lab 6, we will make the lock server use your
  // RSM layer.
#define	RSM
#ifdef RSM
  rsm rsm(argv[1], argv[2]);
#endif

#ifndef RSM
  lock_server ls;
  rpcs server(atoi(argv[1]));
  server.reg(lock_protocol::stat, &ls, &lock_server::stat);
  server.reg(lock_protocol::acquire, &ls, &lock_server::acquire);
  server.reg(lock_protocol::release, &ls, &lock_server::release);
#endif



  while(1)
    sleep(1000);
}
