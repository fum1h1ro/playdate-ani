#include "pdani.h"
#include "pd_api/pd_api_gfx.h"

#ifndef NDEBUG
#   define ASSERT(v) { if (!(v)) { s_api->system->error("%s:%d failed in %s\n%s", __FILE__, __LINE__, __func__, #v); } }
#   define PRINT(...) { s_api->system->logToConsole(__VA_ARGS__); }
#else
#   define ASSERT(v)
#   define PRINT(...)
#endif

#define BIT_SET(v, f) (v) |= (f)
#define BIT_CHECK(v, f) ((v) & (f))
#define BIT_CLEAR(v, f) (v) &= ~(f)

static PlaydateAPI *s_api = NULL;
static int s_frame_ms = 1000 / 20;
static const char *s_chunk_names[(int)PDANI_CHUNK_TYPE_MAX] = {
    "INFO",
    "TAGS",
    "LAYS",
    "FRAM",
    "CELS",
    "COLS",
    "IMAG",
    "STRG",
};
static const LCDRect screen_rect = { .left = 0, .right = LCD_COLUMNS, .top = 0, .bottom = LCD_ROWS };

static inline const void* chunkGetMisc(const struct pdani_chunk *chunk)
{
    return &chunk->misc[0];
}

static inline const void* chunkGetData(const struct pdani_chunk *chunk)
{
    return chunk + 1;
}

static inline const void* seek(const struct pdani_file *file, int offset)
{
    return ((const char*)file->header + offset);
}

static inline const void* seekChunkData(const struct pdani_chunk *chunk, int offset)
{
    return ((const char*)chunkGetData(chunk) + offset);
}

static inline const char* getString(const struct pdani_file *file, int index)
{
    const char *stream = (const char*)chunkGetData(file->chunks[PDANI_CHUNK_TYPE_STRING]);
    return &stream[index];
}

static enum pdani_chunk_type detectChunkType(const struct pdani_chunk *chunk)
{
    for (int i = 0; i < (int)PDANI_CHUNK_TYPE_MAX; ++i) {
        if (*(uint32_t*)&chunk->id[0] == *(uint32_t*)s_chunk_names[i]) {
            return (enum pdani_chunk_type)i;
        }
    }
    ASSERT(0 && "invalid type");
    return PDANI_CHUNK_TYPE_MAX;
}

static inline int clamp_int(int v, int min, int max)
{
    return (v < min)? min : (v > max)? max : v;
}

static inline float clamp_float(float v, float min, float max)
{
    return (v < min)? min : (v > max)? max : v;
}

#define CLAMP(v, min, max) \
    _Generic((v), \
        int: clamp_int, \
        float: clamp_float \
    )((v), (min), (max))

// @return 完全不可視ならfalse
static bool clip_rect(LCDRect* rc, const LCDRect* cliprc)
{
    rc->left = CLAMP(rc->left, cliprc->left, cliprc->right);
    rc->right = CLAMP(rc->right, cliprc->left, cliprc->right);
    rc->top = CLAMP(rc->top, cliprc->top, cliprc->bottom);
    rc->bottom = CLAMP(rc->bottom, cliprc->top, cliprc->bottom);
    return rc->right - rc->left > 0 && rc->bottom - rc->top > 0;
}

static void* mem_alloc(const size_t sz)
{
    return s_api->system->realloc(NULL, sz);
}

static void mem_free(void *buf)
{
    s_api->system->realloc(buf, 0);
}

static void* loadfile(const char *path)
{
    SDFile *file = s_api->file->open(path, kFileRead);
    s_api->file->seek(file, 0, SEEK_END);
    int len = s_api->file->tell(file);
    s_api->file->seek(file, 0, SEEK_SET);
    void *buf = mem_alloc(len);
    s_api->file->read(file, buf, len);
    s_api->file->close(file);
    return buf;
}

static LCDBitmap* loadbitmap(const char *path)
{
    const char *err = NULL;
    LCDBitmap *bmp = s_api->graphics->loadBitmap(path, &err);
    if (err != NULL)
    {
        s_api->system->error(err);
    }
    return bmp;
}

void pdani_global_initialize(PlaydateAPI *api)
{
    ASSERT(s_api == NULL);
    s_api = api;
}

void pdani_set_fps(int fps)
{
    ASSERT(s_api != NULL);
    s_frame_ms = 1000 / fps;
}

