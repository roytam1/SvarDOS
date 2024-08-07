/* Sved, the SvarDOS editor
 *
 * Copyright (C) 2023 Mateusz Viste
 *
 * Sved is released under the terms of the MIT license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <dos.h>      /* _dos_open(), _dos_read(), _dos_close(), ... */
#include <fcntl.h>    /* O_RDONLY, O_WRONLY */
#include <string.h>

#include "mdr\bios.h"
#include "mdr\cout.h"
#include "mdr\dos.h"

#include "svarlang\svarlang.h"


#define PVER "2023.1"
#define PDATE "2023"

/*****************************************************************************
 * global variables and definitions                                          *
 *****************************************************************************/

/* preload the mono scheme (to be overloaded at runtime if color adapter present) */
static unsigned char SCHEME_TEXT   = 0x07,
                     SCHEME_MENU   = 0x70,
                     SCHEME_MENU_CUR= 0x0f,
                     SCHEME_MENU_SEL= 0x00,
                     SCHEME_STBAR1 = 0x70,
                     SCHEME_STBAR2 = 0x70, /* greyed out information */
                     SCHEME_STBAR3 = 0x70, /* query */
                     SCHEME_SCROLL = 0x70,
                     SCHEME_MSG    = 0x70,
                     SCHEME_ERR    = 0x70;

static unsigned char screenw, screenh, screenlastrow, screenlastcol;
static unsigned char glob_monomode, glob_tablessmode;

static struct {
    unsigned char from;
    unsigned char to;
    unsigned char statusbar;
} uidirty = {0, 0xff, 1}; /* make sure to redraw entire UI at first run */

#define SCROLL_CURSOR 0xB1

struct line {
  unsigned short len;
  struct line far *next;
  struct line far *prev;
  char payload[1];
};

struct file {
  struct line far *cursor;
  unsigned short xoffset;
  unsigned short cursorposx;
  unsigned short cursorposy;
  unsigned short totlines;
  unsigned short curline;
  unsigned short curline_prev;
  char lfonly;   /* set if line endings are LF (CR/LF otherwise) */
  char modflag;  /* non-zero if file has been modified since last save */
  char modflagprev;
  char fname[128];
};


/*****************************************************************************
 * functions                                                                 *
 *****************************************************************************/

static struct line far *line_calloc(unsigned short siz) {
  struct line far *res;
  unsigned int seg;
  if (_dos_allocmem((sizeof(struct line) + siz + 15) / 16, &seg) != 0) return(NULL);
  res = MK_FP(seg, 0);
  _fmemset(res, 0, sizeof(struct line) + siz);
  return(MK_FP(seg, 0));
}


static void line_free(struct line far *ptr) {
  _dos_freemem(FP_SEG(ptr));
}


static int curline_resize(struct file far *db, unsigned short newsiz) {
  unsigned int maxavail;
  struct line far *newptr;

  /* try resizing the block (much faster) */
  if (_dos_setblock((sizeof(struct line) + newsiz + 15) / 16, FP_SEG(db->cursor), &maxavail) == 0) return(0);

  /* create a new block and copy data over */
  newptr = line_calloc(newsiz);
  if (newptr == NULL) return(-1);
  _fmemmove(newptr, db->cursor, sizeof(struct line) + db->cursor->len);

  /* rewire the linked list */
  db->cursor = newptr;
  if (newptr->next) newptr->next->prev = newptr;
  if (newptr->prev) newptr->prev->next = newptr;

  return(0);
}


/* adds a new line at cursor position into file linked list and advance cursor
 * returns non-zero on error */
static int line_add(struct file *db, const char far *line, unsigned short slen) {
  struct line far *l;

  l = line_calloc(slen);
  if (l == NULL) return(-1);

  l->prev = db->cursor;
  if (db->cursor) {
    l->next = db->cursor->next;
    db->cursor->next = l;
    l->next->prev = l;
  }
  db->cursor = l;
  if (slen > 0) {
    _fmemmove(l->payload, line, slen);
    l->len = slen;
  }

  db->totlines += 1;
  db->curline += 1;

  return(0);
}


static void ui_getstring(const char *query, char *s, unsigned short maxlen) {
  unsigned short len = 0;
  unsigned char y, x;
  int k;

  if (maxlen == 0) return;
  maxlen--; /* make room for the nul terminator */

  y = screenlastrow;

  /* print query string */
  x = mdr_cout_str(y, 0, query, SCHEME_STBAR3, 40);
  mdr_cout_char_rep(y, x++, ' ', SCHEME_STBAR3, screenw - x);

  for (;;) {
    mdr_cout_locate(y, x + len);
    k = mdr_dos_getkey2();

    switch (k) {
      case 0x1b: /* ESC */
        s[0] = 0;
        return;
      case '\r':
        s[len] = 0;
        return;
      case 0x08: /* BKSPC */
        if (len > 0) {
          len--;
          mdr_cout_char(y, x + len, ' ', SCHEME_STBAR3);
        }
        break;
      default:
        if ((k <= 0xff) && (k >= ' ') && (len < maxlen)) {
          mdr_cout_char(y, x + len, k, SCHEME_STBAR3);
          s[len++] = k;
        }
    }
  }

}


