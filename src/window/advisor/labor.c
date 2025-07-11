#include "labor.h"

#include "city/finance.h"
#include "city/labor.h"
#include "core/calc.h"
#include "graphics/arrow_button.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "window/labor_priority.h"

#define ADVISOR_HEIGHT 26

static void arrow_button_wages(int is_down, int param2);
static void button_priority(const generic_button *button);

static generic_button category_buttons[] = {
    {40, 77, 560, 22, button_priority},
    {40, 102, 560, 22, button_priority, 0, 1},
    {40, 127, 560, 22, button_priority, 0, 2},
    {40, 152, 560, 22, button_priority, 0, 3},
    {40, 177, 560, 22, button_priority, 0, 4},
    {40, 202, 560, 22, button_priority, 0, 5},
    {40, 227, 560, 22, button_priority, 0, 6},
    {40, 252, 560, 22, button_priority, 0, 7},
    {40, 277, 560, 22, button_priority, 0, 8},
};

static arrow_button wage_buttons[] = {
    {158, 354, 17, 24, arrow_button_wages, 1, 0},
    {182, 354, 15, 24, arrow_button_wages, 0, 0}
};

static unsigned int focus_button_id;
static unsigned int arrow_button_focus;

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS), 10, 10, COLOR_MASK_NONE, SCALE_NONE);

    lang_text_draw(50, 0, 60, 12, FONT_LARGE_BLACK);   // Labor Allocation

    // table headers
    lang_text_draw_centered(50, 21, 32, 56, 100, FONT_SMALL_PLAIN);  // Priority
    lang_text_draw(50, 22, 172, 56, FONT_SMALL_PLAIN); // Sector
    lang_text_draw_centered(50, 23, 383, 56, 100, FONT_SMALL_PLAIN); // Need
    lang_text_draw_centered(50, 24, 483, 56, 100, FONT_SMALL_PLAIN); // Have

    // xx employed, yy unemployed
    int width = text_draw_number(city_labor_workers_employed(), '@', " ", 32, 320, FONT_NORMAL_BLACK, 0);
    width += lang_text_draw(50, 12, 32 + width, 320, FONT_NORMAL_BLACK);
    width += text_draw_number(city_labor_workers_unemployed(), '@', " ", 50 + width, 320, FONT_NORMAL_BLACK, 0);
    width += lang_text_draw(50, 13, 50 + width, 320, FONT_NORMAL_BLACK);
    text_draw_number(city_labor_unemployment_percentage(), '@', "%)", 50 + width, 320, FONT_NORMAL_BLACK, 0);

    // wages panel
    inner_panel_draw(64, 350, 32, 2);
    lang_text_draw(50, 14, 70, 359, FONT_NORMAL_WHITE);
    width = text_draw_number(city_labor_wages(), '@', " ", 230, 359, FONT_NORMAL_WHITE, 0);
    width += lang_text_draw(50, 15, 230 + width, 359, FONT_NORMAL_WHITE);
    width += lang_text_draw(50, 18, 230 + width, 359, FONT_NORMAL_WHITE);
    text_draw_number(city_labor_wages_rome(), '@', " )", 230 + width, 359, FONT_NORMAL_WHITE, 0);

    // estimated wages
    width = lang_text_draw(50, 19, 64, 390, FONT_NORMAL_BLACK);
    text_draw_money(city_finance_estimated_wages(), 64 + width, 390, FONT_NORMAL_BLACK);

    return ADVISOR_HEIGHT;
}

static void draw_foreground(void)
{
    arrow_buttons_draw(0, 0, wage_buttons, 2);

    inner_panel_draw(32, 70, 36, 15);

    for (unsigned int i = 0; i < 9; i++) {
        int focus = i == focus_button_id - 1;
        int y_offset = 82 + 25 * i;
        button_border_draw(40, 77 + 25 * i, 560, 22, focus);
        const labor_category_data *cat = city_labor_category(i);
        if (cat->priority) {
            image_draw(image_group(GROUP_LABOR_PRIORITY_LOCK), 70, y_offset - 2, COLOR_MASK_NONE, SCALE_NONE);
            text_draw_number(cat->priority, '@', " ", 90, y_offset, FONT_NORMAL_WHITE, 0);
        }
        lang_text_draw(50, i + 1, 170, y_offset, FONT_NORMAL_WHITE);
        text_draw_number(cat->workers_needed, '@', " ", 410, y_offset, FONT_NORMAL_WHITE, 0);
        font_t font = FONT_NORMAL_WHITE;
        if (cat->workers_needed != cat->workers_allocated) {
            font = FONT_NORMAL_RED;
        }
        text_draw_number(cat->workers_allocated, '@', " ", 510, y_offset, font, 0);
    }
}

static int handle_mouse(const mouse *m)
{
    if (generic_buttons_handle_mouse(m, 0, 0, category_buttons, 9, &focus_button_id)) {
        return 1;
    }
    return arrow_buttons_handle_mouse(m, 0, 0, wage_buttons, 2, &arrow_button_focus);
}

static void arrow_button_wages(int is_down, int param2)
{
    city_labor_change_wages(is_down ? -1 : 1);
    city_finance_estimate_wages();
    city_finance_calculate_totals();
    window_invalidate();
}

static void button_priority(const generic_button *button)
{
    int category = button->parameter1;
    window_labor_priority_show(category);
}

static void get_tooltip_text(advisor_tooltip_result *r)
{
    if (focus_button_id) {
        r->text_id = 90;
    } else if (arrow_button_focus) {
        r->text_id = 91;
    }
}

const advisor_window_type *window_advisor_labor(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        get_tooltip_text
    };
    return &window;
}
