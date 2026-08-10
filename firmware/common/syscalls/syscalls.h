#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

int *__errno(void);
int _write(int file, char* ptr, int len);
int _read(int file, char* ptr, int len);
int _close(int file);
int _fstat(int file, struct stat* st);
int _isatty(int file);
int _lseek(int file, int ptr, int dir);
void* _sbrk(int incr);
int _getpid(void);
int _getpid_r(struct _reent* r);
int _kill(int pid, int sig);
int _kill_r(struct _reent* r, int pid, int sig);

#ifdef __cplusplus
}
#endif

#endif // SYSCALLS_H
