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
 * rename/ren
 */

static enum cmd_result cmd_rename(struct cmd_funcparam *p) {
  char *src = p->BUFFER;
  char *dst = p->BUFFER + 256;
  char *buff1 = p->BUFFER + 512;
  char *buff2 = p->BUFFER + 1024;
  unsigned short i, fnameoffset;
  struct DTA *dta = (void *)0x80; /* use default DTA in PSP */

  if (cmd_ishlp(p)) {
    outputnl("Renames a file or files");
    outputnl("");
    outputnl("RENAME [drive:][path]filename1 filename2");
    outputnl("REN [drive:][path]filename1 filename2");
    outputnl("");
    outputnl("Note that you cannot specify a new drive or path for your destination file.");
    outputnl("Use MOVE to rename a directory, or to move files from one directory to another.");
    return(CMD_OK);
  }

  /* I expect exactly two arguments */
  if (p->argc != 2) {
    outputnl("Invalid syntax");
    return(CMD_FAIL);
  }

  /* convert src to truename format */
  i = file_truename(p->argv[0], src);
  if (i != 0) {
    outputnl(doserr(i));
    return(CMD_FAIL);
  }

  /* copy src path to buffers and remember where the filename starts */
  fnameoffset = 0;
  for (i = 0;; i++) {
    buff1[i] = src[i];
    buff2[i] = src[i];
    if (buff1[i] == '\\') fnameoffset = i + 1;
    if (buff1[i] == 0) break;
  }

  /* now append dst filename to the buffer and validate it: cannot contain backslash, slash or ':' */
  for (i = 0;; i++) {
    switch (p->argv[1][i]) {
      case ':':
      case '\\':
      case '/':
        outputnl("Invalid destination");
        return(CMD_FAIL);
    }
    buff1[fnameoffset + i] = p->argv[1][i];
    if (buff1[fnameoffset + i] == 0) break;
  }

  /* apply truename to dest to normalize wildcards into ? chars */
  i = file_truename(buff1, dst);
  if (i != 0) {
    outputnl(doserr(i));
    return(CMD_FAIL);
  }

  /* we're good to go, src and dst should look somehow like that now:
   * src   =   C:\TEMP\PATH\FILE????.TXT
   * dst   =   C:\TEMP\PATH\FILE????.DOC
   * buff1 =   C:\TEMP\PATH\
   * buff2 =   C:\TEMP\PATH\
   * fnameoffset = 13
   *
   * src is used for FindFirst/FindNext iterations, then buff1 is filled with
   * the source filename found by FindFirst/FindNext and buff2 is filled with
   * the destination file (with ?'s replaced by whatever is found at the same
   * location in buff1).
   */

  i = findfirst(dta, src, 0);
  if (i != 0) outputnl(doserr(i));

  while (i == 0) {
    /* write found fname into buff1 and dst fname into buff2 - both in FCB
     * format (MYFILE  EXT) so it is easy to compare them */
    file_fname2fcb(buff1 + fnameoffset, dta->fname);
    file_fname2fcb(buff2 + fnameoffset, dst + fnameoffset);

    /* scan buff2 fname for '?' and replace them with whatever is in buff1 */
    for (i = fnameoffset; buff2[i] != 0; i++) {
      if (buff2[i] == '?') buff2[i] = buff1[i];
    }

    /* fill buff1 with the 8+3 found file and convert the one in buff2 to 8+3 as well */
    file_fcb2fname(buff1 + fnameoffset, buff2 + fnameoffset);
    strcpy(buff2 + fnameoffset, buff1 + fnameoffset);
    strcpy(buff1 + fnameoffset, dta->fname);

    /* buff1 contains now a fully resolved source and buff2 a proper destination */
    #if 0  /* DEBUG ("if 1" to enable) */
    output(buff1);
    output(" -> ");
    outputnl(buff2);
    #endif
    /* call DOS to do the actual job */
    i = 0;
    _asm {
      push ax
      push di
      push dx
      push es

      mov ah, 0x56  /* rename file: DS:DX=ASCIIZ of src  ES:DI=ASCIIZ of dst */
      push ds
      pop es
      mov dx, buff1
      mov di, buff2
      int 0x21      /* CF clear on success, otherwise err code in AX */
      jnc DONE
      mov [i], ax   /* copy error code to i */
      DONE:

      pop es
      pop dx
      pop di
      pop ax
    }
    if (i != 0) {
      output(buff1 + fnameoffset);
      output(" -> ");
      output(buff2 + fnameoffset);
      output("  ");
      outputnl(doserr(i));
    }
    /* next please */
    i = findnext(dta);
  }

  return(CMD_OK);
}
