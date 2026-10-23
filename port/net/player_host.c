/* Player-host lifecycle is now implemented in the EXE, not by Node.js.
   Keep the existing front-end API unchanged so the menu continues working. */
#include "player_host.h"
#include "native_host.h"

int player_host_start(void) { return native_host_start(); }
int player_host_state(void) { return native_host_state(); }
const char *player_host_error(void) { return native_host_error(); }
void player_host_stop(void) { native_host_stop(); }
