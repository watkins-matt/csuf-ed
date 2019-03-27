#include "grep.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *braelist[NBRA];  // Execute, backref
char *braslist[NBRA];  // Execute, backref
char *globp;
char *linebp;
char *loc1;
char *loc2;
char *nextip;
char expbuf[ESIZE + 4];
char file[FNSIZE];
char genbuf[LBSIZE];
char ibuff[BLKSIZE];
char linebuf[LBSIZE];
char obuff[BLKSIZE];
char Q[] = "";
char savedfile[FNSIZE];
char T[] = "";
int given;
int io;
int lastc;
int ninbuf;
int nleft;
int peekc;
int tfile = -1;
int tline;
unsigned int *addr1, *addr2;
unsigned int *dol;   // dol: dollar sign, a pointer to the last line
unsigned int *dot;   // dot: period, a pointer to the current line
unsigned int *zero;  // zero: a pointer to before the first line
unsigned nlall = 128;

int search_directory(char *path, const char* pattern) {
    int match_count = 0;
    DIR *dir_to_open = opendir(path);

    if (dir_to_open != NULL) {
        struct dirent *directory = readdir(dir_to_open);
        while (directory != NULL) {
            if (!(strcmp(directory->d_name, ".") == 0 || strcmp(directory->d_name, "..") == 0)) {
                if (is_file(directory->d_name) && !is_binary(directory->d_name)) {
                    match_count += search_file(directory->d_name, pattern, 1);
                }

                else {
                    match_count += search_directory(directory->d_name, pattern);
                }

            }

            directory = readdir(dir_to_open);
        }

        closedir(dir_to_open);
    }

    return match_count;
}

int is_file(const char *path) {
    DIR *directory = opendir(path);
    int is_file = directory == NULL && errno == ENOTDIR;
    closedir(directory);
    return is_file;
}

int is_binary(char *path) {
    char *original_path = path;

    while (*path != '.' && *path != '\0') {
        path++;
    }

    int binary_file = strcmp(path, ".exe") == 0 || strcmp(path, ".pdb") == 0 ||
                      strcmp(path, ".out") == 0 || strcmp(path, ".obj") == 0 ||
                      strcmp(path, ".bin") == 0 || strcmp(path, ".png") == 0 || 
                      strcmp(path, ".ipch") == 0 || strcmp(path, ".pdf") == 0;

    if (binary_file) {
        putstr_n(original_path);
        putstr(": Binary file detected, skipping.");
    }

    return binary_file;
}

int main(int argc, char *argv[]) {
    argc--;
    argv++;

    if (argc <= 1) {
        printf("Usage: grep [Pattern] [File]\n");
        printf("Search for a pattern in specified files.\n");
        return 1;
    }

    char pattern[100];
    strcpy(pattern, argv[0]);
    strcat(pattern, "\n");
    argc--;
    argv++;

    int match_count = 0;
    for (int i = 0; i < argc; i++) {
        DIR *dir_to_open = opendir(argv[i]);

        if (dir_to_open != NULL) {
            struct dirent *directory = readdir(dir_to_open);
            while (directory != NULL) {
                if (!(strcmp(directory->d_name, ".") == 0 || strcmp(directory->d_name, "..") == 0)) {
                    if (is_file(directory->d_name) && !is_binary(directory->d_name)) {
                        match_count += search_file(directory->d_name, pattern, 1);
                    }

                    else {
                        match_count += search_directory(directory->d_name, pattern);
                    }
                }

                directory = readdir(dir_to_open);
            }

            closedir(dir_to_open);
        }

        else if (!is_binary(argv[i])) {
            match_count += search_file(argv[i], pattern, argc == 1 ? 0 : 1);
        }
    }

    return match_count > 0 ? 0 : 1;
}

int search_file(const char *file_name, const char *pattern, int show_file_name) {
    zero = (unsigned *)malloc(nlall * sizeof(unsigned));
    dot = dol = zero;
    command_read_file(file_name);
    return search_for_string(pattern, show_file_name ? file_name : NULL);
}

char read_char(const char *search_string) {
    static int index = -1;
    int last_index = strlen(search_string) - 1;

    index = index > last_index ? 0 : index + 1;
    return search_string[index];
}

void putstr_n(const char *sp) {
    while (*sp) putchr(*sp++);
}

