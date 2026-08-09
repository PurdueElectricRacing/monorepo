#include "syscalls.h"

int* __errno(void) {
    static int errno_val;
    return &errno_val;
}

int _write(int file, const char* ptr, int len) {
    return len;
}

int _read(int file, const char* ptr, int len) {
    return 0;
}

int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat* st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

void* _sbrk(int incr) {
    extern char _end; // defined by linker script
    static char* heap_end;
    char* prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_end;
    }

    prev_heap_end = heap_end;
    heap_end += incr;
    return (void*)prev_heap_end;
}

int _kill(int pid, int sig) {
    return -1;
}

// cppcheck-suppress constParameterPointer  // newlib _reent syscall signature
int _kill_r(struct _reent* r, int pid, int sig) {
    return _kill(pid, sig);
}

int _getpid(void) {
    return 1;
}

// cppcheck-suppress constParameterPointer  // newlib _reent syscall signature
int _getpid_r(struct _reent* r) {
    return _getpid();
}