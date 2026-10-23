/* Embedded native Windows relay. No external runtime, Node.js or scripts. */
#ifndef DHD_NATIVE_HOST_H
#define DHD_NATIVE_HOST_H
enum { NATIVE_HOST_STOPPED = 0, NATIVE_HOST_STARTING = 1,
       NATIVE_HOST_READY = 2, NATIVE_HOST_FAILED = 3 };
int native_host_start(void);
int native_host_state(void);
const char *native_host_error(void);
/* 0 pending, 1 registered, -1 directory unavailable; not a NAT probe. */
int native_host_directory_state(void);
int native_host_is_local_code(const char *code);
void native_host_stop(void);
#endif
