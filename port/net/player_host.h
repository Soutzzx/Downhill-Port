/* Windows helper for opt-in local player-hosted races. The game owns and
   terminates the background Wrangler process tree when hosting ends. */
#ifndef DHD_PLAYER_HOST_H
#define DHD_PLAYER_HOST_H

enum {
    PLAYER_HOST_STOPPED = 0,
    PLAYER_HOST_STARTING = 1,
    PLAYER_HOST_READY = 2,
    PLAYER_HOST_FAILED = 3
};

/* Starts an invisible subprocess using online/player-host.ps1. Nonblocking.
   Return nonzero if startup began; errors are available via player_host_error.
   The script plus online/server must accompany the executable somewhere in
   the exe directory or its parent tree; Node.js 22+ remains required. */
int player_host_start(void);
int player_host_state(void);
const char *player_host_error(void);
void player_host_stop(void);

#endif
