/****************************************************************************

  Win32 File compatibility for DOS.
  [This version does support LFNs, if available.]

  Written by: Kenneth J. Davis
  Date:       August, 2000
  Contact:    jeremyd@computer.org


Copyright (c): Public Domain [United States Definition]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR AUTHORS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

****************************************************************************/


/*** Expects Pointers to be near (Tiny, Small, and Medium models ONLY) ***/

#include <dos.h>
#include <stdlib.h>
#include <string.h>

#include "dosdisk.h"

#define searchAttr ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | \
   FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE )

/* If this variable is nonzero then will 1st attempt LFN findfirst
 * (findfirst calls sets flag, so findnext/findclose know proper method to continue)
 * else if 0 then only attempt old 0x4E findfirst.
 * This is mostly a debugging tool, may be useful during runtime.
 * Default is LFN_ENABLE.
 */
int LFN_Enable_Flag = LFN_ENABLE;


/* copy old style findfirst data FFDTA to a WIN32_FIND_DATA
 * NOTE: does not map exactly.
 * internal to this module only.
 */
static void copyFileData(struct WIN32_FIND_DATA *findData, const struct FFDTA *finfo)
{
  /* Copy requried contents over into required structure */
  strcpy(findData->cFileName, finfo->ff_name);
  findData->dwFileAttributes = (DWORD)finfo->ff_attr;

  /* copy over rest (not quite properly) */
  findData->ftCreationTime.ldw[0] = finfo->ff_ftime;
  findData->ftLastAccessTime.ldw[0] = finfo->ff_ftime;
  findData->ftLastWriteTime.ldw[0] = finfo->ff_ftime;
  findData->ftCreationTime.ldw[1] = finfo->ff_fdate;
  findData->ftLastAccessTime.ldw[1] = finfo->ff_fdate;
  findData->ftLastWriteTime.ldw[1] = finfo->ff_fdate;
  findData->ftCreationTime.hdw = 0;
  findData->ftLastAccessTime.hdw = 0;
  findData->ftLastWriteTime.hdw = 0;
  findData->nFileSizeHigh = 0;
  findData->nFileSizeLow = (DWORD)finfo->ff_fsize;
  findData->dwReserved0 = 0;
  findData->dwReserved1 = 0;
}

