/*
 * SvarCOM is a command-line interpreter.
 *
 * a little memory area is allocated as high as possible. it contains:
 *  - a signature (like XMS drivers do)
 *  - a routine for exec'ing programs
 *  - a "last command" buffer for input history
 *
 * when svarcom starts, it tries locating the routine in memory.
 *
 * if found:
 *   waits for user input and processes it. if execing something is required, set the "next exec" field in routine's memory and quit.
 *
 * if not found:
 *   installs it by creating a new PSP, set int 22 vector to the routine, set my "parent PSP" to the routine
 *   and quit.
 *
 * PSP structure
 * http://www.piclist.com/techref/dos/psps.htm
 *
 *
 *
 * === MCB ===
 *
 * each time that DOS allocates memory, it prefixes the allocated memory with
 * a 16-bytes structure called a "Memory Control Block" (MCB). This control
 * block has the following structure:
 *
 * Offset  Size     Description
 *   00h   byte     'M' =  non-last member of the MCB chain
 *                  'Z' = indicates last entry in MCB chain
 *                  other values cause "Memory Allocation Failure" on exit
 *   01h   word     PSP segment address of the owner (Process Id)
 *                  possible values:
 *                    0 = free
 *                    8 = Allocated by DOS before first user pgm loaded
 *                    other = Process Id/PSP segment address of owner
 *   03h  word      number of paragraphs related to this MCB (excluding MCB)
 *   05h  11 bytes  reserved
 *   10h  ...       start of actual allocated memory block
 */

#include <i86.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <process.h>

#include "cmd.h"
#include "env.h"
#include "helpers.h"
#include "redir.h"
#include "rmodinit.h"

struct config {
  int locate;
  int install;
  unsigned short envsiz;
} cfg;


static void parse_argv(struct config *cfg, int argc, char **argv) {
  int i;
  memset(cfg, 0, sizeof(*cfg));

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "/locate") == 0) {
      cfg->locate = 1;
    }
    if (strstartswith(argv[i], "/e:") == 0) {
      if ((atouns(&(cfg->envsiz), argv[i] + 3) != 0) || (cfg->envsiz < 64)) {
        cfg->envsiz = 0;
      }
    }
  }
}


static void buildprompt(char *s, unsigned short envseg) {
  /* locate the prompt variable or use the default pattern */
  const char far *fmt = env_lookup(envseg, "PROMPT");
  while ((fmt != NULL) && (*fmt != 0)) {
    fmt++;
    if (fmt[-1] == '=') break;
  }
  if ((fmt == NULL) || (*fmt == 0)) fmt = "$p$g"; /* fallback to default if empty */
  /* build the prompt string based on pattern */
  for (; *fmt != 0; fmt++) {
    if (*fmt != '$') {
      *s = *fmt;
      s++;
      continue;
    }
    /* escape code ($P, etc) */
    fmt++;
    switch (*fmt) {
      case 'Q':  /* $Q = = (equal sign) */
      case 'q':
        *s = '=';
        s++;
        break;
      case '$':  /* $$ = $ (dollar sign) */
        *s = '$';
        s++;
        break;
      case 'T':  /* $t = current time */
      case 't':
        s += sprintf(s, "00:00"); /* TODO */
        break;
      case 'D':  /* $D = current date */
      case 'd':
        s += sprintf(s, "1985-07-29"); /* TODO */
        break;
      case 'P':  /* $P = current drive and path */
      case 'p':
        _asm {
          mov ah, 0x19    /* DOS 1+ - GET CURRENT DRIVE */
          int 0x21
          mov bx, s
          mov [bx], al  /* AL = drive (00 = A:, 01 = B:, etc */
        }
        *s += 'A';
        s++;
        *s = ':';
        s++;
        *s = '\\';
        s++;
        _asm {
          mov ah, 0x47    /* DOS 2+ - CWD - GET CURRENT DIRECTORY */
          xor dl,dl       /* DL = drive number (00h = default, 01h = A:, etc) */
          mov si, s       /* DS:SI -> 64-byte buffer for ASCIZ pathname */
          int 0x21
        }
        while (*s != 0) s++; /* move ptr forward to end of pathname */
        break;
      case 'V':  /* $V = DOS version number */
      case 'v':
        s += sprintf(s, "VER"); /* TODO */
        break;
      case 'N':  /* $N = current drive */
      case 'n':
        _asm {
          mov ah, 0x19    /* DOS 1+ - GET CURRENT DRIVE */
          int 0x21
          mov bx, s
          mov [bx], al  /* AL = drive (00 = A:, 01 = B:, etc */
        }
        *s += 'A';
        s++;
        break;
      case 'G':  /* $G = > (greater-than sign) */
      case 'g':
        *s = '>';
        s++;
        break;
      case 'L':  /* $L = < (less-than sign) */
      case 'l':
        *s = '<';
        s++;
        break;
      case 'B':  /* $B = | (pipe) */
      case 'b':
        *s = '|';
        s++;
        break;
      case 'H':  /* $H = backspace (erases previous character) */
      case 'h':
        *s = '\b';
        s++;
        break;
      case 'E':  /* $E = Escape code (ASCII 27) */
      case 'e':
        *s = 27;
        s++;
        break;
      case '_':  /* $_ = CR+LF */
        *s = '\r';
        s++;
        *s = '\n';
        s++;
        break;
    }
  }
  *s = '$';
}


