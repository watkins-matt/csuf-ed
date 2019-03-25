#ifndef Grep_H
#define Grep_H

#ifndef NULL
#define NULL 0
#endif

#ifndef EOF
#define EOF -1
#endif

// All these values must be increased or 
// proper results won't be returned for large files
#define BLKSIZE 32768 //4096
#define ESIZE 2048 //256
#define FNSIZE 1024 //128
#define KSIZE 144 //9
#define LBSIZE 32768 //4096
#define NBLK 16376 //2047
#define NBRA 5
#define READ 0
#define WRITE 1

#define CBRA 1
#define CCHR 2
#define CDOT 4
#define CCL 6
#define NCCL 8
#define CDOL 10
#define CEOF 11
#define CKET 12
#define CBACK 14
#define CCIRC 15
#define STAR 01

// Functions in the original ed.c
char *getblock(unsigned int atl, int iof);
char *get_line(unsigned int tl);
int advance(char *lp, char *ep);
int append(int (*f)(void), unsigned int *a);
int backref(int i, char *lp);
int cclass(char *set, int c, int af);
int execute(unsigned int *addr);
int getchr(void);
int getfile(void);
int putline(void);
void blkio(int b, char *buf, int (*iofcn)(int, char *, int));
void compile(int eof, const char* search_string);
void error(char *s);
void filename(int comm);
void onhup(int n);
void print_line(unsigned int *line);
void print(void);
void putchr(int ac);
void putstr(char *sp);

// Newly added functions
void command_read_file(const char *file_name);
void read_file(const char *file_name);
void search_for_string();

// POSIX functions
#ifdef _WIN32
#define close _close
#define lseek _lseek
#define open _open
#define write _write
#endif

int close(int);
int open(char *, int);
int read(int, char *, int);
int write(int, char *, int);
long lseek(int, long, int);

#endif // Grep_H