#include <stdio.h>
#include <stdlib.h>
#include "pd_api.h"
#include "pdani.h"
#include "common.h"

static PlaydateAPI *api = NULL;
static LCDFont *font;
static struct pdani_file anifile;
static struct pdani_player player;
static float ax, ay;


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

    PDButtons c, p, r;
    api->system->getButtonState(&c, &p, &r);

    enum pdani_flip flip = pdani_player_get_flip(&player);
    if (c & kButtonLeft)
    {
        ax -= 1;
    }
    if (c & kButtonRight)
    {
        ax += 1;
    }
    if (c & kButtonUp)
    {
        ay -= 1;
    }
    if (c & kButtonDown)
    {
        ay += 1;
    }
    if (p & kButtonA)
    {
        flip ^= PDANI_FLIP_HORIZONTALLY;
    }
    if (p & kButtonB)
    {
        flip ^= PDANI_FLIP_VERTICALLY;
    }
    pdani_player_set_flip(&player, flip);

    pdani_player_update(&player, 1000/50, markCallback, NULL);
    pdani_player_check_collision(&player, 64, 64, collisionCallback, NULL);
    pdani_player_draw(&player, NULL, ax, ay);

    char text[128];
    sprintf(text, "Move: D-pad\nA: Flip-H\nB: Flip-V\n%d,%d", (int)ax, (int)ay);
    api->graphics->drawText(text, strlen(text), kASCIIEncoding, 0, 0);

    api->graphics->markUpdatedRows(0, 240-1);

    return 1;
}

int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, __attribute__ ((unused)) uint32_t arg)
{
    if (event == kEventInit)
    {
        api = playdate;

        common_initialize(playdate);

        char *buf = common_loadfile("ani/miata.ani");
        LCDBitmap *bmp = common_loadbitmap("ani/miata.png");

        pdani_initialize(api);
        pdani_file_initialize(&anifile, buf, bmp);
        pdani_file_dump(&anifile);
        pdani_player_initialize(&player, &anifile);
        pdani_player_play(&player, "run");

        font = api->graphics->loadFont("/System/Fonts/Asheville-Sans-14-Bold.pft", NULL);

        api->display->setRefreshRate(50);
        api->system->setUpdateCallback(UpdateCallback, NULL);

        ax = 128;
        ay = 128;
    }

    return 0;
}
