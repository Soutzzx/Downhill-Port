/* The local race relay is an optional Windows child process.
   Start it without a visible CMD window on demand, verify /v1/health
   asynchronously, and terminate the entire process tree when the host
   leaves or this dhd.exe exits (Windows JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE).
   Public online rooms are entirely unaffected. */
#include "player_host.h"
#include "ws_winhttp.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

enum { HOST_PORT = 8787, BOOT_CHECKS = 360 }; /* up to ~3 minutes incl. first npm ci */

static HANDLE host_job;
static HANDLE host_process;
static HANDLE startup_thread;
static volatile LONG host_state;
static volatile LONG host_stop;
static char host_error[200];

static int have_file(const wchar_t *path)
{
    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

/* Works with an EXE in the repository root, a bundled release folder,
   or build/port/clean/... while testing from Visual Studio Code. */
static int find_host_script(wchar_t *out, size_t capacity)
{
    wchar_t folder[2048];
    DWORD length = GetModuleFileNameW(NULL, folder, (DWORD)(sizeof folder / sizeof *folder));
    if (!length || length >= sizeof folder / sizeof *folder) return 0;
    wchar_t *slash = wcsrchr(folder, L'\\');
    if (!slash) return 0;
    *slash = 0;
    for (int depth = 0; depth < 8; depth++) {
        wchar_t candidate[2048], package[2048];
        if (swprintf(candidate, sizeof candidate / sizeof *candidate,
                     L"%ls\\online\\player-host.ps1", folder) < 0 ||
            swprintf(package, sizeof package / sizeof *package,
                     L"%ls\\online\\server\\package.json", folder) < 0) return 0;
        if (have_file(candidate) && have_file(package)) {
            if (wcslen(candidate) + 1 > capacity) return 0;
            wcscpy(out, candidate);
            return 1;
        }
        slash = wcsrchr(folder, L'\\');
        if (!slash || slash == folder + 2) break; /* stop at drive root */
        *slash = 0;
    }
    return 0;
}

static void set_error(const char *message)
{
    snprintf(host_error, sizeof host_error, "%s", message);
    InterlockedExchange(&host_state, PLAYER_HOST_FAILED);
}

static DWORD WINAPI wait_for_relay(void *arg)
{
    (void)arg;
    char body[256];
    int healthy = 0;
    for (int i = 0; i < BOOT_CHECKS; i++) {
        if (InterlockedCompareExchange(&host_stop, 0, 0)) return 0;
        if (host_process && WaitForSingleObject(host_process, 0) == WAIT_OBJECT_0) {
            set_error("Local server exited. See the player-host log in TEMP.");
            return 0;
        }
        /* Health check on localhost, NOT against the remote code directory.
           Calls happen in this thread so the game/menu never stalls.
           Require consecutive replies with the child still alive so an
           already running manual PlayerHost.cmd cannot be mistaken for
           the newly launched and game-managed host. */
        if (http_get("http://127.0.0.1:8787/v1/health", body, sizeof body) > 0 &&
            strstr(body, "\"ok\"") && strstr(body, "true") &&
            strstr(body, "\"protocol\"")) {
            if (++healthy >= 4) {
                if (!InterlockedCompareExchange(&host_stop, 0, 0))
                    InterlockedExchange(&host_state, PLAYER_HOST_READY);
                return 0;
            }
        } else {
            healthy = 0;
        }
        Sleep(500);
    }
    if (!InterlockedCompareExchange(&host_stop, 0, 0))
        set_error("Local relay startup timed out. See the player-host log in TEMP.");
    return 0;
}

int player_host_state(void)
{
    return (int)InterlockedCompareExchange(&host_state, 0, 0);
}

const char *player_host_error(void) { return host_error; }

int player_host_start(void)
{
    if (player_host_state() == PLAYER_HOST_STARTING ||
        player_host_state() == PLAYER_HOST_READY) return 1;
    player_host_stop();

    /* Do not attach to somebody else's already running PlayerHost.cmd:
       we could not close that independently owned process on leaving. */
    char existing[256];
    if (http_get("http://127.0.0.1:8787/v1/health", existing, sizeof existing) > 0 &&
        strstr(existing, "\"ok\"") && strstr(existing, "\"protocol\"")) {
        set_error("Close the manually opened PlayerHost.cmd first.");
        return 0;
    }

    wchar_t script[2048], cmd[4096], logfile[2048], tempdir[1800];
    if (!find_host_script(script, sizeof script / sizeof *script)) {
        set_error("Missing online/server beside the game (or in a parent folder).");
        return 0;
    }
    DWORD temp_len = GetTempPathW((DWORD)(sizeof tempdir / sizeof *tempdir), tempdir);
    if (!temp_len || temp_len >= sizeof tempdir / sizeof *tempdir) {
        set_error("Windows TEMP folder is unavailable.");
        return 0;
    }
    if (swprintf(logfile, sizeof logfile / sizeof *logfile,
                 L"%lsdhd-player-host.log", tempdir) < 0) {
        set_error("Cannot create log file path.");
        return 0;
    }
    SECURITY_ATTRIBUTES sec = {sizeof sec, NULL, TRUE};
    HANDLE log = CreateFileW(logfile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sec, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    &sec, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE || null_input == INVALID_HANDLE_VALUE) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (null_input != INVALID_HANDLE_VALUE) CloseHandle(null_input);
        set_error("Cannot open the background Player Hosted log in TEMP.");
        return 0;
    }

    host_job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    ZeroMemory(&limits, sizeof limits);
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!host_job || !SetInformationJobObject(host_job, JobObjectExtendedLimitInformation,
                                              &limits, sizeof limits)) {
        CloseHandle(log);
        CloseHandle(null_input);
        set_error("Could not manage the host process tree.");
        player_host_stop();
        return 0;
    }

    /* Quotes support spaces/OneDrive paths. Do not invoke CMD. The only
       command-line path is the fixed trusted script found beside this build. */
    if (swprintf(cmd, sizeof cmd / sizeof *cmd,
                 L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"%ls\"",
                 script) < 0) {
        CloseHandle(log);
        CloseHandle(null_input);
        set_error("Could not form the local host command.");
        player_host_stop();
        return 0;
    }
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof startup);
    ZeroMemory(&process, sizeof process);
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = null_input;
    startup.hStdOutput = log;
    startup.hStdError = log;
    BOOL created = CreateProcessW(NULL, cmd, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL,
                                  &startup, &process);
    CloseHandle(log);
    CloseHandle(null_input);
    if (!created) {
        set_error("Cannot start PowerShell. See Windows settings.");
        player_host_stop();
        return 0;
    }
    host_process = process.hProcess;
    if (!AssignProcessToJobObject(host_job, process.hProcess)) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        set_error("Cannot attach local server to the game process.");
        player_host_stop();
        return 0;
    }
    InterlockedExchange(&host_stop, 0);
    InterlockedExchange(&host_state, PLAYER_HOST_STARTING);
    host_error[0] = 0;
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    startup_thread = CreateThread(NULL, 0, wait_for_relay, NULL, 0, NULL);
    if (!startup_thread) {
        set_error("Cannot monitor the local server startup.");
        player_host_stop();
        return 0;
    }
    return 1;
}

void player_host_stop(void)
{
    InterlockedExchange(&host_stop, 1);
    if (host_job) {
        /* Workerd, Wrangler, npm and PowerShell are in the same Job.
           Terminate all children immediately, including after a crash. */
        TerminateJobObject(host_job, 0);
    }
    if (startup_thread) {
        WaitForSingleObject(startup_thread, INFINITE);
        CloseHandle(startup_thread);
        startup_thread = NULL;
    }
    if (host_process) {
        CloseHandle(host_process);
        host_process = NULL;
    }
    if (host_job) {
        CloseHandle(host_job);
        host_job = NULL;
    }
    InterlockedExchange(&host_state, PLAYER_HOST_STOPPED);
}
