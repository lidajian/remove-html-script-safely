#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

// if capital character, to lower case
#define TOLOWER(c) ((c >= 'A' && c <= 'Z') ? (c + 32) : c)

// 1 if is alphanumeric, 0 otherwise
#define ISALPHANUM(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? 1 : 0)

// 1 if is space, 0 otherwise
#define ISSPACE(c) ((c == ' ' || c == '\n' || c == '\t') ? 1 : 0)

// 1 if is quote, 0 otherwise
#define ISQUOTE(c) ((c == '\'' || c == '\"') ? 1 : 0)

#define BUFFER_INCREMENT 5

/*
 * Global variables
 */

// length of buffers
size_t len_buf = 0;

// read buffer
char * rbuf = NULL;

// write buffer
char * wbuf = NULL;

/*
 * state of state machine
 * ACCEPT: move character from read buffer to write buffer in this state, should detect '<'
 * INTAG:  in a pair of <>, should detect attributes that contains javascript
 * DROP:   skip the current character because '<script>', should detect '</script>'
 */
enum {
    ACCEPT,
    INTAG,
    DROP
} state;

/*
 * Win function
 */

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
 * Utility functions
 */

/*
 * parseArgs: parse arguments from command line
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

/*
 * strncmp_lower: lower case string compare, translate capital letter in
 *                str1 to lower case and compare with letter in str2
 */
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

/*
 * reachGreatThan: Advance the cursor to the first occurrence of '>'
 */
char * reachGreatThan(char * buf_end, char * cursor) {
    while (cursor < buf_end && *cursor != '>') {
        ++cursor;
    }
    return cursor;
}

/*
 * reachEndOfName: Advance the cursor to the first non-alphanumeric character
 */
char * reachEndOfName(char * buf_end, char * cursor) {
    while (cursor < buf_end && ISALPHANUM(*cursor)) {
        cursor++;
    }
    return cursor;
}

/*
 * skipSpace: Advance the cursor to the first character that is NOT space
 */
char * skipSpace(char * buf_end, char * cursor) {
    while (cursor < buf_end && ISSPACE(*cursor)) {
        ++cursor;
    }
    return cursor;
}

/*
 * fetchMore: The current tag is not complete, read more data into the buffer.
 *            If the the first character is '<' we extend the buffers and
 *            refill the buffer. Otherwise, we move '<' to the beginning of
 *            read buffer and refill the read buffer.
 */
void fetchMore(char ** rbuf_end, char ** cursor_w, char ** cursor_r) {
    int rv;
    char * old_rbuf, * old_wbuf;
    if (*cursor_r == rbuf) {
        // if '<' at the beginning of buffer, try to extend buffer using realloc
        old_rbuf = rbuf;
        old_wbuf = wbuf;

        // Extend buffer
        len_buf += BUFFER_INCREMENT;
        rbuf = Realloc(rbuf, len_buf);
        wbuf = Realloc(wbuf, len_buf);

        // Migrate pointers to new buffers
        *cursor_r = rbuf + (*cursor_r - old_rbuf);
        *cursor_w = wbuf + (*cursor_w - old_wbuf);
        *rbuf_end = rbuf + (*rbuf_end - old_rbuf);

        // Read addictional trunk
        rv = Read(*rbuf_end, len_buf - (*rbuf_end - rbuf));

        if (rv == 0) {
            raiseErr("Unclosed tag.");
        }
        *rbuf_end += rv;

        fprintf(stderr, "Realloc: %d byte read, length is %ld\n", rv, *rbuf_end - rbuf);
        fprintf(stderr, "-------\n[%s]\n", rbuf);

    } else {
        // if '<' not at the beginning of buffer, try to move '<' to beginning
        // and read cursor_r byte
        if (*cursor_w > wbuf) {
            Write(wbuf, *cursor_w - wbuf);
            fprintf(stderr, "Written: %ld\n", *cursor_w - wbuf);
            *cursor_w = wbuf;
        }

        // move incomplete data to the beginning of buffer
        memcpy(rbuf, *cursor_r, *rbuf_end - *cursor_r);

        // read data
        rv = Read(rbuf + (*rbuf_end - *cursor_r), *cursor_r - rbuf);
        if (rv == 0) {
            raiseErr("Unclosed tag.");
        }
        *rbuf_end += rv - (*cursor_r - rbuf);

        fprintf(stderr, "%d byte read, length is %ld\n", rv, *rbuf_end - rbuf);
        fprintf(stderr, "-------\n[%s]\n", rbuf);

        // reset cursor of read buffer
        *cursor_r = rbuf;
    }
}

