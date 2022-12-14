/* This file is part of the SvarCOM project and is published under the terms
 * of the MIT license.
 *
 * Copyright (C) 2021-2022 Mateusz Viste
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
 * ver
 */

#define PVER "2022.2"
#define COPYRDATE "2021-2022"

static enum cmd_result cmd_ver(struct cmd_funcparam *p) {
  char *buff = p->BUFFER;
  unsigned char maj = 0, min = 0;

  /* help screen */
  if (cmd_ishlp(p)) {
    nls_outputnl(20,0); /* "Displays the DOS version." */
    outputnl("");
    outputnl("ver [/about]");
#ifdef VERDBG
    outputnl("ver /dbg");
#endif
    return(CMD_OK);
  }

#ifdef VERDBG
  if ((p->argc == 1) && (imatch(p->argv[0], "/dbg"))) {
    unsigned short far *rmod_envseg = MK_FP(p->rmod->rmodseg, RMOD_OFFSET_ENVSEG);
    unsigned char far *rmod_exitcode = MK_FP(p->rmod->rmodseg, RMOD_OFFSET_LEXITCODE);
    unsigned short far *rmod_comspecptr = MK_FP(p->rmod->rmodseg, RMOD_OFFSET_COMSPECPTR);
    char far *fptr;
    unsigned short i;
    printf("rmod->rmodseg = 0x%04X\r\n", p->rmod->rmodseg);
    printf("rmod->origparent = %04X:%04X\r\n", p->rmod->origparent >> 16, p->rmod->origparent & 0xffff);
    printf("rmod->origenvseg = 0x%04X\r\n", p->rmod->origenvseg);
    printf("rmod->flags = 0x%02X\r\n", p->rmod->flags);
    printf("[rmod:RMOD_OFFSET_ENVSEG] = 0x%04X\r\n", *rmod_envseg);
    printf("environment allocated size: %u bytes\r\n", env_allocsz(*rmod_envseg));
    for (fptr = MK_FP(p->rmod->rmodseg, RMOD_OFFSET_BOOTDRIVE), i = 0; *fptr != 0; fptr++) buff[i++] = *fptr;
    buff[i] = 0;
    printf("[rmod:RMOD_OFFSET_BOOTCOMSPEC] = '%s'\r\n", buff);
    if (*rmod_comspecptr == 0) {
      sprintf(buff, "NULL");
    } else {
      for (fptr = MK_FP(*rmod_envseg, *rmod_comspecptr), i = 0; *fptr != 0; fptr++) buff[i++] = *fptr;
      buff[i] = 0;
    }
    printf("[rmod:RMOD_OFFSET_COMSPECPTR] = '%s'\r\n", buff);
    printf("[rmod:RMOD_OFFSET_LEXITCODE] = %u\r\n", *rmod_exitcode);
    printf("rmod dump (first 64 bytes at [rmodseg:0100h]):\r\n");
    fptr = MK_FP(p->rmod->rmodseg, 0x100);
    for (i = 0; i < 64; i += 16) {
      int ii;
      for (ii = i; ii < i + 16; ii++) printf(" %02X", fptr[ii]);
      printf("   ");
      for (ii = i; ii < i + 16; ii++) {
        if (fptr[ii] < ' ') {
          printf(".");
        } else {
          printf("%c", fptr[ii]);
        }
      }
      printf("\r\n");
    }

    return(CMD_OK);
  }
#endif

  if ((p->argc == 1) && (imatch(p->argv[0], "/about"))) {
    nls_outputnl(20,3); /* "SvarCOM is a shell interpreter for DOS kernels compatible with MS-DOS 5+." */
    outputnl("");
    nls_outputnl(20,4); /* "This software is distributed under the terms of the MIT license." */
    outputnl("Copyright (C) " COPYRDATE " Mateusz Viste");
    outputnl("");
    outputnl("Program ten dedykuje Milenie i Mojmirowi. Zycze wam, byscie w swoim zyciu");
    outputnl("potrafili docenic wartosci minionych pokolen, jednoczesnie czerpiac radosc");
    outputnl("z prostych przyjemnosci dnia codziennego.  Lair, jesien 2021.");
    return(CMD_OK);
  }

  if (p->argc != 0) {
    nls_outputnl(0,6); /* "Invalid parameter" */
    return(CMD_FAIL);
  }

  _asm {
    push ax
    push bx
    push cx
    mov ah, 0x30   /* Get DOS version number */
    int 0x21       /* AL=maj_ver_num  AH=min_ver_num  BX,CX=OEM */
    mov [maj], al
    mov [min], ah
    pop cx
    pop bx
    pop ax
  }

  sprintf(buff, svarlang_str(20,1), maj, min); /* "DOS kernel version %u.%u" */

  outputnl(buff);
  nls_output(20,2); /* "SvarCOM shell ver" */
  outputnl(" " PVER);
  return(CMD_OK);
}
