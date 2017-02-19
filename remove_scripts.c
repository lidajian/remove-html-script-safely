#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAX_TAG_SIZE 12
#define MAX_BUF_SIZE 2080

char rbuf[MAX_BUF_SIZE + 1];
char wbuf[MAX_BUF_SIZE + 1];

enum html_state_t {
    FREE,
    TAG,
    DROP
} state;

/*
struct stack_node {
    struct stack_node * next;
    char tag[MAX_TAG_SIZE + 1];
} bottom;

struct stack_node * stack_top;
*/

void win() {
    fprintf(stderr, "You have obtained code execution.");
}

void raiseErr(char * err_str) {
    fprintf(stderr, "%s\n", err_str);
    exit(EXIT_FAILURE);
}

int Open(const char * path, int oflag) {
    int rv;
    if (oflag & O_CREAT) {
        rv = open(path, oflag, S_IWUSR | S_IRUSR);
    } else {
        rv = open(path, oflag);
    }
    if (rv < 0) {
        fprintf(stderr, "Cannot open file %s\n", path);
        exit(EXIT_FAILURE);
    }
    return rv;
}

int Dup2(int from_fd, int to_fd) {
    int rv = dup2(from_fd, to_fd);
    if (rv < 0) {
        raiseErr("Dup2 failed.");
    }
    return rv;
}

int Read(void *buf, size_t nbyte) {
    int rv = read(0, buf, nbyte);
    if (rv < 0) {
        raiseErr("Read Error");
    }
    return rv;
}

int Write(const void *buf, size_t nbyte) {
    int rv = write(1, buf, nbyte);
    if (rv < 0) {
        raiseErr("Write error.");
    }
    return rv;
}

void parseArgs(int argc, char * argv[]) {
    int i;
    // Parse the parameters
    for (i = 1; i < argc; i++) {
        if (strcmp("-i", argv[i]) == 0) {
            if (i + 1 < argc) {
                Dup2(Open(argv[i + 1], O_RDONLY), 0);
                i++;
            }
        } else if (strcmp("-o", argv[i]) == 0) {
            if (i + 1 < argc) {
                Dup2(Open(argv[i + 1], O_WRONLY | O_CREAT), 1);
                i++;
            }
        }
    }
}

int reachGreatThan(int len, int cursor) {
    while (cursor < len && rbuf[cursor] != '>') {
        ++cursor;
    }
    return cursor;
}

