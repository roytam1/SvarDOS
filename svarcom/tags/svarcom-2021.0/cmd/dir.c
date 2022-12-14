/* This file is part of the SvarCOM project and is published under the terms
 * of the MIT license.
 *
 * Copyright (C) 2021 Mateusz Viste
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * dir
 *
 * Displays a list of files and subdirectories in a directory.
 *
 * DIR [drive:][path][filename] [/P] [/W] [/A[:]attributes] [/O[[:]sortorder]] [/S] [/B] [/L]
 *
 * /P Pauses after each screenful of information.
 * /W Uses wide list format.
 *
 * /A Displays file with specified attributes:
 *     D Directories           R Read-only files     H Hidden files
 *     A Ready for archiving   S System files        - prefix meaning "not"
 *
 * /O List files in sorted order:
 *     N by name            S by size              E by extension
 *     D by date            G group dirs first     - prefix to reverse order
 *
 * /S Displays files in specified directory and all subdirectories.
 * /B Uses bare format (no heading information or summary)
 * /L Uses lowercases
 */

/* NOTE: /A attributes are matched in an exclusive way, ie. only files with
 *       the specified attributes are matched. This is different from how DOS
 *       itself matches attributes hence DIR cannot rely on the attributes
 *       filter within FindFirst.
 *
 * NOTE: Multiple /A are not supported - only the last one is significant.
 */

#define WCOLWIDTH 15  /* width of a column in wide mode output */


/* fills freebytes with free bytes for drv (A=0, B=1, etc)
 * returns DOS ERR code on failure */
static unsigned short cmd_dir_df(unsigned long *freebytes, unsigned char drv) {
  unsigned short res = 0;
  unsigned short sects_per_clust = 0, avail_clusts = 0, bytes_per_sect = 0;

  _asm {
    push ax
    push bx
    push cx
    push dx

    mov ah, 0x36  /* DOS 2+ -- Get Disk Free Space */
    mov dl, [drv] /* A=1, B=2, etc (0 = DEFAULT DRIVE) */
    inc dl
    int 0x21      /* AX=sects_per_clust, BX=avail_clusts, CX=bytes_per_sect, DX=tot_clusters */
    cmp ax, 0xffff /* AX=0xffff on error (invalid drive) */
    jne COMPUTEDF
    mov [res], 0x0f /* fill res with DOS error code 15 ("invalid drive") */
    jmp DONE

    COMPUTEDF:
    /* freebytes = AX * BX * CX */
    mov [sects_per_clust], ax
    mov [avail_clusts], bx
    mov [bytes_per_sect], cx

    DONE:
    pop dx
    pop cx
    pop bx
    pop ax
  }

  /* multiple steps to avoid uint16 overflow */
  *freebytes = sects_per_clust;
  *freebytes *= avail_clusts;
  *freebytes *= bytes_per_sect;

  return(res);
}


