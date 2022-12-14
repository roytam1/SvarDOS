/*
 * Video routines used by the SvarDOS installer
 * Copyright (C) 2016 Mateusz Viste
 */

#ifndef VIDEO_H_SENTINEL
#define VIDEO_H_SENTINEL

int video_init(void);
void video_clear(unsigned short attr, int offset, int offsetend);
void video_putchar(int y, int x, unsigned short attr, int c);
void video_putcharmulti(int y, int x, unsigned short attr, int c, int repeat, int step);
void video_putstring(int y, int x, unsigned short attr, const char *str, int maxlen);
void video_putstringfix(int y, int x, unsigned short attr, const char *s, int w);
void video_movecursor(int y, int x);

#endif