/* append a nul-terminated string to line at cursor position */
static int line_append(struct file *f, const char far *buf, unsigned short len) {
  if (sizeof(struct line) + f->cursor->len + len < len) goto ERR; /* overflow check */
  if (curline_resize(f, f->cursor->len + len) != 0) goto ERR;

  _fmemmove(f->cursor->payload + f->cursor->len, buf, len);
  f->cursor->len += len;

  return(0);
  ERR:
  return(-1);
}


static void db_rewind(struct file *db) {
  if (db->cursor == NULL) return;
  while (db->cursor->prev) db->cursor = db->cursor->prev;
  db->curline = 0;
}


static void ui_statusbar(const struct file *db, unsigned char slotnum) {
  const char *s = svarlang_strid(0); /* ESC=MENU */
  unsigned short helpcol = screenw - strlen(s);
  unsigned short col;

  /* slot number (guaranteed to be 0-9) */
  {
    char slot[4] = "#00";
    if (slotnum == 9) {
      slot[1] = '1';
    } else {
      slotnum++;
      slot[2] += slotnum;
    }
    mdr_cout_str(screenlastrow, 0, slot, SCHEME_STBAR2, 3);
  }

  /* fill rest of status bar with background */
  mdr_cout_char_rep(screenlastrow, 3, ' ', SCHEME_STBAR1, helpcol - 3);

  /* eol type */
  {
    const char *eoltype = "CRLF";
    if (db->lfonly) eoltype += 2;
    mdr_cout_str(screenlastrow, helpcol - 5, eoltype, SCHEME_STBAR1, 5);
  }

  /* line numbers */
  {
    unsigned short x;
    unsigned char count = 0;
    col = helpcol - 7;

    x = db->totlines;
    AGAIN:
    do {
      mdr_cout_char(screenlastrow, col--, '0' + (x % 10), SCHEME_STBAR1);
      x /= 10;
    } while (x);
    /* redo same exercise, but printing the current line now */
    if (count == 0) {
      count = 1;
      mdr_cout_char(screenlastrow, col--, '/', SCHEME_STBAR1);
      x = 1 + db->curline;
      goto AGAIN;
    }
  }

  /* filename and modflag */
  {
    const char *fn;
    unsigned short x;
    unsigned short maxfnlen = col - 6;
    if (db->fname[0] == 0) {
      fn = svarlang_str(0, 1); /* "UNTITLED" */
    } else {
      /* display filename up to maxfnlen chars */
      fn = db->fname;
      x = strlen(fn);
      if (x > maxfnlen) fn += x - maxfnlen;
    }
    x = mdr_cout_str(screenlastrow, 4, fn, SCHEME_STBAR1, maxfnlen);
    if (db->modflag) mdr_cout_char(screenlastrow, 5 + x, '!', SCHEME_STBAR2);
  }

  mdr_cout_str(screenlastrow, helpcol, s, SCHEME_STBAR2, 40);
}


static void ui_msg(const char *msg1, const char *msg2, unsigned char attr) {
  unsigned short x, y, msglen, i;
  unsigned short msg2flag = 0;

  msglen = strlen(msg1);
  if (msg2) {
    msg2flag = 1;
    i = strlen(msg2);
    if (i > msglen) msglen = i;
  }

  y = (screenh - 6) >> 1;
  x = (screenw - msglen - 4) >> 1;
  for (i = y+2+msg2flag; i >= y; i--) mdr_cout_char_rep(i, x, ' ', attr, msglen + 2);
  x++;

  mdr_cout_str(y+1, x, msg1, attr, msglen);
  if (msg2) mdr_cout_str(y+2, x, msg2, attr, msglen);

  if (uidirty.from > y) uidirty.from = y;
  if (uidirty.to < y+4) uidirty.to = y+4;
}


static unsigned char ui_confirm_if_unsaved(const struct file *db) {
  unsigned char r = 0;
  if (db->modflag == 0) return(0);

  mdr_cout_cursor_hide();

  /* if file has been modified then ask for confirmation */
  ui_msg(svarlang_str(0,4), svarlang_str(0,5), SCHEME_MSG);
  if (mdr_dos_getkey2() != '\r') r = 1;

  mdr_cout_cursor_show();

  return(r);
}


