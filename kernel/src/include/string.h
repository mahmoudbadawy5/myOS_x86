#ifndef _STRING_H
#define _STRING_H

unsigned char *memcpy(unsigned char *dest, const unsigned char *src, int count);
unsigned char *memset(unsigned char *dest, unsigned char val, int count);
unsigned short *memsetw(unsigned short *dest, unsigned short val, int count);
int strlen(const char *str);
int strcmp(const char *str1, const char *str2);
char* strcpy(char* destination, const char* source);

#endif