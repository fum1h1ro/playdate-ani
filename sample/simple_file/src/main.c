#include <stdio.h>
#include <stdlib.h>
#include "pd_api.h"
#include "pdani.h"

static PlaydateAPI *api = NULL;
static struct pdani_file anifile;
static float ax, ay;
static bool fliph = false;
static bool flipv = false;

int UpdateCallback(void *ptr)
{
    api->graphics->clear(kColorWhite);

    PDButtons c, p, r;
    api->system->getButtonState(&c, &p, &r);

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
    pdani_file_draw(&anifile, NULL, ax, ay, 1, fliph, flipv);

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
        pdani_file_initialize_with_filename(&anifile, "ani/test.ani", "ani/test.png");
        pdani_file_dump(&anifile);
        ax = 128;
        ay = 128;
    }
    if (event == kEventTerminate)
    {
        api->system->logToConsole("TERMINATE");
        pdani_file_finalize(&anifile);
    }

    return 0;
}