int isAlphaNum(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

int reachEndOfName(int len, int cursor) {
    while (cursor < len && isAlphaNum(rbuf[cursor])) {
        cursor++;
    }
    return cursor;
}

void fetchMore(int * len, int * len_w, int * cursor_r) {
    if (*cursor_r == 0) {
        raiseErr("Don't you think this tag is too long?");
    }
    if (*len_w > 0) {
        Write(wbuf, *len_w);
    }
    memcpy(rbuf, rbuf + *cursor_r, *len - *cursor_r);
    *len += Read(rbuf + (*len - *cursor_r), *cursor_r) - *cursor_r;
    rbuf[*len] = 0;
    fprintf(stderr, "%d byte read, length is %d\n", *cursor_r, *len);
    fprintf(stderr, "--in---\n%s\n", rbuf);
    *cursor_r = 0;
    *len_w = 0;
}

void initiateEndTag(int * cursor_r, int * len_w, int cursor_t, int len) {
    int cursor_n = reachEndOfName(len, *cursor_r + 2);
    int temp_len;
    if ((cursor_n - *cursor_r - 2 == 6) && strncmp(rbuf + *cursor_r + 2, "script", 6) == 0) {
        state = FREE;
        *cursor_r = cursor_t + 1;
    } else {
        temp_len = cursor_n - *cursor_r;
        memcpy(wbuf + *len_w, rbuf + *cursor_r, temp_len);
        *len_w += temp_len;
        *cursor_r = cursor_n;
        state = TAG;
    }
}

void initiateStartTag(int * cursor_r, int * len_w, int cursor_t, int len) {
    int cursor_n = reachEndOfName(len, *cursor_r + 1);
    int temp_len;
    if (rbuf[*cursor_r + 1] == '!') {
        temp_len = cursor_t - *cursor_r + 1;
        memcpy(wbuf + *len_w, rbuf + *cursor_r, temp_len);
        *len_w += temp_len;
        *cursor_r = cursor_t + 1;
    } else if ((cursor_n - *cursor_r - 1 == 6) && strncmp(rbuf + *cursor_r + 1, "script", 6) == 0) {
        if (rbuf[cursor_t - 1] != '/') {
            state = DROP;
        }
        *cursor_r = cursor_t + 1;
    } else {
        temp_len = cursor_n - *cursor_r;
        memcpy(wbuf + *len_w, rbuf + *cursor_r, temp_len);
        *len_w += temp_len;
        *cursor_r = cursor_n;
        state = TAG;
    }
}

int skipSpace(int len, int cursor) {
    while (cursor < len && rbuf[cursor] == ' ') {
        ++cursor;
    }
    return cursor;
}

int reachEndOfAttr(int len, int cursor) {
    char c;
    cursor = skipSpace(len, cursor);
    cursor = reachEndOfName(len, cursor);
    cursor = skipSpace(len, cursor);
    if (rbuf[cursor] != '=') {
        raiseErr("Attribute error!");
    }
    ++cursor;
    cursor = skipSpace(len, cursor);
    if (rbuf[cursor] != '\'' && rbuf[cursor] != '\"') {
        raiseErr("Attribute error!");
    }
    c = rbuf[cursor];
    ++cursor;
    while (cursor < len && rbuf[cursor] != c) {
        ++cursor;
    }
    if (cursor == len) {
        raiseErr("Unclosed quote.");
    }
    return cursor;
}

int parseBuffer(int len) {
    int cursor_r = 0;
    int len_w = 0;
    int cursor_t;
    int cursor_eoa;
    while (cursor_r < len) {
        if (rbuf[cursor_r] != '<') {
            if (state == FREE) {
                wbuf[len_w++] = rbuf[cursor_r++];
            } else if (state == TAG) {
                // find '>'
                if (rbuf[cursor_r] == '>') {
                    state = FREE;
                    wbuf[len_w++] = rbuf[cursor_r++];
                } else if (rbuf[cursor_r] == '/') {
                    wbuf[len_w++] = rbuf[cursor_r++];
                } else {
                    cursor_eoa = reachEndOfAttr(cursor_t, cursor_r);
                    if (strncmp(rbuf + skipSpace(cursor_t, cursor_r), "on", 2) != 0) {
                        memcpy(wbuf + len_w, rbuf + cursor_r, cursor_eoa - cursor_r + 1);
                        len_w += cursor_eoa - cursor_r + 1;
                    }
                    cursor_r = cursor_eoa + 1;
                }
            } else {
                ++cursor_r;
            }
        } else {
            if (state == TAG) {
                raiseErr("Nested tag is not allowed!");
            } else if (state == FREE) {
                cursor_t = reachGreatThan(len, cursor_r);
                if (cursor_t >= len) {
                    // not found
                    fetchMore(&len, &len_w, &cursor_r);
                } else {
                    // found
                    if (rbuf[cursor_r + 1] == '/') {
                        initiateEndTag(&cursor_r, &len_w, cursor_t, len);
                    } else if (isAlphaNum(rbuf[cursor_r + 1])) {
                        initiateStartTag(&cursor_r, &len_w, cursor_t, len);
                    } else {
                        wbuf[len_w++] = rbuf[cursor_r++];
                    }

                }
            } else if (state == DROP) {
                cursor_t = reachGreatThan(len, cursor_r);
                if (cursor_t >= len) {
                    // not found
                    fetchMore(&len, &len_w, &cursor_r);
                } else {
                    // found
                    if (rbuf[cursor_r + 1] == '/') {
                        initiateEndTag(&cursor_r, &len_w, cursor_t, len);
                    } else if (isAlphaNum(rbuf[cursor_r + 1])) {
                        initiateStartTag(&cursor_r, &len_w, cursor_t, len);
                    } else {
                        wbuf[len_w++] = rbuf[cursor_r++];
                    }

                }
            }

        }
    }
    wbuf[len_w] = 0;
    return len_w;
}

int main(int argc, char* argv[]) {
    int i;
    int rv;

    /*
    stack_top = &bottom;

    strncpy(bottom.tag, ">", 2);
    */

    state = FREE;

    parseArgs(argc, argv);

    // read and write
    while ((rv = read(0, rbuf, MAX_BUF_SIZE)) > 0) {
        rbuf[rv] = 0;
        fprintf(stderr, "%d byte read\n", rv);
        fprintf(stderr, "-------\n%s\n", rbuf);
        Write(wbuf, parseBuffer(rv));
    }
    return 0;

}