static void ui_refresh(const struct file *db) {
  unsigned char x;
  const struct line far *l;
  unsigned char y = db->cursorposy;

  /* quit early if nothing to refresh */
  if (uidirty.from == 0xff) return;

#ifdef DBG_REFRESH
  static char m = 'a';
  m++;
  if (m > 'z') m = 'a';
#endif

  /* rewind cursor line to first line that needs redrawing */
  for (l = db->cursor; y > uidirty.from; y--) l = l->prev;

  /* iterate over lines and redraw whatever needs to be redrawn */
  for (; l != NULL; l = l->next, y++) {

    /* skip lines that do not need to be refreshed */
    if (y < uidirty.from) continue;
    if (y > uidirty.to) break;

    x = 0;
    if (db->xoffset < l->len) {
      unsigned char i, limit;
      if (l->len - db->xoffset < screenw) {
        limit = l->len;
      } else {
        limit = db->xoffset + screenlastcol;
      }
      for (i = db->xoffset; i < limit; i++) mdr_cout_char(y, x++, l->payload[i], SCHEME_TEXT);
    }

    /* write empty spaces until end of line */
    if (x < screenlastcol) mdr_cout_char_rep(y, x, ' ', SCHEME_TEXT, screenlastcol - x);

#ifdef DBG_REFRESH
    mdr_cout_char(y, 0, m, SCHEME_STBAR1);
#endif

    if (y == screenh - 2) break;
  }

  /* fill all lines below if empty (and they need to be redrawn) */
  if (l == NULL) {
    while ((y < screenlastrow) && (y < uidirty.to)) {
      mdr_cout_char_rep(y++, 0, ' ', SCHEME_TEXT, screenlastcol);
    }
  }

  /* scroll bar */
  for (y = 0; y < screenlastrow; y++) {
    mdr_cout_char(y, screenlastcol, SCROLL_CURSOR, SCHEME_SCROLL);
  }

  /* scroll cursor */
  if (db->totlines >= screenh) {
    unsigned short topline = db->curline - db->cursorposy;
    unsigned short col;
    unsigned short totlines = db->totlines - screenh + 1;
    if (db->totlines - screenh > screenh) {
      col = topline / (totlines / screenlastrow);
    } else {
      col = topline * screenlastrow / totlines;
    }
    if (col >= screenlastrow) col = screenh - 2;
    mdr_cout_char(col, screenlastcol, ' ', SCHEME_SCROLL);
  }

  /* clear out the dirty flag */
  uidirty.from = 0xff;
}


static void check_cursor_not_after_eol(struct file *db) {
  if (db->xoffset + db->cursorposx <= db->cursor->len) return;

  if (db->cursor->len < db->xoffset) {
    db->cursorposx = 0;
    db->xoffset = db->cursor->len;
    uidirty.from = 0;
    uidirty.to = 0xff;
  } else {
    db->cursorposx = db->cursor->len - db->xoffset;
  }
}


static void cursor_up(struct file *db) {
  if (db->cursor->prev == NULL) return;

  db->curline -= 1;
  db->cursor = db->cursor->prev;
  if (db->cursorposy == 0) {
    uidirty.from = 0;
    uidirty.to = 0xff;
  } else {
    db->cursorposy -= 1;
  }
}


static void cursor_eol(struct file *db) {
  /* adjust xoffset to make sure eol is visible on screen */
  if (db->xoffset > db->cursor->len) {
    db->xoffset = db->cursor->len - 1;
    uidirty.from = 0;
    uidirty.to = 0xff;
  }

  if (db->xoffset + screenlastcol <= db->cursor->len) {
    db->xoffset = db->cursor->len - screenw + 2;
    uidirty.from = 0;
    uidirty.to = 0xff;
  }
  db->cursorposx = db->cursor->len - db->xoffset;
}


static void cursor_down(struct file *db) {
  if (db->cursor->next == NULL) return;

  db->curline += 1;
  db->cursor = db->cursor->next;
  if (db->cursorposy < screenh - 2) {
    db->cursorposy += 1;
  } else {
    uidirty.from = 0;
    uidirty.to = 0xff;
  }
}


static void cursor_left(struct file *db) {
  if (db->cursorposx > 0) {
    db->cursorposx -= 1;
  } else if (db->xoffset > 0) {
    db->xoffset -= 1;
    uidirty.from = 0;
    uidirty.to = 0xff;
  } else if (db->cursor->prev != NULL) { /* jump to end of line above */
    cursor_up(db);
    cursor_eol(db);
  }
}


static void cursor_home(struct file *db) {
  db->cursorposx = 0;
  if (db->xoffset != 0) {
    db->xoffset = 0;
    uidirty.from = 0;
    uidirty.to = 0xff;
  }
}


static void cursor_right(struct file *db) {
  if (db->cursor->len > db->xoffset + db->cursorposx) {
    if (db->cursorposx < screenw - 2) {
      db->cursorposx += 1;
    } else {
      db->xoffset += 1;
      uidirty.from = 0;
      uidirty.to = 0xff;
    }
  } else if (db->cursor->next != NULL) { /* jump to start of next line */
    cursor_down(db);
    cursor_home(db);
  }
}


static void del(struct file *db) {
  if (db->cursorposx + db->xoffset < db->cursor->len) {
    _fmemmove(db->cursor->payload + db->cursorposx + db->xoffset, db->cursor->payload + db->cursorposx + db->xoffset + 1, db->cursor->len - db->cursorposx - db->xoffset);
    db->cursor->len -= 1; /* do this AFTER memmove so the copy includes the nul terminator */
    uidirty.from = db->cursorposy;
    uidirty.to = db->cursorposy;
    db->modflag = 1;
  } else if (db->cursor->next != NULL) { /* cursor is at end of line: merge current line with next one (if there is a next one) */
    struct line far *nextline = db->cursor->next;
    if (db->cursor->next->len > 0) {
      if (curline_resize(db, db->cursor->len + db->cursor->next->len + 1) == 0) {
        _fmemmove(db->cursor->payload + db->cursor->len, db->cursor->next->payload, db->cursor->next->len + 1);
        db->cursor->len += db->cursor->next->len;
      }
    }

    db->cursor->next = db->cursor->next->next;
    db->cursor->next->prev = db->cursor;

    line_free(nextline);
    uidirty.from = db->cursorposy;
    uidirty.to = 0xff;
    db->totlines -= 1;
    db->modflag = 1;
  }
}