HANDLE FindFirstFile(const char *pathname, struct WIN32_FIND_DATA *findData)
{
  static char path[1024];
  HANDLE hnd;
  short cflag = 0;  /* used to indicate if findfirst is succesful or not */

  /* verify findData is valid */
  if (findData == NULL)
    return INVALID_HANDLE_VALUE;

  /* allocate memory for the handle */
  if ((hnd = (HANDLE)malloc(sizeof(struct FindFileStruct))) == NULL)
    return INVALID_HANDLE_VALUE;

  /* initialize structure (clear) */
  /* hnd->handle = 0;  hnd->ffdtaptr = NULL; */
  memset(hnd, 0, sizeof(struct FindFileStruct));

  /* Clear findData, this is to fix a glitch under NT, with 'special' $???? files */
  memset(findData, 0, sizeof(struct WIN32_FIND_DATA));

  /* First try DOS LFN (0x714E) findfirst, going to old (0x4E) if error */
  if (LFN_Enable_Flag)
  {
    unsigned short attr = searchAttr;
    unsigned short varax = 0xffff;
    unsigned short finddata_seg = FP_SEG(findData);
    unsigned short finddata_off = FP_OFF(findData);
    unsigned char cf = 1;
    unsigned short pathname_seg = FP_SEG(pathname);
    unsigned short pathname_off = FP_OFF(pathname);
    hnd->flag = FINDFILELFN;

    _asm {
      push ax
      push bx
      push cx
      push dx
      push es
      push si
      push di

      stc                          //; In case not supported
      mov si, 1                    //; same format as when old style used, set to 0 for 64bit value
      mov di, finddata_off         //; Set address of findData into ES:DI
      mov es, finddata_seg
      mov ax, 0x714E               //; LFN version of FindFirst
      mov cx, attr
      mov dx, pathname_off         //; Load DS:DX with pointer to path for Findfirst
      push ds
      mov ds, pathname_seg
      int 0x21                     //; Execute interrupt
      pop ds
      jc lfnerror
      mov cf, 0
      mov varax, ax
      lfnerror:

      pop di
      pop si
      pop es
      pop dx
      pop cx
      pop bx
      pop ax
    }
    if (cf == 0) {
      hnd->fhnd.handle = varax;  /* store handle finally :) */
      return hnd;
    }

    /* AX is supposed to contain 7100h if function not supported, else real error */
    /* However, FreeDOS returns AX = 1 = Invalid function number instead.         */
    if ((varax != 0x7100) && (varax != 0x0001)) {
      free(hnd);
      return INVALID_HANDLE_VALUE;
    }
  }

  /* Use DOS (0x4E) findfirst, returning if error */
  hnd->flag = FINDFILEOLD;

  /* allocate memory for the FFDTA */
  if ((hnd->fhnd.ffdtaptr = (struct FFDTA *)malloc(sizeof(struct FFDTA))) == NULL)
  {
    free(hnd);
    return INVALID_HANDLE_VALUE;
  }

  /* if pathname ends in \* convert to \*.* */
  strcpy(path, pathname);
  {
  int eos = strlen(path) - 1;
  if ((path[eos] == '*') && (path[eos - 1] == '\\')) strcat(path, ".*");
  }

  {
    unsigned short ffdta_seg, ffdta_off;
    unsigned short path_seg, path_off;
    unsigned short sattr = searchAttr;
    ffdta_seg = FP_SEG((*hnd).fhnd.ffdtaptr);
    ffdta_off = FP_OFF((*hnd).fhnd.ffdtaptr);
    path_seg = FP_SEG(path);
    path_off = FP_OFF(path);

  _asm {
    push ax
    push bx
    push cx
    push dx
    push es

    mov ah, 0x2F                   //; Get Current DTA
    int 0x21                       //; Execute interrupt, returned in ES:BX
    push bx                        //; Store its Offset, then Segment
    push es
    mov ah, 0x1A                   //; Set Current DTA to our buffer, DS:DX
    mov dx, ffdta_off
    push ds
    mov ds, ffdta_seg
    int 0x21                       //; Execute interrupt
    pop ds
    mov ax, 0x4E00                 //; Actual findfirst call
    mov cx, sattr
    mov dx, path_off               //; Load DS:DX with pointer to path for Findfirt
    push ds
    mov ds, path_seg
    int 0x21                       //; Execute interrupt
    pop ds
    jnc success                    //; If carry is not set then succesful
    mov [cflag], ax                //; Set flag with error.
success:
    mov ah, 0x1A              //; Set Current DTA back to original, DS:DX
    mov dx, ds                //; Store DS, must be preserved
    pop ds                    //; Popping ES into DS since thats where we need it.
    pop bx                    //; Now DS:BX points to original DTA
    int 0x21                  //; Execute interrupt to restore.
    mov ds, dx                //; Restore DS

    pop es
    pop dx
    pop cx
    pop bx
    pop ax
  }
  }

  if (cflag)
  {
    free(hnd->fhnd.ffdtaptr);
    free(hnd);
    return INVALID_HANDLE_VALUE;
  }

  /* copy its results over */
  copyFileData(findData, hnd->fhnd.ffdtaptr);

  return hnd;
}


