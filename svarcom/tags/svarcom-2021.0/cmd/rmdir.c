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
 * rmdir
 */

static int cmd_rmdir(struct cmd_funcparam *p) {
  const char *dname = p->argv[0];
  unsigned short err = 0;

  if (cmd_ishlp(p)) {
    outputnl("Removes (deletes) a directory");
    outputnl("");
    outputnl("RMDIR [drive:]path");
    outputnl("RD [drive:]path");
    return(-1);
  }

  if (p->argc == 0) {
    outputnl("Required parameter missing");
    return(-1);
  }

  if (p->argc > 1) {
    outputnl("Too many parameters");
    return(-1);
  }

  if (p->argv[0][0] == '/') {
    outputnl("Invalid parameter");
    return(-1);
  }

  _asm {
    push ax
    push dx

    mov ah, 0x3a   /* delete a directory, DS:DX points to ASCIIZ dir name */
    mov dx, [dname]
    int 0x21
    jnc DONE
    mov [err], ax
    DONE:

    pop dx
    pop ax
  }

  if (err != 0) outputnl(doserr(err));

  return(-1);
}
