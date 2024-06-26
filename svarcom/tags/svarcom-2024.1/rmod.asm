;
; rmod - resident module of the SvarCOM command interpreter (NASM code)
;
; Copyright (C) 2021-2022 Mateusz Viste
; MIT license
;
; this is installed in memory by the transient part of SvarCOM. it has only
; two jobs: providing a resident buffer for command history, environment, etc
; and respawning COMMAND.COM whenever necessary.

CPU 8086
org 0x100

PSP_ENVSEG equ 0x2C

section .text    ; all goes into code segment

                 ; offset
SIG1 dw 0x1983   ;  +0
SIG2 dw 0x1985   ;  +2
SIG3 dw 0x2017   ;  +4
SIG4 dw 0x2019   ;  +6  acts also as a guardval to detect severe stack overflows

; Buffer used to remember previous command, when SvarCOM calls the buffered
; input service at INT 21h,AH=0x0A.
; This buffer is right before the stack, so in case of a stack overflow event
; (for example because of a "too ambitious" TSR) only this buffer is damaged,
; and can be invalidated without much harm. To detect such damage, SvarCOM's
; transient part is appending a signature at the end of the buffer.
INPUTBUF: times 132 db 0 ; 130 bytes for the input buffer + 2 for signature

; DOS int 21h functions that I use require at least 40 bytes of stack under
; DOS-C (FreeDOS) kernel, so here I reserve 64 bytes juste to be sure
STACKBUF db "XXX  SVARCOM RMOD BY MATEUSZ VISTE  XXXXXXXXXXXXXXXXXXXXXXXXXXXX"
STACKPTR dw 0

; offset of the COMSPEC variable in the environment block, 0 means "use
; boot drive". this value is patched by the transient part of COMMAND.COM
COMSPECPTR dw 0  ; +CEh

; fallback COMSPEC string used if no COMPSEC is present in the environment
; drive. drive is patched by the transient part of COMMAND.COM
COMSPECBOOT db "@:\COMMAND.COM", 0 ; +D0h

; exit code of last application
LEXCODE  db 0    ; +DFh

; ExecParamRec used by INT 21h, AX=4b00 (load and execute program), 14 bytes:
;  offset  size  content
;     +0     2   segment of environment for child (0 = current)
;     +2     4   address of command line to place at PSP:0080
;     +6     4   address of an FCB to be placed at PSP:005c
;    +0Ah    4   address of an FCB to be placed at PSP:006c
EXEC_PARAM_REC db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   ; +E0h

; Program to execute, preset by SvarCOM (128 bytes, ASCIIZ)
EXECPROG: times 128 db 0                                     ; +EEh

; File where stdin and stdout should be redirected (0 = no redirection)
REDIR_INFIL:     times 128 db 0     ; +16Eh
REDIR_OUTFIL:    times 128 db 0     ; +1EEh
REDIR_OUTAPPEND: dw 0               ; +26Eh
REDIR_DEL_STDIN: db 0               ; +270h  indicates that the stdin file
                                    ;        should be deleted (pipes). This
                                    ;        MUST contain the 1st char of
                                    ;        REDIR_INFIL!

; CTRL+BREAK (int 23h) handler
; According to the TechHelp! Manual: "If you want to abort (exit to the parent
; process), then set the carry flag and return via a FAR RET. This causes DOS
; to perform normal cleanup and exit to the parent." (otherwise use iret)
BREAK_HANDLER:            ; +271h
stc
retf

; INT 0x2E handler
INT2E:
xor ax, ax
iret

skipsig:                  ; +276h

; set up CS=DS=SS and point SP to my private stack buffer
mov ax, cs
mov ds, ax
mov es, ax
mov ss, ax
mov sp, STACKPTR

; set up myself as break handler (int 0x23)
mov ax, 0x2523  ; set int vector 23h
mov dx, BREAK_HANDLER
int 0x21

; set up myself as int 0x2E handler ("pass command to shell")
mov ax, 0x252E
mov dx, INT2E ; TODO do something meaningful instead of a no-op
int 0x21

