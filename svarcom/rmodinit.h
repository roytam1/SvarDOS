#ifndef RMODINIT_H
#define RMODINIT_H

#define RMOD_OFFSET_ENVSEG  0x08
#define RMOD_OFFSET_INPBUFF 0x0A
#define RMOD_OFFSET_ROUTINE 0x8C

unsigned short rmod_install(unsigned short envsize);
unsigned short rmod_find(void);

#endif
