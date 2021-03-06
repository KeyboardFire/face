/*
 * face - fairly awful c-based esolang
 * Copyright (C) 2017  Keyboard Fire <andy@keyboardfire.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ffi.h>
#if __GNUC__ >= 8
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#endif

#include "face.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NVARS 128

#define ARG(n) (vars[(int)data[ip + (n)]])
#define ARG1 ARG(-1)
#define ARG2 ARG(-2)
#define ARG3 ARG(-3)
#define ARG4 ARG(-4)

#define OARG(n) (vars_orig[(int)data[ip + (n)]])
#define OARG1 OARG(-1)
#define OARG2 OARG(-2)
#define OARG3 OARG(-3)
#define OARG4 OARG(-4)

#define TYPE_SIZE ( \
    nummode == CHAR    ? sizeof(char)        : \
    nummode == SHORT   ? sizeof(short)       : \
    nummode == INT     ? sizeof(int)         : \
    nummode == LONG    ? sizeof(long)        : \
    nummode == LLONG   ? sizeof(long long)   : \
    nummode == FLOAT   ? sizeof(float)       : \
    nummode == DOUBLE  ? sizeof(double)      : \
    nummode == LDOUBLE ? sizeof(long double) : \
    nummode == PTR     ? sizeof(void*)       : \
    0 \
    )

#define DEREF_AS(type, x) ( \
    nummode == CHAR    ? (numsigned ? (type)*(signed char*)(x)      : (type)*(unsigned char*)(x))      : \
    nummode == SHORT   ? (numsigned ? (type)*(signed short*)(x)     : (type)*(unsigned short*)(x))     : \
    nummode == INT     ? (numsigned ? (type)*(signed int*)(x)       : (type)*(unsigned int*)(x))       : \
    nummode == LONG    ? (numsigned ? (type)*(signed long*)(x)      : (type)*(unsigned long*)(x))      : \
    nummode == LLONG   ? (numsigned ? (type)*(signed long long*)(x) : (type)*(unsigned long long*)(x)) : \
    nummode == FLOAT   ? (type)*(float*)(x)       : \
    nummode == DOUBLE  ? (type)*(double*)(x)      : \
    nummode == LDOUBLE ? (type)*(long double*)(x) : \
    (type)0 \
    )

#define TF(x) (\
    nummode == CHAR    ? (numsigned ? (*(signed char*)(x)      ? 1 : 0) : (*(unsigned char*)(x)      ? 1 : 0)) : \
    nummode == SHORT   ? (numsigned ? (*(signed short*)(x)     ? 1 : 0) : (*(unsigned short*)(x)     ? 1 : 0)) : \
    nummode == INT     ? (numsigned ? (*(signed int*)(x)       ? 1 : 0) : (*(unsigned int*)(x)       ? 1 : 0)) : \
    nummode == LONG    ? (numsigned ? (*(signed long*)(x)      ? 1 : 0) : (*(unsigned long*)(x)      ? 1 : 0)) : \
    nummode == LLONG   ? (numsigned ? (*(signed long long*)(x) ? 1 : 0) : (*(unsigned long long*)(x) ? 1 : 0)) : \
    nummode == FLOAT   ? (*(float*)(x)       ? 1 : 0) : \
    nummode == DOUBLE  ? (*(double*)(x)      ? 1 : 0) : \
    nummode == LDOUBLE ? (*(long double*)(x) ? 1 : 0) : \
    nummode == PTR     ? (*(void**)(x)       ? 1 : 0) : \
    0 \
    )

#define OP1(res, op, x) do { \
    if (nummode == CHAR) { \
        if (numsigned) { \
            *((signed char*)(res)) = op *((signed char*)(x)); \
        } else { \
            *((unsigned char*)(res)) = op *((unsigned char*)(x)); \
        } \
    } else if (nummode == SHORT) { \
        if (numsigned) { \
            *((signed short*)(res)) = op *((signed short*)(x)); \
        } else { \
            *((unsigned short*)(res)) = op *((unsigned short*)(x)); \
        } \
    } else if (nummode == INT) { \
        if (numsigned) { \
            *((signed int*)(res)) = op *((signed int*)(x)); \
        } else { \
            *((unsigned int*)(res)) = op *((unsigned int*)(x)); \
        } \
    } else if (nummode == LONG) { \
        if (numsigned) { \
            *((signed long*)(res)) = op *((signed long*)(x)); \
        } else { \
            *((unsigned long*)(res)) = op *((unsigned long*)(x)); \
        } \
    } else if (nummode == LLONG) { \
        if (numsigned) { \
            *((signed long long*)(res)) = op *((signed long long*)(x)); \
        } else { \
            *((unsigned long long*)(res)) = op *((unsigned long long*)(x)); \
        } \
    } else if (nummode == FLOAT) { \
        *((float*)(res)) = op *((float*)(x)); \
    } else if (nummode == DOUBLE) { \
        *((double*)(res)) = op *((double*)(x)); \
    } else if (nummode == LDOUBLE) { \
        *((long double*)(res)) = op *((long double*)(x)); \
    } \
} while (0)

#define OP1_INT(res, op, x) do { \
    if (nummode == CHAR) { \
        if (numsigned) { \
            *((signed char*)(res)) = op *((signed char*)(x)); \
        } else { \
            *((unsigned char*)(res)) = op *((unsigned char*)(x)); \
        } \
    } else if (nummode == SHORT) { \
        if (numsigned) { \
            *((signed short*)(res)) = op *((signed short*)(x)); \
        } else { \
            *((unsigned short*)(res)) = op *((unsigned short*)(x)); \
        } \
    } else if (nummode == INT) { \
        if (numsigned) { \
            *((signed int*)(res)) = op *((signed int*)(x)); \
        } else { \
            *((unsigned int*)(res)) = op *((unsigned int*)(x)); \
        } \
    } else if (nummode == LONG) { \
        if (numsigned) { \
            *((signed long*)(res)) = op *((signed long*)(x)); \
        } else { \
            *((unsigned long*)(res)) = op *((unsigned long*)(x)); \
        } \
    } else if (nummode == LLONG) { \
        if (numsigned) { \
            *((signed long long*)(res)) = op *((signed long long*)(x)); \
        } else { \
            *((unsigned long long*)(res)) = op *((unsigned long long*)(x)); \
        } \
    } \
} while (0)

#define OP2(res, x, op, y) do { \
    if (nummode == CHAR) { \
        if (numsigned) { \
            *((signed char*)(res)) = *((signed char*)(x)) op *((signed char*)(y)); \
        } else { \
            *((unsigned char*)(res)) = *((unsigned char*)(x)) op *((unsigned char*)(y)); \
        } \
    } else if (nummode == SHORT) { \
        if (numsigned) { \
            *((signed short*)(res)) = *((signed short*)(x)) op *((signed short*)(y)); \
        } else { \
            *((unsigned short*)(res)) = *((unsigned short*)(x)) op *((unsigned short*)(y)); \
        } \
    } else if (nummode == INT) { \
        if (numsigned) { \
            *((signed int*)(res)) = *((signed int*)(x)) op *((signed int*)(y)); \
        } else { \
            *((unsigned int*)(res)) = *((unsigned int*)(x)) op *((unsigned int*)(y)); \
        } \
    } else if (nummode == LONG) { \
        if (numsigned) { \
            *((signed long*)(res)) = *((signed long*)(x)) op *((signed long*)(y)); \
        } else { \
            *((unsigned long*)(res)) = *((unsigned long*)(x)) op *((unsigned long*)(y)); \
        } \
    } else if (nummode == LLONG) { \
        if (numsigned) { \
            *((signed long long*)(res)) = *((signed long long*)(x)) op *((signed long long*)(y)); \
        } else { \
            *((unsigned long long*)(res)) = *((unsigned long long*)(x)) op *((unsigned long long*)(y)); \
        } \
    } else if (nummode == FLOAT) { \
        *((float*)(res)) = *((float*)(x)) op *((float*)(y)); \
    } else if (nummode == DOUBLE) { \
        *((double*)(res)) = *((double*)(x)) op *((double*)(y)); \
    } else if (nummode == LDOUBLE) { \
        *((long double*)(res)) = *((long double*)(x)) op *((long double*)(y)); \
    } \
} while (0)

#define OP2_INT(res, x, op, y) do { \
    if (nummode == CHAR) { \
        if (numsigned) { \
            *((signed char*)(res)) = *((signed char*)(x)) op *((signed char*)(y)); \
        } else { \
            *((unsigned char*)(res)) = *((unsigned char*)(x)) op *((unsigned char*)(y)); \
        } \
    } else if (nummode == SHORT) { \
        if (numsigned) { \
            *((signed short*)(res)) = *((signed short*)(x)) op *((signed short*)(y)); \
        } else { \
            *((unsigned short*)(res)) = *((unsigned short*)(x)) op *((unsigned short*)(y)); \
        } \
    } else if (nummode == INT) { \
        if (numsigned) { \
            *((signed int*)(res)) = *((signed int*)(x)) op *((signed int*)(y)); \
        } else { \
            *((unsigned int*)(res)) = *((unsigned int*)(x)) op *((unsigned int*)(y)); \
        } \
    } else if (nummode == LONG) { \
        if (numsigned) { \
            *((signed long*)(res)) = *((signed long*)(x)) op *((signed long*)(y)); \
        } else { \
            *((unsigned long*)(res)) = *((unsigned long*)(x)) op *((unsigned long*)(y)); \
        } \
    } else if (nummode == LLONG) { \
        if (numsigned) { \
            *((signed long long*)(res)) = *((signed long long*)(x)) op *((signed long long*)(y)); \
        } else { \
            *((unsigned long long*)(res)) = *((unsigned long long*)(x)) op *((unsigned long long*)(y)); \
        } \
    } \
} while (0)

#define PTR_ADD(res, x, n) do { \
         if (nummode == CHAR)    { (res) = (char*)(x)        + (n); } \
    else if (nummode == SHORT)   { (res) = (short*)(x)       + (n); } \
    else if (nummode == INT)     { (res) = (int*)(x)         + (n); } \
    else if (nummode == LONG)    { (res) = (long*)(x)        + (n); } \
    else if (nummode == LLONG)   { (res) = (long long*)(x)   + (n); } \
    else if (nummode == FLOAT)   { (res) = (float*)(x)       + (n); } \
    else if (nummode == DOUBLE)  { (res) = (double*)(x)      + (n); } \
    else if (nummode == LDOUBLE) { (res) = (long double*)(x) + (n); } \
    else if (nummode == PTR)     { (res) = (void**)(x)       + (n); } \
} while (0)

#define ASSIGN(x, val) do { \
    if (nummode == CHAR) { \
        if (numsigned) *(signed char*)(x)        = (signed char)(val); \
        else           *(unsigned char*)(x)      = (unsigned char)(val); \
    } else if (nummode == SHORT) { \
        if (numsigned) *(signed short*)(x)       = (signed short)(val); \
        else           *(unsigned short*)(x)     = (unsigned short)(val); \
    } else if (nummode == INT) { \
        if (numsigned) *(signed int*)(x)         = (signed int)(val); \
        else           *(unsigned int*)(x)       = (unsigned int)(val); \
    } else if (nummode == LONG) { \
        if (numsigned) *(signed long*)(x)        = (signed long)(val); \
        else           *(unsigned long*)(x)      = (unsigned long)(val); \
    } else if (nummode == LLONG) { \
        if (numsigned) *(signed long long*)(x)   = (signed long long)(val); \
        else           *(unsigned long long*)(x) = (unsigned long long)(val); \
    } else if (nummode == FLOAT) { \
        *(float*)(x)       = (float)(val); \
    } else if (nummode == DOUBLE) { \
        *(double*)(x)      = (double)(val); \
    } else if (nummode == LDOUBLE) { \
        *(long double*)(x) = (long double)(val); \
    } \
} while (0)

#define FFUNC1(func, res, arg) do { \
    if (nummode == FLOAT) { \
        *(float*)(res) = (func ## f)(*(float*)arg); \
    } else if (nummode == DOUBLE) { \
        *(double*)(res) = func(*(double*)arg); \
    } else if (nummode == LDOUBLE) { \
        *(long double*)(res) = (func ## l)(*(long double*)arg); \
    } else { \
        ASSIGN((res), func(DEREF_AS(double, (arg)))); \
    } \
} while (0)

#define FFUNC2(func, res, arg1, arg2) do { \
    if (nummode == FLOAT) { \
        *(float*)(res) = (func ## f)(*(float*)(arg1), *(float*)(arg2)); \
    } else if (nummode == DOUBLE) { \
        *(double*)(res) = func(*(double*)(arg1), *(double*)(arg2)); \
    } else if (nummode == LDOUBLE) { \
        *(long double*)(res) = (func ## l)(*(long double*)(arg1), *(long double*)(arg2)); \
    } else { \
        ASSIGN((res), func(DEREF_AS(double, (arg1)), DEREF_AS(double, (arg2)))); \
    } \
} while (0)

enum nummode {
    CHAR,
    SHORT,
    INT,
    LONG,
    LLONG,
    FLOAT,
    DOUBLE,
    LDOUBLE,
    PTR
};

/*
 * are there duplicate pointers to ptr in vars_orig? this is used to determine
 * whether to free stuff
 */