int FindNextFile(HANDLE hnd, struct WIN32_FIND_DATA *findData)
{
  short cflag = 0;  /* used to indicate if dos findnext succesful or not */

  /* if bad handle given return */
  if ((hnd == NULL) || (hnd == INVALID_HANDLE_VALUE)) return 0;

  /* verify findData is valid */
  if (findData == NULL) return 0;

  /* Clear findData, this is to fix a glitch under NT, with 'special' $???? files */
  memset(findData, 0, sizeof(struct WIN32_FIND_DATA));

  /* Flag indicate if using LFN DOS (0x714F) or not */
  if (hnd->flag == FINDFILELFN)
  {
    unsigned short handle = hnd->fhnd.handle;
    unsigned char cf = 0;
    _asm {
      push ax
      push bx
      push cx
      push dx
      push es
      push si
      push di

      mov bx, handle               //; Move the Handle returned by prev findfirst into BX
      stc                          //; In case not supported
      mov si, 1                    //; same format as when old style used, set to 0 for 64bit value
      mov ax, ds                   //; Set address of findData into ES:DI
      mov es, ax
      mov di, [findData]
      mov ax, 0x714F               //; LFN version of FindNext
      int 0x21                     //; Execute interrupt
      jnc DONE
      mov cf, 1
      DONE:

      pop di
      pop si
      pop es
      pop dx
      pop cx
      pop bx
      pop ax
    }
    if (cf == 0) return 1;   /* success */
    return 0;   /* Any errors here, no other option but to return error/no more files */
  } else { /* Use DOS (0x4F) findnext, returning if error */
    unsigned short dta_seg = FP_SEG((*hnd).fhnd.ffdtaptr);
    unsigned short dta_off = FP_OFF((*hnd).fhnd.ffdtaptr);
    _asm {
      push ax
      push bx
      push cx
      push dx
      push es

      mov ah, 0x2F                  //; Get Current DTA
      int 0x21                      //; Execute interrupt, returned in ES:BX
      push bx                       //; Store its Offset, then Segment
      push es
      mov ax, 0x1A00                //; Set Current DTA to our buffer, DS:DX
      mov dx, dta_off
      push ds
      mov ds, dta_seg
      int 0x21                      //; Execute interrupt
      pop ds
      mov ax, 0x4F00                //; Actual findnext call
      int 0x21                      //; Execute interrupt
      JNC success                   //; If carry is not set then succesful
      mov [cflag], ax               //; Set flag with error.
success:
      mov ah, 0x1A                  //; Set Current DTA back to original, DS:DX
      MOV dx, ds                    //; Store DS, must be preserved
      pop ds                        //; Popping ES into DS since thats where we need it.
      pop bx                        //; Now DS:BX points to original DTA
      int 0x21                      //; Execute interrupt to restore.
      mov ds, dx                    //; Restore DS

      pop es
      pop dx
      pop cx
      pop bx
      pop ax
    }

  if (cflag) return 0;

  /* copy its results over */
  copyFileData(findData, hnd->fhnd.ffdtaptr);

  return 1;
  }
}


/* free resources to prevent memory leaks */
void FindClose(HANDLE hnd)
{
  /* 1st check if valid handle given */
  if ((hnd != NULL) && (hnd != INVALID_HANDLE_VALUE))
  {
    /* See if its for the new or old style findfirst */
    if (hnd->flag == FINDFILEOLD) /* Just free memory allocated */
    {
      if (hnd->fhnd.ffdtaptr != NULL)
        free(hnd->fhnd.ffdtaptr);
      hnd->fhnd.ffdtaptr = NULL;
    }
    else /* must call LFN findclose */
    {
      unsigned short handle = hnd->fhnd.handle;
      _asm {
        push ax
        push bx

        mov bx, handle            /* Move handle returned from findfirst into BX */
        stc
        mov ax, 0x71A1
        int 0x21                  /* carry set on error */

        pop bx
        pop ax
      }
      hnd->fhnd.handle = 0;
    }

    free(hnd);                    /* Free memory used for the handle itself */
  }
}

#include <stdio.h>

/**
 Try LFN getVolumeInformation 1st
 if successful, assume valid drive/share (ie will return 1 unless error getting label)
 if failed (any error other than unsupported) return 0
 if a drive (ie a hard drive, ram drive, network mapped to letter) [has : as second letter]
 {
   try findfirst for volume label. (The LFN api does not seem to support LABEL attribute searches.)
     If getVolInfo unsupported but get findfirst succeed, assume valid (ie return 1)
   Try Get Serial#
 }
 else a network give \\server\share and LFN getVol unsupported, assume failed 0, as the
   original findfirst/next I haven't seen support UNC naming, clear serial and volume.
*** Currently trying to find a way to get a \\server\share 's serial & volume if LFN available ***
*/

