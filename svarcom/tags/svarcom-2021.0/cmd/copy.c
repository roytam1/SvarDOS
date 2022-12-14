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
 * copy
 */

/* /A - Used to copy ASCII files. Applies to the filename preceding it and to
 * all following filenames. Files will be copied until an end-of-file mark is
 * encountered in the file being copied. If an end-of-file mark is encountered
 * in the file, the rest of the file is not copied. DOS will append an EOF
 * mark at the end of the copied file.
 *
 * /B - Used to copy binary files. Applies to the filename preceding it and to
 * all following filenames. Copied files will be read by size (according to
 * the number of bytes indicated in the file`s directory listing). An EOF mark
 * is not placed at the end of the copied file.
 *
 * /V - Checks after the copy to assure that a file was copied correctly. If
 * the copy cannot be verified, the program will display an error message.
 * Using this option will result in a slower copying process.
 *
 * special case: "COPY A+B+C+D" means "append B, C and D files to the A file"
 * if A does not exist, then "append C and D to B", etc.
 */

struct copy_setup {
  const char *src[64];
  unsigned short src_count; /* how many sources are declared */
  char cursrc[256];         /* buffer for currently processed src */
  char dst[256];
  unsigned short dstlen;
  char src_asciimode[64];
  char dst_asciimode;
  char last_asciimode; /* /A or /B impacts the file preceding it and becomes the new default for all files that follow */
  char verifyflag;
  char lastitemwasplus;
  unsigned short databufsz;
  char databuf[1];
};


/* copies src to dst, overwriting or appending to the destination.
 * - copy is performed in ASCII mode if asciiflag set (stop at first EOF in src
 *   and append an EOF in dst).
 * - returns zero on success, DOS error code on error */
static unsigned short cmd_copy_internal(const char *dst, char dstascii, const char *src, char srcascii, unsigned char appendflag, void *buff, unsigned short buffsz) {
  unsigned short errcode = 0;
  unsigned short srch = 0xffff, dsth = 0xffff;
  _asm {

    /* open src */
    OPENSRC:
    mov ax, 0x3d00 /* DOS 2+ -- open an existing file, read access mode */
    mov dx, src    /* ASCIIZ fname */
    int 0x21       /* CF clear on success, handle in AX */
    jc FAIL
    mov [srch], ax /* store src handle in memory */

    /* check appendflag so I know if I have to try opening dst for append */
    xor al, al
    or al, [appendflag]
    jz CREATEDST

    /* try opening dst first if appendflag set */
    mov ax, 0x3d01 /* DOS 2+ -- open an existing file, write access mode */
    mov dx, dst    /* ASCIIZ fname */
    int 0x21       /* CF clear on success, handle in AX */
    jc CREATEDST   /* failed to open file (file does not exist) */
    mov [dsth], ax /* store dst handle in memory */

    /* got file open, LSEEK to end of it now so future data is appended */
    mov bx, ax     /* file handle in BX (was still in AX) */
    mov ax, 0x4202 /* DOS 2+ -- set file pointer to end of file + CX:DX */
    xor cx, cx     /* offset zero */
    xor dx, dx     /* offset zero */
    int 0x21       /* CF set on error */
    jc FAIL
    jmp COPY

    /* create dst */
    CREATEDST:
    mov ah, 0x3c   /* DOS 2+ -- create a file */
    mov dx, dst
    xor cx, cx     /* zero out attributes */
    int 0x21       /* handle in AX on success, CF set on error */
    jc FAIL
    mov [dsth], ax /* store dst handle in memory */

    /* perform actual copy */
    COPY:
    /* read a block from src */
    mov ah, 0x3f   /* DOS 2+ -- read from file */
    mov bx, [srch]
    mov cx, [buffsz]
    mov dx, [buff] /* DX points to buffer */
    int 0x21       /* CF set on error, bytes read in AX (0=EOF) */
    jc FAIL        /* abort on error */
    /* EOF? (ax == 0) */
    test ax, ax
    jz ENDOFFILE
    /* write block of AX bytes to dst */
    mov cx, ax     /* block length */
    mov ah, 0x40   /* DOS 2+ -- write to file (CX bytes from DS:DX) */
    mov bx, [dsth] /* file handle */
    /* mov dx, [buff] */ /* DX points to buffer already */
    int 0x21       /* CF clear and AX=CX on success */
    jc FAIL
    cmp ax, cx     /* sould be equal, otherwise failed */
    mov ax, 0x08   /* preset to DOS error "Insufficient memory" */
    jne FAIL
    jmp COPY

    ENDOFFILE:
    /* if dst ascii mode -> add an EOF (ASCII mode not supported for the time being) */

    jmp CLOSESRC

    FAIL:
    mov [errcode], ax

    CLOSESRC:
    /* close src and dst */
    mov bx, [srch]
    cmp bx, 0xffff
    je CLOSEDST
    mov ah, 0x3e   /* DOS 2+ -- close a file handle */
    int 0x21

    CLOSEDST:
    mov bx, [dsth]
    cmp bx, 0xffff
    je DONE
    mov ah, 0x3e   /* DOS 2+ -- close a file handle */
    int 0x21

    DONE:
  }
  return(errcode);
}