static void file_initialize(struct pdani_file *file, void *data, LCDBitmap *bitmap)
{
    ASSERT(s_api != NULL && "need to call pdani_global_initialize)");

    memset(file, 0, sizeof(struct pdani_file));
    file->header = data;
    file->bitmap = bitmap;
    s_api->graphics->getBitmapData(
        bitmap,
        &file->bitmap_info.width,
        &file->bitmap_info.height,
        &file->bitmap_info.rowbytes,
        &file->bitmap_info.mask,
        &file->bitmap_info.texel
    );

    const struct pdani_chunk *chunk = (const struct pdani_chunk*)(file->header + 1);
    do {
        enum pdani_chunk_type type = detectChunkType(chunk);
        ASSERT(0 <= type && type < PDANI_CHUNK_TYPE_MAX && "invalid chunk type");
        file->chunks[(int)type] = chunk;
        if (chunk->next == 0) break;
        chunk = (const struct pdani_chunk*)seek(file, chunk->next << 4);
    } while (1);
}

void pdani_file_initialize(struct pdani_file *file, void *data, LCDBitmap *bitmap)
{
    file_initialize(file, data, bitmap);
}

void pdani_file_initialize_with_filename(struct pdani_file *file, const char *anifilename, const char *bitmapfilename)
{
    void *ani = loadfile(anifilename);
    LCDBitmap *bmp = loadbitmap(bitmapfilename);
    file_initialize(file, ani, bmp);
    BIT_SET(file->flags, PDANI_FILE_FLAG_SELF_ALLOCATE);
}

void pdani_file_finalize(struct pdani_file *file)
{
    if (BIT_CHECK(file->flags, PDANI_FILE_FLAG_SELF_ALLOCATE))
    {
        s_api->graphics->freeBitmap(file->bitmap);
        mem_free(file->bitmap);
        mem_free(file->header);
    }
}

int pdani_file_get_width(const struct pdani_file *file)
{
    const struct pdani_info_misc *info = (const struct pdani_info_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_INFO]);
    return info->width;
}

int pdani_file_get_height(const struct pdani_file *file)
{
    const struct pdani_info_misc *info = (const struct pdani_info_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_INFO]);
    return info->height;
}

// tag
int pdani_file_get_tag_count(const struct pdani_file *file)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_TAG] != NULL);
    return ((const struct pdani_tag_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_TAG]))->count;
}

static inline const struct pdani_tag_data* spriteGetTagData(const struct pdani_file *file, int index)
{
    ASSERT(0 <= index && index < pdani_file_get_tag_count(file));
    return ((const struct pdani_tag_data*)chunkGetData(file->chunks[PDANI_CHUNK_TYPE_TAG]) + index);
}

static const struct pdani_tag_data* spriteFindTagData(const struct pdani_file *file, const char *name)
{
    // @todo improvement
    for (int i = 0; i < pdani_file_get_tag_count(file); ++i) {
        const struct pdani_tag_data *tag = spriteGetTagData(file, i);
        const char *tagname = getString(file, tag->name);
        if (strcmp(tagname, name) == 0) {
            return tag;
        }
    }
    return NULL;
}

const char* pdani_file_get_tag_name(const struct pdani_file *file, int index)
{
    const struct pdani_tag_data *tag = spriteGetTagData(file, index);
    return getString(file, tag->name);
}

// layer
int pdani_file_get_layer_count(const struct pdani_file *file)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_LAYER] != NULL);
    return ((const struct pdani_layer_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_LAYER]))->count;
}

static inline const struct pdani_layer_data* spriteGetLayerData(const struct pdani_file *file, int index)
{
    ASSERT(0 <= index && index < pdani_file_get_layer_count(file));
    return (const struct pdani_layer_data*)chunkGetData(file->chunks[PDANI_CHUNK_TYPE_LAYER]) + index;
}

const char* pdani_file_get_layer_name(const struct pdani_file *file, int index)
{
    const struct pdani_layer_data *layer = spriteGetLayerData(file, index);
    return getString(file, layer->name);
}

// frame
int pdani_file_get_frame_count(const struct pdani_file *file)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_FRAME] != NULL);
    return ((const struct pdani_frame_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_FRAME]))->count;
}

