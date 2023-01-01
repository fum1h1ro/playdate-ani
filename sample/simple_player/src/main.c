#include <stdio.h>
#include <stdlib.h>
#include "pd_api.h"
#include "pdani.h"

static PlaydateAPI *api = NULL;
static struct pdani_player player;
static float ax, ay;

void markCallback(const struct pdani_file *a, int framenum, const char *mark, void *ptr)
{
    api->system->logToConsole("%d:%s", framenum, mark);
}

int UpdateCallback(void *ptr)
{
    api->graphics->clear(kColorWhite);

    PDButtons c, p, r;
    api->system->getButtonState(&c, &p, &r);

    bool fliph = pdani_player_get_flip_horizontally(&player);
    bool flipv = pdani_player_get_flip_vertically(&player);
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
        fliph = !fliph;
    }
    if (p & kButtonB)
    {
        flipv = !flipv;
    }
    pdani_player_set_flip(&player, fliph, flipv);

    pdani_player_update(&player, 20, markCallback, NULL);
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
        api->system->logToConsole("INIT");
        api->display->setRefreshRate(50);
        api->system->setUpdateCallback(UpdateCallback, NULL);

        pdani_global_initialize(api);
        pdani_player_initialize_with_filename(&player, "ani/miata.ani", "ani/miata.png");
        pdani_file_dump(pdani_player_get_file(&player));
        pdani_player_play(&player, "run");
        ax = 128;
        ay = 128;
    }
    if (event == kEventTerminate)
    {
        api->system->logToConsole("TERMINATE");
        pdani_player_finalize(&player);
    }

    return 0;
}