static int cmd_copy(struct cmd_funcparam *p) {
  struct copy_setup *setup = (void *)(p->BUFFER);
  unsigned short i;
  unsigned short copiedcount_in = 0, copiedcount_out = 0; /* number of input/output copied files */
  struct DTA *dta = (void *)0x80; /* use DTA at default location in PSP */

  if (cmd_ishlp(p)) {
    outputnl("Copies one or more files to another location.");
    outputnl("");
    outputnl("COPY [/A|/B] source [/A|/B] [+source [/A|/B] [+...]] [destination [/A|/B]] [/V]");
    outputnl("");
    outputnl("source       Specifies the file or files to be copied");
    outputnl("/A           Indicates an ASCII text file");
    outputnl("/B           Indicates a binary file");
    outputnl("destination  Specifies the directory and/or filename for the new file(s)");
    outputnl("/V           Verifies that new files are written correctly");
    outputnl("");
    outputnl("To append files, specify a single file for destination, but multiple files");
    outputnl("for source (using wildcards or file1+file2+file3 format).");
    outputnl("");
    outputnl("NOTE: /A and /B are no-ops (ignored), provided only for compatibility reasons.");
    return(-1);
  }

  /* parse cmdline and fill the setup struct accordingly */

  memset(setup, 0, sizeof(*setup));
  setup->databufsz = p->BUFFERSZ - sizeof(*setup);

  for (i = 0; i < p->argc; i++) {

    /* switch? */
    if (p->argv[i][0] == '/') {
      if ((imatch(p->argv[i], "/a")) || (imatch(p->argv[i], "/b"))) {
        setup->last_asciimode = 'b';
        if (imatch(p->argv[i], "/a")) setup->last_asciimode = 'a';
        /* */
        if (setup->dst[0] != 0) {
          setup->dst_asciimode = setup->last_asciimode;
        } else if (setup->src_count != 0) {
          setup->src_asciimode[setup->src_count - 1] = setup->last_asciimode;
        }
      } else if (imatch(p->argv[i], "/v")) {
        setup->verifyflag = 1;
      } else {
        outputnl("Invalid switch");
        return(-1);
      }
      continue;
    }

    /* not a switch - must be either a source, a destination or a + */
    if (p->argv[i][0] == '+') {
      /* a plus cannot appear after destination or before first source */
      if ((setup->dst[0] != 0) || (setup->src_count == 0)) {
        outputnl("Invalid syntax");
        return(-1);
      }
      setup->lastitemwasplus = 1;
      /* a plus may be immediately followed by a filename - if so, emulate
       * a new argument */
      if (p->argv[i][1] != 0) {
        p->argv[i] += 1;
        i--;
      }
      continue;
    }

    /* src? (first non-switch or something that follows a +) */
    if ((setup->lastitemwasplus) || (setup->src_count == 0)) {
      setup->src[setup->src_count] = p->argv[i];
      setup->src_asciimode[setup->src_count] = setup->last_asciimode;
      setup->src_count++;
      setup->lastitemwasplus = 0;
      continue;
    }

    /* must be a dst then */
    if (setup->dst[0] != 0) {
      outputnl("Invalid syntax");
      return(-1);
    }
    if (file_truename(p->argv[i], setup->dst) != 0) {
      outputnl("Invalid destination");
      return(-1);
    }
    setup->dst_asciimode = setup->last_asciimode;
    /* if dst is a directory then append a backslash */
    setup->dstlen = path_appendbkslash_if_dir(setup->dst);
  }

  /* DEBUG: output setup content ("if 1" to enable) */
  #if 0
  printf("src: ");
  for (i = 0; i < setup->src_count; i++) {
    if (i != 0) printf(", ");
    printf("%s [%c]", setup->src[i], setup->src_asciimode[i]);
  }
  printf("\r\n");
  printf("dst: %s [%c]\r\n", setup->dst, setup->dst_asciimode);
  printf("verify: %s\r\n", (setup->verifyflag)?"ON":"OFF");
  #endif

  /* must have at least one source */
  if (setup->src_count == 0) {
    outputnl("Required parameter missing");
    return(-1);
  }

  /* perform the operation based on setup directives:
   * iterate over every source and copy it to dest */

  for (i = 0; i < setup->src_count; i++) {
    unsigned short t;
    unsigned short cursrclen;
    unsigned short pathendoffset;

    /* resolve truename of src and write it to buffer */
    t = file_truename(setup->src[i], setup->cursrc);
    if (t != 0) {
      output(setup->src[i]);
      output(" - ");
      outputnl(doserr(t));
      continue;
    }
    cursrclen = strlen(setup->cursrc); /* remember cursrc length */

    /* if length zero, skip (not sure why this would be possible, though) */
    if (cursrclen == 0) continue;

    /* if src does not end with a backslash AND it is a directory then append a backslash */
    cursrclen = path_appendbkslash_if_dir(setup->cursrc);

    /* if src ends with a '\' then append *.* */
    if (setup->cursrc[cursrclen - 1] == '\\') {
      strcat(setup->cursrc, "*.*");
    }

    /* remember where the path in cursrc ends */
    for (t = 0; setup->cursrc[t] != 0; t++) {
      if (setup->cursrc[t] == '\\') pathendoffset = t + 1;
    }

    /* */
    if (findfirst(dta, setup->cursrc, 0) != 0) {
      continue;
    }

    do {
      char appendflag;
      if (dta->attr & DOS_ATTR_DIR) continue; /* skip directories */

      /* compute full path/name of the file */
      strcpy(setup->cursrc + pathendoffset, dta->fname);

      /* if there was no destination, then YOU are the destination now!
       * this handles situations like COPY a.txt+b.txt+c.txt */
      if (setup->dst[0] == NULL) {
        strcpy(setup->dst, setup->cursrc);
        setup->dstlen = strlen(setup->dst);
        copiedcount_in++;
        copiedcount_out++;
        continue;
      }

      /* is dst ending with a backslash? then append fname to it */
      if (setup->dst[setup->dstlen - 1] == '\\') strcpy(setup->dst + setup->dstlen, dta->fname);

      /* now cursrc contains the full source and dst contains the full dest... COPY TIME! */

      /* if dst file exists already -> overwrite it or append?
          - if dst is a dir (dstlen-1 points at a \\) -> overwrite
          - otherwise: if copiedcount_in==0 overwrite, else append */
      output(setup->cursrc);
      if ((setup->dst[setup->dstlen - 1] == '\\') || (copiedcount_in == 0)) {
        appendflag = 0;
        output(" > ");
        copiedcount_out++;
      } else {
        appendflag = 1;
        output(" >> ");
      }
      outputnl(setup->dst);

      t = cmd_copy_internal(setup->dst, 0, setup->cursrc, 0, appendflag, setup->databuf, setup->databufsz);
      if (t != 0) {
        outputnl(doserr(t));
        return(-1);
      }

      copiedcount_in++;
    } while (findnext(dta) == 0);

  }

  sprintf(setup->databuf, "%u file(s) copied", copiedcount_out);
  outputnl(setup->databuf);

  return(-1);
}
