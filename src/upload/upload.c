#include <stdio.h>
#include "upload.h"

int upload_ini(const char *path)
{
#ifdef NETISA_ENABLED
    /* Phase 5 — real NetISA upload lands here */
    (void)path;
    fputs("NetISA upload not yet implemented\n", stderr);
    return UPLOAD_NETWORK;
#else
    (void)path;
    fputs("Upload disabled in this build (rebuild with NETISA=1 to enable).\n",
          stderr);
    return UPLOAD_DISABLED;
#endif
}