static inline const struct pdani_frame_data* spriteGetFrameData(const struct pdani_file *file, int frameNumber)
{
    const struct pdani_chunk *chunk = file->chunks[PDANI_CHUNK_TYPE_FRAME];
    ASSERT(chunk != NULL);
    uint16_t *table = (uint16_t*)chunkGetData(chunk);
    ASSERT(1 <= frameNumber && frameNumber <= pdani_file_get_frame_count(file));
    return (const struct pdani_frame_data*)seekChunkData(chunk, table[frameNumber - 1]);
}

static inline const struct pdani_frame_layer* spriteGetFrameLayer(const struct pdani_file *file, int frameNumber)
{
    const struct pdani_chunk *chunk = file->chunks[PDANI_CHUNK_TYPE_FRAME];
    ASSERT(chunk != NULL);
    uint16_t *table = (uint16_t*)chunkGetData(chunk);
    ASSERT(1 <= frameNumber && frameNumber <= pdani_file_get_frame_count(file));
    return &((const struct pdani_frame_data*)seekChunkData(chunk, table[frameNumber - 1]))->layers[0];
}

// image
static inline int spriteGetImageCount(const struct pdani_file *file)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_IMAGE] != NULL);
    return ((const struct pdani_frame_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_IMAGE]))->count;
}

static inline const struct pdani_image_data* spriteGetImageData(const struct pdani_file *file, int index)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_IMAGE] != NULL);
    ASSERT(0 <= index && index < spriteGetImageCount(file));
    return ((const struct pdani_image_data*)chunkGetData(file->chunks[PDANI_CHUNK_TYPE_IMAGE])) + index;
}

// cel
static inline int spriteGetCelCount(const struct pdani_file *file)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_CEL] != NULL);
    return ((const struct pdani_cel_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_CEL]))->count;
}

static inline const struct pdani_cel_data* spriteGetCelData(const struct pdani_file *file, int index)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_CEL] != NULL);
    ASSERT(index < spriteGetCelCount(file));
    return ((const struct pdani_cel_data*)chunkGetData(file->chunks[PDANI_CHUNK_TYPE_CEL])) + index;
}

// collider
static inline int spriteGetColliderCount(const struct pdani_file *file)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_COLLIDER] != NULL);
    return ((const struct pdani_collider_misc*)chunkGetMisc(file->chunks[PDANI_CHUNK_TYPE_COLLIDER]))->count;
}

static inline const struct pdani_collider_data* spriteGetColliderData(const struct pdani_file *file, int index)
{
    ASSERT(file->chunks[PDANI_CHUNK_TYPE_COLLIDER] != NULL);
    ASSERT(index < spriteGetColliderCount(file));
    return ((const struct pdani_collider_data*)chunkGetData(file->chunks[PDANI_CHUNK_TYPE_COLLIDER])) + index;
}

static inline uint32_t bitFlip8(uint32_t a)
{
    uint32_t b = a;
    a = a & 0x55555555;
    b = b ^ a;
    a = a << 2;
    a = a | b;

    b = a;
    a = a & (0x33333333<<1);
    b = b ^ a;
    a = a << 4;
    a = a | b;

    b = a;
    a = a & (0x0f0f0f0f<<3);
    b = b ^ a;
    a = a << 8;
    a = a | b;

    a = a >> 7;
    return a;
}