static void bkspc(struct file *db) {
  /* backspace is basically "left + del", not applicable only if cursor is on 1st byte of the file */
  if ((db->cursorposx + db->xoffset == 0) && (db->cursor->prev == NULL)) return;

  cursor_left(db);
  del(db);
}


#define LOADFILE_FILENOTFOUND 2

/* returns 0 on success, 1 on file not found, 2 on other error */
static unsigned char loadfile(struct file *db, const char *fname) {
  char buff[512]; /* read one entire sector at a time (faster) */
  char *buffptr;
  unsigned int len, llen;
  int fd;
  unsigned char eolfound;
  unsigned char err = 0;

  /* free the entire linked list of lines */
  db_rewind(db);
  while (db->cursor) {
    struct line far *victim;
    victim = db->cursor;
    db->cursor = db->cursor->next;
    line_free(victim);
  }

  /* zero out the struct */
  bzero(db, sizeof(struct file));

  /* start by adding an empty line */
  if (line_add(db, NULL, 0) != 0) return(2);

  if (fname == NULL) goto SKIPLOADING;

  mdr_dos_truename(db->fname, fname);

  err = _dos_open(fname, O_RDONLY, &fd);
  if (err != 0) goto SKIPLOADING;

  db->lfonly = 1;

  for (eolfound = 0;;) {
    unsigned short consumedbytes;

    if ((_dos_read(fd, buff, sizeof(buff), &len) != 0) || (len == 0)) break;
    buffptr = buff;

    FINDLINE:

    /* look for nearest \n (also expand tabs) */
    for (consumedbytes = 0;; consumedbytes++) {

      if (buffptr[consumedbytes] == '\t') {
        llen = consumedbytes;
        break;
      }

      if (consumedbytes == len) {
        llen = consumedbytes;
        break;
      }
      if (buffptr[consumedbytes] == '\r') {
        llen = consumedbytes;
        consumedbytes++;
        db->lfonly = 0;
        break;
      }
      if (buffptr[consumedbytes] == '\n') {
        eolfound = 1;
        llen = consumedbytes;
        consumedbytes++;
        break;
      }
    }

    /* consumedbytes is the amount of bytes processed from buffptr,
     * llen is the length of line's payload (without its line terminator) */

    /* append content, if line is non-empty */
    if ((llen > 0) && (line_append(db, buffptr, llen) != 0)) {
      goto IOERR;
    }

    /* allocate the next line if current line ended */
    if (eolfound) {
      if (line_add(db, NULL, 0) != 0) {
        goto IOERR;
      }
      eolfound = 0;
    }

    /* append 8 spaces if tab char found */
    if ((consumedbytes < len) && (buffptr[consumedbytes] == '\t') && (glob_tablessmode == 0)) {
      consumedbytes++;
      if (line_append(db, "        ", 8) != 0) {
        goto IOERR;
      }
    }

    /* anything left? process the buffer leftover again */
    if (consumedbytes < len) {
      len -= consumedbytes;
      buffptr += consumedbytes;
      goto FINDLINE;
    }

  }

  _dos_close(fd);

  SKIPLOADING:

  /* rewind cursor to top of file because it has been used by line_add() */
  db_rewind(db);

  return(err);

  IOERR:
  _dos_close(fd);
  return(1);
}


/* a custom argv-parsing routine that looks directly inside the PSP, avoids the need
 * of argc and argv, saves some 330 bytes of binary size
 * returns non-zero on error */
static int parseargv(struct file *dbarr) {
  char *tail = (void *)0x81; /* THIS WORKS ONLY IN SMALL MEMORY MODEL */
  unsigned short count = 0;
  char *arg;
  unsigned short lastarg = 0;
  unsigned char err;

  while (!lastarg) {
    /* jump to nearest arg */
    while (*tail == ' ') {
      *tail = 0;
      tail++;
    }

    if (*tail == '\r') {
      *tail = 0;
      break;
    }

    arg = tail;

    /* jump to next delimiter */
    while ((*tail != ' ') && (*tail != '\r')) tail++;

    /* if \r then remember this is the last arg */
    if (*tail == '\r') lastarg = 1;

    *tail = 0;
    tail++;

    /* look at the arg now */
    if (*arg == '/') {
      if (arg[1] == 't') { /* /t = do not expand tabs */
        glob_tablessmode = 1;

      } else if (arg[1] == 'm') { /* /m = force mono mode */
        glob_monomode = 1;

      } else {  /* help screen */
        mdr_coutraw_str(svarlang_str(1,3)); /* Sved, the SvarDOS editor */
        mdr_coutraw_str(" [");
        mdr_coutraw_str(svarlang_str(1,4)); /* ver */
        mdr_coutraw_puts(" " PVER "]");
        mdr_coutraw_puts("Copyright (C) " PDATE " Mateusz Viste");
        mdr_coutraw_crlf();
        mdr_coutraw_str("sved [/m] [/t] ");
        mdr_coutraw_puts(svarlang_str(1,1)); /* args syntax */
        mdr_coutraw_crlf();
        mdr_coutraw_puts(svarlang_str(1,10)); /* /m */
        mdr_coutraw_puts(svarlang_str(1,11)); /* /t */
        return(-1);
      }
      continue;
    }

    /* looks to be a filename */
    if (count == 10) {
      mdr_coutraw_puts(svarlang_str(0,12)); /* too many files */
      return(-1);
    }

    /* try loading it */
    mdr_coutraw_str(svarlang_str(1,2));
    mdr_coutraw_char(' ');
    mdr_coutraw_puts(arg);
    err = loadfile(&(dbarr[count]), arg);
    if (err) {
      if (err == LOADFILE_FILENOTFOUND) { /* file not found */
        if ((count == 0) && (lastarg != 0)) { /* a 'file not found' is fine if only one file was given */
          err = 0;
        } else {
          err = 11;
        }
      } else { /* general error */
        err = 10;
      }
      if (err) {
        mdr_coutraw_puts(svarlang_str(0,err));
        return(-1);
      }
    }
    count++;
  }

  return(0);
}


