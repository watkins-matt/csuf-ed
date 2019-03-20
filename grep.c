#include <stdlib.h>
#include <string.h>
#include "grep.h"

#ifndef NULL
#define NULL 0
#endif

#define BLKSIZE 4096
#define EOF -1
#define ESIZE 256
#define FNSIZE 128
#define KSIZE 9
#define LBSIZE 4096
#define NBLK 2047
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
char T[] = "TMP";
int given;
int io;
int lastc;
int ninbuf;
int nleft;
int peekc;
int tfile = -1;
int tline;
long count;
unsigned int *addr1, *addr2;
unsigned int *dol;   // dol: dollar sign, a pointer to the last line
unsigned int *dot;   // dot: period, a pointer to the current line
unsigned int *zero;  // zero: a pointer to before the first line
unsigned nlall = 128;

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

int main(int argc, char *argv[]) {
    char *p1, *p2;
    argv++;

    if (argc > 1) {
        p1 = *argv;
        p2 = savedfile;
        while ((*p2++ = *p1++))
            if (p2 >= &savedfile[sizeof(savedfile)]) p2--;
        globp = "r";
    }
    zero = (unsigned *)malloc(nlall * sizeof(unsigned));
    dot = dol = zero;
    command_read_file("Test.txt");
    search_for_string();
    return 0;
}

char read_char() {
    static int index = -1;
    const char *search_string = "is\n";
    int last_index = strlen(search_string) - 1;

    index = index > last_index ? 0 : index + 1;
    return search_string[index];
}

void search_for_string() {
    addr1 = zero + 1;
    addr2 = dol;
    compile('/');
    unsigned int *a = addr1;

    while (a <= dol) {
        if (execute(a)) {
            print_line(a);
        }
        a++;
    }
}

void read_file(char *file_name) {
    filename('r');
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

void command_read_file(char *file_name) {
    unsigned int *a1;
    int c;
    char lastsep;

    unsigned int *a = dot;
    unsigned int *b = a;

    c = '\n';
    for (addr1 = 0;;) {
        lastsep = c;
        //a1 = address();
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
    puts(getline(*line));
}

void filename(int comm) {
    char *p1, *p2;
    int c;

    count = 0;
    c = getchr();
    if (c == '\n' || c == EOF) {
        p1 = savedfile;
        if (*p1 == 0 && comm != 'f') error(Q);
        p2 = file;
        while ((*p2++ = *p1++))
            ;
        return;
    }
    if (c != ' ') error(Q);
    while ((c = getchr()) == ' ')
        ;
    if (c == '\n') error(Q);
    p1 = file;
    do {
        if (p1 >= &file[sizeof(file) - 1] || c == ' ' || c == EOF) error(Q);
        *p1++ = c;
    } while ((c = getchr()) != '\n');
    *p1++ = 0;
    if (savedfile[0] == 0 || comm == 'e' || comm == 'f') {
        p1 = savedfile;
        p2 = file;
        while ((*p1++ = *p2++))
            ;
    }
}

void error(char *s) {
    puts(s);
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
                    // Stop with the \n messages please, ancient Ed code
                    //puts("'\\n' appended");
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
        count++;
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
                // error("MEM?");
                // onhup(0);
                exit(1);
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

char *getline(unsigned int tl) {
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

void compile(int eof) {
    int c;
    char *ep;
    char *lastep;
    char bracket[NBRA], *bracketp;
    int cclcnt;
    int nbra = 0;

    ep = expbuf;
    bracketp = bracket;
    /*if ((c = read_char()) == '\n') {
        peekc = c;
        c = eof;
    }
    if (c == eof) {
        if (*ep == 0) error(Q);
        return;
    }*/

    if (c == '^') {
        c = read_char();
        *ep++ = CCIRC;
    }
    peekc = c;
    lastep = 0;
    for (;;) {
        if (ep >= &expbuf[ESIZE]) goto cerror;
        c = read_char();
        if (c == '\n') {
            peekc = c;
            c = eof;
        }
        if (c == eof) {
            if (bracketp != bracket) goto cerror;
            *ep++ = CEOF;
            return;
        }
        if (c != '*') lastep = ep;
        switch (c) {
            case '\\':
                if ((c = read_char()) == '(') {
                    if (nbra >= NBRA) goto cerror;
                    *bracketp++ = nbra;
                    *ep++ = CBRA;
                    *ep++ = nbra++;
                    continue;
                }
                if (c == ')') {
                    if (bracketp <= bracket) goto cerror;
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
                if (c == '\n') goto cerror;
                *ep++ = c;
                continue;

            case '.':
                *ep++ = CDOT;
                continue;

            case '\n':
                goto cerror;

            case '*':
                if (lastep == 0 || *lastep == CBRA || *lastep == CKET)
                    goto defchar;
                *lastep |= STAR;
                continue;

            case '$':
                if ((peekc = read_char()) != eof && peekc != '\n') goto defchar;
                *ep++ = CDOL;
                continue;

            case '[':
                *ep++ = CCL;
                *ep++ = 0;
                cclcnt = 1;
                if ((c = read_char()) == '^') {
                    c = read_char();
                    ep[-2] = NCCL;
                }
                do {
                    if (c == '\n') goto cerror;
                    if (c == '-' && ep[-1] != 0) {
                        if ((c = read_char()) == ']') {
                            *ep++ = '-';
                            cclcnt++;
                            break;
                        }
                        while (ep[-1] < c) {
                            *ep = ep[-1] + 1;
                            ep++;
                            cclcnt++;
                            if (ep >= &expbuf[ESIZE]) goto cerror;
                        }
                    }
                    *ep++ = c;
                    cclcnt++;
                    if (ep >= &expbuf[ESIZE]) goto cerror;
                } while ((c = read_char()) != ']');
                lastep[1] = cclcnt;
                continue;

            defchar:
            default:
                *ep++ = CCHR;
                *ep++ = c;
        }
    }
cerror:
    expbuf[0] = 0;
    nbra = 0;
    error(Q);
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
        p1 = getline(*addr);
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
                goto star;

            case CCHR | STAR:
                curlp = lp;
                while (*lp++ == *ep)
                    ;
                ep++;
                goto star;

            case CCL | STAR:
            case NCCL | STAR:
                curlp = lp;
                while (cclass(ep, *lp++, ep[-1] == (CCL | STAR)))
                    ;
                ep += *ep;
                goto star;

            star:
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

void puts(char *sp) {
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