static int cmd_dir(struct cmd_funcparam *p) {
  const char *filespecptr = NULL;
  struct DTA *dta = (void *)0x80; /* set DTA to its default location at 80h in PSP */
  unsigned short i;
  unsigned short availrows;  /* counter of available rows on display (used for /P) */
  unsigned short wcols = screen_getwidth() / WCOLWIDTH; /* number of columns in wide mode */
  unsigned char wcolcount;
  struct nls_patterns *nls = (void *)(p->BUFFER + (p->BUFFERSZ / 2));
  char *buff2 = p->BUFFER + (p->BUFFERSZ / 2) + sizeof(*nls);
  unsigned long summary_fcount = 0;
  unsigned long summary_totsz = 0;
  unsigned char drv = 0;

  #define DIR_FLAG_PAUSE  1
  #define DIR_FLAG_RECUR  4
  #define DIR_FLAG_LCASE  8
  unsigned char flags = 0;

  #define DIR_OUTPUT_NORM 1
  #define DIR_OUTPUT_WIDE 2
  #define DIR_OUTPUT_BARE 3
  unsigned char format = DIR_OUTPUT_NORM;

  if (cmd_ishlp(p)) {
    outputnl("Displays a list of files and subdirectories in a directory");
    outputnl("");
    outputnl("DIR [drive:][path][filename] [/P] [/W] [/A[:]attributes] [/O[[:]sortorder]] [/S] [/B] [/L]");
    outputnl("");
    outputnl("/P Pauses after each screenful of information");
    outputnl("/W Uses wide list format");
    outputnl("");
    outputnl("/A Displays files with specified attributes:");
    outputnl("    D Directories            R Read-only files        H Hidden files");
    outputnl("    A Ready for archiving    S System files           - prefix meaning \"not\"");
    outputnl("");
    outputnl("/O List files in sorted order:");
    outputnl("    N by name                S by size                E by extension");
    outputnl("    D by date                G group dirs first       - prefix to reverse order");
    outputnl("");
    outputnl("/S Displays files in specified directory and all subdirectories");
    outputnl("/B Uses bare format (no heading information or summary)");
    outputnl("/L Uses lowercases");
    return(-1);
  }

  i = nls_getpatterns(nls);
  if (i != 0) outputnl(doserr(i));

  /* parse command line */
  for (i = 0; i < p->argc; i++) {
    if (p->argv[i][0] == '/') {
      char arg;
      char neg = 0;
      /* detect negations and get actual argument */
      if (p->argv[i][1] == '-') neg = 1;
      arg = p->argv[i][1 + neg];
      /* */
      switch (arg) {
        case 'a':
        case 'A':
          /* TODO */
          outputnl("/A NOT IMPLEMENTED YET");
          return(-1);
          break;
        case 'b':
        case 'B':
          format = DIR_OUTPUT_BARE;
          break;
        case 'l':
        case 'L':
          flags |= DIR_FLAG_LCASE;
          break;
        case 'o':
        case 'O':
          /* TODO */
          outputnl("/O NOT IMPLEMENTED YET");
          return(-1);
          break;
        case 'p':
        case 'P':
          flags |= DIR_FLAG_PAUSE;
          if (neg) flags &= (0xff ^ DIR_FLAG_PAUSE);
          break;
        case 's':
        case 'S':
          /* TODO */
          outputnl("/S NOT IMPLEMENTED YET");
          return(-1);
          break;
        case 'w':
        case 'W':
          format = DIR_OUTPUT_WIDE;
          break;
        default:
          outputnl("Invalid switch");
          return(-1);
      }
    } else {  /* filespec */
      if (filespecptr != NULL) {
        outputnl("Too many parameters");
        return(-1);
      }
      filespecptr = p->argv[i];
    }
  }

  if (filespecptr == NULL) filespecptr = ".";

  /* special case: "DIR drive:" (truename() fails on "C:" under MS-DOS 6.0) */
  if ((filespecptr[0] != 0) && (filespecptr[1] == ':') && (filespecptr[2] == 0)) {
    if ((filespecptr[0] >= 'a') && (filespecptr[0] <= 'z')) {
      p->BUFFER[0] = filespecptr[0] - ('a' - 1);
    } else {
      p->BUFFER[0] = filespecptr[0] - ('A' - 1);
    }
    i = curpathfordrv(p->BUFFER, p->BUFFER[0]);
  } else {
    i = file_truename(filespecptr, p->BUFFER);
  }
  if (i != 0) {
    outputnl(doserr(i));
    return(-1);
  }

  if (format != DIR_OUTPUT_BARE) {
    drv = p->BUFFER[0];
    if (drv >= 'a') {
      drv -= 'a';
    } else {
      drv -= 'A';
    }
    cmd_vol_internal(drv, buff2);
    sprintf(buff2, "Directory of %s", p->BUFFER);
    /* trim at first '?', if any */
    for (i = 0; buff2[i] != 0; i++) if (buff2[i] == '?') buff2[i] = 0;
    outputnl(buff2);
    outputnl("");
  }

  /* if dir: append a backslash (also get its len) */
  i = path_appendbkslash_if_dir(p->BUFFER);

  /* if ends with a \ then append ????????.??? */
  if (p->BUFFER[i - 1] == '\\') strcat(p->BUFFER, "????????.???");

  i = findfirst(dta, p->BUFFER, DOS_ATTR_RO | DOS_ATTR_HID | DOS_ATTR_SYS | DOS_ATTR_DIR | DOS_ATTR_ARC);
  if (i != 0) {
    outputnl(doserr(i));
    return(-1);
  }

  availrows = screen_getheight();
  wcolcount = 0; /* may be used for columns counting with wide mode */

  do {
    if (flags & DIR_FLAG_LCASE) _strlwr(dta->fname); /* OpenWatcom extension, probably does not care about NLS so results may be odd with non-A-Z characters... */

    summary_fcount++;
    if ((dta->attr & DOS_ATTR_DIR) == 0) summary_totsz += dta->size;

    switch (format) {
      case DIR_OUTPUT_NORM:
        /* print fname-space-extension (unless it's "." or "..", then print as-is) */
        if (dta->fname[0] == '.') {
          output(dta->fname);
          i = strlen(dta->fname);
          while (i++ < 12) output(" ");
        } else {
          file_fname2fcb(buff2, dta->fname);
          memmove(buff2 + 9, buff2 + 8, 4);
          buff2[8] = ' ';
          output(buff2);
        }
        output(" ");
        /* either <DIR> or right aligned 10-chars byte size */
        memset(buff2, ' ', 10);
        if (dta->attr & DOS_ATTR_DIR) {
          strcpy(buff2 + 10, "<DIR>");
        } else {
          _ultoa(dta->size, buff2 + 10, 10); /* OpenWatcom extension */
        }
        output(buff2 + strlen(buff2) - 10);
        /* two spaces and NLS DATE */
        buff2[0] = ' ';
        buff2[1] = ' ';
        nls_format_date(buff2 + 2, dta->date_yr + 1980, dta->date_mo, dta->date_dy, nls);
        output(buff2);

        /* one space and NLS TIME */
        nls_format_time(buff2 + 1, dta->time_hour, dta->time_min, 0xff, nls);
        outputnl(buff2);
        break;

      case DIR_OUTPUT_WIDE: /* display in columns of 12 chars per item */
        i = strlen(dta->fname);
        if (dta->attr & DOS_ATTR_DIR) {
          i += 2;
          output("[");
          output(dta->fname);
          output("]");
        } else {
          output(dta->fname);
        }
        while (i++ < WCOLWIDTH) output(" ");
        if (++wcolcount == wcols) {
          wcolcount = 0;
          outputnl("");
        }
        break;

      case DIR_OUTPUT_BARE:
        outputnl(dta->fname);
        break;
    }

    if ((flags & DIR_FLAG_PAUSE) && (--availrows < 2)) {
      press_any_key();
      availrows = screen_getheight();
    }

  } while (findnext(dta) == 0);

  if (wcolcount != 0) outputnl(""); /* in wide mode make sure to end on a clear row */

  /* print out summary (unless bare output mode) */
  if (format != DIR_OUTPUT_BARE) {
    unsigned short alignpos;
    /* x file(s) */
    memset(buff2, ' ', 13); /* 13 is the max len of a 32 bit number with thousand separators (4'000'000'000) */
    i = nls_format_number(buff2 + 13, summary_fcount, nls);
    alignpos = sprintf(buff2 + 13 + i, " %s ", "file(s)");
    output(buff2 + i);
    /* xxxx bytes */
    i = nls_format_number(buff2 + 13, summary_totsz, nls);
    output(buff2 + i);
    output(" ");
    outputnl("bytes");
    /* xxxx bytes free */
    i = cmd_dir_df(&summary_totsz, drv);
    if (i != 0) outputnl(doserr(i));
    alignpos += 13 + 13;
    memset(buff2, ' ', alignpos); /* align the freebytes value to same column as totbytes */
    i = nls_format_number(buff2 + alignpos, summary_totsz, nls);
    output(buff2 + i);
    output(" ");
    outputnl("bytes free");
  }

  return(-1);
}
