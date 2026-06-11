#ifndef AIO_LVGL_COMPAT_H
#define AIO_LVGL_COMPAT_H

#include "lvgl.h"

#if LVGL_VERSION_MAJOR >= 9

#ifndef LV_COLOR_16_SWAP
#define LV_COLOR_16_SWAP 0
#endif

#ifndef LV_COLOR_SIZE
#define LV_COLOR_SIZE LV_COLOR_DEPTH
#endif

#ifndef LV_IMG_CF_INDEXED_1BIT
#define LV_IMG_CF_INDEXED_1BIT LV_COLOR_FORMAT_I1
#endif
#ifndef LV_IMG_CF_INDEXED_2BIT
#define LV_IMG_CF_INDEXED_2BIT LV_COLOR_FORMAT_I2
#endif
#ifndef LV_IMG_CF_INDEXED_4BIT
#define LV_IMG_CF_INDEXED_4BIT LV_COLOR_FORMAT_I4
#endif
#ifndef LV_IMG_CF_INDEXED_8BIT
#define LV_IMG_CF_INDEXED_8BIT LV_COLOR_FORMAT_I8
#endif
#ifndef LV_IMG_CF_TRUE_COLOR
#define LV_IMG_CF_TRUE_COLOR LV_COLOR_FORMAT_RGB565
#endif
#ifndef LV_IMG_CF_TRUE_COLOR_ALPHA
#define LV_IMG_CF_TRUE_COLOR_ALPHA LV_COLOR_FORMAT_RGB565A8
#endif
#ifndef LV_IMG_PX_SIZE_ALPHA_BYTE
#define LV_IMG_PX_SIZE_ALPHA_BYTE LV_COLOR_NATIVE_WITH_ALPHA_SIZE
#endif

#ifndef lv_mem_alloc
#define lv_mem_alloc lv_malloc
#endif
#ifndef lv_mem_free
#define lv_mem_free lv_free
#endif
#ifndef lv_task_handler
#define lv_task_handler lv_timer_handler
#endif

static inline uint8_t aio_lv_color_to8(lv_color_t c)
{
    return (uint8_t)((c.red & 0xE0) | ((c.green & 0xE0) >> 3) | (c.blue >> 6));
}

#ifndef lv_color_to8
#define lv_color_to8 aio_lv_color_to8
#endif

static inline bool aio_lv_font_fmt_txt_is_legacy(const lv_font_fmt_txt_dsc_t *fdsc)
{
    if(fdsc == NULL || fdsc->cmap_num != 1) return false;
    const lv_font_fmt_txt_cmap_t *cmap = &fdsc->cmaps[0];
    return cmap->type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY &&
           cmap->unicode_list != NULL &&
           cmap->glyph_id_ofs_list == NULL &&
           cmap->glyph_id_start == 0 &&
           cmap->list_length > 0 &&
           cmap->unicode_list[0] == cmap->range_start;
}

static inline int aio_lv_font_binsearch(const uint16_t *values, int len, uint32_t key)
{
    int low = 0;
    int high = len - 1;
    while(low <= high) {
        int mid = (low + high) >> 1;
        uint32_t value = values[mid];
        if(key < value) high = mid - 1;
        else if(key > value) low = mid + 1;
        else return mid;
    }
    return -1;
}

static inline bool aio_lv_font_get_glyph_dsc_fmt_txt(const lv_font_t *font,
                                                      lv_font_glyph_dsc_t *dsc_out,
                                                      uint32_t unicode_letter,
                                                      uint32_t unicode_letter_next)
{
    lv_font_fmt_txt_dsc_t *fdsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    if(!aio_lv_font_fmt_txt_is_legacy(fdsc)) {
        return lv_font_get_glyph_dsc_fmt_txt(font, dsc_out, unicode_letter, unicode_letter_next);
    }

    LV_UNUSED(unicode_letter_next);

    const lv_font_fmt_txt_cmap_t *cmap = &fdsc->cmaps[0];
    if(unicode_letter < cmap->range_start || unicode_letter > cmap->range_length) return false;

    int glyph_id = aio_lv_font_binsearch(cmap->unicode_list, cmap->list_length, unicode_letter);
    if(glyph_id < 0) return false;

    const lv_font_fmt_txt_glyph_dsc_t *gdsc = &fdsc->glyph_dsc[glyph_id];
    dsc_out->adv_w = gdsc->adv_w;
    dsc_out->box_h = gdsc->box_h;
    dsc_out->box_w = gdsc->box_w;
    dsc_out->ofs_x = gdsc->ofs_x;
    dsc_out->ofs_y = gdsc->ofs_y;
    dsc_out->stride = 0;
    dsc_out->format = (lv_font_glyph_format_t)fdsc->bpp;
    dsc_out->is_placeholder = false;
    dsc_out->req_raw_bitmap = 0;
    dsc_out->gid.index = (uint32_t)glyph_id;
    return true;
}

static inline uint8_t aio_lv_font_expand_alpha(uint8_t value, uint8_t bpp)
{
    if(bpp == 1) return value ? 0xff : 0x00;
    if(bpp == 2) return (uint8_t)(value * 85U);
    if(bpp == 4) return (uint8_t)(value * 17U);
    return value;
}

static inline const void *aio_lv_font_get_bitmap_fmt_txt(lv_font_glyph_dsc_t *g_dsc,
                                                          lv_draw_buf_t *draw_buf)
{
    const lv_font_t *font = g_dsc->resolved_font;
    lv_font_fmt_txt_dsc_t *fdsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    if(!aio_lv_font_fmt_txt_is_legacy(fdsc)) {
        return lv_font_get_bitmap_fmt_txt(g_dsc, draw_buf);
    }

    uint32_t glyph_id = g_dsc->gid.index;
    const lv_font_fmt_txt_glyph_dsc_t *gdsc = &fdsc->glyph_dsc[glyph_id];
    const uint8_t *bitmap_in = &fdsc->glyph_bitmap[gdsc->bitmap_index];

    if(g_dsc->req_raw_bitmap) return bitmap_in;
    if(draw_buf == NULL || draw_buf->data == NULL) return NULL;
    if(fdsc->bitmap_format != LV_FONT_FMT_TXT_PLAIN) return NULL;

    uint8_t *bitmap_out = draw_buf->data;
    uint32_t stride_out = lv_draw_buf_width_to_stride(gdsc->box_w, LV_COLOR_FORMAT_A8);
    uint8_t bpp = (uint8_t)fdsc->bpp;
    uint8_t mask = (uint8_t)((1U << bpp) - 1U);
    uint32_t bit_pos = 0;

    for(uint32_t y = 0; y < gdsc->box_h; y++) {
        uint8_t *row = bitmap_out + y * stride_out;
        for(uint32_t x = 0; x < gdsc->box_w; x++) {
            uint32_t byte_index = bit_pos >> 3;
            uint8_t shift = (uint8_t)(8U - bpp - (bit_pos & 0x7U));
            uint8_t value = (uint8_t)((bitmap_in[byte_index] >> shift) & mask);
            row[x] = aio_lv_font_expand_alpha(value, bpp);
            bit_pos += bpp;
        }
    }

    lv_draw_buf_flush_cache(draw_buf, NULL);
    return draw_buf;
}

#define lv_font_get_glyph_dsc_fmt_txt aio_lv_font_get_glyph_dsc_fmt_txt
#define lv_font_get_bitmap_fmt_txt aio_lv_font_get_bitmap_fmt_txt

#endif /* LVGL_VERSION_MAJOR >= 9 */

#endif /* AIO_LVGL_COMPAT_H */