static void run_as_external(const char far *cmdline) {
  char buff[256];
  char const *argvlist[256];
  int i, n;
  /* copy buffer to a near var (incl. trailing CR), insert a space before
     every slash to make sure arguments are well separated */
  n = 0;
  i = 0;
  for (;;) {
    if (cmdline[i] == '/') buff[n++] = ' ';
    buff[n++] = cmdline[i++];
    if (buff[n] == 0) break;
  }

  cmd_explode(buff, cmdline, argvlist);

  /* for (i = 0; argvlist[i] != NULL; i++) printf("arg #%d = '%s'\r\n", i, argvlist[i]); */

  /* must be an external command then. this call should never return, unless
   * the other program failed to be executed. */
  execvp(argvlist[0], argvlist);
}


static void set_comspec_to_self(unsigned short envseg) {
  unsigned short *psp_envseg = (void *)(0x2c); /* pointer to my env segment field in the PSP */
  char far *myenv = MK_FP(*psp_envseg, 0);
  unsigned short varcount;
  char buff[256] = "COMSPEC=";
  char *buffptr = buff + 8;
  /* who am i? look into my own environment, at the end of it should be my EXEPATH string */
  while (*myenv != 0) {
    /* consume a NULL-terminated string */
    while (*myenv != 0) myenv++;
    /* move to next string */
    myenv++;
  }
  /* get next word, if 1 then EXEPATH follows */
  myenv++;
  varcount = *myenv;
  myenv++;
  varcount |= (*myenv << 8);
  myenv++;
  if (varcount != 1) return; /* NO EXEPATH FOUND */
  while (*myenv != 0) {
    *buffptr = *myenv;
    buffptr++;
    myenv++;
  }
  *buffptr = 0;
  /* printf("EXEPATH: '%s'\r\n", buff); */
  env_setvar(envseg, buff);
}


