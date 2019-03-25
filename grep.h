#ifndef Grep_H
#define Grep_H

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

#endif // Grep_H