static int savefile(const struct file *db, const char *newfname) {
  int fd;
  const struct line far *l;
  unsigned int bytes;
  unsigned char eollen;
  unsigned char eolbuf[2];
  int errflag = 0;

  /* either create a new file if newfname provided, or... */
  if (newfname) {
    if (_dos_creatnew(newfname, _A_NORMAL, &fd) != 0) return(-1);
  } else { /* ...open db->fname */
    if (_dos_open(db->fname, O_WRONLY, &fd) != 0) return(-1);
  }

  l = db->cursor;
  while (l->prev) l = l->prev;

  /* preset line terminators */
  if (db->lfonly) {
    eolbuf[0] = '\n';
    eollen = 1;
  } else {
    eolbuf[0] = '\r';
    eolbuf[1] = '\n';
    eollen = 2;
  }

  while (l) {
    /* do not write the last empty line, it is only useful for edition */
    if (l->len != 0) {
      errflag |= _dos_write(fd, l->payload, l->len, &bytes);
    } else if (l->next == NULL) {
      break;
    }
    errflag |= _dos_write(fd, eolbuf, eollen, &bytes);
    l = l->next;
  }

  /* emit a 0-bytes write - this means "truncate file at current position" */
  errflag |= _dos_write(fd, NULL, 0, &bytes);

  errflag |= _dos_close(fd);

  return(errflag);
}


static void insert_in_line(struct file *db, const char *databuf, unsigned short len) {
  if (curline_resize(db, db->cursor->len + len) == 0) {
    unsigned short off = db->xoffset + db->cursorposx;
    db->modflag = 1;
    _fmemmove(db->cursor->payload + off + len, db->cursor->payload + off, db->cursor->len - off + 1);
    db->cursor->len += len;
    uidirty.from = db->cursorposy;
    uidirty.to = db->cursorposy;
    while (len--) {
      db->cursor->payload[off++] = *databuf;
      databuf++;
      cursor_right(db);
    }
  }
}


/* recompute db->curline by counting nodes in linked list */
static void recompute_curline(struct file *db) {
  const struct line far *l = db->cursor;

  db->curline = 0;
  while (l->prev != NULL) {
    db->curline += 1;
    l = l->prev;
  }
}


enum MENU_ACTION {
  MENU_NONE   = 0,
  MENU_OPEN   = 1,
  MENU_SAVE   = 2,
  MENU_SAVEAS = 3,
  MENU_CLOSE  = 4,
  MENU_CHGEOL = 5,
  MENU_QUIT   = 6
};

static enum MENU_ACTION ui_menu(void) {
  unsigned short i, curchoice, attr, x, slen;
  unsigned short xorigin, yorigin;

  /* find out the longest string */
  slen = 0;
  for (i = MENU_OPEN; i <= MENU_QUIT; i++) {
    x = strlen(svarlang_str(8, i));
    if (x > slen) slen = x;
  }

  /* calculate where to draw the menu on screen */
  xorigin = (screenw - (slen + 5)) / 2;
  yorigin = (screenh - (MENU_QUIT - MENU_OPEN + 6)) / 2;

  /* */
  uidirty.from = yorigin;
  uidirty.to = 0xff;
  uidirty.statusbar = 1;

  /* hide the cursor */
  mdr_cout_cursor_hide();

  curchoice = MENU_OPEN;
  for (;;) {
    /* render menu */
    for (i = MENU_NONE; i <= MENU_QUIT + 1; i++) {
      mdr_cout_char_rep(yorigin + i, xorigin, ' ', SCHEME_MENU, slen+4);
      if (i == curchoice) {
        attr = SCHEME_MENU_CUR;
        mdr_cout_char_rep(yorigin + i, xorigin + 1, ' ', SCHEME_MENU_SEL, slen + 2);
      } else {
        attr = SCHEME_MENU;
      }
      mdr_cout_str(yorigin + i, xorigin + 2, svarlang_str(8, i), attr, slen);
    }
    /* wait for key */
    switch (mdr_dos_getkey2()) {
      case 0x150: /* down */
        if (curchoice == MENU_QUIT) {
          curchoice = MENU_OPEN;
        } else {
          curchoice++;
        }
        break;
      case 0x148: /* up */
        if (curchoice == MENU_OPEN) {
          curchoice = MENU_QUIT;
        } else {
          curchoice--;
        }
        break;
      default:
        curchoice = MENU_NONE;
        /* FALLTHRU */
      case '\r': /* ENTER */
        mdr_cout_cursor_show();
        return(curchoice);
    }
  }
}


