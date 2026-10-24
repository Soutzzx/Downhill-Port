/* The release this build is: the one place its version is written (finding 054).

   - The online hello identifies a build so local hosts can reject mismatches.
   - The updater (port/hle/update.c) compares it with the newest release's latest.json.
   - port/tools/package_release.py names the zip after it and writes latest.json for it.

   Bump it here, and only here, for each release: "MAJOR.MINOR.PATCH". */
#ifndef DHD_VERSION_H
#define DHD_VERSION_H

/* Keep these numeric parts in step with DHD_VERSION; the compiler and
   Windows resource version are validated together by the build. */
#define DHD_VERSION_MAJOR 0
#define DHD_VERSION_MINOR 2
#define DHD_VERSION_PATCH 8
#define DHD_VERSION "0.2.8"

#endif