static void drawBitmapWithRect(const struct pdani_file *file, uint8_t *framebuf, int x, int y, int u, int v, int w, int h, _Bool fh, _Bool fv)
{
    uint8_t *dst = framebuf;
    uint8_t *texel = file->bitmap_info.texel;
    uint8_t *mask = file->bitmap_info.mask;

    // clip
    if (y < 0) {
        v += -y;
        h -= -y;
        y = 0;
    }
    if (y + h > LCD_ROWS) {
        int m = ((y + h) - LCD_ROWS);
        h -= m;
        if (fv) v += m;
    }
    if (h <= 0) return;

    const int bw = (w + (8-1)) >> 3;
    const int vdir = (fv)? -1 : +1;
    const int bufstep = file->bitmap_info.rowbytes * vdir;

    x = (fh)? x - ((bw << 3) - w) : x;
    v = (fv)? v + (h - 1) : v;

    texel += file->bitmap_info.rowbytes * v + (u >> 3);
    mask += file->bitmap_info.rowbytes * v + (u >> 3);
    dst += LCD_ROWSIZE * y;

    int dst_startbyte = x >> 3;
    int dst_endbyte = dst_startbyte + bw;

    const int write1size = 8 - (x - dst_startbyte * 8);
    const int write2size = 8 - write1size;
    const uint8_t write1mask = (uint8_t)(((uint16_t)1 << write1size) - 1);
    const uint8_t write2mask = (uint8_t)(((uint16_t)1 << write2size) - 1) << write1size;

    int startx = (fh)? bw - 1 : 0;
    const int sinc = (fh)? -1 : 1;

    if (dst_startbyte < 0) {
        startx += -dst_startbyte;
        dst_startbyte = 0;
    }
    if (dst_endbyte > LCD_COLUMNS >> 3)
    {
        dst_endbyte = LCD_COLUMNS >> 3;
    }
    if (write2size == 0) {
        if (fh) {
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = startx, dx = dst_startbyte; dx < dst_endbyte; sx += sinc, ++dx) {
                    const uint8_t t = bitFlip8(*(texel + sx));
                    const uint8_t m = bitFlip8(*(mask + sx));
                    const uint8_t d = *(dst + dx);
                    *(dst + dx) = (d & ~m) | (t & m);
                }
                texel += bufstep;
                mask += bufstep;
                dst += LCD_ROWSIZE;
            }
        } else {
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = startx, dx = dst_startbyte; dx < dst_endbyte; sx += sinc, ++dx) {
                    const uint8_t t = *(texel + sx);
                    const uint8_t m = *(mask + sx);
                    const uint8_t d = *(dst + dx);
                    *(dst + dx) = (d & ~m) | (t & m);
                }
                texel += bufstep;
                mask += bufstep;
                dst += LCD_ROWSIZE;
            }
        }
    } else {
        if (fh) {
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = startx, dx = dst_startbyte; dx < dst_endbyte; sx += sinc, ++dx) {
                    const uint8_t t = bitFlip8(*(texel + sx));
                    const uint8_t m = bitFlip8(*(mask + sx));
                    const uint8_t t1 = (t >> write2size) & write1mask;
                    const uint8_t t2 = (t << write1size) & write2mask;
                    const uint8_t m1 = (m >> write2size) & write1mask;
                    const uint8_t m2 = (m << write1size) & write2mask;
                    const uint8_t d1 = *(dst + dx + 0);
                    const uint8_t d2 = *(dst + dx + 1);
                    *(dst + dx + 0) = (d1 & ~m1) | (t1 & m1);
                    *(dst + dx + 1) = (d2 & ~m2) | (t2 & m2);
                }
                texel += bufstep;
                mask += bufstep;
                dst += LCD_ROWSIZE;
            }
        } else {
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = startx, dx = dst_startbyte; dx < dst_endbyte; sx += sinc, ++dx) {
                    const uint8_t t = *(texel + sx);
                    const uint8_t m = *(mask + sx);
                    const uint8_t t1 = (t >> write2size) & write1mask;
                    const uint8_t t2 = (t << write1size) & write2mask;
                    const uint8_t m1 = (m >> write2size) & write1mask;
                    const uint8_t m2 = (m << write1size) & write2mask;
                    const uint8_t d1 = *(dst + dx + 0);
                    const uint8_t d2 = *(dst + dx + 1);
                    *(dst + dx + 0) = (d1 & ~m1) | (t1 & m1);
                    *(dst + dx + 1) = (d2 & ~m2) | (t2 & m2);
                }
                texel += bufstep;
                mask += bufstep;
                dst += LCD_ROWSIZE;
            }
        }
    }
}

//! @internal
typedef struct
{
    int layer_index;
    const struct pdani_layer_data *layer_data;
    const struct pdani_frame_layer *frame_layer;
} SpriteFrameLayerIterator;

static inline void spriteFrameLayerBegin(SpriteFrameLayerIterator *it, const struct pdani_file *file, int frame_number)
{
    it->layer_index = 0;
    it->layer_data = spriteGetLayerData(file, 0);
    it->frame_layer = spriteGetFrameLayer(file, frame_number);
}

static inline void spriteFrameLayerEnd(SpriteFrameLayerIterator *it, const struct pdani_file *file, int frame_number)
{
    const int layerCount = pdani_file_get_layer_count(file);
    it->layer_index = layerCount;
    it->layer_data = NULL;
    it->frame_layer = NULL;
}

static inline void spriteFrameLayerNext(SpriteFrameLayerIterator *it)
{
    if (it->layer_data->type == PDANI_LAYER_TYPE_LAYER || it->layer_data->type == PDANI_LAYER_TYPE_COLLIDER) {
        it->frame_layer += 1;
    }
    it->layer_index += 1;
    it->layer_data += 1;
}

