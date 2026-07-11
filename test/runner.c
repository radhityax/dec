#include <stdio.h>

int test_util(void);
int test_toml(void);
int test_template(void);

int
main(void)
{
    int failures = 0, total = 0;

#define RUN_TEST(name) do { \
    total++; \
    printf("[%d] " #name "... ", total); \
    if (test_##name() != 0) { \
        printf("FAIL\n"); \
        failures++; \
    } else \
        printf("OK\n"); \
} while (0)

    RUN_TEST(util);
    RUN_TEST(toml);
    RUN_TEST(template);

    printf("\n%d/%d tests passed\n", total - failures, total);
    return failures ? 1 : 0;
}
