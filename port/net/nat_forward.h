/* Optional Windows UPnP mapping for native Player Hosted TCP 8787.
   Runs asynchronously and never blocks starting the local relay. */
#ifndef DHD_NAT_FORWARD_H
#define DHD_NAT_FORWARD_H
void nat_forward_start(void);
void nat_forward_stop(void);
/* 0: mapping in progress, 1: router accepted, -1: unavailable. */
int nat_forward_state(void);
#endif
