#include <sys/types.h>
#include <sys/stat.h>
int mkdir(const char *path, mode_t mode) { (void)path;(void)mode;return 0; }
int _unlink(const char *path) { (void)path; return -1; }
int _link(const char *o, const char *n) { (void)o;(void)n; return -1; }