static inline bool spriteFrameLayerCompare(SpriteFrameLayerIterator *it0, SpriteFrameLayerIterator *it1)
{
    return it0->layer_index == it1->layer_index;
}

void pdani_file_draw(const struct pdani_file *file, LCDBitmap *target, int x, int y, int framenumber, bool fliph, bool flipv)
{
    ASSERT(s_api != NULL);
    ASSERT(file != NULL);
    ASSERT(1 <= framenumber && framenumber <= pdani_file_get_frame_count(file));

    uint8_t *framebuf = NULL;
    if (target != NULL) {
        s_api->graphics->getBitmapData(target, NULL, NULL, NULL, NULL, &framebuf);
    } else {
        framebuf = s_api->graphics->getFrame();
    }

    SpriteFrameLayerIterator it, end;
    const int sw = pdani_file_get_width(file);
    const int sh = pdani_file_get_height(file);

    LCDRect rc = LCDMakeRect(x, y, sw, sh);
    if (!clip_rect(&rc, &screen_rect)) return;

    spriteFrameLayerEnd(&end, file, framenumber);
    for (spriteFrameLayerBegin(&it, file, framenumber); !spriteFrameLayerCompare(&it, &end); spriteFrameLayerNext(&it)) {
        const struct pdani_layer_data *layer = it.layer_data;
        const struct pdani_frame_layer *framelayer = it.frame_layer;

        if (layer->type != PDANI_LAYER_TYPE_LAYER || framelayer->cel < 0) continue;

        const struct pdani_cel_data *cel = spriteGetCelData(file, framelayer->cel);
        const struct pdani_image_data *image = spriteGetImageData(file, cel->image);
        const int dx = (fliph)? x + sw - cel->x - image->w : x + cel->x;
        const int dy = (flipv)? y + sh - cel->y - image->h : y + cel->y;
        drawBitmapWithRect(file, framebuf, dx, dy, image->u, image->v, image->w, image->h, fliph, flipv);
    }
}

static void spriteCheckFrameTrigger(const struct pdani_file *file, int framenumber, pdani_frame_layer_callback callback, void *ptr)
{
    ASSERT(s_api != NULL);
    ASSERT(file != NULL);
    ASSERT(1 <= framenumber && framenumber <= pdani_file_get_frame_count(file));

    if (callback == NULL) return;

    SpriteFrameLayerIterator it, end;

    spriteFrameLayerEnd(&end, file, framenumber);
    for (spriteFrameLayerBegin(&it, file, framenumber); !spriteFrameLayerCompare(&it, &end); spriteFrameLayerNext(&it)) {
        const struct pdani_layer_data *layer = it.layer_data;
        const struct pdani_frame_layer *framelayer = it.frame_layer;
        if (layer->type != PDANI_LAYER_TYPE_GROUP && framelayer->userCallback > 0) {
            (*callback)(file, framenumber, getString(file, framelayer->userCallback), ptr);
        }
    }
}

void pdani_file_check_collision(const struct pdani_file *file, int x, int y, int framenumber, bool fliph, bool flipv, pdani_collider_callback callback, void *ptr)
{
    ASSERT(s_api != NULL);
    ASSERT(file != NULL);
    ASSERT(1 <= framenumber && framenumber <= pdani_file_get_frame_count(file));

    if (callback == NULL) return;

    SpriteFrameLayerIterator it, end;
    const int sw = pdani_file_get_width(file);
    const int sh = pdani_file_get_height(file);

    spriteFrameLayerEnd(&end, file, framenumber);
    for (spriteFrameLayerBegin(&it, file, framenumber); !spriteFrameLayerCompare(&it, &end); spriteFrameLayerNext(&it)) {
        const struct pdani_layer_data *layer = it.layer_data;
        const struct pdani_frame_layer *framelayer = it.frame_layer;
        if (layer->type == PDANI_LAYER_TYPE_COLLIDER && framelayer->collider >= 0) {
            const struct pdani_collider_data *col = spriteGetColliderData(file, framelayer->collider);
            const int dx = (fliph)? x + sw - col->x - col->w : x + col->x;
            const int dy = (flipv)? y + sh - col->y - col->h : y + col->y;
            (*callback)(file, getString(file, layer->name), dx, dy, col->w, col->h, ptr);
        }
    }
}