int main(int argc, char **argv) {
  static struct config cfg;
  static unsigned short rmod_seg;
  static unsigned short far *rmod_envseg;
  static unsigned short far *lastexitcode;
  static unsigned char BUFFER[4096];

  parse_argv(&cfg, argc, argv);

  rmod_seg = rmod_find();
  if (rmod_seg == 0xffff) {
    rmod_seg = rmod_install(cfg.envsiz);
    if (rmod_seg == 0xffff) {
      outputnl("ERROR: rmod_install() failed");
      return(1);
    }
/*    printf("rmod installed at seg 0x%04X\r\n", rmod_seg); */
  } else {
/*    printf("rmod found at seg 0x%04x\r\n", rmod_seg); */
  }

  rmod_envseg = MK_FP(rmod_seg, RMOD_OFFSET_ENVSEG);
  lastexitcode = MK_FP(rmod_seg, RMOD_OFFSET_LEXITCODE);

  /* make COMPSEC point to myself */
  set_comspec_to_self(*rmod_envseg);

/*  {
    unsigned short envsiz;
    unsigned short far *sizptr = MK_FP(*rmod_envseg - 1, 3);
    envsiz = *sizptr;
    envsiz *= 16;
    printf("rmod_inpbuff at %04X:%04X, env_seg at %04X:0000 (env_size = %u bytes)\r\n", rmod_seg, RMOD_OFFSET_INPBUFF, *rmod_envseg, envsiz);
  }*/

  for (;;) {
    char far *cmdline = MK_FP(rmod_seg, RMOD_OFFSET_INPBUFF + 2);
    unsigned char far *echostatus = MK_FP(rmod_seg, RMOD_OFFSET_ECHOFLAG);

    if (*echostatus != 0) outputnl(""); /* terminate the previous command with a CR/LF */

    SKIP_NEWLINE:

    /* print shell prompt (only if ECHO is enabled) */
    if (*echostatus != 0) {
      char *promptptr = BUFFER;
      buildprompt(promptptr, *rmod_envseg);
      _asm {
        push ax
        push dx
        mov ah, 0x09
        mov dx, promptptr
        int 0x21
        pop dx
        pop ax
      }
    }

    /* revert input history terminator to \r */
    if (cmdline[-1] != 0) {
      cmdline[(unsigned short)(cmdline[-1])] = '\r';
    }

    /* wait for user input */
    _asm {
      push ax
      push bx
      push cx
      push dx
      push ds

      /* is DOSKEY support present? (INT 2Fh, AX=4800h, returns non-zero in AL if present) */
      mov ax, 0x4800
      int 0x2f
      mov bl, al /* save doskey status in BL */

      /* set up buffered input */
      mov ax, rmod_seg
      push ax
      pop ds
      mov dx, RMOD_OFFSET_INPBUFF

      /* execute either DOS input or DOSKEY */
      test bl, bl /* zf set if no DOSKEY present */
      jnz DOSKEY

      mov ah, 0x0a
      int 0x21
      jmp short DONE

      DOSKEY:
      mov ax, 0x4810
      int 0x2f

      DONE:
      /* terminate command with a CR/LF */
      mov ah, 0x02 /* display character in dl */
      mov dl, 0x0d
      int 0x21
      mov dl, 0x0a
      int 0x21

      pop ds
      pop dx
      pop cx
      pop bx
      pop ax
    }

    /* if nothing entered, loop again (but without appending an extra CR/LF) */
    if (cmdline[-1] == 0) goto SKIP_NEWLINE;

    /* replace \r by a zero terminator */
    cmdline[(unsigned char)(cmdline[-1])] = 0;

    /* move pointer forward to skip over any leading spaces */
    while (*cmdline == ' ') cmdline++;

    /* update rmod's ptr to COMPSPEC so it is always up to date */
    rmod_updatecomspecptr(rmod_seg, *rmod_envseg);

    /* handle redirections (if any) */
    if (redir_parsecmd(cmdline, BUFFER) != 0) {
      outputnl("");
      continue;
    }

    /* try matching (and executing) an internal command */
    {
      int ecode = cmd_process(rmod_seg, *rmod_envseg, cmdline, BUFFER);
      if (ecode >= 0) *lastexitcode = ecode;
      if (ecode >= -1) { /* internal command executed */
        redir_revert(); /* revert stdout (in case it was redirected) */
        continue;
      }
    }

    /* if here, then this was not an internal command */
    run_as_external(cmdline);

    /* revert stdout (in case it was redirected) */
    redir_revert();

    /* execvp() replaces the current process by the new one
    if I am still alive then external command failed to execute */
    outputnl("Bad command or file name");

  }

  return(0);
}