// The filename may be null, in which case the filename will not be printed
int search_for_string(const char *search_string, const char *file_name) {
    addr1 = zero + 1;
    addr2 = dol;
    compile('/', search_string);
    unsigned int *a = addr1;
    int match_count = 0;

    while (a <= dol) {
        if (execute(a)) {
            if (file_name != NULL) {
                putstr_n(file_name);
                putstr_n(": ");
            }

            print_line(a);
            match_count++;
        }
        a++;
    }

    return match_count;
}

void read_file(const char *filename) {
    char file_name[50];
    strcpy(file_name, filename);

    if ((io = open(file_name, 0)) < 0) {
        lastc = '\n';
        error(file_name);
    }
    // Code from setwide
    if (!given) {
        addr1 = zero + (dol > zero);
        addr2 = dol;
    }
    // Code from squeeze
    if (addr1 < zero || addr2 > dol || addr1 > addr2) error(Q);

    ninbuf = 0;
    append(getfile, addr2);
    close(io);
    io = -1;
}

void command_read_file(const char *file_name) {
    unsigned int *a1;
    int c;
    char lastsep;

    unsigned int *a = dot;
    unsigned int *b = a;
    globp = "r";

    c = '\n';
    for (addr1 = 0;;) {
        lastsep = c;
        a1 = 0;
        c = getchr();
        if (c != ',' && c != ';') break;
        if (lastsep == ',') error(Q);
        if (a1 == 0) {
            a1 = zero + 1;
            if (a1 > dol) a1--;
        }
        addr1 = a1;
        if (c == ';') dot = a1;
    }

    if (lastsep != '\n' && a1 == 0) a1 = dol;

    if ((addr2 = a1) == 0) {
        given = 0;
        addr2 = dot;
    } else
        given = 1;
    if (addr1 == 0) addr1 = addr2;

    read_file(file_name);
}

void print_line(unsigned int *line) {
    putstr(get_line(*line));
}

void error(char *s) {
    putstr(s);
}

int getchr(void) {
    char c;
    if ((lastc = peekc)) {
        peekc = 0;
        return (lastc);
    }
    if (globp) {
        if ((lastc = *globp++) != 0) return (lastc);
        globp = 0;
        return (EOF);
    }
    if (read(0, &c, 1) <= 0) return (lastc = EOF);

    lastc = c & 0177;
    return (lastc);
}

int getfile(void) {
    int c;
    char *lp, *fp;

    lp = linebuf;
    fp = nextip;
    do {
        if (--ninbuf < 0) {
            if ((ninbuf = read(io, genbuf, LBSIZE) - 1) < 0) {
                if (lp > linebuf) {
                    *genbuf = '\n';
                } else {
                    return (EOF);
                }
            }
            fp = genbuf;
            while (fp < &genbuf[ninbuf]) {
                if (*fp++ & 0200) break;
            }
            fp = genbuf;
        }
        c = *fp++;
        if (c == '\0') continue;
        if (c & 0200 || lp >= &linebuf[LBSIZE]) {
            lastc = '\n';
            error(Q);
        }
        *lp++ = c;
    } while (c != '\n');
    *--lp = 0;
    nextip = fp;
    return (0);
}

// Needed to read the information from the file
int append(int (*f)(void), unsigned int *a) {
    unsigned int *a1, *a2, *rdot;
    int nline, tl;

    nline = 0;
    dot = a;
    while ((*f)() == 0) {
        if ((dol - zero) + 1 >= nlall) {
            unsigned *ozero = zero;

            nlall += 1024;
            if ((zero = (unsigned *)realloc(
                     (char *)zero, nlall * sizeof(unsigned))) == NULL) {
                exit(1);  // error("MEM?");
            }
            dot += zero - ozero;
            dol += zero - ozero;
        }
        tl = putline();
        nline++;
        a1 = ++dol;
        a2 = a1 + 1;
        rdot = ++dot;
        while (a1 > rdot) *--a2 = *--a1;
        *rdot = tl;
    }
    return (nline);
}

char *get_line(unsigned int tl) {
    char *bp, *lp;
    int nl;

    lp = linebuf;
    bp = getblock(tl, READ);
    nl = nleft;
    tl &= ~((BLKSIZE / 2) - 1);
    while ((*lp++ = *bp++))
        if (--nl == 0) {
            bp = getblock(tl += (BLKSIZE / 2), READ);
            nl = nleft;
        }
    return (linebuf);
}