void pdani_file_dump(const struct pdani_file *file)
{
    PRINT("top: %p", file->header);
    PRINT("id: %c%c%c%c version: %d", file->header->id[0], file->header->id[1], file->header->id[2], file->header->id[3], file->header->version);

    for (int i = 0; i < PDANI_CHUNK_TYPE_MAX; ++i) {
        const struct pdani_chunk *chunk = file->chunks[i];

        PRINT("chunk: %p %s", chunk, s_chunk_names[detectChunkType(chunk)]);
        PRINT("id: %c%c%c%c size: %d next: %d", chunk->id[0], chunk->id[1], chunk->id[2], chunk->id[3], chunk->size, chunk->next);
    }

    if (file->chunks[PDANI_CHUNK_TYPE_TAG] != NULL) {
        PRINT("tagCount: %d", pdani_file_get_tag_count(file));
        for (int i = 0; i < pdani_file_get_tag_count(file); ++i) {
            const struct pdani_tag_data *tag = spriteGetTagData(file, i);
            PRINT("  from:%d to:%d name:%s", tag->from, tag->to, getString(file, tag->name));
        }
    }

    if (file->chunks[PDANI_CHUNK_TYPE_LAYER] != NULL) {
        PRINT("layerCount: %d", pdani_file_get_layer_count(file));
        for (int i = 0; i < pdani_file_get_layer_count(file); ++i) {
            const struct pdani_layer_data *layer = spriteGetLayerData(file, i);
            PRINT("  [%2d] type:%c parent:%d name:%s sub:%d", i, layer->type, layer->parent, getString(file, layer->name), layer->layerCount);
        }
        //aniSpriteTraverseLayer(file, LayerCallback, NULL);
    }

    if (file->chunks[PDANI_CHUNK_TYPE_FRAME] != NULL) {
        PRINT("frameCount: %d", pdani_file_get_frame_count(file));
        for (int i = 1; i <= pdani_file_get_frame_count(file); ++i) {
            const struct pdani_frame_data *frame = spriteGetFrameData(file, i);
            PRINT("  frame:%d duration:%dms", i, frame->duration);//, frame->layerCount);
            int celidx = 0;
            for (int j = 0; j < pdani_file_get_layer_count(file); ++j) {
                const struct pdani_layer_data *layer = spriteGetLayerData(file, j);
                switch (layer->type) {
                case 'L': {
                        const struct pdani_frame_layer *framelayer = &frame->layers[celidx++];
                        PRINT("    userCallback:%s cel:%d", getString(file, framelayer->userCallback), framelayer->cel);
                    }
                    break;
                case 'G':
                    break;
                case 'C': {
                        const struct pdani_frame_layer *framelayer = &frame->layers[celidx++];
                        PRINT("    userCallback:%s collider:%d", getString(file, framelayer->userCallback), framelayer->collider);
                    }
                    break;
                }
            }
        }
    }

    if (file->chunks[PDANI_CHUNK_TYPE_IMAGE] != NULL) {
        PRINT("imageCount: %d", spriteGetImageCount(file));
        for (int i = 0; i < spriteGetImageCount(file); ++i) {
            const struct pdani_image_data *image = spriteGetImageData(file, i);
            PRINT(" image:%d %d %d %d", image->u, image->v, image->w, image->h);
        }
    }

    if (file->chunks[PDANI_CHUNK_TYPE_CEL] != NULL) {
        PRINT("celCount: %d", spriteGetCelCount(file));
        for (int i = 0; i < spriteGetCelCount(file); ++i) {
            const struct pdani_cel_data *cel = spriteGetCelData(file, i);
            PRINT(" cel:%d %d %d", cel->image, cel->x, cel->y);
        }
    }

    if (file->chunks[PDANI_CHUNK_TYPE_COLLIDER] != NULL) {
        PRINT("colliderCount: %d", spriteGetColliderCount(file));
        for (int i = 0; i < spriteGetColliderCount(file); ++i) {
            const struct pdani_collider_data *col = spriteGetColliderData(file, i);
            PRINT(" col:%d %d %d %d", col->x, col->y, col->w, col->h);
        }
    }
}




static void player_initialize(struct pdani_player *player, struct pdani_file *file)
{
    ASSERT(s_api != NULL);
    memset(player, 0, sizeof(struct pdani_player));

    player->file = file;
    player->start_frame = 1;
    player->end_frame = pdani_file_get_frame_count(player->file);
    player->is_playing = false;
    BIT_SET(player->flags, PDANI_PLAYER_FLAG_FRAME_SKIPPABLE);
}

