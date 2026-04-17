#ifndef CERBERUS_UPLOAD_H
#define CERBERUS_UPLOAD_H

#define UPLOAD_OK        0
#define UPLOAD_DISABLED  1
#define UPLOAD_NO_TSR    2
#define UPLOAD_NETWORK   3
#define UPLOAD_SERVER    4

int upload_ini(const char *path);

#endif