; revert stdin/stdout redirections (if any) to their initial state
call REVERT_REDIR_IF_ANY

; redirect stdin and/or stdout if required
call REDIR_INOUTFILE_IF_REQUIRED

; should I execute command.com or a pre-set application?
cmp [EXECPROG], byte 0
jz EXEC_COMMAND_COM

; TODO: perhaps I should call the DOS SetPSP function here? But if I do, the
;       int 21h, ah=50h call freezes...
;mov ah, 0x50           ; DOS 2+ -- Set PSP
;mov bx, cs
;int 0x21

; exec an application preset (by SvarCOM) in the ExecParamRec
mov ax, 0x4B00         ; DOS 2+ - load & execute program
mov dx, EXECPROG       ; DS:DX  - ASCIZ program name (preset at PSP[already)
mov bx, EXEC_PARAM_REC ; ES:BX  - parameter block pointer
int 0x21
mov [cs:EXECPROG], byte 0 ; do not run app again (+DS might have been changed)

jmp short skipsig      ; enforce valid ds/ss/etc (can be lost after int 21,4b)

EXEC_COMMAND_COM:

; collect the exit code of previous application
mov ah, 0x4D
int 0x21
mov [LEXCODE], al

; zero out the exec param block (14 bytes)
mov al, 0              ; byte to write
mov cx, 14             ; how many times
mov di, EXEC_PARAM_REC ; ES:DI = destination
cld                    ; stosb must move forward
rep stosb              ; repeat cx times

; preset the default COMSPEC pointer to ES:DX (ES is already set to DS)
mov dx, COMSPECBOOT

; do I have a valid COMSPEC?
or [COMSPECPTR], word 0
jz USEDEFAULTCOMSPEC
; set ES:DX to actual COMSPEC (in env segment)
mov es, [PSP_ENVSEG]
mov dx, [COMSPECPTR]
USEDEFAULTCOMSPEC:

; prepare the exec param block
mov ax, [PSP_ENVSEG]
mov [EXEC_PARAM_REC], ax
mov [EXEC_PARAM_REC+2], word CMDTAIL
mov [EXEC_PARAM_REC+4], cs

; execute command.com
mov ax, 0x4B00         ; DOS 2+ - load & execute program
push es                ;
pop ds                 ;
;mov dx, COMSPEC       ; DS:DX  - ASCIZ program name (preset already)
push cs
pop es
mov bx, EXEC_PARAM_REC ; ES:BX  - parameter block pointer
int 0x21

; if all went well, jump back to start
jnc skipsig

; restore DS=CS
mov bx, cs
mov ds, bx

; update error string so it contains the error number
add al, '0'
mov [ERRLOAD + 4], al

; display error message
mov ah, 0x09
mov dx, ERRLOAD
int 0x21

; wait for keypress
mov ah, 0x08
int 0x21

; back to program start
jmp skipsig

; command.com tail arguments, in PSP format: length byte followed by args and
; terminated with \r) - a single 0x0A byte is passed so SvarCOM knows it is
; called as respawn (as opposed to being invoked as a normal application)
; this allows multiple copies of SvarCOM to stack upon each other.
CMDTAIL db 0x01, 0x0A, 0x0D

ERRLOAD db "ERR x, FAILED TO LOAD COMMAND.COM", 13, 10, '$'

; variables used to revert stdin/stdout to their initial state
OLD_STDOUT dw 0xffff
OLD_STDIN  dw 0xffff


; ****************************************************************************
; *** ROUTINES ***************************************************************
; ****************************************************************************

; ----------------------------------------------------------------------------
; revert stdin/stdout redirections (if any) to their initial state
REVERT_REDIR_IF_ANY:
; is stdout redirected?
mov bx, [OLD_STDOUT]
cmp bx, 0xffff
je STDOUT_DONE
; revert the stdout handle (dst in BX already)
mov cx, 1        ; src handle (1=stdout)
mov ah, 0x46     ; redirect a handle
int 0x21
; close the old handle (still in bx)
mov ah, 0x3e
int 0x21
mov [OLD_STDOUT], word 0xffff ; mark stdout as "not redirected"
STDOUT_DONE:

; is stdin redirected?
mov bx, [OLD_STDIN]
cmp bx, 0xffff
je STDIN_DONE
; revert the stdin handle (dst in BX already)
xor cx, cx       ; src handle (0=stdin)
mov ah, 0x46     ; redirect a handle
int 0x21
; close the old handle (still in bx)
mov ah, 0x3e
int 0x21
mov [OLD_STDIN], word 0xffff ; mark stdin as "not redirected"

; delete stdin file if required
cmp [REDIR_DEL_STDIN], byte 0
je STDIN_DONE
; revert the original file and delete it
mov ah, [REDIR_DEL_STDIN]
mov [REDIR_INFIL], ah
mov ah, 0x41     ; DOS 2+ - delete file pointed at by DS:DX
mov dx, REDIR_INFIL
int 0x21
mov [REDIR_INFIL], byte 0
mov [REDIR_DEL_STDIN], byte 0

STDIN_DONE:

ret
; ----------------------------------------------------------------------------


; ----------------------------------------------------------------------------
; redirect stdout if REDIR_OUTFIL points to something
REDIR_INOUTFILE_IF_REQUIRED:
cmp [REDIR_OUTFIL], byte 0
je NO_STDOUT_REDIR
mov si, REDIR_OUTFIL   ; si = output file
mov ax, 0x6c00         ; Extended Open/Create
mov bx, 1              ; access mode (0=read, 1=write, 2=r+w)
xor cx, cx             ; file attribs when(if) file is created (0=normal)
mov dx, [REDIR_OUTAPPEND] ; action if file exist (0x11=open, 0x12=truncate)
int 0x21               ; ax=handle on success (CF clear)
mov [REDIR_OUTFIL], byte 0
jc NO_STDOUT_REDIR     ; TODO: abort with an error message instead

; jump to end of file if flag was 0x11 (required for >> redirections)
cmp [REDIR_OUTAPPEND], word 0x11
jne SKIP_JMPEOF
mov bx, ax
mov ax, 0x4202         ; jump to position EOF - CX:DX in handle BX
xor cx, cx
xor dx, dx
int 0x21
mov ax, bx             ; put my handle back in ax, as expected by later code
SKIP_JMPEOF:

; duplicate current stdout so I can revert it later
push ax                ; save my file handle in stack
mov ah, 0x45           ; duplicate file handle BX
mov bx, 1              ; 1 = stdout
int 0x21               ; ax=new (duplicated) file handle
mov [OLD_STDOUT], ax   ; save the old handle in memory

; redirect stdout to my file
pop bx                 ; dst handle
mov cx, 1              ; src handle (1=stdout)
mov ah, 0x46           ; "redirect a handle"
int 0x21

; close the original file handle, I no longer need it
mov ah, 0x3e           ; close a file handle (handle in BX)
int 0x21
NO_STDOUT_REDIR:

; *** redirect stdin if REDIR_INFIL points to something ***
cmp [REDIR_INFIL], byte 0
je NO_STDIN_REDIR
mov dx, REDIR_INFIL    ; dx:dx = file
mov ax, 0x3d00         ; open file for read
int 0x21               ; ax=handle on success (CF clear)
mov [REDIR_INFIL], byte 0
jc NO_STDIN_REDIR      ; TODO: abort with an error message instead

; duplicate current stdin so I can revert it later
push ax                ; save my file handle in stack
mov ah, 0x45           ; duplicate file handle BX
xor bx, bx             ; 0=stdin
int 0x21               ; ax=new (duplicated) file handle
mov [OLD_STDIN], ax    ; save the old handle in memory

; redirect stdout to my file
pop bx                 ; dst handle
xor cx, cx             ; src handle (0=stdin)
mov ah, 0x46           ; "redirect a handle"
int 0x21

; close the original file handle, I no longer need it
mov ah, 0x3e           ; close a file handle (handle in BX)
int 0x21
NO_STDIN_REDIR:
ret
; ----------------------------------------------------------------------------