/*
 * initiateEndTag: Change state base on the tag name. If tag name is 'script'
 *                 change state back to ACCEPT, otherwise set state to INTAG.
 */
void initiateEndTag(char ** cursor_r, char ** cursor_w, char * cursor_t) {
    char * cursor_n = reachEndOfName(cursor_t, *cursor_r + 2);
    int temp_len;
    if ((cursor_n - *cursor_r - 2 == 6) && strncmp_lower(*cursor_r + 2, "script", 6) == 0) {
        // </script...>: set state ACCEPT and step through it
        state = ACCEPT;
        *cursor_r = cursor_t + 1;
    } else {
        // set state as INTAG, copy to write buffer and step pass the name
        temp_len = cursor_n - *cursor_r;
        memcpy(*cursor_w, *cursor_r, temp_len);
        *cursor_w += temp_len;
        *cursor_r = cursor_n;
        state = INTAG;
    }
}

/*
 * initiateStartTag: Chage state base on the tag name. If tag name is 'script'
 *                   change state to DROP, if is comment or something copy
 *                   directly, otherwise set state to INTAG
 */
void initiateStartTag(char ** cursor_r, char ** cursor_w, char * cursor_t) {
    char * cursor_n = reachEndOfName(cursor_t, *cursor_r + 1);
    int temp_len;
    if (*(*cursor_r + 1) == '!') {
        // <!...> tag: simply step pass it
        temp_len = cursor_t - *cursor_r + 1;
        memcpy(*cursor_w, *cursor_r, temp_len);
        *cursor_w += temp_len;
        *cursor_r = cursor_t + 1;
    } else if ((cursor_n - *cursor_r - 1 == 6) && strncmp_lower(*cursor_r + 1, "script", 6) == 0) {
        // <script...>: set state as DROP, step pass it
        if (*(cursor_t - 1) != '/') {
            state = DROP;
        }
        *cursor_r = cursor_t + 1;
    } else {
        // set state as INTAG, copy to write buffer and step pass the name
        temp_len = cursor_n - *cursor_r;
        memcpy(*cursor_w, *cursor_r, temp_len);
        *cursor_w += temp_len;
        *cursor_r = cursor_n;
        state = INTAG;
    }
}

/*
 * reachEndOfAttr: Advance the cursor to the end of Attribute
 */
char * reachEndOfAttr(char * rbuf_end, char * cursor, int * skip_attribute) {
    char c;
    int is_sensitive_tag = 0;

    // skip space
    cursor = skipSpace(rbuf_end, cursor);

    if (strncmp_lower(cursor, "on", 2) == 0) {
        *skip_attribute = 1;
    } else if (strncmp_lower(cursor, "href", 4) == 0 ||
            strncmp_lower(cursor, "action", 6) == 0 ||
            strncmp_lower(cursor, "formaction", 10) == 0) {
        is_sensitive_tag = 1;
    }

    // step through the name of attribute
    cursor = reachEndOfName(rbuf_end, cursor);

    // skip space
    cursor = skipSpace(rbuf_end, cursor);

    // match an '='
    if (*(cursor++) != '=') {
        raiseErr("Attribute error!");
    }

    // skip space
    cursor = skipSpace(rbuf_end, cursor);

    // match quote sign
    if (!ISQUOTE(*cursor)) {
        raiseErr("Attribute error!");
    }
    c = *(cursor++);

    // skip space
    cursor = skipSpace(rbuf_end, cursor);

    if (is_sensitive_tag && (strncmp_lower(cursor, "javascript:", 11) == 0)) {
        *skip_attribute = 1;
    }

    // match another quote sign
    while (cursor < rbuf_end && *cursor != c) {
        ++cursor;
    }
    if (cursor == rbuf_end) {
        raiseErr("Unclosed quote.");
    }
    return cursor;
}

