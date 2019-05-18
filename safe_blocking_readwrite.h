#ifndef SAFE_BLOCKING_READWRITE_H
#define SAFE_BLOCKING_READWRITE_H

#include<unistd.h>

int sb_read(int fd, unsigned char *buf, size_t buflen);
int sb_read_len(int fd, unsigned char *buf, size_t buflen);
int sb_write_len(int fd, unsigned char *buf, size_t buflen);

#endif // SAFE_BLOCKING_READWRITE_H