int dups(void **vars_orig, char *data, size_t *ip, void *ptr) {
    if (ptr == data || ptr == ip) return 1;
    int found = 0;
    for (int i = 0; i < NVARS; ++i) {
        if (vars_orig[i] == ptr) {
            if (found) return 1;
            found = 1;
        }
    }
    return 0;
}

size_t preprocess(char *data, size_t data_len) {
    size_t i, j;
    for (i = 0, j = 0; i < data_len; ++i, ++j) {
        if (data[i] == '@') data[j] = '\0';
        else if (data[i] == '#') {
            char c = data[++i];
            data[j] = ('?' <= c && c <= '_' && c != '@') ? c ^ 0x40 : c;
        } else data[j] = data[i];
    }
    return j;
}

/*
 * the main entry point for outside callers - runs data as face code with the
 * given arguments
 */
void face_run(char *data, size_t data_len, int argc, char **argv, int debug) {
    data_len = preprocess(data, data_len);

    // instruction pointer - where we are inside of data
    size_t ip = 0;

    // variables - correspond to ASCII chars
    void* vars[NVARS] = { NULL };
    void* vars_orig[NVARS] = { NULL };

    // what kind of thing are we interpreting numbers as?
    enum nummode nummode = INT;
    int numsigned = 1;

    while (ip < data_len) {
        if (debug) {
            fprintf(stderr, "%c", data[ip]);
            fflush(stderr);
        }
        switch (data[ip]) {

        case '!':
            // logical NOT
            ip += 3;
            OP1(ARG2, !, ARG1);
            break;

        case '"':
            // shift pointer
            ip += 3;
            PTR_ADD(ARG2, ARG2, DEREF_AS(int, ARG1));
            break;

        case '$':
            // assign pointer
            ip += 3;
            if (!dups(vars_orig, data, &ip, OARG2)) free(OARG2);
            OARG2 = OARG1;
            ARG2 = ARG1;
            break;

        case '%':
            // modulo
            ip += 4;
            if (nummode == FLOAT) {
                *(float*)ARG3 = fmodf(*(float*)ARG2, *(float*)ARG1);
            } else if (nummode == DOUBLE) {
                *(double*)ARG3 = fmod(*(double*)ARG2, *(double*)ARG1);
            } else if (nummode == LDOUBLE) {
                *(long double*)ARG3 = fmodl(*(long double*)ARG2, *(long double*)ARG1);
            } else OP2_INT(ARG3, ARG2, %, ARG1);
            break;

        case '&':
            // bitwise AND
            ip += 4;
            OP2_INT(ARG3, ARG2, &, ARG1);
            break;

        case '\'':
            // increment pointer
            ip += 2;
            PTR_ADD(ARG1, ARG1, 1);
            break;

        case '(':
            // comment
            while (data[ip] != ')' && ip < data_len) ++ip;
            break;

        case '*':
            // multiplication
            ip += 4;
            OP2(ARG3, ARG2, *, ARG1);
            break;

        case '+':
            // addition
            ip += 4;
            OP2(ARG3, ARG2, +, ARG1);
            break;

        case ',':
            // change num mode / signedness
            ip += 2;
            switch (data[ip-1]) {
                case 'c': nummode = CHAR; numsigned = 0; break;
                case 'C': nummode = CHAR; numsigned = 1; break;
                case 's': nummode = SHORT; numsigned = 0; break;
                case 'S': nummode = SHORT; numsigned = 1; break;
                case 'i': nummode = INT; numsigned = 0; break;
                case 'I': nummode = INT; numsigned = 1; break;
                case 'l': nummode = LONG; numsigned = 0; break;
                case 'L': nummode = LONG; numsigned = 1; break;
                case 'm': nummode = LLONG; numsigned = 0; break;
                case 'M': nummode = LLONG; numsigned = 1; break;
                case 'f': nummode = FLOAT; numsigned = 0; break;
                case 'F': nummode = FLOAT; numsigned = 1; break;
                case 'd': nummode = DOUBLE; numsigned = 0; break;
                case 'D': nummode = DOUBLE; numsigned = 1; break;
                case 'e': nummode = LDOUBLE; numsigned = 0; break;
                case 'E': nummode = LDOUBLE; numsigned = 1; break;
                case 'p': nummode = PTR; numsigned = 0; break;
                case 'P': nummode = PTR; numsigned = 1; break;
            }
            break;

        case '-':
            // subtraction
            ip += 4;
            OP2(ARG3, ARG2, -, ARG1);
            break;

        case '.':
            // unconditional jump
            ip += 2;
            goto jump;

        case '/':
            // division
            ip += 4;
            OP2(ARG3, ARG2, /, ARG1);
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ip += 2;
            ASSIGN(ARG1, data[ip-2] - '0');
            break;

        case ':':
            // label, skip the name
            ip += 2;
            break;

        case ';':
            // type conversion
            ip += 3;
            switch (data[ip-2]) {
                case 'c': *(unsigned char*)     ARG1 = DEREF_AS(unsigned char,      ARG1); break;
                case 'C': *(signed char*)       ARG1 = DEREF_AS(signed char,        ARG1); break;
                case 's': *(unsigned short*)    ARG1 = DEREF_AS(unsigned short,     ARG1); break;
                case 'S': *(signed short*)      ARG1 = DEREF_AS(signed short,       ARG1); break;
                case 'i': *(unsigned int*)      ARG1 = DEREF_AS(unsigned int,       ARG1); break;
                case 'I': *(signed int*)        ARG1 = DEREF_AS(signed int,         ARG1); break;
                case 'l': *(unsigned long*)     ARG1 = DEREF_AS(unsigned long,      ARG1); break;
                case 'L': *(signed long*)       ARG1 = DEREF_AS(signed long,        ARG1); break;
                case 'm': *(unsigned long long*)ARG1 = DEREF_AS(unsigned long long, ARG1); break;
                case 'M': *(signed long long*)  ARG1 = DEREF_AS(signed long long,   ARG1); break;
                case 'f':
                case 'F': *(float*)             ARG1 = DEREF_AS(float,              ARG1); break;
                case 'd':
                case 'D': *(double*)            ARG1 = DEREF_AS(double,             ARG1); break;
                case 'e':
                case 'E': *(long double*)       ARG1 = DEREF_AS(long double,        ARG1); break;
            }
            break;

        case '<':
            // less than
            ip += 4;
            OP2(ARG3, ARG2, <, ARG1);
            break;

        case '=':
            // equal to
            ip += 4;
            OP2(ARG3, ARG2, ==, ARG1);
            break;

        case '>':
            // greater than
            ip += 4;
            OP2(ARG3, ARG2, >, ARG1);
            break;

        case '?':
            // conditional jump
            ip += 3;
            if (TF(ARG2)) {
jump:
                for (char lbl = data[ip-1];;) {
                    if (ip == data_len) ip = 0;
                    if (data[ip] == ':' && data[ip+1] == lbl) break;
                    ++ip;
                }
            }
            break;

        case 'A':
            switch (data[++ip]) {
            case 'c':
                ip += 2;
                ASSIGN(ARG1, argc);
                break;
            case 'v':
                ip += 2;
                ARG1 = OARG1 = argv;
                break;
            }
            break;

        case 'M':
            switch (data[++ip]) {
            case '(': // it looks like a C and both c and C are taken
                ip += 3;
                FFUNC1(ceil, ARG2, ARG1);
                break;
            case 'C':
                ip += 3;
                FFUNC1(acos, ARG2, ARG1);
                break;
            case 'M':
                ip += 4;
                if (nummode == CHAR || nummode == SHORT || nummode == INT ||
                        nummode == LONG || nummode == LLONG) {
                    long long a = DEREF_AS(long long, ARG2),
                              b = DEREF_AS(long long, ARG1);
                    ASSIGN(ARG3, a > b ? a : b);
                } else FFUNC2(fmax, ARG3, ARG2, ARG1);
                break;
            case 'S':
                ip += 3;
                FFUNC1(asin, ARG2, ARG1);
                break;
            case 'T':
                ip += 3;
                FFUNC1(atan, ARG2, ARG1);
                break;
            case '^':
                ip += 4;
                FFUNC2(pow, ARG3, ARG2, ARG1);
                break;
            case 'a':
                ip += 3;
                if (nummode == CHAR || nummode == SHORT) {
                    ASSIGN(ARG2, DEREF_AS(int, ARG1));
                } else if (nummode == INT) {
                    *(int*)ARG2 = abs(*(int*)ARG1);
                } else if (nummode == LONG) {
                    *(long*)ARG2 = labs(*(long*)ARG1);
                } else if (nummode == LLONG) {
                    *(long long*)ARG2 = llabs(*(long long*)ARG1);
                } else FFUNC1(fabs, ARG2, ARG1);
                break;
            case 'c':
                ip += 3;
                FFUNC1(cos, ARG2, ARG1);
                break;
            case 'e':
                ip += 3;
                FFUNC1(exp, ARG2, ARG1);
                break;
            case 'f':
                ip += 3;
                FFUNC1(floor, ARG2, ARG1);
                break;
            case 'l':
                ip += 3;
                FFUNC1(log, ARG2, ARG1);
                break;
            case 'm':
                ip += 4;
                if (nummode == CHAR || nummode == SHORT || nummode == INT ||
                        nummode == LONG || nummode == LLONG) {
                    long long a = DEREF_AS(long long, ARG2),
                              b = DEREF_AS(long long, ARG1);
                    ASSIGN(ARG3, a < b ? a : b);
                } else FFUNC2(fmin, ARG3, ARG2, ARG1);
                break;
            case 'p':
                ip += 2;
                ASSIGN(ARG1, M_PI);
                break;
            case 'r':
                ip += 3;
                FFUNC1(round, ARG2, ARG1);
                break;
            case 's':
                ip += 3;
                FFUNC1(sin, ARG2, ARG1);
                break;
            case 't':
                ip += 3;
                FFUNC1(tan, ARG2, ARG1);
                break;
            }
            break;

        case 'R':
            switch (data[++ip]) {
            case 'm':
                ip += 2;
                ASSIGN(ARG1, RAND_MAX);
                break;
            case 'r':
                ip += 2;
                ASSIGN(ARG1, rand());
                break;
            case 's':
                ip += 2;
                srand(DEREF_AS(unsigned int, ARG1));
                break;
            }
            break;

        case 'S':
            switch (data[++ip]) {
            case 'c':
                ip += 4;
                ASSIGN(ARG3, strcmp(ARG2, ARG1));
                break;
            case 'l':
                ip += 3;
                ASSIGN(ARG2, strlen(ARG1));
                break;
            }
            break;

        case '[':
            // deref (*)
            ip += 3;
            ARG2 = OARG2 = *(void**)ARG1;
            break;

        case '\\':
            // assign pointers to source code and instruction pointer
            ip += 3;
            if (!dups(vars_orig, data, &ip, OARG2)) free(OARG2);
            if (!dups(vars_orig, data, &ip, OARG1)) free(OARG1);
            ARG2 = OARG2 = data;
            ARG1 = OARG1 = &ip;
            break;

        case ']':
            // reference-of (&)
            ip += 3;
            ARG2 = OARG2 = &ARG1;
            break;

        case '^':
            // bitwise XOR
            ip += 4;
            OP2_INT(ARG3, ARG2, ^, ARG1);
            break;

        case '_':
            // "rewind" / reset to original
            ip += 2;
            ARG1 = OARG1;
            break;

        case '`':
            // decrement pointer
            ip += 2;
            PTR_ADD(ARG1, ARG1, -1);
            break;

        case 'c':
            // calloc
            ip += 3;
            if (!dups(vars_orig, data, &ip, OARG2)) free(OARG2);
            ARG2 = OARG2 = calloc(DEREF_AS(size_t, ARG1), TYPE_SIZE);
            break;

        case 'e':
            // get stderr handle
            ip += 2;
            ARG1 = OARG1 = stderr;
            break;

        case 'i':
            // get stdin handle
            ip += 2;
            ARG1 = OARG1 = stdin;
            break;

        case 'm':
            // malloc/realloc
            ip += 3;
            if (dups(vars_orig, data, &ip, OARG2)) {
                OARG2 = malloc(DEREF_AS(size_t, ARG1) * TYPE_SIZE);
            } else {
                OARG2 = realloc(OARG2, DEREF_AS(size_t, ARG1) * TYPE_SIZE);
            }
            ARG2 = OARG2;
            break;

        case 'o':
            // get stdout handle
            ip += 2;
            ARG1 = OARG1 = stdout;
            break;

        case 'p': {
            // sprintf - acts kinda like asprintf
            ip += 4;
            ffi_cif cif;
            int count = 0, i;
            char d;
            for (i = ip; d = data[i] | 0x20,
                d == 'c' || d == 's' || d == 'i' || d == 'l' || d == 'm' ||
                d == 'f' || d == 'd' || d == 'e' || d == 'p'; i += 2, ++count);
            ffi_type **atypes = malloc((3 + count) * sizeof *atypes);
            void **avalues = malloc((3 + count) * sizeof *avalues);
            void *np = 0;
            unsigned long size = 0;
            atypes[0] = &ffi_type_pointer;
            avalues[0] = &np;
            atypes[1] = &ffi_type_ulong; // this should really be size_t
            avalues[1] = &size;
            atypes[2] = &ffi_type_pointer;
            avalues[2] = &ARG1;
            for (int n = 0; n < count; ++n) {
                switch (data[ip + n*2]) {
                case 'c': atypes[n+3] = &ffi_type_schar;      break;
                case 'C': atypes[n+3] = &ffi_type_uchar;      break;
                case 's': atypes[n+3] = &ffi_type_sshort;     break;
                case 'S': atypes[n+3] = &ffi_type_ushort;     break;
                case 'i': atypes[n+3] = &ffi_type_sint;       break;
                case 'I': atypes[n+3] = &ffi_type_uint;       break;
                case 'l': atypes[n+3] = &ffi_type_slong;      break;
                case 'L': atypes[n+3] = &ffi_type_ulong;      break;
                case 'm': atypes[n+3] = &ffi_type_sint64;     break; // this is bad
                case 'M': atypes[n+3] = &ffi_type_uint64;     break;
                case 'f':
                case 'F': atypes[n+3] = &ffi_type_float;      break;
                case 'd':
                case 'D': atypes[n+3] = &ffi_type_double;     break;
                case 'e':
                case 'E': atypes[n+3] = &ffi_type_longdouble; break;
                case 'p':
                case 'P': atypes[n+3] = &ffi_type_pointer;    break;
                }
                avalues[n+3] = (data[ip + n*2] | 0x20) == 'p' ?
                    &ARG(n*2 + 1) : ARG(n*2 + 1);
            }
            // call snprintf for the first time, to figure out how much space
            // we're gonna need to allocate
            ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 3, 3 + count,
                    &ffi_type_sint, atypes);
            ffi_arg res;
            ffi_call(&cif, (void(*)())snprintf, &res, avalues);
            size = ((int)res) + 1;
            // basically copy/paste of malloc command
            if (dups(vars_orig, data, &ip, OARG2)) {
                OARG2 = malloc(size * TYPE_SIZE);
            } else {
                OARG2 = realloc(OARG2, size * TYPE_SIZE);
            }
            ARG2 = OARG2;
            // call snprintf for the second time
            avalues[0] = &ARG2;
            ip = i;
            ffi_call(&cif, (void(*)())snprintf, &res, avalues);
            ASSIGN(ARG(-3 - count*2), (int)res);
            break;
        }

        case 'q':
            // quit
            return;

        case 'r':
            // read
            ip += 5;
            ASSIGN(ARG4, fread(ARG3, 1, DEREF_AS(size_t, ARG2), ARG1));
            break;

        case 's': {
            // sscanf
            ip += 4;
            ffi_cif cif;
            int count = 0, i;
            for (i = ip; (data[i] | 0x20) == 'p'; i += 2, ++count);
            ffi_type **atypes = malloc((2 + count) * sizeof *atypes);
            void **avalues = malloc((2 + count) * sizeof *avalues);
            atypes[0] = &ffi_type_pointer;
            avalues[0] = &ARG2;
            atypes[1] = &ffi_type_pointer;
            avalues[1] = &ARG1;
            for (int n = 0; n < count; ++n) {
                atypes[n+2] = &ffi_type_pointer;
                avalues[n+2] = &ARG(n*2 + 1);
            }
            ip = i;
            ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 2, 2 + count,
                    &ffi_type_sint, atypes);
            ffi_arg res;
            ffi_call(&cif, (void(*)())sscanf, &res, avalues);
            ASSIGN(ARG(-3 - count*2), (int)res);
            break;
        }

        case 'w':
            // write
            ip += 5;
            ASSIGN(ARG4, fwrite(ARG3, 1, DEREF_AS(size_t, ARG2), ARG1));
            break;

        case '{':
            ip += 2;
            OARG1 = ARG1;
            break;

        case '|':
            // bitwise OR
            ip += 4;
            OP2_INT(ARG3, ARG2, |, ARG1);
            break;

        case '}':
            ip += 3;
            ASSIGN(ARG2, ((char*)ARG1 - (char*)OARG1) / TYPE_SIZE);
            break;

        case '~':
            // bitwise NOT
            ip += 3;
            OP1_INT(ARG2, ~, ARG1);
            break;

        default:
            ++ip;

        }
    }
}
