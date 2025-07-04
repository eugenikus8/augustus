#include "government.h"

#include "assets/assets.h"
#include "building/building.h"
#include "city/constants.h"
#include "core/dir.h"
#include "game/resource.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "translation/translation.h"
#include "window/building/figures.h"

void window_building_draw_forum(building_info_context *c)
{
    c->advisor_button = ADVISOR_FINANCIAL;
    c->help_id = 76;
    window_building_play_sound(c, "wavs/forum.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(106, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    image_draw(resource_get_data(RESOURCE_DENARII)->image.icon, c->x_offset + 16, c->y_offset + 36,
        COLOR_MASK_NONE, SCALE_NONE);

    building *b = building_get(c->building_id);
    int width = lang_text_draw(106, 2, c->x_offset + 44, c->y_offset + 43, FONT_NORMAL_BLACK);
    lang_text_draw_amount(8, 0, b->tax_income_or_storage,
        c->x_offset + 44 + width, c->y_offset + 43, FONT_NORMAL_BLACK);

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 72, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description_at(c, 72, 106, 10);
    } else if (c->worker_percentage >= 100) {
        window_building_draw_description_at(c, 72, 106, 5);
    } else if (c->worker_percentage >= 75) {
        window_building_draw_description_at(c, 72, 106, 6);
    } else if (c->worker_percentage >= 50) {
        window_building_draw_description_at(c, 72, 106, 7);
    } else if (c->worker_percentage >= 25) {
        window_building_draw_description_at(c, 72, 106, 8);
    } else {
        window_building_draw_description_at(c, 72, 106, 9);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 150);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, 230, 106, 1);
}

void window_building_draw_senate(building_info_context *c)
{
    c->advisor_button = ADVISOR_RATINGS;
    c->help_id = 77;
    window_building_play_sound(c, "wavs/senate.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(105, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    image_draw(resource_get_data(RESOURCE_DENARII)->image.icon, c->x_offset + 16, c->y_offset + 36,
        COLOR_MASK_NONE, SCALE_NONE);

    building *b = building_get(c->building_id);
    int width = lang_text_draw(105, 2, c->x_offset + 44, c->y_offset + 43, FONT_NORMAL_BLACK);
    lang_text_draw_amount(8, 0, b->tax_income_or_storage,
        c->x_offset + 44 + width, c->y_offset + 43, FONT_NORMAL_BLACK);

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 72, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description_at(c, 72, 106, 10);
    } else if (c->worker_percentage >= 100) {
        window_building_draw_description_at(c, 72, 106, 5);
    } else if (c->worker_percentage >= 75) {
        window_building_draw_description_at(c, 72, 106, 6);
    } else if (c->worker_percentage >= 50) {
        window_building_draw_description_at(c, 72, 106, 7);
    } else if (c->worker_percentage >= 25) {
        window_building_draw_description_at(c, 72, 106, 8);
    } else {
        window_building_draw_description_at(c, 72, 106, 9);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 150);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, 230, 105, 1);
}

void window_building_draw_governor_home(building_info_context *c)
{
    c->help_id = 78;
    window_building_play_sound(c, "wavs/gov_palace.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(103, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 143, 103, 1);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 68, c->y_offset + 16);
}

void window_building_draw_garden(building_info_context *c)
{
    c->help_id = 80;
    window_building_play_sound(c, "wavs/park.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(79, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 158, 79, 1);
}

void window_building_draw_plaza(building_info_context *c)
{
    c->help_id = 80;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Road.ogg");
    window_building_prepare_figure_list(c);
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(137, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    window_building_draw_figure_list(c);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 113, 137, 1);
}

void window_building_draw_statue(building_info_context *c)
{
    c->help_id = 79;
    window_building_play_sound(c, "wavs/statue.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(80, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 158, 80, 1);
}

void window_building_draw_large_statue(building_info_context *c)
{
    building *b = building_get(c->building_id);
    window_building_draw_statue(c);
    if (!b->has_water_access) {
        lang_text_draw_multiline(CUSTOM_TRANSLATION, TR_WINDOW_BUILDING_GOVERNMENT_LARGE_STATUE_WATER_WARNING,
            c->x_offset + 32, c->y_offset + 44, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_GREEN);
    }
}

void window_building_draw_triumphal_arch(building_info_context *c)
{
    c->help_id = 79;
    window_building_play_sound(c, "wavs/statue.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(80, 2, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 328, 80, 3);
    image_draw(assets_get_image_id("UI", "Triumphal_Arch_Banner"),
        c->x_offset + 37, c->y_offset + 125, COLOR_MASK_NONE, SCALE_NONE);
    image_draw_border(assets_get_image_id("UI", "Large_Banner_Border"),
        c->x_offset + 32, c->y_offset + 120, COLOR_MASK_NONE);
}

void window_building_draw_pond(building_info_context *c)
{
    c->help_id = 80;
    window_building_play_sound(c, "wavs/fountain.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

    text_draw_centered(translation_for(TR_BUILDING_WINDOW_POND),
        c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, 0);
    window_building_draw_description_at(c, 96, CUSTOM_TRANSLATION, TR_BUILDING_POND_DESC);
}

void window_building_draw_obelisk(building_info_context *c)
{
    c->help_id = 79;
    window_building_play_sound(c, "wavs/statue.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

    text_draw_centered(translation_for(TR_BUILDING_OBELISK),
        c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, 0);
    window_building_draw_description_at(c, 96, CUSTOM_TRANSLATION, TR_BUILDING_OBELISK_DESC);
}
