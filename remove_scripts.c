#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

// if capital character, to lower case
#define TOLOWER(c) ((c >= 'A' && c <= 'Z') ? (c + 32) : c)

// 1 if is alphanumeric, 0 otherwise
#define ISALPHANUM(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? 1 : 0)

#define BUFFER_INCREMENT 5

size_t len_buf = 0;
char * rbuf = NULL;
char * wbuf = NULL;

enum {
    FREE,
    TAG,
    DROP
} state;

void win() {
    fprintf(stderr, "You have obtained code execution.");
}

/*
 * Wrapped helper functions
 */

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

void * Malloc(size_t size) {
    void * p = malloc(size);
    if (p == NULL) {
        raiseErr("Not enough space.");
    }
    return p;
}

void * Realloc(void * ptr, size_t size) {
    void * p = realloc(ptr, size);
    if (p == NULL) {
        raiseErr("Not enough space.");
    }
    return p;
}

/*
 * End of wrapped functions
 */

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

// case insensitive string compare
// str2 should be lower case string
int strncmp_lower(const char * str1, const char * str2, size_t num) {
    char c;
    while (num != 0) {
        c = *str1;
        if (TOLOWER(c) != *str2) {
            return -1;
        }
        ++str1;
        ++str2;
        --num;
    }
    return 0;
}

// Advance the cursor to the first occurrence of '>'
int reachGreatThan(int len, int cursor) {
    while (cursor < len && rbuf[cursor] != '>') {
        ++cursor;
    }
    return cursor;
}

// Advance the cursor to the first non-alphanumeric character
int reachEndOfName(int len, int cursor) {
    while (cursor < len && ISALPHANUM(rbuf[cursor])) {
        cursor++;
    }
    return cursor;
}

// The current tag is not complete, read more data into the buffer
void fetchMore(int * len, int * len_w, int * cursor_r) {
    int rv;
    if (*cursor_r == 0) {
        // if '<' at the beginning of buffer, try to extend buffer using realloc

        // Extend buffer
        len_buf += BUFFER_INCREMENT;
        rbuf = Realloc(rbuf, len_buf);
        wbuf = Realloc(wbuf, len_buf);

        // Read addictional trunk
        rv = Read(rbuf + *len, len_buf - 1 - *len);

        if (rv == 0) {
            raiseErr("Unclosed tag.");
        }
        *len += rv;
        rbuf[*len] = 0;

        fprintf(stderr, "Realloc: %d byte read, length is %d\n", rv, *len);
        fprintf(stderr, "-------\n[%s]\n", rbuf);

    } else {
        // if '<' not at the beginning of buffer, try to move '<' to beginning
        // and read cursor_r byte
        if (*len_w > 0) {
            Write(wbuf, *len_w);
            fprintf(stderr, "Written: %d\n", *len_w);
            *len_w = 0;
        }

        // move incomplete data to the beginning of buffer
        memcpy(rbuf, rbuf + *cursor_r, *len - *cursor_r);

        // read data
        rv = Read(rbuf + (*len - *cursor_r), *cursor_r);
        if (rv == 0) {
            raiseErr("Unclosed tag.");
        }
        *len += rv - *cursor_r;
        rbuf[*len] = 0;

        fprintf(stderr, "%d byte read, length is %d\n", rv, *len);
        fprintf(stderr, "-------\n[%s]\n", rbuf);

        // reset cursor of read buffer
        *cursor_r = 0;
    }
}

void initiateEndTag(int * cursor_r, int * len_w, int cursor_t, int len) {
    int cursor_n = reachEndOfName(len, *cursor_r + 2);
    int temp_len;
    if ((cursor_n - *cursor_r - 2 == 6) && strncmp_lower(rbuf + *cursor_r + 2, "script", 6) == 0) {
        // </script...>: set state FREE and step through it
        state = FREE;
        *cursor_r = cursor_t + 1;
    } else {
        // set state as TAG, copy to write buffer and step pass the name
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
        // <!...> tag: simply step pass it
        temp_len = cursor_t - *cursor_r + 1;
        memcpy(wbuf + *len_w, rbuf + *cursor_r, temp_len);
        *len_w += temp_len;
        *cursor_r = cursor_t + 1;
    } else if ((cursor_n - *cursor_r - 1 == 6) && strncmp_lower(rbuf + *cursor_r + 1, "script", 6) == 0) {
        // <script...>: set state as DROP, step pass it
        if (rbuf[cursor_t - 1] != '/') {
            state = DROP;
        }
        *cursor_r = cursor_t + 1;
    } else {
        // set state as TAG, copy to write buffer and step pass the name
        temp_len = cursor_n - *cursor_r;
        memcpy(wbuf + *len_w, rbuf + *cursor_r, temp_len);
        *len_w += temp_len;
        *cursor_r = cursor_n;
        state = TAG;
    }
}

