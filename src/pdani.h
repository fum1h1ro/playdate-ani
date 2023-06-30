#ifndef __PDANI_H__
#define __PDANI_H__

#include "pd_api.h"
#include <stdbool.h>

enum pdani_chunk_type {
    PDANI_CHUNK_TYPE_INFO,
    PDANI_CHUNK_TYPE_TAG,
    PDANI_CHUNK_TYPE_LAYER,
    PDANI_CHUNK_TYPE_FRAME,
    PDANI_CHUNK_TYPE_CEL,
    PDANI_CHUNK_TYPE_COLLIDER,
    PDANI_CHUNK_TYPE_IMAGE,
    PDANI_CHUNK_TYPE_STRING,
    PDANI_CHUNK_TYPE_MAX,
};

enum pdani_player_loop_type {
    PDANI_LOOP_TYPE_LOOP,
    PDANI_LOOP_TYPE_ONESHOT,
};

enum pdani_layer_type {
    PDANI_LAYER_TYPE_LAYER = 'L',
    PDANI_LAYER_TYPE_GROUP = 'G',
    PDANI_LAYER_TYPE_COLLIDER = 'C',
};

enum pdani_file_flags {
    PDANI_FILE_FLAG_SELF_ALLOCATE = (1<<0),
    PDANI_FILE_FLAG_FORCE_U32 = 0xffffffff, //< @internal
};

enum pdani_player_flags {
    PDANI_PLAYER_FLAG_SELF_ALLOCATE = (1<<0),
    PDANI_PLAYER_FLAG_FRAME_SKIPPABLE = (1<<1), //< フレームをスキップできるかどうか
    PDANI_PLAYER_FLAG_FLIP_HORIZONTALLY = (1<<2), //< 水平方向の反転
    PDANI_PLAYER_FLAG_FLIP_VERTICALLY = (1<<3), //< 垂直方向の反転
    PDANI_PLAYER_FLAG_FORCE_U32 = 0xffffffff, //< @internal
};



struct pdani_chunk {
    int8_t id[4];
    uint16_t size;
    uint16_t next; // * 16
    int misc[2];
};

struct pdani_info_misc {
    uint16_t width, height;
    uint16_t totalFrame;
};

struct pdani_tag_misc {
    uint16_t count;
};

struct pdani_tag_data {
    uint16_t from;
    uint16_t to;
    uint16_t name;
};

struct pdani_layer_misc {
    uint16_t count;
};

struct pdani_layer_data {
    int8_t type;
    int8_t parent;
    uint16_t name;
    uint16_t layerCount;
};

struct pdani_frame_misc {
    uint16_t count;
};

struct pdani_frame_layer {
    uint16_t userCallback;
    union {
        int16_t cel;
        int16_t collider;
    };
};

struct pdani_frame_data {
    uint16_t duration; // ms
    struct pdani_frame_layer layers[0];
};

struct pdani_image_misc {
    uint16_t count;
};

struct pdani_image_data {
    int16_t u, v;
    uint16_t w, h;
};

struct pdani_cel_misc {
    uint16_t count;
};

struct pdani_cel_data {
    uint16_t image;
    int16_t x, y;
};

struct pdani_collider_misc {
    uint16_t count;
};

struct pdani_collider_data {
    int16_t x, y;
    uint16_t w, h;
};

struct pdani_file {
    enum pdani_file_flags flags;
    struct {
        int8_t id[4];
        uint32_t version;
        int32_t padding[2];
    } *header;
    const struct pdani_chunk *chunks[PDANI_CHUNK_TYPE_MAX];
    LCDBitmap *bitmap;
    struct {
        int width, height;
        int rowbytes;
        uint8_t *texel;
        uint8_t *mask;
    } bitmap_info;
};

struct pdani_player {
    struct pdani_file *file; //< @internal
    uint16_t start_frame;
    uint16_t end_frame;
    const struct pdani_frame_data *current_frame;
    int16_t frame_number;
    int16_t previous_frame_number;
    int16_t frame_elapsed;
    int32_t total_elapsed;
    bool is_playing;
    enum pdani_player_flags flags;
    enum pdani_player_loop_type loop_type;
};

struct pdani_sprite {
    struct pdani_file file;
    struct pdani_player player;
    LCDSprite *sprite;
    LCDSprite **colliders;
    int origin_x, origin_y;
};

typedef void (*pdani_frame_layer_callback)(const struct pdani_file *file, int framenum, const char *name, void *ptr);
typedef void (*pdani_collider_callback)(const struct pdani_file *file, const char *name, int x, int y, int w, int h, void *ptr);

#ifdef __cplusplus
extern "C"
{
#endif

void pdani_global_initialize(PlaydateAPI *api);

// file2
/// @fn 初期化
void pdani_file_initialize(struct pdani_file *file, void *data, LCDBitmap *bitmap);
void pdani_file_initialize_with_filename(struct pdani_file *file, const char *anifilename, const char *bmpfilename);
void pdani_file_finalize(struct pdani_file *file);
int pdani_file_get_width(const struct pdani_file *file);
int pdani_file_get_height(const struct pdani_file *file);
int pdani_file_get_tag_count(const struct pdani_file *file);
const char* pdani_file_get_tag_name(const struct pdani_file *file, int index);
int pdani_file_get_layer_count(const struct pdani_file *file);
const char* pdani_file_get_layer_name(const struct pdani_file *file, int index);
int pdani_file_get_frame_count(const struct pdani_file *file);
void pdani_file_draw(const struct pdani_file *file, LCDBitmap *target, int x, int y, int frame, bool fliph, bool flipv);
void pdani_file_check_collision(const struct pdani_file *file, int x, int y, int framenum, bool fliph, bool flipv, pdani_collider_callback callback, void *ptr);
void pdani_file_dump(const struct pdani_file *file);

// player
void pdani_player_initialize(struct pdani_player *player, struct pdani_file *file);
void pdani_player_initialize_with_filename(struct pdani_player *player, const char *anifilename, const char *bmpfilename);
void pdani_player_finalize(struct pdani_player *player);
static inline const struct pdani_file* pdani_player_get_file(const struct pdani_player *player) { return player->file; }
void pdani_player_play(struct pdani_player *player, const char *tagname);
void pdani_player_stop(struct pdani_player *player);
void pdani_player_resume(struct pdani_player *player);
void pdani_player_seek_frame(struct pdani_player *player, int frame_number);
bool pdani_player_get_flip_horizontally(const struct pdani_player *player);
bool pdani_player_get_flip_vertically(const struct pdani_player *player);
void pdani_player_set_flip(struct pdani_player *player, bool fliph, bool flipv);
void pdani_player_check_collision(const struct pdani_player *player, int x, int y, pdani_collider_callback callback, void *ptr);
void pdani_player_update(struct pdani_player *player, int ms, pdani_frame_layer_callback callback, void *ptr);
void pdani_player_draw(const struct pdani_player *player, LCDBitmap *target, int x, int y);

// sprite
void pdani_sprite_initialize(struct pdani_sprite *anisprite, void *data, LCDBitmap *bitmap, LCDSprite *sprite);
void pdani_sprite_finalize(struct pdani_sprite *anisprite);
static inline LCDSprite* pdani_sprite_get_sprite(struct pdani_sprite *anisprite) { return anisprite->sprite; }





#ifdef __cplusplus
}
#endif

#endif // __PDANI_H__