void pdani_player_initialize(struct pdani_player *player, struct pdani_file *file)
{
    player_initialize(player, file);
}

void pdani_player_initialize_with_filename(struct pdani_player *player, const char *anifilename, const char *bmpfilename)
{
    struct pdani_file *file = mem_alloc(sizeof(struct pdani_file));
    pdani_file_initialize_with_filename(file, anifilename, bmpfilename);
    player_initialize(player, file);
    BIT_SET(player->flags, PDANI_PLAYER_FLAG_SELF_ALLOCATE);
}

void pdani_player_finalize(struct pdani_player *player)
{
    if (BIT_CHECK(player->flags, PDANI_PLAYER_FLAG_SELF_ALLOCATE)) {
        pdani_file_finalize(player->file);
    }
}

void pdani_player_play(struct pdani_player *player, const char *tagname)
{
    ASSERT(player != NULL);

    if (tagname != NULL) {
        const struct pdani_tag_data *tag = spriteFindTagData(player->file, tagname);
        ASSERT(tag != NULL && "not found");
        player->start_frame = tag->from;
        player->end_frame = tag->to;
    } else {
        player->start_frame = 1;
        player->end_frame = pdani_file_get_frame_count(player->file);
    }

    pdani_player_seek_frame(player, player->start_frame);
    player->is_playing = true;
}

void pdani_player_stop(struct pdani_player *player)
{
    ASSERT(player != NULL);
    player->is_playing = false;
}

void pdani_player_resume(struct pdani_player *player)
{
    ASSERT(player != NULL);
    player->is_playing = true;
}

void pdani_player_seek_frame(struct pdani_player *player, int frame_number)
{
    ASSERT(player != NULL);
    player->frame_number = frame_number;
    player->current_frame = spriteGetFrameData(player->file, player->frame_number);
    player->frame_elapsed = 0;
    player->total_elapsed = 0;
    player->previous_frame_number = -1;
}

inline bool pdani_player_get_flip_horizontally(const struct pdani_player *player)
{
    return BIT_CHECK(player->flags, PDANI_PLAYER_FLAG_FLIP_HORIZONTALLY);
}

inline bool pdani_player_get_flip_vertically(const struct pdani_player *player)
{
    return BIT_CHECK(player->flags, PDANI_PLAYER_FLAG_FLIP_VERTICALLY);
}

void pdani_player_set_flip(struct pdani_player *player, bool fliph, bool flipv)
{
    if (fliph) {
        BIT_SET(player->flags, PDANI_PLAYER_FLAG_FLIP_HORIZONTALLY);
    } else {
        BIT_CLEAR(player->flags, PDANI_PLAYER_FLAG_FLIP_HORIZONTALLY);
    }

    if (flipv) {
        BIT_SET(player->flags, PDANI_PLAYER_FLAG_FLIP_VERTICALLY);
    } else {
        BIT_CLEAR(player->flags, PDANI_PLAYER_FLAG_FLIP_VERTICALLY);
    }
}

/// @internal
static inline int playerCalculateNextFrame(const struct pdani_player *player, int current_frame_number)
{
    if (current_frame_number >= player->end_frame) {
        switch (player->loop_type) {
        case PDANI_LOOP_TYPE_ONESHOT:
            return current_frame_number;
        case PDANI_LOOP_TYPE_LOOP:
            return player->start_frame;
        }
    }
    return current_frame_number + 1;
}

void pdani_player_check_collision(const struct pdani_player *player, int x, int y, pdani_collider_callback callback, void *ptr)
{
    ASSERT(player != NULL);
    if (!player->is_playing) return;
    if (callback == NULL) return;

    const bool fliph = pdani_player_get_flip_horizontally(player);
    const bool flipv = pdani_player_get_flip_vertically(player);

    if (player->previous_frame_number < 0) {
        pdani_file_check_collision(player->file, x, y, player->frame_number, fliph, flipv, callback, ptr);
    } else if (player->previous_frame_number != player->frame_number) {
        int f = player->previous_frame_number;
        do {
            f = playerCalculateNextFrame(player, f);
            pdani_file_check_collision(player->file, x, y, f, fliph, flipv, callback, ptr);
        } while (f != player->frame_number);
    } else {
        pdani_file_check_collision(player->file, x, y, player->frame_number, fliph, flipv, callback, ptr);
    }
}