/* returns zero on failure, if lpRootPathName is NULL or "" we use current
 * default drive. */
int GetVolumeInformation(const char *lpRootPathName, char *lpVolumeNameBuffer,
  DWORD nVolumeNameSize, DWORD *lpVolumeSerialNumber) {

  /* Using DOS interrupt to get serial number */
  struct media_info {
    short dummy;
    DWORD serial;
    char volume[11];
    short ftype[8];
  } media;

  /* buffer to store file system name in lfn get volume information */
  char fsystem[32];

  /* Stores the root path we use. */
  char pathname[260];

  /* Used to determine if drive valid (success/failure of this function)
   * 0 = success
   * 1 = failure
   * 2 = LFN api unsupported (tentative failure)
   */
  int cflag=2;

  /* validate root path to obtain info on, NULL or "" means use current */
  if ((lpRootPathName == NULL) || (*lpRootPathName == '\0')) {
    /* Assume if NULL user wants current drive, eg C:\ */
    _asm {
      push ax
      push bx

      mov ah, 0x19            //; Get Current Default Drive
      int 0x21                //; returned in AL, 0=A, 1=B,...
      lea bx, pathname        //; load pointer to our buffer
      add al, 'A'             //; Convert #returned to a letter
      mov [bx], al            //; Store drive letter
      inc bx                  //; point to next character
      mov byte ptr [bx], ':'  //; Store the colon
      inc bx
      mov byte ptr [bx], '\'  //; this gets converted correctly as a single bkslsh
      inc bx
      mov byte ptr [bx], 0    //; Most importantly the '\0' terminator

      pop bx
      pop ax
    }
  } else {
    strcpy(pathname, lpRootPathName);
  }

  /* Flag indicate if using LFN DOS or not */
  if (LFN_Enable_Flag)
  {
    _asm {
      push ax
      push bx
      push cx
      push dx
      push es
      push di

      mov ax, 0x71A0           //; LFN GetVolumeInformation
      mov dx, ds               //; Load buffer for file system name into ES:DI
      MOV es, dx
      lea di, fsystem
      mov cx, 32               //; size of ES:DI buffer
      lea dx, pathname         //; Load root name into DS:DX
      stc                      //; in case LFN api unsupported
      int 0x21
      jc getvolerror           //; on any error skip storing any info
      mov cflag, 0             //; indicate no error
      jmp endgetvol
    /* store stuff
       BX = file system flags (see #01783)
       CX = maximum length of file name [usually 255]
       DX = maximum length of path [usually 260]
       fsystem buffer filled (ASCIZ, e.g. "FAT","NTFS","CDFS")
    */
getvolerror:
      cmp ax, 0x7100           //; see if real error or unsupported
      je endgetvol             //; if so skip ahead
      cmp ax, 0x0001           //; FreeDOS returns AX = 1 = Invalid function number
      je endgetvol
      mov cflag, 1             //; indicate failure
endgetvol:

      pop di
      pop es
      pop dx
      pop cx
      pop bx
      pop ax
    }
  }


  if (cflag != 1)  /* if no error validating volume info or LFN getVolInfo unsupported */
  {
    /* if a drive, test if valid, get volume, and possibly serial # */
    if (pathname[1] == ':') {
      struct FFDTA finfo;
      unsigned short attr_seg = FP_SEG(finfo.ff_attr);
      unsigned short attr_off = FP_OFF(finfo.ff_attr);
      unsigned short finfo_seg = FP_SEG(&finfo);
      unsigned short finfo_off = FP_OFF(&finfo);
      unsigned short pathname_seg = FP_SEG(pathname);
      unsigned short pathname_off = FP_OFF(pathname);

      /* assume these calls will succeed, change on an error */
      cflag = 0;

      /* get path ending in \*.*, */
      if (pathname[strlen(pathname)-1] != '\\')
	strcat(pathname, "\\*.*");
      else
	strcat(pathname, "*.*");

      /* Search for volume using old findfirst, as LFN version (NT5 DOS box) does
       * not recognize FILE_ATTRIBUTE_LABEL = 0x0008 as label attribute.
       */
      _asm {
        push ax
        push bx
        push cx
        push dx
        push es

        mov ah, 0x2F              //; Get Current DTA
        int 0x21                  //; Execute interrupt, returned in ES:BX
        push bx                   //; Store its Offset, then Segment
        push es
        mov ah, 0x1A              //; Set Current DTA to our buffer, DS:DX
        mov dx, finfo_off         //; Load our buffer for new DTA.
        push ds
        mov ds, finfo_seg
        int 0x21                  //; Execute interrupt
        pop ds
        mov ax, 0x4E00            //; Actual findfirst call
        mov cx, FILE_ATTRIBUTE_LABEL
        mov dx, pathname_off      //; Load DS:DX with pointer to modified RootPath for Findfirt
        push ds
        mov ds, pathname_seg
        int 0x21                  //; Execute interrupt, Carry set on error, unset on success
        pop ds
        jnc success               //; If carry is not set then succesful
        mov [cflag], ax           //; Set flag with error.
        jmp cleanup               //; Restore DTA
success:                        //; True volume entry only has volume attribute set [MS' LFNspec]
        mov bx, attr_off
        push es
        mov es, attr_seg
        mov al, [es:bx]              //; Looking for a BYTE that is FILE_ATTRIBUTE_LABEL only
        pop es
        and al, 0xDF              //; Ignore Archive bit
        cmp al, FILE_ATTRIBUTE_LABEL
        je cleanup                //; They match, so should be true volume entry.
        mov ax, 0x4F00            //; Otherwise keep looking (findnext)
        int 0x21                  //; Execute interrupt
        jnc success               //; If carry is not set then succesful
        mov [cflag], ax           //; Set flag with error.
cleanup:
        mov ah, 0x1A              //; Set Current DTA back to original, DS:DX
        mov dx, ds                //; Store DS, must be preserved
        pop ds                    //; Popping ES into DS since thats where we need it.
        pop bx                    //; Now DS:BX points to original DTA
        int 0x21                  //; Execute interrupt to restore.
        mov ds, dx                //; Restore DS

        pop es
        pop dx
        pop cx
        pop bx
        pop ax
      }
      /* copy over volume label, if buffer given */
      if (lpVolumeNameBuffer != NULL)
      {
	if (cflag != 0)    /* error or no label */
	  lpVolumeNameBuffer[0] = '\0';
	else                        /* copy up to buffer's size of label */
	{
	  strncpy(lpVolumeNameBuffer, finfo.ff_name, nVolumeNameSize);
	  lpVolumeNameBuffer[nVolumeNameSize-1] = '\0';
          /* slide characters over if longer than 8 to remove . */
          if (lpVolumeNameBuffer[8] == '.')
          {
            lpVolumeNameBuffer[8] = lpVolumeNameBuffer[9];
            lpVolumeNameBuffer[9] = lpVolumeNameBuffer[10];
            lpVolumeNameBuffer[10] = lpVolumeNameBuffer[11];
            lpVolumeNameBuffer[11] = '\0';
          }
        }
      }
      /* Test for no label found, which is not an error,
         Note: added the check for 0x02 as FreeDOS returns this instead
         at least for disks with LFN entries and no volume label.
      */
      if ((cflag == 0x12) || /* No more files or   */
          (cflag == 0x02))   /* File not found     */
        cflag = 0;       /* so assume valid drive  */


      /* Get Serial Number, only supports drives mapped to letters */
      media.serial = 0;         /* set to 0, stays 0 on an error */
      {
        unsigned short media_off = FP_OFF(&media);
        unsigned short media_seg = FP_SEG(&media);
        unsigned char drv = (pathname[0] & 0xDF) - 'A' + 1;
      _asm {
        push ax
        push bx
        push cx
        push dx

        xor bh, bh
        mov bl, drv             //; Clear BH, drive in BL
        mov cx, 0x0866          //; CH=disk drive, CL=Get Serial #
        mov ax, 0x440D          //; Generic IOCTL
        mov dx, media_off       //; DS:DX pointer to media structure
        push ds
        mov ds, media_seg
        int 0x21
        pop ds

        pop dx
        pop cx
        pop bx
        pop ax
      }
      }

/***************** Replaced with 'documented' version of Get Serial Number *********************/
      /* NT2000pro does NOT set or clear the carry for int21h subfunction 6900h
       *   if an error occurs, it leaves media unchanged.
       */
//      asm {
//        MOV AX, 0x6900
//        INT 21h                 //; Should set carry on error, clear on success [note NT5 does not]
//      }
/***************** End with 'undocumented' version of Get Serial Number *********************/

      if (lpVolumeSerialNumber != NULL)
        *lpVolumeSerialNumber = media.serial;
    }
    else /* a network drive, assume results of LFN getVolInfo, no volume or serial [for now] */
    {
      if (lpVolumeNameBuffer != NULL)
        lpVolumeNameBuffer[0] = '\0';

      if (lpVolumeSerialNumber != NULL)
        *lpVolumeSerialNumber = 0x0;
    }
  }

  /* If there was an error getting the validating drive return failure) */
  if (cflag)    /* cflag is nonzero on any errors */
    return 0;   /* zero means error! */
  else
    return 1;   /* Success (drive exists we think anyway) */
}


