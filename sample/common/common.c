#include "pd_api.h"

static PlaydateAPI *api = NULL;

void common_initialize(PlaydateAPI *playdate_api)
{
    api = playdate_api;
}

void* common_loadfile(const char *path)
{
    SDFile *file = api->file->open(path, kFileRead);
    api->file->seek(file, 0, SEEK_END);
    int len = api->file->tell(file);
    api->file->seek(file, 0, SEEK_SET);
    void *buf = api->system->realloc(NULL, len);
    api->file->read(file, buf, len);
    api->file->close(file);
    return buf;
}

LCDBitmap* common_loadbitmap(const char *path)
{
    const char *err = NULL;
    LCDBitmap *bmp = api->graphics->loadBitmap(path, &err);
    if (err != NULL)
    {
        api->system->error(err);
    }
    return bmp;
}

