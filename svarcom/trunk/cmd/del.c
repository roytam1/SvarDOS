/*
 * del/erase
 */

static int cmd_del(struct cmd_funcparam *p) {
  const char *delspec = NULL;
  unsigned short err = 0;
  unsigned short confirmflag = 0;
  unsigned short i;
  unsigned short pathlimit = 0;
  char *buff = p->BUFFER;

  struct DTA *dta = (void *)0x80; /* use the default DTA at location 80h in PSP */
  char *fname = dta->fname;

  if (cmd_ishlp(p)) {
    outputnl("Deletes one or more files.");
    outputnl("");
    outputnl("DEL [drive:][path]filename [/P]");
    outputnl("ERASE [drive:][path]filename [/P]");
    outputnl("");
    outputnl("[drive:][path]filename  Specifies the file(s) to delete.");
    outputnl("/P  Prompts for confirmation before deleting each file.");
    return(-1);
  }

  if (p->argc == 0) {
    outputnl("Required parameter missing");
    return(-1);
  }

  /* scan argv for delspec and possible /p or /v */
  for (i = 0; i < p->argc; i++) {
    /* delspec? */
    if (p->argv[i][0] == '/') {
      if (imatch(p->argv[i], "/p")) {
        confirmflag = 1;
      } else {
        output("Invalid switch:");
        output(" ");
        outputnl(p->argv[i]);
        return(-1);
      }
    } else if (delspec != NULL) { /* otherwise its a delspec */
      outputnl("Too many parameters");
      return(-1);
    } else {
      delspec = p->argv[i];
    }
  }

  /* convert path to canonical form */
  file_truename(delspec, buff);

  /* is delspec pointing at a directory? if so, add a \*.* */
  { int attr = file_getattr(delspec);
    if ((attr > 0) && (attr & DOS_ATTR_DIR)) strcat(buff, "\\????????.???");
  }

  /* parse delspec in buff and remember where last backslash or slash is */
  for (i = 0; buff[i] != 0; i++) if (buff[i] == '\\') pathlimit = i + 1;

  /* is this about deleting all content inside a directory? if no per-file
   * confirmation set, ask for a global confirmation */
  if ((confirmflag == 0) && (imatch(buff + pathlimit, "????????.???"))) {
    outputnl("All files in directory will be deleted!");
    if (askchoice("Are you sure (Y/N)?", "YN") != 0) return(-1);
  }

  for (i = 0;; i = 1) {

    /* exec FindFirst or FindNext */
    if (i == 0) {
      err = findfirst(dta, buff, DOS_ATTR_RO | DOS_ATTR_SYS | DOS_ATTR_HID);
    } else {
      err = findnext(dta);
    }

    if (err != 0) break;

    /* ask if confirmation required: PLIK.TXT  Delete (Y/N)? */
    if (confirmflag) {
      strcpy(buff + pathlimit, fname); /* note: buff contained the search pattern but it no longer needed so I can reuse it now */
      output(buff);
      output(" \t");
      if (askchoice("Delete (Y/N)?", "YN") != 0) continue;
    }

    /* del found file */
    _asm {
      mov ah, 0x41      /* delete a file, DS:DX points to an ASCIIZ filespec (no wildcards allowed) */
      mov dx, fname
      int 0x21
      jnc DONE
      mov [err], ax
      DONE:
    }

    if (err != 0) {
      output(fname);
      output(": ");
      outputnl(doserr(err));
      break;
    }
  }
  return(-1);
}