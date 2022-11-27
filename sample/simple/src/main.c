#include <stdio.h>
#include <stdlib.h>
#include "pd_api.h"
#include "pdani.h"
#include "common.h"

static PlaydateAPI *api = NULL;
static LCDFont *font;
static struct pdani_file anifile;
static struct pdani_player player;


void markCallback(const struct pdani_file *a, int framenum, const char *mark, void *ptr)
{
    api->system->logToConsole("%d:%s", framenum, mark);
}

void collisionCallback(const struct pdani_file *s, const char *name, int x, int y, int w, int h, void *ptr)
{
    api->system->logToConsole("%d,%d-%d,%d %s", x, y, w, h, name);
}


int UpdateCallback(void *ptr)
{
    api->graphics->clear(kColorWhite);
    pdani_player_update(&player, markCallback, NULL);
    pdani_player_check_collision(&player, 64, 64, collisionCallback, NULL);


    pdani_player_draw(&player, NULL, 64, 64);
    api->graphics->markUpdatedRows(0, 240-1);


    PDButtons c, p, r;
    api->system->getButtonState(&c, &p, &r);

    if (p & kButtonA)
    {
        //flip ^= kANIFlipHorizontal;
    }
    if (p & kButtonB)
    {
        //flip ^= kANIFlipVertical;
    }

    pdani_player_end_frame(&player, 20);

    return 1;
}

int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, __attribute__ ((unused)) uint32_t arg)
{
    if (event == kEventInit)
    {
        api = playdate;

        common_initialize(playdate);

        char *buf = common_loadfile("ani/sprite.ani");
        LCDBitmap *bmp = common_loadbitmap("ani/sprite.png");

        pdani_static_initialize(api);
        pdani_file_initialize(&anifile, buf, bmp);
        pdani_file_dump(&anifile);
        pdani_player_initialize(&player, &anifile);
        pdani_player_play(&player, NULL);

        font = api->graphics->loadFont("/System/Fonts/Asheville-Sans-14-Bold.pft", NULL);

        api->display->setRefreshRate(50);
        api->system->setUpdateCallback(UpdateCallback, NULL);
    }

    return 0;
}






