/* #define USTR_CONF_COMPILE_USE_INLINE 0 */
#include "ustr.h"

#include <getopt.h>
#include <errno.h>
#include <ctype.h>

static int always_raw = USTR_FALSE;
static int interp     = USTR_FALSE;
static int fnames     = USTR_FALSE;

static void die(const char *prog_name, const char *msg)
{
  fprintf(stderr, "%s: %s\n", prog_name, msg);
  exit (EXIT_FAILURE);
}

static void usage(const char *prog_name, int xcode)
{
  fprintf((xcode == EXIT_SUCCESS) ? stdout : stderr, "\
 Format: %s [-rhV] <string> [string]...\n\
    --filenames -f = Open filenames instead of taking the arg values.\n\
    --interpret -i = Interpret input as C escapes.\n\
    --raw       -r = Always output raw, even if utf8_valid() fails.\n\
           -h = Print help.\n\
           -V = Print version.\n\
", prog_name ? prog_name : "ustr");
  
  exit (xcode);
}

/* NOTE: copied from netstr ... */
/* This function is implemented showing how you can simply use the Ustr as
   a container for storing a growable byte array+length, and then easily do
   the more complex operations the same way you would if you managed the
   entire thing by hand using just a malloc() and storing the length
   separately. */
static int cescape_decode(Ustr **ps1)
{ /* Convert \n \t etc. into their correct bytes. */
  size_t fpos = 0;
  size_t len = ustr_len(*ps1);
  char  *src = NULL;
  char  *dst = NULL;

  if (!(fpos = ustr_srch_chr_fwd(*ps1, 0, '\\')) || (fpos == len))
    return (USTR_TRUE);

  if (!(dst = src = ustr_sc_wstr(ps1)))
    return (USTR_FALSE);

  --fpos;
  src += fpos;
  dst += fpos;
  len -= fpos;
  while (len--)
  {
    if ((*src != '\\') || !len)
    {
     normal:
      *dst++ = *src++;
    }
    else
    {
      --len; ++src;
      switch (*src)
      {
        default:
          goto normal;

        case '\n': ++src; break;
        case '"':  ++src; break;
        case '\'': ++src; break;
          
        case 'b':  ++src; *dst++ = '\b'; break;
        case 'f':  ++src; *dst++ = '\f'; break;
        case 'n':  ++src; *dst++ = '\n'; break;
        case 'r':  ++src; *dst++ = '\r'; break;
        case 't':  ++src; *dst++ = '\t'; break;
        case 'v':  ++src; *dst++ = '\v'; break;
        case '\\': ++src; *dst++ = '\\'; break;

        case 'o':
        case 'O':
          if ((len < 1) || !isdigit((unsigned char)src[1]))
          {
            *dst++ = '\\';
            goto normal;
          }
          --len; ++src;

          /* FALL THROUGH */

        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
        {
          unsigned char val = 0;

          if (USTR_FALSE)
          { }
          else if ((len >= 2) && isdigit((unsigned char)src[1]) &&
                   isdigit((unsigned char)src[2]))
          {
            val =  (*src++ - '0');
            val *= 8;
            val += (*src++ - '0');
            val *= 8;
            val += (*src++ - '0');
            len -= 2;
          }
          else if ((len >= 1) && isdigit((unsigned char)src[1]))
          {
            val =  (*src++ - '0');
            val *= 8;
            val += (*src++ - '0');
            len -= 1;
          }
          else
            val = (*src++ - '0');

          *dst++ = val;
          break;
        }
        
        case 'x':
        case 'X':
        {
          unsigned char val = 0;
          Ustr *bconv = USTR1(\x16, "0123456789ABCDEFabcdef");
          Ustr *uconv = USTR1(\x10, "0123456789ABCDEF");
          Ustr *lconv = USTR1(\x6, "abcdef");
          size_t pos = 0;
          
          if ((len < 2) || /* only does \xDD and not \xDDDD like C */
              !ustr_srch_chr_fwd(bconv, 0, src[1]) ||
              !ustr_srch_chr_fwd(bconv, 0, src[2]))
          {
            *dst++ = '\\';
            goto normal;
          }

          if (!(pos = ustr_srch_chr_fwd(uconv, 0, src[1])))
            pos = ustr_srch_chr_fwd(lconv, 0, src[1]) + 10;
          val = pos - 1;
          val *= 16;
          if (!(pos = ustr_srch_chr_fwd(uconv, 0, src[2])))
            pos = ustr_srch_chr_fwd(lconv, 0, src[2]) + 10;
          val += pos - 1;
          *dst++ = val;
          
          len -= 2;
          src += 3;
        }
        break;
      }
    }
  }

  ustr_del(ps1, src - dst); /* chop the end, if we converted */
  
  return (USTR_TRUE);
}

