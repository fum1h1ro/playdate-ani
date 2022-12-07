#include <stdio.h>
#include <stdlib.h>
#include "pd_api.h"
#include "pdani.h"
#include "common.h"

static PlaydateAPI *api = NULL;
static LCDFont *font;
static struct pdani_sprite anisprite;
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

    PDButtons c, p, r;
    api->system->getButtonState(&c, &p, &r);

    //enum pdani_flip flip = pdani_player_get_flip(&player);
    //if (p & kButtonA)
    //{
    //    flip ^= PDANI_FLIP_HORIZONTALLY;
    //}
    //if (p & kButtonB)
    //{
    //    flip ^= PDANI_FLIP_VERTICALLY;
    //}
    //pdani_player_set_flip(&player, flip);

    //pdani_player_update(&player, 1000/50, markCallback, NULL);
    //pdani_player_check_collision(&player, 64, 64, collisionCallback, NULL);
    //pdani_player_draw(&player, NULL, 64, 64);

    //LCDSprite *s = pdani_sprite_get_sprite(&anisprite);
    //api->sprite->markDirty(s);



    //api->sprite->drawSprites();
    api->sprite->updateAndDrawSprites();
    api->graphics->markUpdatedRows(0, 240-1);

    return 1;
}

int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, __attribute__ ((unused)) uint32_t arg)
{
    if (event == kEventInit)
    {
        api = playdate;

        common_initialize(playdate);
        pdani_initialize(api);



        {
            LCDSprite *s = api->sprite->newSprite();
            LCDBitmap *bmp = common_loadbitmap("sprite.png");
            api->sprite->setImage(s, bmp, kBitmapUnflipped);
            api->sprite->addSprite(s);
            api->sprite->moveTo(s, 0, 0);
            PDRect rc = api->sprite->getBounds(s);
            api->system->logToConsole("%f %f %f %f", rc.x, rc.y, rc.width, rc.height);
        }





        {
            char *buf = common_loadfile("ani/miata.ani");
            LCDBitmap *bmp = common_loadbitmap("ani/miata.png");

            pdani_sprite_initialize(&anisprite, buf, bmp);
            LCDSprite *s = pdani_sprite_get_sprite(&anisprite);
            api->sprite->addSprite(s);
            api->sprite->moveTo(s, -160, -160);
            pdani_player_play(&anisprite.player, "run");


            //pdani_file_initialize(&anifile, buf, bmp);
            //pdani_file_dump(&anifile);
            //pdani_player_initialize(&player, &anifile);
            //pdani_player_play(&player, "run");
        }

        font = api->graphics->loadFont("/System/Fonts/Asheville-Sans-14-Bold.pft", NULL);

        api->display->setRefreshRate(50);
        api->system->setUpdateCallback(UpdateCallback, NULL);
    }

    return 0;
}






