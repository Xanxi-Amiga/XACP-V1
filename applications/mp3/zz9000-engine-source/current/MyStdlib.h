
void *my_malloc (unsigned long size);
void my_free (void* ptr);
void *my_memset (void *dest, int val, unsigned long len);
void *my_memcpy (void *dest, const void *src, unsigned long len);
int my_memcmp(const void *s1, const void *s2, unsigned long n);
char *my_strcpy (char * destination, const char * source);
char *my_strncpy(char *dst, const char *src, unsigned long n);
unsigned long my_strlen(const char *str);
int my_strncmp(const char *s1, const char *s2, unsigned long n);

int my_abs (int i);
int	my_rand(void);

