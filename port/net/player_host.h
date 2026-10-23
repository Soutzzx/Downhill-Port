/* Windows single-executable Player Hosted: the local WebSocket relay is
   compiled into dhd.exe. No Node, PowerShell, scripts, or extracted files. */
#ifndef DHD_PLAYER_HOST_H
#define DHD_PLAYER_HOST_H

enum { PLAYER_HOST_STOPPED = 0, PLAYER_HOST_STARTING = 1,
       PLAYER_HOST_READY = 2, PLAYER_HOST_FAILED = 3 };
/* Start/stop the built-in listener. Only called when the user selects
   ONLINE > Host A Private Game. The public Cloudflare mode is untouched. */
int player_host_start(void);
int player_host_state(void);
const char *player_host_error(void);
int player_host_directory_state(void);
void player_host_stop(void);

#endif