/* NOTE: copied from custr ... */
static int cescape_encode(Ustr **ret, const char *src, size_t len)
{ /* Convert to a C-style string ... lowest common denominator,
   * mostly use: \xXX. */
  const char *last = NULL;

  if (!ustr_sc_ensure_owner(ret))
    return (USTR_FALSE);
  
  ustr_add_rep_chr(ret, '"', 1);

  last = src;
  while (len--)
  {
    if ((*src == 0x20) || (*src == 0x21) || /* not 0x22 == " */
        ((*src >= 0x23) && (*src <= 0x7E)))
      ++src;
    else
    {
      if (last != src)
        ustr_add_buf(ret, last, src - last);

      switch (*src)
      {
        case '\b': ustr_add_cstr(ret, "\\b"); break;
        case '\f': ustr_add_cstr(ret, "\\f"); break;
        case '\n': ustr_add_cstr(ret, "\\n"); break;
        case '\r': ustr_add_cstr(ret, "\\r"); break;
        case '\t': ustr_add_cstr(ret, "\\t"); break;
        case '\v': ustr_add_cstr(ret, "\\v"); break;
        default:
          if (!len || /* no problems with merging */
              !(((src[1] >= '0') && (src[1] <= '9')) ||
                ((src[1] >= 'A') && (src[1] <= 'F')) ||
                ((src[1] >= 'a') && (src[1] <= 'f'))))
          {
            if (!*src)
              ustr_add_cstr(ret, "\\0");
            else
              ustr_add_fmt(ret, "\\x%02hhX", *src);
          }
          else if (!*src)
            ustr_add_cstr(ret, "\" \"\\0\" \"");
          else
            ustr_add_fmt(ret, "\\x%02hhX\" \"", *src);
      }

      ++src;

      last = src;
    }
  }

  if (last != src)
    ustr_add_buf(ret, last, src - last);
  
  ustr_add_rep_chr(ret, '"', 1);
  
  return (!ustr_enomem(*ret));
}

static Ustr *gen_ustr_buf(const char *buf, size_t len)
{
  Ustr *ret = USTR_NULL;
  
  if (!(ret = ustr_dup_buf(buf, len)))
    goto fail_mem;
  
  if (interp)
  {
    if (!cescape_decode(&ret))
      goto fail_mem;
  }
  
  return (ret);

 fail_mem:
  ustr_free(ret);

  return (USTR_NULL);
}

static Ustr *gen_ustr_file(const char *fname)
{
  Ustr *ret = USTR("");
  
  if (!ustr_io_getfilename(&ret, fname))
    goto fail_mem;
  
  if (interp)
  {
    if (!cescape_decode(&ret))
      goto fail_mem;
  }
  
  return (ret);

 fail_mem:
  ustr_free(ret);

  return (USTR_NULL);
}

static void proc_data(const char *prog_name, Ustr *arg)
{
  int valid         = USTR_FALSE;
  size_t len        = 0;
  size_t utf8_len   = 0;
  size_t utf8_width = 0;
  
  if (!arg)
    die(prog_name, strerror(errno));
  
  valid      = ustr_utf8_valid(arg);
  len        = ustr_len(arg);
  utf8_len   = ustr_utf8_len(arg);
  utf8_width = ustr_utf8_width(arg);
  
  printf("Input: ");
  if ((always_raw || valid) && !ustr_io_putfileline(&arg, stdout))
    die(prog_name, strerror(errno));
  printf("Valid: %s\nLen: %zu\nUTF-8 Len: %zu\nWidth: %zu\n",
         valid ? "yes" : "no", len, utf8_len, utf8_width);
  
  ustr_free(arg);
}


int main(int argc, char *argv[])
{
  struct option long_options[] =
  {
   {"always-raw", no_argument, NULL, 'r'},
   {"filenames", no_argument, NULL, 'f'},
   {"interpret", no_argument, NULL, 'i'},
   
   {"help", no_argument, NULL, 'h'},
   {"version", no_argument, NULL, 'V'},
   
   {NULL, 0, NULL, 0}
  };
  int scan = 0;
  int optchar = -1;
  const char *prog_name = NULL;
  
  if (!argc)
    usage(NULL, EXIT_FAILURE);
  
  if ((prog_name = strrchr(argv[0], '/')))
    ++prog_name;
  else
    prog_name = argv[0];
  
  while ((optchar = getopt_long(argc, argv, "firhV", long_options, NULL)) != -1)
    switch (optchar)
    {
      case '?': usage(prog_name, EXIT_FAILURE);
      case 'h': usage(prog_name, EXIT_SUCCESS);
      case 'V':
        printf("%s version %s\n", prog_name, "1.0.0");
        exit (EXIT_SUCCESS);

      case 'f': fnames      = !fnames;    break;
      case 'i': interp     = !interp;     break;
      case 'r': always_raw = !always_raw; break;
    }

  argc -= optind;
  argv += optind;

  if (!argc)
    usage(prog_name, EXIT_FAILURE);

  if (fnames)
    while (scan < argc)
      proc_data(prog_name, gen_ustr_file(argv[scan++]));
  else
    while (scan < argc)
    {
      proc_data(prog_name, gen_ustr_buf(argv[scan], strlen(argv[scan])));
      ++scan;
    }
  
  USTR_CNTL_MALLOC_CHECK_END();
  
  return (EXIT_SUCCESS);
}