static struct file *select_slot(struct file *dbarr, unsigned char curfile) {
  uidirty.from = 0;
  uidirty.to = 0xff;
  uidirty.statusbar = 1;

  dbarr = &(dbarr[curfile]);
  /* force redraw now, because the main() routine might not if this is exit
   * time and we want to show the user which file has unsaved changes */
  ui_statusbar(dbarr, curfile);
  ui_refresh(dbarr);
  return(dbarr);
}


/* main returns nothing, ie. sved always exits with a zero exit code
 * (this saves 20 bytes of executable footprint) */
void main(void) {
  static struct file dbarr[10];
  unsigned char curfile;
  struct file *db = dbarr; /* visible file is the first slot by default */
  struct line far *clipboard = NULL;
  unsigned char original_breakflag;

  { /* load NLS resource */
    unsigned short i = 0;
    const char far *selfptr;
    char self[128], lang[8];
    selfptr = mdr_dos_selfexe();
    if (selfptr != NULL) {
      do {
        self[i] = selfptr[i];
      } while (self[i++] != 0);
      svarlang_autoload_exepath(self, mdr_dos_getenv(lang, "LANG", sizeof(lang)));
    }
  }

  /* preload all slots with empty files */
  for (curfile = 9;; curfile--) {
    loadfile(&(dbarr[curfile]), NULL);
    if (curfile == 0) break;
  }

  /* parse argv (and load files, if any passed on) */
  if (parseargv(dbarr) != 0) return;

  if ((mdr_cout_init(&screenw, &screenh) != 0) && (glob_monomode == 0)) {
    /* load color scheme if mdr_cout_init returns a color flag */
    SCHEME_TEXT = 0x17;
    SCHEME_MENU = 0x70;
    SCHEME_MENU_CUR = 0x6f;
    SCHEME_MENU_SEL = 0x66;
    SCHEME_STBAR1 = 0x70;
    SCHEME_STBAR2 = 0x78;
    SCHEME_STBAR3 = 0x3f;
    SCHEME_SCROLL = 0x70;
    SCHEME_MSG = 0x6f;
    SCHEME_ERR = 0x4f;
  }
  screenlastrow = screenh - 1;
  screenlastcol = screenw - 1;

  /* instruct DOS to stop detecting CTRL+C because user needs it for
   * copy/paste operations. also remember the original status of the BREAK
   * flag so I can restore it as it was before quitting. */
  original_breakflag = mdr_dos_ctrlc_disable();

  for (;;) {
    int k;

    /* add an extra empty line if cursor is on last line and this line is not empty */
    if ((db->cursor->next == NULL) && (db->cursor->len != 0)) {
      if (line_add(db, NULL, 0) == 0) {
        db->cursor = db->cursor->prev; /* line_add() changes the cursor pointer */
        db->curline -= 1;
      }
    }

    check_cursor_not_after_eol(db);
    mdr_cout_locate(db->cursorposy, db->cursorposx);

    ui_refresh(db);

    if ((uidirty.statusbar != 0) || (db->modflagprev != db->modflag) || (db->curline_prev != db->curline)) {
      ui_statusbar(db, curfile);
      uidirty.statusbar = 0;
      db->modflagprev = db->modflag;
      db->curline_prev = db->curline;
    }
#ifdef DBG_LINENUM
      {
        char ddd[10];
        db->curline += 1;
        ddd[0] = '0' + db->curline / 100;
        ddd[1] = '0' + (db->curline % 100) / 10;
        ddd[2] = '0' + (db->curline % 10);
        db->curline -= 1;
        ddd[3] = '/';
        ddd[4] = '0' + db->totlines / 100;
        ddd[5] = '0' + (db->totlines % 100) / 10;
        ddd[6] = '0' + (db->totlines % 10);
        ddd[7] = 0;
        mdr_cout_str(screenh - 1, 40, ddd, SCHEME_STBAR1, sizeof(ddd));
      }
#endif

    k = mdr_dos_getkey2();

    if (k == 0x150) { /* down */
      cursor_down(db);

    } else if (k == 0x148) { /* up */
      cursor_up(db);

    } else if (k == 0x14D) { /* right */
      cursor_right(db);

    } else if (k == 0x14B) { /* left */
      cursor_left(db);

    } else if (k == 0x149) { /* pgup */
      unsigned char dist = db->cursorposy + screenh - 1;
      while ((dist != 0) && (db->cursor->prev != NULL)) {
        db->cursor = db->cursor->prev;
        dist--;
      }
      if (dist != 0) {
        db->cursorposy = 0;
        db->cursorposx = 0;
      } else {
        dist = db->cursorposy;
        while ((dist--) && (db->cursor->next)) db->cursor = db->cursor->next;
      }
      uidirty.from = 0;
      uidirty.to = 0xff;
      recompute_curline(db);

    } else if (k == 0x151) { /* pgdown */
      unsigned char dist = screenh + screenh - db->cursorposy - 3;
      while ((dist != 0) && (db->cursor->next != NULL)) {
        db->cursor = db->cursor->next;
        dist--;
      }
      if (dist != 0) {
        db->cursorposy = screenh - 2;
        if (db->totlines <= db->cursorposy) db->cursorposy = db->totlines - 1;
        db->cursorposx = 0;
      } else {
        dist = screenh - 2 - db->cursorposy;
        while ((dist--) && (db->cursor->prev)) db->cursor = db->cursor->prev;
      }
      uidirty.from = 0;
      uidirty.to = 0xff;
      recompute_curline(db);

    } else if (k == 0x147) { /* home */
       cursor_home(db);

    } else if (k == 0x14F) { /* end */
       cursor_eol(db);

    } else if (k == 0x1B) { /* ESC */
      int quitnow = 0;
      char fname[64];
      int saveflag = 0;
      enum MENU_ACTION ui_action;

      /* collect the exact menu action and clear the screen */
      ui_action = ui_menu();
      ui_refresh(db);

      switch (ui_action) {

        case MENU_NONE:
          break;

        case MENU_OPEN:
          /* display a warning if unsaved changes are pending */
          if (db->modflag != 0) ui_msg(svarlang_str(0,4), svarlang_str(0,8), SCHEME_MSG);

          /* ask for filename */
          ui_getstring(svarlang_str(0,7), fname, sizeof(fname));
          if (fname[0] != 0) {
            unsigned char err;
            err = loadfile(db, fname);
            if (err != 0) {
              if (err == LOADFILE_FILENOTFOUND) {
                ui_msg(svarlang_str(0,11), NULL, SCHEME_ERR); /* file not found */
              } else {
                ui_msg(svarlang_str(0,10), NULL, SCHEME_ERR);  /* ERROR */
              }
              mdr_bios_tickswait(44); /* 3s */
              loadfile(db, NULL);
            }
          }
          uidirty.from = 0;
          uidirty.to = 0xff;
          uidirty.statusbar = 1;
          break;

        case MENU_SAVEAS:
          saveflag = 1;
          /* FALLTHRU */
        case MENU_SAVE:
          if ((saveflag != 0) || (db->fname[0] == 0)) { /* save as... */
            ui_getstring(svarlang_str(0,6), fname, sizeof(fname));
            if (*fname == 0) break;
            saveflag = savefile(db, fname);
            if (saveflag == 0) mdr_dos_truename(db->fname, fname);
          } else {
            saveflag = savefile(db, NULL);
          }

          mdr_cout_cursor_hide();

          if (saveflag == 0) {
            db->modflag = 0;
            ui_msg(svarlang_str(0, 2), NULL, SCHEME_MSG);
            mdr_bios_tickswait(11); /* 11 ticks is about 600 ms */
          } else {
            ui_msg(svarlang_str(0, 10), NULL, SCHEME_ERR);
            mdr_bios_tickswait(36); /* 2s */
          }
          mdr_cout_cursor_show();
          break;

        case MENU_CLOSE:
          if (ui_confirm_if_unsaved(db) == 0) {
            loadfile(db, NULL);
          }
          uidirty.from = 0;
          uidirty.to = 0xff;
          uidirty.statusbar = 1;
          break;

        case MENU_CHGEOL:
          db->modflag = 1;
          db->lfonly ^= 1;
          break;

        case MENU_QUIT:
          quitnow = 1;
          for (curfile = 0; curfile < 10; curfile++) {
            if (dbarr[curfile].modflag) {
              db = select_slot(dbarr, curfile);
              if (ui_confirm_if_unsaved(db) != 0) {
                quitnow = 0;
                break;
              }
            }
          }
          break;
      }

      if (quitnow) break;

    } else if (k == 0x0D) { /* ENTER */
      unsigned short off = db->xoffset + db->cursorposx;
      /* add a new line */
      if (line_add(db, db->cursor->payload + off, db->cursor->len - off) == 0) {
        db->modflag = 1;
        db->cursor = db->cursor->prev; /* back to original line */
        db->curline -= 1;
        /* trim the line above */
        db->cursor->len = off;
        /* move cursor to the (new) line below */
        uidirty.from = db->cursorposy;
        uidirty.to = 0xff;
        cursor_down(db);
        cursor_home(db);
      } else {
        /* ERROR: OUT OF MEMORY */
      }

    } else if (k == 0x153) {  /* DEL */
      del(db);

    } else if (k == 0x008) { /* BKSPC */
      bkspc(db);

    } else if ((k >= 0x20) && (k <= 0xff)) { /* "normal" character */
      char c = k;
      insert_in_line(db, &c, 1);

    } else if (k == 0x009) { /* TAB */
      if (glob_tablessmode == 0) {
        insert_in_line(db, "        ", 8);
      } else {
        insert_in_line(db, "\t", 1);
      }

    } else if ((k >= 0x13b) && (k <= 0x144)) { /* F1..F10 */
      curfile = k - 0x13b;
      db = select_slot(dbarr, curfile);

    } else if (k == 0x174) { /* CTRL+ArrRight - jump to next word */
      /* if currently cursor is on a non-space, then fast-forward to nearest space or EOL */
      for (;;) {
        if (db->xoffset + db->cursorposx == db->cursor->len) break;
        if (db->cursor->payload[db->xoffset + db->cursorposx] == ' ') break;
        cursor_right(db);
      }
      /* now skip to next non-space or end of file */
      for (;;) {
        cursor_right(db);
        if (db->cursor->payload[db->xoffset + db->cursorposx] != ' ') break;
        if ((db->cursor->next == NULL) && (db->cursorposx + db->xoffset == db->cursor->len)) break;
      }

    } else if (k == 0x173) { /* CTRL+ArrLeft - jump to prev word */
      cursor_left(db);
      /* if currently cursor is on a space, then fast-forward to nearest non-space or start of line */
      for (;;) {
        if ((db->xoffset == 0) && (db->cursorposx == 0)) break;
        if (db->cursor->payload[db->xoffset + db->cursorposx] != ' ') break;
        cursor_left(db);
      }
      /* now skip to next space or start of file */
      for (;;) {
        cursor_left(db);
        if (db->cursor->payload[db->xoffset + db->cursorposx] == ' ') {
          cursor_right(db);
          break;
        }
        if ((db->cursorposx == 0) && (db->xoffset == 0)) break;
      }

    } else if ((k == 0x003) || (k == 0x018)) { /* CTRL+C or CTRL+X */
      /* free clipboard if anything in it */
      if (clipboard != NULL) line_free(clipboard);

      /* copy cursor line to clipboard */
      clipboard = line_calloc(db->cursor->len);
      if (clipboard == NULL) {
        ui_msg(svarlang_str(0, 10), NULL, SCHEME_ERR); /* ERROR */
        mdr_bios_tickswait(18); /* 1s */
      } else {
        mdr_cout_char_rep(db->cursorposy, 0, ' ', ((SCHEME_TEXT >> 4) | (SCHEME_TEXT << 4)) & 0xff, screenlastcol);
        uidirty.from = db->cursorposy;
        uidirty.to = db->cursorposy;
        if (db->cursor->len != 0) {
          _fmemmove(clipboard->payload, db->cursor->payload, db->cursor->len);
          clipboard->len = db->cursor->len;
        }
        mdr_bios_tickswait(2); /* ca 100ms */

        /* if this is about cutting the line (CTRL+X) then delete cur line */
        if ((k == 0x018) && ((db->cursor->next != NULL) || (db->cursor->prev != NULL))) {
          if (db->cursor->next) db->cursor->next->prev = db->cursor->prev;
          if (db->cursor->prev) db->cursor->prev->next = db->cursor->next;
          clipboard->prev = db->cursor;
          if (db->cursor->next) {
            db->cursor = db->cursor->next;
          } else {
            cursor_up(db);
          }
          line_free(clipboard->prev);
          db->totlines -= 1;
          uidirty.from = 0;
          uidirty.to = 0xff;
          recompute_curline(db);
        }
      }

    } else if ((k == 0x016) && (clipboard != NULL)) { /* CTRL+V */
      if (line_add(db, clipboard->payload, clipboard->len) != 0) {
        ui_msg(svarlang_str(0, 10), NULL, SCHEME_ERR); /* ERROR */
        mdr_bios_tickswait(18); /* 1s */
      } else {
        /* rewire the linked list so the new line is on top of the previous one */
        clipboard->prev = db->cursor->prev;
        /* remove prev node from list */
        db->cursor->prev = db->cursor->prev->prev;
        if (db->cursor->prev != NULL) db->cursor->prev->next = db->cursor;
        /* insert the node after cursor now */
        clipboard->prev->next = db->cursor->next;
        if (db->cursor->next != NULL) db->cursor->next->prev = clipboard->prev;
        clipboard->prev->prev = db->cursor;
        db->cursor->next = clipboard->prev;
        cursor_down(db);
      }
      uidirty.from = 0;
      uidirty.to = 0xff;
      recompute_curline(db);

#ifdef DBG_UNHKEYS
    } else { /* UNHANDLED KEY - TODO IGNORE THIS IN PRODUCTION RELEASE */
      char buff[4];
      const char *HEX = "0123456789ABCDEF";
      buff[0] = HEX[(k >> 8) & 15];
      buff[1] = HEX[(k >> 4) & 15];
      buff[2] = HEX[k & 15];
      mdr_cout_str(screenh - 1, 0, "UNHANDLED KEY: 0x", SCHEME_STBAR1, 17);
      mdr_cout_str(screenh - 1, 17, buff, SCHEME_STBAR1, 3);
      mdr_dos_getkey2();
      break;
#endif
    }
  }

  mdr_cout_close();

  /* restore the DOS BREAK flag if it was originally set */
  if (original_breakflag != 0) mdr_dos_ctrlc_enable();

  /* no need to free memory, DOS will do it for me */

  return;
}