// Advance the cursor to the first character that is NOT space
int skipSpace(int len, int cursor) {
    while (cursor < len && rbuf[cursor] == ' ') {
        ++cursor;
    }
    return cursor;
}

// Advance the cursor to the end of Attribute
int reachEndOfAttr(int len, int cursor, int * skip_attribute) {
    char c;
    int is_sensitive_tag = 0;

    // skip space
    cursor = skipSpace(len, cursor);

    if (strncmp_lower(rbuf + cursor, "on", 2) == 0) {
        *skip_attribute = 1;
    } else if (strncmp_lower(rbuf + cursor, "href", 4) == 0) {
        is_sensitive_tag = 1;
    }

    // step through the name of attribute
    cursor = reachEndOfName(len, cursor);

    // skip space
    cursor = skipSpace(len, cursor);

    // match an '='
    if (rbuf[cursor++] != '=') {
        raiseErr("Attribute error!");
    }

    // skip space
    cursor = skipSpace(len, cursor);

    // match quote sign
    if (rbuf[cursor] != '\'' && rbuf[cursor] != '\"') {
        raiseErr("Attribute error!");
    }
    c = rbuf[cursor++];

    // skip space
    cursor = skipSpace(len, cursor);

    if (is_sensitive_tag && (strncmp_lower(rbuf + cursor, "javascript:", 11) == 0)) {
        *skip_attribute = 1;
    }

    // match another quote sign
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
    int skip_attribute = 0;
    int cursor_t;
    int cursor_eoa;
    while (cursor_r < len) {
        if (rbuf[cursor_r] == '<') {
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
                    } else if (ISALPHANUM(rbuf[cursor_r + 1])) {
                        initiateStartTag(&cursor_r, &len_w, cursor_t, len);
                    } else {
                        memcpy(wbuf + len_w, rbuf + cursor_r, cursor_t - cursor_r + 1);
                        len_w += cursor_t - cursor_r + 1;
                        cursor_r = cursor_t + 1;
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
                    } else if (ISALPHANUM(rbuf[cursor_r + 1])) {
                        initiateStartTag(&cursor_r, &len_w, cursor_t, len);
                    } else {
                        memcpy(wbuf + len_w, rbuf + cursor_r, cursor_t - cursor_r + 1);
                        len_w += cursor_t - cursor_r + 1;
                        cursor_r = cursor_t + 1;
                    }
                }
            }
        } else {
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
                    skip_attribute = 0;
                    cursor_eoa = reachEndOfAttr(cursor_t, cursor_r, &skip_attribute);
                    if (!skip_attribute) {
                        memcpy(wbuf + len_w, rbuf + cursor_r, cursor_eoa - cursor_r + 1);
                        len_w += cursor_eoa - cursor_r + 1;
                    }
                    cursor_r = cursor_eoa + 1;
                }
            } else {
                ++cursor_r;
            }
        }
    }
    wbuf[len_w] = 0;
    fprintf(stderr, "Written: %d, %s\n", len_w, wbuf);
    return len_w;
}

int main(int argc, char* argv[]) {
    int rv;

    // Initialize the state to FREE
    state = FREE;

    // Initialize the buffer
    len_buf = 1 + BUFFER_INCREMENT;
    rbuf = Malloc(len_buf);
    wbuf = Malloc(len_buf);

    // Parse arguments
    parseArgs(argc, argv);

    // Read a block from input, process and write to output
    while ((rv = read(0, rbuf, len_buf - 1)) > 0) {
        rbuf[rv] = 0;
        fprintf(stderr, "%d byte read\n-------\n[%s]\n", rv, rbuf);
        // wbuf may change in parseBuffer
        rv = parseBuffer(rv);
        Write(wbuf, rv);
    }
    return 0;

}