// postupdate
void pdani_player_update(struct pdani_player *player, int ms, pdani_frame_layer_callback callback, void *ptr)
{
    ASSERT(player != NULL);
    if (!player->is_playing) return;
    if (callback != NULL)
    {
        //PRINT("%d - %d", player->previous_frame_number, player->frame_number);
        if (player->previous_frame_number < 0) {
            spriteCheckFrameTrigger(player->file, player->frame_number, callback, ptr);
        } else if (player->previous_frame_number != player->frame_number) {
            int f = player->previous_frame_number;
            do {
                f = playerCalculateNextFrame(player, f);
                spriteCheckFrameTrigger(player->file, f, callback, ptr);
            } while (f != player->frame_number);
        }
    }

    const bool is_first = player->previous_frame_number < 0;
    player->previous_frame_number = player->frame_number;
    if (is_first) return;

    player->frame_elapsed += ms;
    player->total_elapsed += ms;

    bool is_frame_skippable = BIT_CHECK(player->flags, PDANI_PLAYER_FLAG_FRAME_SKIPPABLE);

    while (player->current_frame->duration <= player->frame_elapsed) {
        player->frame_elapsed -= player->current_frame->duration;
        player->frame_number = playerCalculateNextFrame(player, player->frame_number);
        if (player->frame_number >= player->end_frame && player->loop_type == PDANI_LOOP_TYPE_ONESHOT) {
            player->is_playing = false;
        }
        player->current_frame = spriteGetFrameData(player->file, player->frame_number);

        if (!is_frame_skippable) {
            player->frame_elapsed = 0;
            break;
        }
    }
}

void pdani_player_draw(const struct pdani_player *player, LCDBitmap *target, int x, int y)
{
    ASSERT(player != NULL);
    const int frame =  (player->is_playing)? player->frame_number : 1;
    const bool fliph = pdani_player_get_flip_horizontally(player);
    const bool flipv = pdani_player_get_flip_vertically(player);
    pdani_file_draw(player->file, target, x, y, frame, fliph, flipv);
}


// sprite

static void sprite_update_function(LCDSprite *sprite)
{
    struct pdani_sprite *anisprite = s_api->sprite->getUserdata(sprite);
    LCDSprite *s = anisprite->sprite;

    pdani_player_update(&anisprite->player, s_frame_ms, NULL, NULL);
    //s_api->system->logToConsole("%d", anisprite->player.frame_number);
    float px, py;
    s_api->sprite->getPosition(s, &px, &py);
    float x = px - anisprite->origin_x;
    float y = py - anisprite->origin_y;
    float w = pdani_file_get_width(&anisprite->file);
    float h = pdani_file_get_height(&anisprite->file);
    s_api->sprite->setBounds(s, PDRectMake(x, y, w, h));
}

static void sprite_draw_function(LCDSprite *sprite, PDRect bounds, PDRect drawrect)
{
    struct pdani_sprite *anisprite = s_api->sprite->getUserdata(sprite);
    LCDSprite *s = anisprite->sprite;
    float x, y;
    s_api->sprite->getPosition(s, &x, &y);
    pdani_player_draw(&anisprite->player, NULL, (int)x - anisprite->origin_x, (int)y - anisprite->origin_y);
}

void pdani_sprite_initialize(struct pdani_sprite *anisprite, void *data, LCDBitmap *bitmap)
{
    ASSERT(s_api != NULL);
    memset(anisprite, 0, sizeof(struct pdani_sprite));

    pdani_file_initialize(&anisprite->file, data, bitmap);
    pdani_player_initialize(&anisprite->player, &anisprite->file);
    anisprite->sprite = s_api->sprite->newSprite();
    int w = pdani_file_get_width(&anisprite->file);
    int h = pdani_file_get_height(&anisprite->file);
    anisprite->origin_x = w >> 1;
    anisprite->origin_y = h >> 1;

    s_api->sprite->setUserdata(anisprite->sprite, anisprite);
    s_api->sprite->setUpdateFunction(anisprite->sprite, sprite_update_function);
    s_api->sprite->setDrawFunction(anisprite->sprite, sprite_draw_function);
}

void pdani_sprite_finalize(struct pdani_sprite *anisprite)
{
    s_api->sprite->freeSprite(anisprite->sprite);
    pdani_player_finalize(&anisprite->player);
    pdani_file_finalize(&anisprite->file);
}