int putline(void) {
    char *bp, *lp;
    int nl;
    unsigned int tl;

    lp = linebuf;
    tl = tline;
    bp = getblock(tl, WRITE);
    nl = nleft;
    tl &= ~((BLKSIZE / 2) - 1);
    while ((*bp = *lp++)) {
        if (*bp++ == '\n') {
            *--bp = 0;
            linebp = lp;
            break;
        }
        if (--nl == 0) {
            bp = getblock(tl += (BLKSIZE / 2), WRITE);
            nl = nleft;
        }
    }
    nl = tline;
    tline += (((lp - linebuf) + 03) >> 1) & 077776;
    return (nl);
}

char *getblock(unsigned int atl, int iof) {
    static int iblock = -1;
    static int oblock = -1;
    static int ichanged = 0;
    int bno, off;

    bno = (atl / (BLKSIZE / 2));
    off = (atl << 1) & (BLKSIZE - 1) & ~03;
    if (bno >= NBLK) {
        lastc = '\n';
        error(T);
    }
    nleft = BLKSIZE - off;
    if (bno == iblock) {
        ichanged |= iof;
        return (ibuff + off);
    }
    if (bno == oblock) return (obuff + off);
    if (iof == READ) {
        if (ichanged) blkio(iblock, ibuff, write);
        ichanged = 0;
        iblock = bno;
        blkio(bno, ibuff, read);
        return (ibuff + off);
    }
    if (oblock >= 0) blkio(oblock, obuff, write);
    oblock = bno;
    return (obuff + off);
}

void blkio(int b, char *buf, int (*iofcn)(int, char *, int)) {
    lseek(tfile, (long)b * BLKSIZE, 0);
    if ((*iofcn)(tfile, buf, BLKSIZE) != BLKSIZE) {
        error(T);
    }
}

void compile_error() {
    expbuf[0] = 0;
    //nbra = 0;
    error(Q);
    exit(2);
}

void compile(int eof, const char *search_string) {
    int c;
    char *ep;
    char *lastep;
    char bracket[NBRA], *bracketp;
    int cclcnt;
    int nbra = 0;

    ep = expbuf;
    bracketp = bracket;
    if ((c = read_char(search_string)) == '\n') {
        peekc = c;
        c = eof;
    }
    if (c == eof) {
        if (*ep == 0) error(Q);
        return;
    }

    if (c == '^') {
        c = read_char(search_string);
        *ep++ = CCIRC;
    }
    peekc = c;
    lastep = 0;
    for (;;) {
        if (ep >= &expbuf[ESIZE]) compile_error();  //cerror;
        c = read_char(search_string);
        if (c == '\n') {
            peekc = c;
            c = eof;
        }
        if (c == eof) {
            if (bracketp != bracket) compile_error();  //cerror;
            *ep++ = CEOF;
            return;
        }
        if (c != '*') lastep = ep;
        switch (c) {
            case '\\':
                if ((c = read_char(search_string)) == '(') {
                    if (nbra >= NBRA) compile_error();  //cerror;
                    *bracketp++ = nbra;
                    *ep++ = CBRA;
                    *ep++ = nbra++;
                    continue;
                }
                if (c == ')') {
                    if (bracketp <= bracket) compile_error();  //cerror;
                    *ep++ = CKET;
                    *ep++ = *--bracketp;
                    continue;
                }
                if (c >= '1' && c < '1' + NBRA) {
                    *ep++ = CBACK;
                    *ep++ = c - '1';
                    continue;
                }
                *ep++ = CCHR;
                if (c == '\n') compile_error();  //cerror;
                *ep++ = c;
                continue;

            case '.':
                *ep++ = CDOT;
                continue;

            case '\n':
                compile_error();  //cerror;

            case '*':
                if (lastep == 0 || *lastep == CBRA || *lastep == CKET) {
                    *ep++ = CCHR;
                    *ep++ = c;
                }

                else {
                    *lastep |= STAR;
                }
                continue;

            case '$':
                if ((peekc = read_char(search_string)) != eof && peekc != '\n') {
                    *ep++ = CCHR;
                    *ep++ = c;
                }  //defchar;

                else {
                    *ep++ = CDOL;
                }

                continue;

            case '[':
                *ep++ = CCL;
                *ep++ = 0;
                cclcnt = 1;
                if ((c = read_char(search_string)) == '^') {
                    c = read_char(search_string);
                    ep[-2] = NCCL;
                }
                do {
                    if (c == '\n') compile_error();  //cerror;
                    if (c == '-' && ep[-1] != 0) {
                        if ((c = read_char(search_string)) == ']') {
                            *ep++ = '-';
                            cclcnt++;
                            break;
                        }
                        while (ep[-1] < c) {
                            *ep = ep[-1] + 1;
                            ep++;
                            cclcnt++;
                            if (ep >= &expbuf[ESIZE]) compile_error();  //cerror;
                        }
                    }
                    *ep++ = c;
                    cclcnt++;
                    if (ep >= &expbuf[ESIZE]) compile_error();  //cerror;
                } while ((c = read_char(search_string)) != ']');
                lastep[1] = cclcnt;
                continue;

            default:
                *ep++ = CCHR;
                *ep++ = c;
        }
    }
}

