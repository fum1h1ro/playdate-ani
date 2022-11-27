#ifndef __COMMON_H__
#define __COMMON_H__

#include "pd_api.h"

void common_initialize(PlaydateAPI *playdate_api);
void* common_loadfile(const char *path);
LCDBitmap* common_loadbitmap(const char *path);

#endif // __COMMON_H__
