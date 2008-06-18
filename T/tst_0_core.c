#ifndef USTR_H
#define USTR_H 1

#ifndef USTR_DEBUG
#define USTR_DEBUG 0
#endif

#if USTR_DEBUG
# include "ustr-conf-debug.h"
#else
# include "ustr-conf.h"
#endif

#include "ustr-main.h"
#endif

#define FALSE 0
#define TRUE  1

#undef NDEBUG /* always use assert */
#undef assert
#undef ASSERT
#define assert(x) do {                                                  \
      if (x) {} else {                                                  \
        fprintf(stderr, " -=> ASSERT (%s) failed in (%s) from %d %s.\n", \
                #x , __func__, __LINE__, __FILE__);                     \
        abort(); }                                                      \
    } while (FALSE)
#define ASSERT(x) do {                                                  \
      if (x) {} else {                                                  \
        fprintf(stderr, " -=> ASSERT (%s) failed in (%s) from %d %s.\n", \
                #x , __func__, __LINE__, __FILE__);                     \
        abort(); }                                                      \
    } while (FALSE)

#define ASSERT_EQ(x, y) do {                                            \
      if ((ustr_len(x) == ustr_len(y)) &&                               \
          !memcmp(ustr_cstr(x), ustr_cstr(y), ustr_len(x))) {} else {   \
        fprintf(stderr, " -=> ASSERT_EQ (%zu:\"%s\", %zu:\"%s\") failed " \
                "in (%s) from %d %s.\n",                                \
                ustr_len(x), ustr_cstr(x), ustr_len(y), ustr_cstr(y),   \
                __func__, __LINE__, __FILE__);                          \
        abort(); }                                                      \
    } while (FALSE)
#define assert_eq(x, y) ASSERT_EQ(x, y)

static struct Ustr *s1 = USTR("");
static struct Ustr *s2 = USTR1(\x02, "s2");

int main(void)
{
  struct Ustr *s2_chk1 = USTR1_CHK(\2, "s2");
  struct Ustr *s2_chk2 = USTR1_CHK(\x2, "s2");
  struct Ustr *s2_chk3 = USTR1_CHK(\x02, "s2");
  struct Ustr *s2_chk4 = USTR2_CHK(\0, \x02, "s2");
  struct Ustr *s2_chk8 = USTR4_CHK(\0, \0, \0, \x02, "s2");

  ASSERT(ustr_len(s2_chk1));
  ASSERT(ustr_len(s2_chk2));
  ASSERT(ustr_len(s2_chk3));
  ASSERT(ustr_len(s2_chk4));
  ASSERT(ustr_len(s2_chk8));
  
  assert(USTR_CNTL_MALLOC_CHECK_MEM("")); /* not enabled yet */
  
  assert(!USTR_CNTL_MALLOC_CHECK_BEG(USTR_TRUE));

  ASSERT(ustr_size(s1) == 0);
  ASSERT(ustr_size(s2) == 2);
  ASSERT(ustr_ro(s1));
  ASSERT(ustr_ro(s2));
  ASSERT(ustr_dup(s1) == s1);
  ASSERT(ustr_dup(s2) == s2);
  
  assert(USTR_CNTL_MALLOC_CHECK_MEM_USTR(s2));
  
  ASSERT_EQ(s1, s1);
  ASSERT_EQ(s2, s2);
  ASSERT_EQ(s2, s2_chk1);
  ASSERT_EQ(s2, s2_chk2);
  ASSERT_EQ(s2, s2_chk3);
  ASSERT_EQ(s2, s2_chk4);
  ASSERT_EQ(s2, s2_chk8);
  
  return 0;
}
