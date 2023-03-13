 
#ifndef __IMAGE_PROCESS__
#define __IMAGE_PROCESS__

#ifdef YUV2JPEG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
 #include <jpeglib.h>
char clip_value(char value, int min, int max);
int YUV420_TO_RGB24(char *yuvBuf_y, char *yuvBuf_uv, int w, int h, char *rgbBuf);
int rgb2jpg(char *jpg_file, char *pdata, int width, int height);
#endif

#endif//IMAGE_PROCESS