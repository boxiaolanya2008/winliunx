#ifndef WINLINUX_TESTS_ASSERT_H
#define WINLINUX_TESTS_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

static int g_failures;
static int g_checks;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__,      \
                    #cond);                                                \
        }                                                                  \
    } while (0)

#define CHECK_STREQ(a, b)                                                  \
    do {                                                                   \
        ++g_checks;                                                        \
        const char *va = (a);                                              \
        const char *vb = (b);                                              \
        if (va == NULL || vb == NULL || strcmp(va, vb) != 0) {             \
            ++g_failures;                                                  \
            fprintf(stderr, "  FAIL %s:%d: \"%s\" != \"%s\"\n",            \
                    __FILE__, __LINE__, va ? va : "(null)",                \
                    vb ? vb : "(null)");                                   \
        }                                                                  \
    } while (0)

#define REPORT_SUITE(name)                                                 \
    do {                                                                   \
        if (g_failures > 0) {                                              \
            fprintf(stderr, "[%s] %d/%d checks failed\n",                  \
                    name, g_failures, g_checks);                           \
        } else {                                                           \
            printf("[%s] ok (%d checks)\n", name, g_checks);               \
        }                                                                  \
    } while (0)

#endif