/* retrieve attributes (ReadOnly/System/...) about file or directory
 * returns (DWORD)-1 on error
 */
DWORD GetFileAttributes(const char *pathname) {
  union REGS r;
  struct SREGS s;
  char buffer[260];
  int slen;

  /* 1st try LFN - Extended get/set attributes (in case LFN used) */
  if (LFN_Enable_Flag)
  {
    r.x.ax = 0x7143;                  /* LFN API, Extended Get/Set Attributes */
    r.x.bx = 0x00;                    /* BL=0, get file attributes            */
    r.x.dx = FP_OFF(pathname);        /* DS:DX points to ASCIIZ filename      */

    segread(&s);                      /* load with current segment values     */
    s.ds = FP_SEG(pathname);          /* get Segment of our filename pointer  */

    r.x.cflag = 1;                    /* should be set when unsupported ***   */
    _asm stc;                          /* but clib usually ignores on entry    */

    /* Actually perform the call, carry should be set on error or unuspported */
    intdosx(&r, &r, &s);         /* Clib function to invoke DOS int21h call   */

    if (!r.x.cflag)              /* if carry not set then cx has desired info */
      return (DWORD)r.x.cx;
    /* else error other than unsupported LFN api or invalid function [FreeDOS]*/
    else if ((r.x.ax != 0x7100) || (r.x.ax != 0x01))
      return (DWORD)-1;
    /* else fall through to standard get/set file attribute call */
  }

  /* we must remove any slashes from end */
  slen = strlen(pathname) - 1;  /* Warning, assuming pathname is not ""   */
  strcpy(buffer, pathname);
  if ((buffer[slen] == '\\') || (buffer[slen] == '/')) /* ends in a slash */
  {
    /* don't remove from root directory (slen == 0),
     * ignore UNC paths as SFN doesn't handle them anyway
     * if slen == 2, then check if drive given (e.g. C:\)
     */
    if (slen && !(slen == 2 &&  buffer[1] == ':'))
      buffer[slen] = '\0';
  }
  /* return standard attributes */
  r.x.ax = 0x4300;                  /* standard Get/Set File Attributes */
  r.x.dx = FP_OFF(buffer);          /* DS:DX points to ASCIIZ filename      */
  segread(&s);                      /* load with current segment values     */
  s.ds = FP_SEG(buffer);            /* get Segment of our filename pointer  */
  intdosx(&r, &r, &s);              /* invoke the DOS int21h call           */

  //if (r.x.cflag) printf("ERROR getting std attributes of %s, DOS err %i\n", buffer, r.x.ax);
  if (r.x.cflag) return (DWORD)-1;  /* error obtaining attributes           */
  return (DWORD)(0x3F & r.x.cx); /* mask off any DRDOS bits     */
}