/*
 * parseBuffer: Filter content in read buffer and put filtered content in write buffer
 */
char * parseBuffer(char * rbuf_end) {
    char * cursor_r = rbuf;
    char * cursor_w = wbuf;
    int skip_attribute = 0;
    char * cursor_t;
    char * cursor_eoa;
    while (cursor_r < rbuf_end) {
        if (*cursor_r == '<') {
            if (state == INTAG) {
                raiseErr("Nested tag is not allowed!");
            } else if (state == ACCEPT) {
                cursor_t = reachGreatThan(rbuf_end, cursor_r);
                if (cursor_t >= rbuf_end) {
                    // not found
                    fetchMore(&rbuf_end, &cursor_w, &cursor_r);
                } else {
                    // found
                    if (*(cursor_r + 1) == '/') {
                        initiateEndTag(&cursor_r, &cursor_w, cursor_t);
                    } else if (ISALPHANUM(*(cursor_r + 1))) {
                        initiateStartTag(&cursor_r, &cursor_w, cursor_t);
                    } else {
                        memcpy(cursor_w, cursor_r, cursor_t - cursor_r + 1);
                        cursor_w += cursor_t - cursor_r + 1;
                        cursor_r = cursor_t + 1;
                    }
                }
            } else if (state == DROP) {
                cursor_t = reachGreatThan(rbuf_end, cursor_r);
                if (cursor_t >= rbuf_end) {
                    // not found
                    fetchMore(&rbuf_end, &cursor_w, &cursor_r);
                } else {
                    // found
                    if (*(cursor_r + 1) == '/') {
                        initiateEndTag(&cursor_r, &cursor_w, cursor_t);
                    } else if (ISALPHANUM(*(cursor_r + 1))) {
                        initiateStartTag(&cursor_r, &cursor_w, cursor_t);
                    } else {
                        memcpy(cursor_w, cursor_r, cursor_t - cursor_r + 1);
                        cursor_w += cursor_t - cursor_r + 1;
                        cursor_r = cursor_t + 1;
                    }
                }
            }
        } else {
            if (state == ACCEPT) {
                *(cursor_w++) = *(cursor_r++);
            } else if (state == INTAG) {
                // find '>'
                if (*(cursor_r) == '>') {
                    state = ACCEPT;
                    *(cursor_w++) = *(cursor_r++);
                } else if (*(cursor_r) == '/') {
                    *(cursor_w++) = *(cursor_r++);
                } else {
                    skip_attribute = 0;
                    cursor_eoa = reachEndOfAttr(cursor_t, cursor_r, &skip_attribute);
                    if (!skip_attribute) {
                        memcpy(cursor_w, cursor_r, cursor_eoa - cursor_r + 1);
                        cursor_w += cursor_eoa - cursor_r + 1;
                    }
                    cursor_r = cursor_eoa + 1;
                }
            } else {
                ++cursor_r;
            }
        }
    }
    fprintf(stderr, "Written: %ld, %s\n", cursor_w - wbuf, wbuf);
    return cursor_w;
}

/*
 * Main function
 */

int main(int argc, char* argv[]) {
    int rv;
    char * wbuf_end;

    // Initialize the state to ACCEPT
    state = ACCEPT;

    // Initialize the buffer
    len_buf = BUFFER_INCREMENT;
    rbuf = Malloc(len_buf);
    wbuf = Malloc(len_buf);

    // Parse arguments
    parseArgs(argc, argv);

    // Read a block from input, process and write to output
    while ((rv = read(0, rbuf, len_buf)) > 0) {
        fprintf(stderr, "%d byte read\n-------\n[%s]\n", rv, rbuf);
        // wbuf may change in parseBuffer
        wbuf_end = parseBuffer(rbuf + rv);
        Write(wbuf, wbuf_end - wbuf);
    }
    return 0;

}