int execute(unsigned int *addr) {
    char *p1, *p2;
    int c;

    for (c = 0; c < NBRA; c++) {
        braslist[c] = 0;
        braelist[c] = 0;
    }
    p2 = expbuf;
    if (addr == (unsigned *)0) {
        if (*p2 == CCIRC) return (0);
        p1 = loc2;
    } else if (addr == zero)
        return (0);
    else
        p1 = get_line(*addr);
    if (*p2 == CCIRC) {
        loc1 = p1;
        return (advance(p1, p2 + 1));
    }
    /* fast check for first character */
    if (*p2 == CCHR) {
        c = p2[1];
        do {
            if (*p1 != c) continue;
            if (advance(p1, p2)) {
                loc1 = p1;
                return (1);
            }
        } while (*p1++);
        return (0);
    }
    /* regular algorithm */
    do {
        if (advance(p1, p2)) {
            loc1 = p1;
            return (1);
        }
    } while (*p1++);
    return (0);
}

int advance(char *lp, char *ep) {
    char *curlp;
    int i;

    for (;;) switch (*ep++) {
            case CCHR:
                if (*ep++ == *lp++) continue;
                return (0);

            case CDOT:
                if (*lp++) continue;
                return (0);

            case CDOL:
                if (*lp == 0) continue;
                return (0);

            case CEOF:
                loc2 = lp;
                return (1);

            case CCL:
                if (cclass(ep, *lp++, 1)) {
                    ep += *ep;
                    continue;
                }
                return (0);

            case NCCL:
                if (cclass(ep, *lp++, 0)) {
                    ep += *ep;
                    continue;
                }
                return (0);

            case CBRA:
                braslist[*ep++] = lp;
                continue;

            case CKET:
                braelist[*ep++] = lp;
                continue;

            case CBACK:
                if (braelist[i = *ep++] == 0) error(Q);
                if (backref(i, lp)) {
                    lp += braelist[i] - braslist[i];
                    continue;
                }
                return (0);

            case CBACK | STAR:
                if (braelist[i = *ep++] == 0) error(Q);
                curlp = lp;
                while (backref(i, lp)) lp += braelist[i] - braslist[i];
                while (lp >= curlp) {
                    if (advance(lp, ep)) return (1);
                    lp -= braelist[i] - braslist[i];
                }
                continue;

            case CDOT | STAR:
                curlp = lp;
                while (*lp++)
                    ;
                do {
                    lp--;
                    if (advance(lp, ep)) return (1);
                } while (lp > curlp);
                return (0);

            case CCHR | STAR:
                curlp = lp;
                while (*lp++ == *ep)
                    ;
                ep++;
                do {
                    lp--;
                    if (advance(lp, ep)) return (1);
                } while (lp > curlp);
                return (0);

            case CCL | STAR:
            case NCCL | STAR:
                curlp = lp;
                while (cclass(ep, *lp++, ep[-1] == (CCL | STAR)))
                    ;
                ep += *ep;
                do {
                    lp--;
                    if (advance(lp, ep)) return (1);
                } while (lp > curlp);
                return (0);

            default:
                error(Q);
        }
}

int backref(int i, char *lp) {
    char *bp;

    bp = braslist[i];
    while (*bp++ == *lp++)
        if (bp >= braelist[i]) return (1);
    return (0);
}

int cclass(char *set, int c, int af) {
    int n;

    if (c == 0) return (0);
    n = *set++;
    while (--n)
        if (*set++ == c) return (af);
    return (!af);
}

void putstr(char *sp) {
    while (*sp) putchr(*sp++);
    putchr('\n');
}

void putchr(int ac) {
    static char line[70];
    static char *linp = line;
    const int STD_OUT = 1;
    const int STD_ERR = 2;

    char *lp;
    int c;

    lp = linp;
    c = ac;
    *lp++ = c;
    if (c == '\n' || lp >= &line[64]) {
        linp = line;
        write(STD_OUT, line, lp - line);
        return;
    }
    linp = lp;
}