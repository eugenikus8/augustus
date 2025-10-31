#include "empire.h"

#include "assets/assets.h"
#include "building/menu.h"
#include "city/military.h"
#include "city/warning.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image_group.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "empire/trade_route.h"
#include "empire/trade_prices.h"
#include "empire/type.h"
#include "game/tutorial.h"
#include "game/system.h"
#include "graphics/arrow_button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/grid_box.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "input/scroll.h"
#include "input/cursor.h"
#include "platform/cursor.h"
#include "scenario/empire.h"
#include "scenario/invasion.h"
#include "window/advisors.h"
#include "window/city.h"
#include "window/empire_sidebar_sort.h"
#include "window/message_dialog.h"
#include "window/popup_dialog.h"
#include "window/resource_settings.h"
#include "window/trade_opened.h"
#include "window/trade_prices.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>  

#define WIDTH_BORDER 16 //dimensions the border image in px, informative only
#define HEIGHT_BORDER 136 
#define SIDEBAR_ENTRY_HEIGHT 120 
#define BOTTOM_PANEL_HEIGHT 120

#define RESOURCE_ICON_WIDTH 26 //dimensions the resource icon in px, informative only
#define RESOURCE_ICON_HEIGHT 26

#define VERTICAL_TILE_WIDTH 40 //dimensions the vertical background tile in px, informative only
#define VERTICAL_TILE_HEIGHT 72
#define CLAMP(a, x, b) (((x) < (a)) ? (a) : \
			((b) < (x)) ? (b) : (x))
#define TRADE_DOT_SPACING 10 //spacing between dots in trade route line
#define MAX_SIDEBAR_CITIES 256 
#define MAX_RESOURCE_BUTTONS 256
#define MAX_TRADE_OPEN_BUTTONS 64
#define MAX_TRADE_EDGES 4096
#define MAX_DOTS_PER_ROUTE 1024
#define MAX_DOTS_ON_MAP (MAX_DOTS_PER_ROUTE * MAX_SIDEBAR_CITIES)
#define TRADE_PULSE_DOT_MS 180 
#define TRADE_DOT_ANIMATION_SCALE 160

#define FONT_SPACE_WIDTH font_definition_for(FONT_NORMAL_GREEN)->space_width
#define FONT_HEIGHT_NORMAL font_definition_for(FONT_NORMAL_GREEN)->line_height
#define FONT_HEIGHT_LARGE font_definition_for(FONT_LARGE_BLACK)->line_height

#define NO_POSITION ((unsigned int) -1) //used as an alterntive to 0 for some of new pointers
//to avoid confusion with when relying on external indexing, which can be 0-based

//typedefs 
typedef enum {
    TRADE_ICON_NONE = -1,
    TRADE_ICON_LAND = 0,
    TRADE_ICON_SEA = 1
} trade_icon_type;

typedef enum {
    EMPIRE_WINDOW_OUTSIDE = 0,
    EMPIRE_WINDOW_MAP = 1,
    EMPIRE_WINDOW_BOTTOM_PANEL = 2,
    EMPIRE_WINDOW_SIDEBAR = 3
} empire_window_area;

typedef enum {
    TRADE_STYLE_MAIN_BAR,
    TRADE_STYLE_SIDEBAR
} trade_style_variant;

typedef struct {
    int x;
    int y;
} px_point;

typedef struct {
    int sidebar_item_id; // number on the list
    int empire_object_id; // empire object id of the city
    int city_id; //city index in the empire's array of cities
    int x;
    int y;
    int width;
    int height;
} sidebar_city_entry;

typedef struct {
    // Region bounds
    int row_width;               // Total width of the row (x_max = x_min + width)
    int row_height;              // Total height of the row (optional: for future clipping)
    // Layout adjustments
    int x_offset_text;
    int y_offset_text;
    int y_offset_icon;       // Vertical offset to nudge icons up/down
    int label_indent;        // Horizontal offset where the first icon is placed (based on "Buys"/"Sells" label width)
    // Segment spacing
    int seg_space_0;         // Space before resource icon
    int seg_space_1;         // Space between icon and first number
    int seg_space_2;         // Space between first number and "of" text
    int seg_space_3;         // Space between "of" text and second number
    int seg_space_4;         // Space after second number
    int segment_width_adjust; // Extra width added/subtracted to total segment width
} trade_row_style;

typedef struct {
    // Region bounds
    int button_x_min;               // Starting x-coordinate of the button
    int button_y_min;               // Starting y-coordinate of the tbutton
    int button_width;               // Total width of the row (x_max = x_min + width)
    int button_height;              // Total height of the row (optional: for future clipping)
    // Layout adjustments
    int y_offset_icon;       // Vertical offset to nudge icons up/down
    int y_offset_text;       // Vertical offset for text baseline alignment
    // Segment spacing
    int seg_space_0;         // Space before border start
    int seg_space_1;         // Space between border start and cost string
    int seg_space_2;         // Space between cost and currency
    int seg_space_3;         // Space between currency and text
    int seg_space_4;         // Space between text and icon
    int seg_space_5;         // Space after icon

    int segment_width_adjust; // Extra width added/subtracted to total segment width
} open_trade_button_style;

typedef struct {
    int x, y, width, height;
    int route_id;
    int do_highlight;
} trade_open_button;

typedef struct {
    int x, y, width, height;
    resource_type res;
    int do_highlight;
} resource_button;
// Edges are DIRECTIONAL (start->end). No normalization.
// One edge can appear in multiple routes; we draw it once per frame via `drawn`.

typedef struct {
    int id;                 // 0-based index in g_trade_edges
    int x1, y1, x2, y2;     // start -> end (exact order preserved)
    int trade_route_id;     // for reference
    int is_sea;             // 1 sea, 0 land
    int drawn;              // set during draw pass to avoid double-drawing same edge
} trade_edge;

typedef struct {
    int x;
    int y;
    int is_sea;
} trade_dot;

struct trade_route_anim {
    int index;
    trade_dot *trade_dots[MAX_DOTS_ON_MAP];
} trade_routes_anim;

static trade_edge g_trade_edges[MAX_TRADE_EDGES];
static int g_trade_edge_count = 0;

// For each route_id: a list of 0-based edge indices, terminated by -1.
static int trade_city_edges[MAX_SIDEBAR_CITIES][MAX_TRADE_EDGES];
// measurements and scales helper functions
static int measure_trade_row_width(const empire_city *city, int is_sell, const trade_row_style *style); // ???
static void image_draw_silh_scaled_centered(int image_id, int x, int y, color_t color, int draw_scale_percent);
static void animation_draw_scaled(const image *img, int image_id, int new_animation, int x, int y, color_t color, int draw_scale_percent);
static int open_trade_button_icon_fits(const empire_city *city, const open_trade_button_style *style, trade_icon_type icon_type);
static void draw_sidebar_city_item(const grid_box_item *item);
static int draw_images_at_interval(int image_id, int x_draw_offset, int y_draw_offset,
    int start_x, int start_y, int end_x, int end_y, int interval, int remaining);
void window_empire_collect_trade_edges(void);
static void window_empire_draw_trade_route_pulses(const empire_object *route_object, int x_offset, int y_offset);
// 'styles' get functions
static trade_row_style get_trade_row_style(const empire_city *city, int is_sell, int max_draw_width, trade_style_variant variant);
static open_trade_button_style get_open_trade_button_style(int x, int y, trade_style_variant variant);

//buttons
static void button_help(int param1, int param2);
static void button_return_to_city(int param1, int param2);
static void button_advisor(int advisor, int param2);
static void button_show_prices(int param1, int param2);
static void button_show_resource_window(int resource_button_index);
static void button_open_trade_by_route(int route_id);

//sidebar show/hide
static void sidebar_collapse(void);
static void sidebar_expand(void);

//helpers for integrating sidebar and map
static void process_selection(void);

//positioning and area checking
static int is_sidebar(const mouse *m);
static int is_sidebar_border(const mouse *m);
static int is_map(const mouse *m);
static void handle_sidebar_border(const mouse *m);
static void on_sidebar_city_click(const grid_box_item *item);

//buttons position registrators to enable dynamic positioning
static void register_resource_button(int x, int y, int width, int height, resource_type r, int highlight);
static void register_open_trade_button(int x, int y, int width, int height, int route_id, int highlight);
static void sorting_order(int is_down, int param2);

//arrays and counts for sidebar trade, resource and sorting buttons
static trade_open_button trade_open_buttons[MAX_TRADE_OPEN_BUTTONS];
static int trade_open_button_count = 0;
static resource_button resource_buttons[MAX_RESOURCE_BUTTONS];
static int resource_button_count = 0;

//sidebar-related arrays and variables
static scrollbar_type sidebar_scrollbar;
static sidebar_city_entry sidebar_cities[MAX_SIDEBAR_CITIES];
static int sidebar_city_count = 0;
static grid_box_type sidebar_grid_box;

//original button properties
static image_button image_button_help[] = {
    {0, 0, 27, 27, IB_NORMAL, GROUP_CONTEXT_ICONS, 0, button_help, button_none, 0, 0, 1}
};
static image_button image_button_return_to_city[] = {
    {0, 0, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4, button_return_to_city, button_none, 0, 0, 1}
};
static image_button image_button_advisor[] = {
    {-4, 0, 24, 24, IB_NORMAL, GROUP_MESSAGE_ADVISOR_BUTTONS, 12, button_advisor, button_none, ADVISOR_TRADE, 0, 1}
};
static image_button image_button_show_prices[] = {
    {-4, 0, 24, 24, IB_NORMAL, GROUP_MESSAGE_ADVISOR_BUTTONS, 30, button_show_prices, button_none, 0, 0, 1}
};
typedef struct {
    int x, y, width, height;
    int is_down; // 1 for down arrow, 0 for up arrow
} arrow_button_info;

//values for drawing resource shields
static px_point trade_amount_px_offsets[5] = {
    { 2, 0 },
    { 5, 2 },
    { 8, 4 },
    { 0, 3 },
    { 4, 6 },
};

static struct {
    unsigned int selected_button;
    int selected_city;
    int selected_trade_route;
    int x_min, x_max, y_min, y_max;
    int x_draw_offset, y_draw_offset;
    int usable_map_width;
    unsigned int focus_button_id;
    int is_scrolling;
    int finished_scroll;
    int hovered_object;
    int hovered_resource_button;
    resource_type focus_resource;
    struct {
        int x_min;
        int x_max;
    } panel;
    struct {
        int x_min, x_max, y_min, y_max;
        int margin_left, margin_right, margin_top, margin_bottom;
        int width, height;
        int scroll;
        int scroll_max;
        int initialised;
        uint8_t width_percent; // sidebar width as percentage of map width (0-100)
        int dragging; // is sidebar being dragged
        uint8_t dragging_width; // width during dragging (0-100)
        uint8_t previous_width; // used to restore the width when dragging ends (0-100)
        struct {
            int x_min;
            int x_max;
            int y_min;
            int y_max;
            int is_collapsed;
            int is_hovered;
        } border_btn;
    } sidebar;
    int trade_route_anim_start;
} data = { 0, 1 , 0 };

// -------------------------------------------------------------------------------------------------------
//                                              INIT + DATA
// -------------------------------------------------------------------------------------------------------

static void init(void)
{
    data.selected_button = NO_POSITION; // no button selected
    data.trade_route_anim_start = 0;
    window_empire_sidebar_sort_set_expanded_main(NO_POSITION);
    if (!data.sidebar.initialised) {
        window_empire_sidebar_sort_set_current_sorting(SORT_BY_NAME); // default sorting method
        window_empire_sidebar_sort_set_current_filtering(FILTER_NONE); // default to no filtering
        window_empire_sidebar_sort_set_selected_filter_resource(RESOURCE_NONE); // no resource selected
    }
    window_empire_sidebar_sort_set_hovered_sorting_button(NO_POSITION);
    window_empire_sidebar_sort_set_resource_selection_active(0); // not in resource selection mode

    data.sidebar.width_percent = config_get(CONFIG_UI_EMPIRE_SIDEBAR_WIDTH); // default sidebar width (25%)
    data.sidebar.dragging = 0; // not dragging initially
    data.sidebar.dragging_width = 0;
    data.sidebar.previous_width = 0;
    data.sidebar.border_btn.is_hovered = 0; // not hovered initially
    data.sidebar.initialised = 1;
    process_selection();
    data.focus_button_id = 0;
    window_empire_collect_trade_edges();
    data.trade_route_anim_start = time_get_millis();
}

static void setup_sidebar(void)
{
    // Calculate sidebar bounds
    int s_width = screen_width();
    int s_height = screen_height();
    int map_width, map_height;
    empire_get_map_size(&map_width, &map_height);

    int max_width = map_width + WIDTH_BORDER;
    int max_height = map_height + HEIGHT_BORDER;

    data.x_min = s_width <= max_width ? 0 : (s_width - max_width) / 2;
    data.x_max = s_width <= max_width ? s_width : data.x_min + max_width;
    data.y_min = s_height <= max_height ? 0 : (s_height - max_height) / 2;
    data.y_max = s_height <= max_height ? s_height : data.y_min + max_height;

    int map_draw_x_min = data.x_min + WIDTH_BORDER;
    int map_draw_x_max = data.x_max - WIDTH_BORDER;
    int map_draw_y_min = data.y_min + WIDTH_BORDER;
    int map_draw_y_max = data.y_max - BOTTOM_PANEL_HEIGHT;

    data.sidebar.margin_left = 3; //margins betwene sidebar and gridbox
    data.sidebar.margin_right = 3;
    data.sidebar.margin_top = 2 * BLOCK_SIZE + 6; //space for sorting buttons
    data.sidebar.margin_bottom = 6;
    data.usable_map_width = map_draw_x_max - map_draw_x_min;

    // Use only one width source - prefer dragging width when actively dragging
    uint8_t active_width_percent = data.sidebar.dragging ? data.sidebar.dragging_width : data.sidebar.width_percent;
    int raw = (data.usable_map_width * active_width_percent) / 100;
    data.sidebar.width = ((raw + (BLOCK_SIZE / 2)) / BLOCK_SIZE) * BLOCK_SIZE + data.sidebar.margin_left + data.sidebar.margin_right;
    //ensure that the inner panels draw predictably

    data.sidebar.height = map_draw_y_max - map_draw_y_min;

    data.sidebar.x_min = map_draw_x_max - data.sidebar.width;
    data.sidebar.x_max = map_draw_x_max;
    data.sidebar.y_min = map_draw_y_min;
    data.sidebar.y_max = map_draw_y_max;
}

static void setup_sidebar_gridbox(void)
{
    if (data.sidebar.width_percent < 1) {
        return;
    }

    int y = data.sidebar.y_min;
    sidebar_city_count = 0;

    for (int i = 1; i < empire_city_get_array_size(); i++) { // skip "no city" entry
        empire_city *city = empire_city_get(i);
        if (!city->in_use || city->type != EMPIRE_CITY_TRADE) continue;
        // apply the chosen filter
        if (!window_empire_sidebar_sort_city_matches_current_filter(city)) continue;
        if (sidebar_city_count >= MAX_SIDEBAR_CITIES) break;

        sidebar_city_entry *entry = &sidebar_cities[sidebar_city_count];
        entry->sidebar_item_id = sidebar_city_count;
        entry->city_id = i;
        entry->empire_object_id = city->empire_object_id;
        entry->x = data.sidebar.x_min + data.sidebar.margin_left;
        entry->y = y;
        y += SIDEBAR_ENTRY_HEIGHT;
        sidebar_city_count++;
    }

    qsort(sidebar_cities, sidebar_city_count, sizeof(sidebar_city_entry), window_empire_sidebar_sort_sidebar_city_sorter);
    int selection_visible = 0;
    for (int i = 0; i < sidebar_city_count; i++) {
        if (sidebar_cities[i].city_id == data.selected_city) { selection_visible = 1; break; }
    }
    if (!selection_visible) {
        data.selected_city = 0; // or keep it but ensure UI handles "not in list"
    }

    sidebar_grid_box.x = data.sidebar.x_min + data.sidebar.margin_left;
    sidebar_grid_box.y = data.sidebar.y_min + data.sidebar.margin_top;
    sidebar_grid_box.width = data.sidebar.width - data.sidebar.margin_right - data.sidebar.margin_left;
    sidebar_grid_box.height = data.sidebar.y_max - data.sidebar.y_min - data.sidebar.margin_top - data.sidebar.margin_bottom;
    sidebar_grid_box.item_height = SIDEBAR_ENTRY_HEIGHT;
    sidebar_grid_box.num_columns = 1;
    sidebar_grid_box.item_margin.horizontal = 0;
    sidebar_grid_box.item_margin.vertical = 5;
    sidebar_grid_box.draw_inner_panel = 0;
    sidebar_grid_box.extend_to_hidden_scrollbar = 1;
    sidebar_grid_box.decorate_scrollbar = 1;
    sidebar_grid_box.total_items = sidebar_city_count;
    sidebar_grid_box.draw_item = draw_sidebar_city_item;
    sidebar_grid_box.on_click = on_sidebar_city_click;
    sidebar_grid_box.handle_tooltip = NULL;
    sidebar_grid_box.offset_scrollbar_x = grid_box_has_scrollbar(&sidebar_grid_box) ? -14 : 0;
    sidebar_grid_box.offset_scrollbar_y = grid_box_has_scrollbar(&sidebar_grid_box) ? -20 : 0;
    grid_box_set_bounds(&sidebar_grid_box, sidebar_grid_box.x, sidebar_grid_box.y, sidebar_grid_box.width, sidebar_grid_box.height);
}

// -------------------------------------------------------------------------------------------------------
//                                              SIDEBAR HELPERS
// -------------------------------------------------------------------------------------------------------


static void sidebar_collapse(void)
{
    data.sidebar.width_percent = 0;
    data.sidebar.border_btn.is_collapsed = 1;
    window_invalidate();
}
static void sidebar_expand(void)
{
    data.sidebar.width_percent = 25;
    data.sidebar.border_btn.is_collapsed = 0;
    window_invalidate();
}

static open_trade_button_style get_open_trade_button_style(int x, int y, trade_style_variant variant)
{
    int is_sidebar = (variant == TRADE_STYLE_SIDEBAR);

    open_trade_button_style style = {
        .button_x_min = (is_sidebar ? x + 15 : (data.panel.x_min + data.panel.x_max - 500) / 2) + 30, //15px offset somewhere got lost, * 2 for 30
        .button_y_min = y + (is_sidebar ? 0 : -9),
        .button_width = is_sidebar ? get_usable_width(&sidebar_grid_box) * 0.75 : 440,  // restored fixed widths // sidebar: main bar
        .button_height = 26,
        .y_offset_icon = is_sidebar ? 2 : 2, //separated in case of resolution considerations - wait for feedback before simplifying
        .y_offset_text = is_sidebar ? 10 : 10, //separated in case of resolution considerations - wait for feedback before simplifying
        .seg_space_0 = is_sidebar ? 0 : 0,  //later centerd in draw_open_trade_button, so adjust carefully
        .seg_space_1 = 0,
        .seg_space_2 = 0,
        .seg_space_3 = 8,
        .seg_space_4 = 0,
        .seg_space_5 = is_sidebar ? 4 : 4,
        .segment_width_adjust = 0
    };

    return style;
}

static trade_row_style get_trade_row_style(const empire_city *city, int is_sell, int max_draw_width, trade_style_variant variant)
{
    int is_main_bar = (variant == TRADE_STYLE_MAIN_BAR);
    // === Initial struct ===
    trade_row_style style = {
        .x_offset_text = is_main_bar ? (city->is_open ? (is_sell ? 0 : 0) : 0)
        /*sidebar*/ : (10),
        .y_offset_text = is_main_bar ? (city->is_open ? (is_sell ? 40 : 71) : 42) //open compact (sell) = 40 : open non-compact (buy) = 71 closed (both compact & non-compact) = 42
        /*sidebar*/ : (6 /*26 icon height, 4 is shields, 2 is gap*/),
        .row_width = max_draw_width,
        .row_height = 0,
        .y_offset_icon = style.y_offset_text - 9
    };
    int count_sells = window_empire_sidebar_sort_count_trade_resources(city, 1);
    int count_buys = window_empire_sidebar_sort_count_trade_resources(city, 0);

    // === Determine compactness ===
    int compact_sells = is_main_bar ? (count_sells > 5) : (count_sells > 2);
    int compact_buys = is_main_bar ? (count_buys > 5) : (count_buys > 2);
    int any_compact = compact_sells || compact_buys;

    int is_compact = is_main_bar
        ? (is_sell ? compact_sells : compact_buys)
        : (city->is_open ? (is_sell ? compact_sells : compact_buys) : any_compact);

    // === Label indent ===
    if (!city->is_open) {
        int label_id = is_sell ? 5 : 4;
        style.label_indent = lang_text_get_width(47, label_id, FONT_NORMAL_GREEN) + (is_compact ? 5 : 20);
    } else { // labels
        int width_sells = lang_text_get_width(47, 10, FONT_NORMAL_GREEN);
        int width_buys = lang_text_get_width(47, 9, FONT_NORMAL_GREEN);
        int max_label_width = (width_sells > width_buys) ? width_sells : width_buys;
        style.label_indent = max_label_width + (any_compact ? 5 : 15) - FONT_SPACE_WIDTH + 5;
    }
    // === Segment layout ===
    if (is_main_bar) {
        style.seg_space_0 = 0;
        style.seg_space_1 = city->is_open ? (is_compact ? 2 : 8) : (is_compact ? 0 : 6); // (open compact : open non-compact) : (closed compact closed non-compact)
        style.seg_space_2 = city->is_open ? (is_compact ? 0 : -1) : (is_compact ? 0 : 3);
        style.seg_space_3 = city->is_open ? (is_compact ? 0 : -1) : (is_compact ? 0 : 3);
        style.seg_space_4 = city->is_open ? (is_compact ? 0 : 14) : (is_compact ? 0 : 10);
        style.segment_width_adjust = city->is_open ? (is_compact ? -3 : 0) : (is_compact ? -4 : -2);
    } else {//sidebar styles
        style.seg_space_0 = 0;
        style.seg_space_1 = city->is_open ? (is_compact ? 2 : 6) : (is_compact ? 0 : 4);
        style.seg_space_2 = city->is_open ? (is_compact ? (0 - FONT_SPACE_WIDTH / 2) : 0) : (is_compact ? 0 : 5);
        style.seg_space_3 = city->is_open ? (is_compact ? (0 - FONT_SPACE_WIDTH / 2) : 0) : (is_compact ? 0 : 5);
        style.seg_space_4 = city->is_open ? (is_compact ? (0 - FONT_SPACE_WIDTH / 2) : 10) : (is_compact ? 0 : 7);
        style.segment_width_adjust = city->is_open ? (is_compact ? 0 : 0) : (is_compact ? 0 : 0);
    }

    return style;
}

static int open_trade_button_icon_fits(const empire_city *city, const open_trade_button_style *style, trade_icon_type icon_type)
{
    if (!city || !style || icon_type == TRADE_ICON_NONE)
        return 0;

    int cost = city->cost_to_open;
    int available_width = style->button_width - 6; // 6px border

    // --- Measure elements ---
    int cost_width = lang_text_get_amount_width(8, 0, cost, FONT_NORMAL_GREEN);
    int label_width = lang_text_get_width(47, 6, FONT_NORMAL_GREEN);
    int icon_width = 28;

    // --- Widths with spacings ---
    int width_cost_only = style->seg_space_0 + style->seg_space_1 + cost_width;
    int width_cost_and_label = width_cost_only + style->seg_space_2 + label_width + style->seg_space_3;
    int width_full = width_cost_and_label + style->seg_space_4 + icon_width + style->seg_space_5;

    return width_full < available_width;
}

static int measure_trade_row_width(const empire_city *city, int is_sell, const trade_row_style *style)
{
    const int ICON_WIDTH = 26;
    int width = 0;

    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!resource_is_storable(r)) continue;
        if ((is_sell && !city->sells_resource[r]) || (!is_sell && !city->buys_resource[r])) continue;

        int w_max = text_get_number_width(trade_route_limit(city->route_id, r), '\0', "", FONT_NORMAL_GREEN);
        int segment_width;

        if (city->is_open) {
            // Also need width of current amount and "of" label
            int w_now = text_get_number_width(trade_route_traded(city->route_id, r), '\0', "", FONT_NORMAL_GREEN);
            int w_of = lang_text_get_width(47, 11, FONT_NORMAL_GREEN);

            segment_width =
                style->seg_space_0 + ICON_WIDTH +
                style->seg_space_1 + w_now +
                style->seg_space_2 + w_of +
                style->seg_space_3 + w_max +
                style->seg_space_4 +
                style->segment_width_adjust;
        } else {
            segment_width =
                style->seg_space_0 + ICON_WIDTH +
                style->seg_space_1 + w_max +
                style->seg_space_4 +
                style->segment_width_adjust;
        }

        width += segment_width;
    }

    if (window_empire_sidebar_sort_count_trade_resources(city, is_sell)) {
        width += style->label_indent;
    }

    return width;
}

// -------------------------------------------------------------------------------------------------------
//                                              DRAW PANELING (BACKGROUND)
// -------------------------------------------------------------------------------------------------------

static void draw_paneling(void)
{
    int image_base = image_group(GROUP_EMPIRE_PANELS);
    int bottom_panel_is_larger = data.x_min != data.panel.x_min;
    int vertical_y_limit = bottom_panel_is_larger ? data.y_max - BOTTOM_PANEL_HEIGHT : data.y_max;

    graphics_set_clip_rectangle(data.panel.x_min, data.y_min,
        data.panel.x_max - data.panel.x_min, data.y_max - data.y_min);

    // bottom panel background
    for (int x = data.panel.x_min; x < data.panel.x_max; x += 70) {
        image_draw(image_base + 3, x, data.y_max - BOTTOM_PANEL_HEIGHT, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 3, x, data.y_max - 80, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 3, x, data.y_max - 40, COLOR_MASK_NONE, SCALE_NONE);
    }

    // horizontal bar borders
    for (int x = data.panel.x_min; x < data.panel.x_max; x += 86) {
        image_draw(image_base + 1, x, data.y_max - BOTTOM_PANEL_HEIGHT, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 1, x, data.y_max - WIDTH_BORDER, COLOR_MASK_NONE, SCALE_NONE);
    }

    // extra vertical bar borders
    if (bottom_panel_is_larger) {
        for (int y = vertical_y_limit + WIDTH_BORDER; y < data.y_max; y += 86) {
            image_draw(image_base, data.panel.x_min, y, COLOR_MASK_NONE, SCALE_NONE);
            image_draw(image_base, data.panel.x_max - WIDTH_BORDER, y, COLOR_MASK_NONE, SCALE_NONE);
        }
    }

    graphics_set_clip_rectangle(data.x_min, data.y_min, data.x_max - data.x_min, vertical_y_limit - data.y_min);

    for (int x = data.x_min; x < data.x_max; x += 86) {
        image_draw(image_base + 1, x, data.y_min, COLOR_MASK_NONE, SCALE_NONE);
    }

    // vertical bar borders
    for (int y = data.y_min + WIDTH_BORDER; y < vertical_y_limit; y += 86) {
        image_draw(image_base, data.x_min, y, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base, data.x_max - WIDTH_BORDER, y, COLOR_MASK_NONE, SCALE_NONE);
    }

    graphics_reset_clip_rectangle();

    // crossbars
    image_draw(image_base + 2, data.x_min, data.y_min, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.x_min, data.y_max - BOTTOM_PANEL_HEIGHT, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.panel.x_min, data.y_max - WIDTH_BORDER, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.x_max - WIDTH_BORDER, data.y_min, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.x_max - WIDTH_BORDER, data.y_max - BOTTOM_PANEL_HEIGHT, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.panel.x_max - WIDTH_BORDER, data.y_max - WIDTH_BORDER, COLOR_MASK_NONE, SCALE_NONE);

    if (bottom_panel_is_larger) {
        image_draw(image_base + 2, data.panel.x_min, data.y_max - BOTTOM_PANEL_HEIGHT, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 2, data.panel.x_max - WIDTH_BORDER, data.y_max - BOTTOM_PANEL_HEIGHT, COLOR_MASK_NONE, SCALE_NONE);
    }
    // Sidebar background 
    graphics_set_clip_rectangle(data.sidebar.x_min - WIDTH_BORDER, data.sidebar.y_min, //clipping - border, to let border be drawn OUTSIDE
        data.sidebar.width + WIDTH_BORDER, //account for width border substracted earlier to make sure textures stretch all the way
        data.sidebar.y_max - data.sidebar.y_min);
    int asset_id = assets_lookup_image_id(ASSET_UI_VERTICAL_EMPIRE_PANEL);

    for (int x = data.sidebar.x_min; x <= data.sidebar.x_max; x += 40) {
        for (int y = data.sidebar.y_min; y < data.sidebar.y_max; y += 70) {
            image_draw(asset_id, x, y, COLOR_MASK_NONE, SCALE_NONE);
        }
    }
    // Sidebar border 
    for (int y = data.sidebar.y_min; y < data.sidebar.y_max; y += 86) {
        image_draw(image_base, data.sidebar.x_min - WIDTH_BORDER, y, COLOR_MASK_NONE, SCALE_NONE);
    }

    data.sidebar.border_btn.is_collapsed = (data.sidebar.width_percent > 0) ? 0 : 1;
    data.sidebar.border_btn.x_min = data.sidebar.x_min - WIDTH_BORDER;
    data.sidebar.border_btn.x_max = data.sidebar.border_btn.is_collapsed ? data.sidebar.x_max : data.sidebar.x_min;
    data.sidebar.border_btn.y_min = data.sidebar.y_min;
    data.sidebar.border_btn.y_max = data.sidebar.y_max;

    // Draw border button highlight if hovered
    if (data.sidebar.border_btn.is_hovered) {
        graphics_shade_rect(
            data.sidebar.border_btn.x_min,
            data.sidebar.border_btn.y_min,
            data.sidebar.border_btn.x_max - data.sidebar.border_btn.x_min,
            data.sidebar.border_btn.y_max - data.sidebar.border_btn.y_min,
            2 // shade style (0-7)
        );
    }

    graphics_reset_clip_rectangle();
    scrollbar_draw(&sidebar_scrollbar);

}

// -------------------------------------------------------------------------------------------------------
//                                          NEW TRADE ROUTES
// -------------------------------------------------------------------------------------------------------

// Return existing/new edge index (0-based), or -1 on overflow.
// Equality is order-sensitive: same (x1,y1)->(x2,y2) AND same is_sea.
static int add_or_get_trade_edge(int start_x, int start_y, int end_x, int end_y, int route_id, int is_sea)
{
    for (int edge_index = 0; edge_index < g_trade_edge_count; edge_index++) {
        trade_edge *edge = &g_trade_edges[edge_index];
        if (edge->is_sea == is_sea &&
            edge->x1 == start_x && edge->y1 == start_y &&
            edge->x2 == end_x && edge->y2 == end_y) {
            return edge_index; // found existing directed edge
        }
    }

    if (g_trade_edge_count >= MAX_TRADE_EDGES) {
        return -1; // cannot add more edges
    }

    trade_edge new_edge = {
        .id = g_trade_edge_count,
        .x1 = start_x,
        .y1 = start_y,
        .x2 = end_x,
        .y2 = end_y,
        .trade_route_id = route_id,
        .is_sea = is_sea,
        .drawn = 0
    };

    g_trade_edges[g_trade_edge_count] = new_edge;
    g_trade_edge_count++;

    return new_edge.id;
}

void window_empire_collect_trade_edges(void)
{
    const empire_object *our_city_object = empire_object_get_our_city();
    g_trade_edge_count = 0;
    memset(g_trade_edges, 0, sizeof(g_trade_edges));
    memset(trade_city_edges, 0xFF, sizeof(trade_city_edges));
    // Pre-fill per-route edge lists with -1 (sentinel terminator).
    for (int object_index = 0; object_index < empire_object_count(); object_index++) {
        const empire_object *route_object = empire_object_get(object_index);
        int is_sea_route = -1;
        if (route_object->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
            is_sea_route = 1;
        } else if (route_object->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
            is_sea_route = 0;
        } else {
            continue; // not a trade route object
        }

        int route_id = route_object->trade_route_id;
        if (route_id < 0 || route_id >= MAX_SIDEBAR_CITIES) {
            continue; // invalid route id; skip this route
        }

        const empire_object *trade_city_object = empire_object_get_trade_city(route_id);
        int segment_start_x = our_city_object->x + 25;
        int segment_start_y = our_city_object->y + 25;
        int route_edge_count = 0;

        // Waypoints belonging to this route are contiguous after the route object
        for (int waypoint_index = object_index + 1; waypoint_index < empire_object_count(); waypoint_index++) {
            const empire_object *waypoint_object = empire_object_get(waypoint_index);

            if (waypoint_object->type != EMPIRE_OBJECT_TRADE_WAYPOINT || waypoint_object->trade_route_id != route_id) {
                break; // reached non-waypoint or different route; waypoint sequence ends
            }

            int edge_index = add_or_get_trade_edge(
                segment_start_x, segment_start_y, waypoint_object->x, waypoint_object->y, route_id, is_sea_route);

            if (edge_index >= 0) {
                if (route_edge_count < MAX_TRADE_EDGES) {
                    trade_city_edges[route_id][route_edge_count] = edge_index;
                    route_edge_count++;
                }
            }

            segment_start_x = waypoint_object->x;
            segment_start_y = waypoint_object->y;
        }

        // Final leg to destination city center
        int city_center_x = trade_city_object->x + 25;
        int city_center_y = trade_city_object->y + 25;

        int final_edge_index = add_or_get_trade_edge(
            segment_start_x, segment_start_y, city_center_x, city_center_y, route_id, is_sea_route);

        if (final_edge_index >= 0) {
            if (route_edge_count < MAX_TRADE_EDGES) {
                trade_city_edges[route_id][route_edge_count] = final_edge_index;
                route_edge_count++;
            }
        }

        if (route_edge_count < MAX_TRADE_EDGES) {
            trade_city_edges[route_id][route_edge_count] = -1; // explicit terminator for clarity
        }
    }
}

void window_empire_draw_static_trade_waypoints(const empire_object *route_object, int x_offset, int y_offset)
{
    int is_sea_route = 0;
    if (route_object->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        is_sea_route = 1;
    } else {
        is_sea_route = 0; // treat anything else here as land (caller should pass a valid route)
    }

    int image_id = assets_get_image_id("UI", is_sea_route ? "SeaRouteDot" : "LandRouteDot");
    int route_id = route_object->trade_route_id;
    if (route_id < 0 || route_id >= MAX_SIDEBAR_CITIES) {
        return; // invalid route id; nothing to draw
    }

    // Even spacing across the whole polychain; remainder carries between edges.
    const empire_object *our_city_object = empire_object_get_our_city();
    (void) our_city_object; // not needed here but kept for parity with collection step

    int remaining_spacing = TRADE_DOT_SPACING;

    for (int list_index = 0; list_index < MAX_TRADE_EDGES; list_index++) {
        int edge_index = trade_city_edges[route_id][list_index];

        if (edge_index < 0) {
            break; // reached sentinel; no more edges for this route
        }

        if (edge_index >= g_trade_edge_count) {
            continue; // stale or out-of-range mapping; skip this edge
        }

        trade_edge *edge = &g_trade_edges[edge_index];

        int draw_image_id = edge->drawn ? 0 : image_id;
        remaining_spacing = draw_images_at_interval(draw_image_id, x_offset, y_offset, edge->x1, edge->y1,
                                                    edge->x2, edge->y2, TRADE_DOT_SPACING, remaining_spacing);

        edge->drawn = 1; // mark as processed for this frame
    }
    if (config_get(CONFIG_UI_ANIMATE_TRADE_ROUTES)) {
        window_empire_draw_trade_route_pulses(route_object, x_offset, y_offset);
    }
}

static void draw_trade_route_pulse_index(int image_id, int x_offset, int y_offset, int route_id, int dot_index)
{
    if (!image_id || !route_id) {
        return;
    }

    int target_distance = dot_index * TRADE_DOT_SPACING;
    int accumulated_distance = 0;

    for (int list_index = 0; list_index < MAX_TRADE_EDGES; list_index++) {
        int edge_index = trade_city_edges[route_id][list_index];
        if (edge_index < 0) {
            break;
        }

        trade_edge *edge = &g_trade_edges[edge_index];
        int dx = edge->x2 - edge->x1;
        int dy = edge->y2 - edge->y1;
        int segment_length = (int) sqrt(dx * dx + dy * dy);

        if (target_distance <= accumulated_distance + segment_length) {
            int along = target_distance - accumulated_distance;
            int x_factor = calc_percentage(dx, segment_length);
            int y_factor = calc_percentage(dy, segment_length);
            int x = calc_adjust_with_percentage(along, x_factor) + edge->x1;
            int y = calc_adjust_with_percentage(along, y_factor) + edge->y1;

            image_draw_scaled_centered(image_id, x_offset + x, y_offset + y, COLOR_MASK_NONE, TRADE_DOT_ANIMATION_SCALE);
            return;
        }
        accumulated_distance += segment_length;
    }
}

static void window_empire_draw_trade_route_pulses(const empire_object *route_object, int x_offset, int y_offset)
{
    int is_sea_route = 0;
    if (route_object->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        is_sea_route = 1;
    } else if (route_object->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
        is_sea_route = 0;
    } else {
        return;
    }
    int route_id = route_object->trade_route_id;
    int pulse_image_id = assets_get_image_id("UI", !is_sea_route ? "SeaRouteDot" : "LandRouteDot"); //opposite image

    int total_length_pixels = 0;
    for (int list_index = 0; list_index < MAX_TRADE_EDGES; list_index++) {
        int edge_index = trade_city_edges[route_id][list_index];
        if (edge_index < 0) {
            break; // no more edges
        }
        trade_edge *edge = &g_trade_edges[edge_index];
        int delta_x = edge->x2 - edge->x1;
        int delta_y = edge->y2 - edge->y1;
        int segment_length = (int) sqrt(delta_x * delta_x + delta_y * delta_y);
        total_length_pixels += segment_length;
    }

    int dot_count = (total_length_pixels / TRADE_DOT_SPACING) + 1;
    if (!dot_count) {
        return;
    }
    time_millis elapsed_millis = time_get_millis() - data.trade_route_anim_start;

    int ticks_since_start = (int) (elapsed_millis / TRADE_PULSE_DOT_MS);
    int forward_index_from_start = ticks_since_start % dot_count;

    int index_from_trade_city = (dot_count - 1) - forward_index_from_start;
    draw_trade_route_pulse_index(pulse_image_id, x_offset, y_offset, route_id, index_from_trade_city);
}

// -------------------------------------------------------------------------------------------------------
//                                              FOREGROUND ELEMENTS DRAWING 
// -------------------------------------------------------------------------------------------------------

void draw_trade_resource(resource_type r, int trade_max, int x, int y)
{
    graphics_draw_inset_rect(x - 1, y - 1, 26, 26, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
    image_draw(resource_get_data(r)->image.empire, x, y, COLOR_MASK_NONE, SCALE_NONE);
    window_empire_draw_resource_shields(trade_max, x, y);
}

void window_empire_draw_resource_shields(int trade_max, int x_offset, int y_offset)
{
    int num_bronze_shields = (trade_max % 100) / 20 + 1;
    if (trade_max >= 600) {
        num_bronze_shields = 5;
    }

    int top_left_x;
    if (num_bronze_shields == 1) {
        top_left_x = x_offset + 19;
    } else if (num_bronze_shields == 2) {
        top_left_x = x_offset + 15;
    } else {
        top_left_x = x_offset + 11;
    }
    int top_left_y = y_offset - 1;
    int bronze_shield = image_group(GROUP_TRADE_AMOUNT);
    for (int i = 0; i < num_bronze_shields; i++) {
        px_point pt = trade_amount_px_offsets[i];
        image_draw(bronze_shield, top_left_x + pt.x, top_left_y + pt.y, COLOR_MASK_NONE, SCALE_NONE);
    }

    int num_gold_shields = trade_max / 100;
    if (num_gold_shields > 5) {
        num_gold_shields = 5;
    }
    top_left_x = x_offset - 1;
    top_left_y = y_offset + 22;
    int gold_shield = assets_lookup_image_id(ASSET_GOLD_SHIELD);
    for (int i = 0; i < num_gold_shields; i++) {
        image_draw(gold_shield, top_left_x + i * 3, top_left_y, COLOR_MASK_NONE, SCALE_NONE);
    }
}

void draw_open_trade_button(const empire_city *city, const open_trade_button_style *style, trade_icon_type icon_type)
{
    int cost = city->cost_to_open;
    int x = style->button_x_min;
    int y = style->button_y_min;
    int available_width = style->button_width - 6; //6 pixels reserved for the button border

    // --- Measure elements ---
    int cost_width = lang_text_get_amount_width(8, 0, cost, FONT_NORMAL_GREEN);
    int label_width = lang_text_get_width(47, 6, FONT_NORMAL_GREEN);
    int icon_width = (icon_type != TRADE_ICON_NONE) ? 28 : 0;

    // --- Measure total widths with segments ---
    int width_cost_only = style->seg_space_0 + style->seg_space_1 + cost_width;
    //inclusion of seg_space_0 is optional, but without it there are no margins considered for centering, which is problematic in smaller resolutions
    int width_cost_and_label = width_cost_only + style->seg_space_2 + label_width + style->seg_space_3;
    int width_full = width_cost_and_label + style->seg_space_4 + icon_width + style->seg_space_5; //add seg_space_0 again to even out

    // --- Decide what to draw ---
    int draw_label = 0, draw_icon = 0;
    int content_width = 0;

    if (width_full < available_width) {
        draw_label = 1;
        draw_icon = (icon_type != TRADE_ICON_NONE);
        content_width = width_full;

    } else if (width_cost_and_label <= available_width) {
        draw_label = 1;
        draw_icon = 0;
        content_width = width_cost_and_label;
    } else if (width_cost_only <= available_width) {
        draw_label = 0;
        draw_icon = 0;
        content_width = width_cost_only;
    } else {
        // Can't fit even cost alone: don't draw button or register it
        return;
    }

    // --- Draw button border and register its position ---
    button_border_draw(x, y, style->button_width, style->button_height, 0);
    register_open_trade_button(x, y, style->button_width, style->button_height, city->route_id, 0);

    // Center content horizontally, preset vertical position
    int cursor_x = x + (available_width - content_width) / 2 - 2; //-2 to account for the button border. Makes small resolutions look better
    int cursor_y = y + style->y_offset_text;

    // Cost - number
    cursor_x += style->seg_space_1;
    cursor_x += lang_text_draw_amount(8, 0, cost, cursor_x, cursor_y, FONT_NORMAL_GREEN);

    // Label
    if (draw_label) {
        cursor_x += style->seg_space_2;
        lang_text_draw(47, 6, cursor_x, cursor_y, FONT_NORMAL_GREEN);
        cursor_x += label_width;
    }

    // Icon
    if (draw_icon) {
        cursor_x += style->seg_space_3;
        int image_id = image_group(GROUP_EMPIRE_TRADE_ROUTE_TYPE) + 1 - icon_type;
        image_draw(image_id, cursor_x, y + style->y_offset_icon + 2 * icon_type, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static int draw_trade_row(const empire_city *city, int is_sell, int x, int y, const trade_row_style *style)
{
    int label_id;
    if (city->is_open) {
        label_id = is_sell ? 10 : 9;
    } else {
        label_id = is_sell ? 5 : 4;
    }
    // Draw "Sells:" or "Buys:" label
    int x_cursor = x + style->x_offset_text;
    int y_cursor = y + style->y_offset_text;
    int dots_width = text_get_width((const uint8_t *) "(...)", FONT_NORMAL_GREEN) + 2;
    int draw_label = window_empire_sidebar_sort_count_trade_resources(city, is_sell); //check if there are any resources to draw
    if (!draw_label) {
        // No resources to draw, return current x position
        return x_cursor;
    }
    if (x_cursor + lang_text_get_width(47, label_id, FONT_NORMAL_GREEN) > x + style->row_width) {
        //not enough space to draw row
        if (x_cursor + dots_width > x + style->row_width) {
            //not enough space even for dots
            return x_cursor;
        } else {
            x_cursor += text_draw((const uint8_t *) "(...)", x_cursor + 2, y_cursor, FONT_NORMAL_GREEN, 0);
        }

        return x_cursor;
    }
    int label_width = lang_text_draw(47, label_id, x_cursor, y_cursor, FONT_NORMAL_GREEN);
    if (city->is_open) {
        x_cursor += style->label_indent; //advance by pre-defined label width for open cities where there's two rows
    } else {
        x_cursor += label_width;
    }


    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!resource_is_storable(r)) continue;
        if ((is_sell && !city->sells_resource[r]) || (!is_sell && !city->buys_resource[r])) continue;

        int trade_max = trade_route_limit(city->route_id, r);
        int trade_now = trade_route_traded(city->route_id, r);
        int icon_y = y + style->y_offset_icon;

        int segment_width, text_x;

        if (city->is_open) {
            // Calculate widths
            int w_now = text_get_number_width(trade_now, '\0', "", FONT_NORMAL_GREEN); // '\0' - 0 length suffix.
            int w_max = text_get_number_width(trade_max, '\0', "", FONT_NORMAL_GREEN);
            int w_of = lang_text_get_width(47, 11, FONT_NORMAL_GREEN);

            segment_width =
                style->seg_space_0 + RESOURCE_ICON_WIDTH +
                style->seg_space_1 + w_now +
                style->seg_space_2 + w_of +
                style->seg_space_3 + w_max +
                style->seg_space_4 +
                style->segment_width_adjust;
        } else {
            int w_max = text_get_number_width(trade_max, '\0', "", FONT_NORMAL_GREEN);
            segment_width =
                style->seg_space_0 + RESOURCE_ICON_WIDTH +
                style->seg_space_1 + w_max +
                style->seg_space_4 +
                style->segment_width_adjust;
        }
        // Clip if segment would overflow
        if (x_cursor + segment_width + dots_width > x + style->row_width) {
            if (x_cursor + dots_width > x + style->row_width) {
                //not enough space even for dots
                break;
            }
            x_cursor += text_draw((const uint8_t *) "(...)", x_cursor + 2, y_cursor, FONT_NORMAL_GREEN, 0);
            break;
        }
        draw_trade_resource(r, trade_max, x_cursor + style->seg_space_0, icon_y);
        // Draw numeric info
        text_x = x_cursor + style->seg_space_0 + RESOURCE_ICON_WIDTH + style->seg_space_1;

        if (city->is_open) {
            int w_now = text_draw_number(trade_now, '\0', "", text_x, y_cursor, FONT_NORMAL_GREEN, 0);
            int of_x = text_x + w_now + style->seg_space_2;
            int w_of = lang_text_draw(47, 11, of_x, y_cursor, FONT_NORMAL_GREEN);
            int max_x = of_x + w_of + style->seg_space_3;
            text_draw_number(trade_max, '\0', "", max_x, y_cursor, FONT_NORMAL_GREEN, 0);
        } else {
            text_draw_number(trade_max, '\0', "", text_x, y_cursor, FONT_NORMAL_GREEN, 0);
        }
        // Register button and hitbox
        register_resource_button(
            x_cursor,
            icon_y,
            segment_width,
            RESOURCE_ICON_HEIGHT,
            r,
            city->is_open
        );

        // Advance
        x_cursor += segment_width;
    }
    return x_cursor; // Final drawing position
}

static void draw_trade_city_info(const empire_object *object, const empire_city *city)
{
    int y_offset = data.y_max - 113;
    const int safe_margin_left = data.panel.x_min + 50;
    const int safe_margin_right = data.panel.x_max - 50;
    int max_draw_width = safe_margin_right - safe_margin_left;

    trade_row_style style_sells = get_trade_row_style(city, 1, max_draw_width, TRADE_STYLE_MAIN_BAR);
    trade_row_style style_buys = get_trade_row_style(city, 0, max_draw_width, TRADE_STYLE_MAIN_BAR);
    // === OPEN CITY ===
    if (city->is_open) {
        int width_sells = measure_trade_row_width(city, 1, &style_sells);
        int width_buys = measure_trade_row_width(city, 0, &style_buys);
        int total_width = (width_sells > width_buys) ? width_sells : width_buys;

        if (total_width > max_draw_width)
            total_width = max_draw_width;

        int x_offset = safe_margin_left + (max_draw_width - total_width) / 2;
        draw_trade_row(city, 1, x_offset, y_offset, &style_sells);
        draw_trade_row(city, 0, x_offset, y_offset, &style_buys);


        // === CLOSED CITY ===
    } else {
        int width_sells = measure_trade_row_width(city, 1, &style_sells);
        int width_buys = measure_trade_row_width(city, 0, &style_buys);
        int total_width = width_sells + width_buys + 15;

        if (total_width > max_draw_width)
            total_width = max_draw_width;

        int x_base = safe_margin_left + (max_draw_width - total_width) / 2;
        draw_trade_row(city, 1, x_base, y_offset, &style_sells);
        draw_trade_row(city, 0, width_sells + x_base, y_offset, &style_buys);

        // Draw cost + type icon
        open_trade_button_style style = get_open_trade_button_style(x_base, y_offset + 73, TRADE_STYLE_MAIN_BAR);
        draw_open_trade_button(city, &style, (trade_icon_type) (city->is_sea_trade));

    }

}

static void draw_sidebar_city_item(const grid_box_item *item)
{
    sidebar_city_entry *entry = &sidebar_cities[item->index];
    empire_city *city = empire_city_get(entry->city_id);
    const uint8_t *name = empire_city_get_name(city);
    //const grid_box_type *sidebar_grid_box;

    int item_usable_width = get_usable_width(&sidebar_grid_box) + sidebar_grid_box.offset_scrollbar_x;
    graphics_set_clip_rectangle(data.sidebar.x_min, data.sidebar.y_min, item_usable_width + 4, data.sidebar.y_max); // 4px extra reserved for the border drawn AROUND the item
    int x_blocks = item_usable_width / BLOCK_SIZE;
    int y_blocks = item->height / BLOCK_SIZE;

    // base offset for all content in the box
    int x_offset = item->x;
    int y_offset = item->y;
    trade_row_style style_sells = get_trade_row_style(city, 1, item_usable_width + sidebar_grid_box.offset_scrollbar_x, TRADE_STYLE_SIDEBAR);
    trade_row_style style_buys = get_trade_row_style(city, 0, item_usable_width + sidebar_grid_box.offset_scrollbar_x, TRADE_STYLE_SIDEBAR);
    // draw background + name + badge
    inner_panel_draw(x_offset, y_offset, x_blocks, y_blocks);
    if (item->is_focused) {
        data.hovered_object = city->empire_object_id + 1;
    }
    if (data.hovered_object == city->empire_object_id + 1) {
        graphics_shade_rect(
            item->x,
            item->y,
            item_usable_width - data.sidebar.margin_right,
            item->height - data.sidebar.margin_bottom,
            2  // 0-7
        );
    }
    if (entry->city_id == data.selected_city) {
        button_border_draw(item->x, item->y, item_usable_width, item->height - data.sidebar.margin_bottom / 2, 1); // margin/2 to not be exactly the same size as the item
    }

    int badge_id = assets_get_image_id("UI", "Empire_sidebar_city_badge");
    int badge_width = image_get(badge_id)->width;
    int image_id = image_group(GROUP_EMPIRE_TRADE_ROUTE_TYPE) + 1 - city->is_sea_trade;
    int available_width = item_usable_width - data.sidebar.margin_right;
    int badge_and_icon_width = badge_width + 2 + 34;
    int badge_margin = 5;
    open_trade_button_style open_trade_style = get_open_trade_button_style(item->x, y_offset, TRADE_STYLE_SIDEBAR);
    int draw_icon_on_top = !open_trade_button_icon_fits(city, &open_trade_style, (trade_icon_type) (city->is_sea_trade));

    if (badge_and_icon_width <= available_width) {
        // Everything fits
        image_draw(badge_id, x_offset + badge_margin, y_offset + badge_margin, COLOR_MASK_NONE, SCALE_NONE);

        text_draw(name, x_offset + badge_margin + BLOCK_SIZE, y_offset + 9, FONT_LARGE_BLACK, 0);
        if (city->is_open || draw_icon_on_top) {
            //if city is open, draw trade route icon to remind of type, same if it doesnt fit in the button
            int trade_route_icon_offset = badge_width + BLOCK_SIZE;
            if ((trade_route_icon_offset + badge_margin + 2 + 34) <= item_usable_width) {
                image_draw(image_id, x_offset + trade_route_icon_offset + badge_margin, y_offset + 9 + 2 * city->is_sea_trade, COLOR_MASK_NONE, SCALE_NONE);
            }
        }

    } else if (badge_width <= available_width) {
        // Only badge fits, check if the icon fits inside it
        image_draw(badge_id, x_offset + badge_margin, y_offset + badge_margin, COLOR_MASK_NONE, SCALE_NONE);
        int city_name_end = text_draw(name, x_offset + badge_margin + BLOCK_SIZE, y_offset + 9, FONT_LARGE_BLACK, 0);
        int icon_fits_in_badge = (city_name_end + badge_margin + 2 + 34) <= (x_offset + badge_margin + badge_width);
        if (icon_fits_in_badge) {
            image_draw(image_id, x_offset + badge_margin + city_name_end + BLOCK_SIZE + 2, y_offset + 9 + 2 * city->is_sea_trade, COLOR_MASK_NONE, SCALE_NONE);
        }

    } else { // Not enough room for badge + icon
        text_draw(name, x_offset + badge_margin, y_offset + 9, FONT_LARGE_BLACK, 0);
    }
    // Move y_offset down for trade info rows
    y_offset += 44;
    if (city->is_open) {
        y_offset += 8; // For Sells
        draw_trade_row(city, 1, x_offset, y_offset, &style_sells);
        y_offset += 26; // For Buys
        draw_trade_row(city, 0, x_offset, y_offset, &style_buys);
    } else {// --- Closed city ---
        int x_cursor = draw_trade_row(city, 1, x_offset, y_offset, &style_sells); //draw sell row
        int sell_row_width = x_cursor - x_offset;
        style_buys.row_width -= sell_row_width; //limit the available row width by the sell row length
        draw_trade_row(city, 0, x_cursor, y_offset, &style_buys);
        y_offset += 35;
        //recalculate the style basing on the new y_offset
        open_trade_button_style open_trade_style_closed = get_open_trade_button_style(item->x, y_offset, TRADE_STYLE_SIDEBAR);
        draw_open_trade_button(city, &open_trade_style_closed, (trade_icon_type) (city->is_sea_trade));
    }
    graphics_reset_clip_rectangle();
}

static void draw_city_info(const empire_object *object)
{
    int x_offset = (data.x_min + data.x_max - 240) / 2;
    int y_offset = data.y_max - 88;
    const empire_city *city = empire_city_get(data.selected_city);
    switch (city->type) {
        case EMPIRE_CITY_DISTANT_ROMAN:
            lang_text_draw_centered(47, 12, x_offset, y_offset + 42, 240, FONT_NORMAL_GREEN);
            break;
        case EMPIRE_CITY_VULNERABLE_ROMAN:
            if (city_military_distant_battle_city_is_roman()) {
                lang_text_draw_centered(47, 12, x_offset, y_offset + 42, 240, FONT_NORMAL_GREEN);
            } else {
                lang_text_draw_centered(47, 13, x_offset, y_offset + 42, 240, FONT_NORMAL_GREEN);
            }
            break;
        case EMPIRE_CITY_FUTURE_TRADE:
        case EMPIRE_CITY_DISTANT_FOREIGN:
        case EMPIRE_CITY_FUTURE_ROMAN:
            lang_text_draw_centered(47, 0, x_offset, y_offset + 42, 240, FONT_NORMAL_GREEN);
            break;
        case EMPIRE_CITY_OURS:
            lang_text_draw_centered(47, 1, x_offset, y_offset + 42, 240, FONT_NORMAL_GREEN);
            break;
        case EMPIRE_CITY_TRADE:
            draw_trade_city_info(object, city);
            break;
    }
}

static void draw_roman_army_info(const empire_object *object)
{
    int x_offset = (data.x_min + data.x_max - 240) / 2;
    int y_offset = data.y_max - 68;
    int text_id;
    if (city_military_distant_battle_roman_army_is_traveling_forth()) {
        text_id = 15;
    } else {
        text_id = 16;
    }
    lang_text_draw_multiline(47, text_id, x_offset, y_offset, 240, FONT_NORMAL_GREEN);
}

static void draw_enemy_army_info(const empire_object *object)
{
    lang_text_draw_multiline(47, 14,
        (data.x_min + data.x_max - 240) / 2,
        data.y_max - 68,
        240, FONT_NORMAL_GREEN);
}

static void draw_object_info(void)
{
    process_selection();
    int selected_object = empire_selected_object();
    if (selected_object) {
        const empire_object *object = empire_object_get(selected_object - 1);
        switch (object->type) {
            case EMPIRE_OBJECT_CITY:
                draw_city_info(object);
                break;
            case EMPIRE_OBJECT_ROMAN_ARMY:
                if (city_military_distant_battle_roman_army_is_traveling()) {
                    if (city_military_distant_battle_roman_months_traveled() == object->distant_battle_travel_months) {
                        draw_roman_army_info(object);
                    }
                }
                break;
            case EMPIRE_OBJECT_ENEMY_ARMY:
                if (city_military_months_until_distant_battle() > 0) {
                    if (city_military_distant_battle_enemy_months_traveled() == object->distant_battle_travel_months) {
                        draw_enemy_army_info(object);
                    }
                }
                break;
            default:
                lang_text_draw_centered(47, 8, data.panel.x_min, data.y_max - 48,
                    data.panel.x_max - data.panel.x_min, FONT_NORMAL_GREEN);
                break;
        }
    } else {
        lang_text_draw_centered(47, 8, data.panel.x_min, data.y_max - 48,
            data.panel.x_max - data.panel.x_min, FONT_NORMAL_GREEN);
    }
}

static void draw_background(void)
{
    int s_width = screen_width();
    int s_height = screen_height();
    int map_width, map_height;
    empire_get_map_size(&map_width, &map_height);
    int max_width = map_width + WIDTH_BORDER;
    int max_height = map_height + HEIGHT_BORDER;

    data.x_min = s_width <= max_width ? 0 : (s_width - max_width) / 2;
    data.x_max = s_width <= max_width ? s_width : data.x_min + max_width;
    data.y_min = s_height <= max_height ? 0 : (s_height - max_height) / 2;
    data.y_max = s_height <= max_height ? s_height : data.y_min + max_height;

    int bottom_panel_width = data.x_max - data.x_min;
    if (bottom_panel_width < 608) {
        bottom_panel_width = 640;
        int difference = bottom_panel_width - (data.x_max - data.x_min);
        int odd = difference % 1;
        difference /= 2;
        data.panel.x_min = data.x_min - difference - odd;
        data.panel.x_max = data.x_max + difference;
    } else {
        data.panel.x_min = data.x_min;
        data.panel.x_max = data.x_max;
    }

    if (data.x_min || data.y_min) {
        image_draw_blurred_fullscreen(image_group(GROUP_EMPIRE_MAP), 3);
        graphics_shade_rect(0, 0, screen_width(), screen_height(), 7);
    }
    setup_sidebar();

}

static int draw_images_at_interval(int image_id, int x_draw_offset, int y_draw_offset,
    int start_x, int start_y, int end_x, int end_y, int interval, int remaining)
{
    int x_diff = end_x - start_x;
    int y_diff = end_y - start_y;
    int dist = (int) sqrt(x_diff * x_diff + y_diff * y_diff);
    int x_factor = calc_percentage(x_diff, dist);
    int y_factor = calc_percentage(y_diff, dist);
    int offset = interval - remaining;
    if (offset > dist) {
        return offset;
    }
    dist -= offset;
    int num_dots = dist / interval;
    remaining = dist % interval;
    if (image_id) {
        for (int j = 0; j <= num_dots; j++) {
            int x = calc_adjust_with_percentage(j * interval + offset, x_factor) + start_x;
            int y = calc_adjust_with_percentage(j * interval + offset, y_factor) + start_y;
            image_draw(image_id, x_draw_offset + x, y_draw_offset + y, COLOR_MASK_NONE, SCALE_NONE);
        }
    }
    return remaining;
}

void window_empire_draw_border(const empire_object *border, int x_offset, int y_offset)
{
    const empire_object *first_edge = empire_object_get(border->id + 1);
    if (first_edge->type != EMPIRE_OBJECT_BORDER_EDGE) {
        return;
    }
    int last_x = first_edge->x;
    int last_y = first_edge->y;
    int image_id = first_edge->image_id;
    int remaining = border->width;

    // Align the coordinate to the base of the border flag's mast
    x_offset -= 0;
    y_offset -= 14;

    for (int i = first_edge->id + 1; i < empire_object_count(); i++) {
        empire_object *obj = empire_object_get(i);
        if (obj->type != EMPIRE_OBJECT_BORDER_EDGE) {
            break;
        }
        int animation_offset = 0;
        int x = x_offset;
        int y = y_offset;
        if (image_id) {
            const image *img = image_get(image_id);
            draw_images_at_interval(image_id, x, y, last_x, last_y, obj->x, obj->y, border->width, remaining);
            if (img->animation && img->animation->speed_id) {
                animation_offset = empire_object_update_animation(obj, image_id);
                x += img->animation->sprite_offset_x;
                y += img->animation->sprite_offset_y;
            }
            remaining = draw_images_at_interval(image_id + animation_offset, x, y, last_x, last_y, obj->x, obj->y,
                border->width, remaining);
        } else {
            remaining = border->width;
        }
        last_x = obj->x;
        last_y = obj->y;
        image_id = obj->image_id;
    }
    if (!image_id) {
        return;
    }
    int animation_offset = 0;
    const image *img = image_get(image_id);
    if (img->animation && img->animation->speed_id) {
        animation_offset = empire_object_update_animation(border, image_id);
    }
    draw_images_at_interval(image_id, x_offset, y_offset, last_x, last_y, first_edge->x, first_edge->y,
        border->width, remaining);
    if (animation_offset) {
        draw_images_at_interval(image_id + animation_offset,
                x_offset + img->animation->sprite_offset_x, y_offset + img->animation->sprite_offset_y,
                last_x, last_y, first_edge->x, first_edge->y, border->width, remaining);
    }
}

static void draw_empire_object(const empire_object *obj)
{
    if (obj->type == EMPIRE_OBJECT_TRADE_WAYPOINT || obj->type == EMPIRE_OBJECT_BORDER_EDGE) {
        return;
    }
    if (obj->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE || obj->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        if (!empire_city_is_trade_route_open(obj->trade_route_id)) {
            return; // dont draw the icon if route is closed
        }
    }
    int x, y, image_id;
    if (scenario_empire_is_expanded()) {
        x = obj->expanded.x;
        y = obj->expanded.y;
        image_id = obj->expanded.image_id;
    } else {
        x = obj->x;
        y = obj->y;
        image_id = obj->image_id;
    }
    if (obj->type == EMPIRE_OBJECT_BORDER) {
        window_empire_draw_border(obj, data.x_draw_offset, data.y_draw_offset);
    }
    if (obj->type == EMPIRE_OBJECT_CITY) {
        const empire_city *city = empire_city_get(empire_city_get_for_object(obj->id));
        if (city->type == EMPIRE_CITY_DISTANT_FOREIGN ||
            city->type == EMPIRE_CITY_FUTURE_ROMAN) {
            image_id = image_group(GROUP_EMPIRE_FOREIGN_CITY);
        } else if (city->type == EMPIRE_CITY_TRADE) {
            // Fix cases where empire map still gives a blue flag for new trade cities
            // (e.g. Massilia in campaign Lugdunum)
            image_id = image_group(GROUP_EMPIRE_CITY_TRADE);
        }
    }
    if (obj->type == EMPIRE_OBJECT_BATTLE_ICON) {
        // handled later
        return;
    }
    if (obj->type == EMPIRE_OBJECT_ENEMY_ARMY) {
        if (city_military_months_until_distant_battle() <= 0) {
            return;
        }
        if (city_military_distant_battle_enemy_months_traveled() != obj->distant_battle_travel_months) {
            return;
        }
    }
    if (obj->type == EMPIRE_OBJECT_ROMAN_ARMY) {
        if (!city_military_distant_battle_roman_army_is_traveling()) {
            return;
        }
        if (city_military_distant_battle_roman_months_traveled() != obj->distant_battle_travel_months) {
            return;
        }
    }
    if (obj->type == EMPIRE_OBJECT_ORNAMENT) {
        if (image_id < 0) {
            image_id = assets_lookup_image_id(ASSET_FIRST_ORNAMENT) - 1 - image_id;
        }
    }
    const image *img = image_get(image_id);
    if ((((unsigned int) data.hovered_object == obj->id + 1) && obj->type == EMPIRE_OBJECT_CITY) ||
        ((empire_selected_object() == obj->id + 1) && obj->type == EMPIRE_OBJECT_CITY)) {
        // actions for currently hovered or selected city objects 
        if ((empire_selected_object() == obj->id + 1) && obj->type == EMPIRE_OBJECT_CITY) {
            const int offsets[16][2] = {
                {1, 0}, {0, 1}, {-1, 0}, {0, -1},
                {3, 0}, {0, 3}, {-3, 0}, {0, -3},
                {1, 1}, {-1, 1}, {-1, -1}, {1, -1},
                {3, 3}, {-3, 3}, {-3, -3}, {3, -3}
            }; // 3 an 1 offsets worked best in testing, other values can be used for readability if necessary
            for (int i = 0; i < 16; i++) {
                int dx = offsets[i][0];
                int dy = offsets[i][1];
                image_draw_silh_scaled_centered(image_id,
                    data.x_draw_offset + x + dx, data.y_draw_offset + y + dy, COLOR_MASK_ORANGE_GOLD, 130);
                // any mask will work
            }

            image_draw_scaled_centered(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, 130);

            int new_animation = empire_object_update_animation(obj, image_id);
            animation_draw_scaled(img, image_id, new_animation, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, 130);

        } else {
            image_draw_scaled_centered(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, 120);

            if (img->animation && img->animation->speed_id) {
                int new_animation = empire_object_update_animation(obj, image_id);
                animation_draw_scaled(img, image_id, new_animation, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, 120);
            }
        }

    } else {
        image_draw(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, SCALE_NONE);
        if (img->animation && img->animation->speed_id) {
            int new_animation = empire_object_update_animation(obj, image_id);
            image_draw(image_id + new_animation,
                data.x_draw_offset + x + img->animation->sprite_offset_x,
                data.y_draw_offset + y + img->animation->sprite_offset_y,
                COLOR_MASK_NONE, SCALE_NONE);
        }
    }

    // Manually fix the Hagia Sophia
    if (obj->image_id == 8122) {
        image_id = assets_lookup_image_id(ASSET_HAGIA_SOPHIA_FIX);
        image_draw(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static void empire_draw_object_trade_route(const empire_object *obj)
{
    if (obj->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE || obj->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        if (scenario_empire_id() == SCENARIO_CUSTOM_EMPIRE) {
            if (empire_city_is_trade_route_open(obj->trade_route_id)) {
                window_empire_draw_static_trade_waypoints(obj, data.x_draw_offset, data.y_draw_offset);
            }
        }
    }
    return;
}

static void animation_draw_scaled(const image *img, int image_id, int new_animation, int x, int y, color_t color, int draw_scale_percent)
{
    float obj_draw_scale = 100.0f / draw_scale_percent;
    float base_scaled_x = (((x) +img->width / 2.0f) - (img->width / obj_draw_scale) / 2.0f) * obj_draw_scale;
    float base_scaled_y = (((y) +img->height / 2.0f) - (img->height / obj_draw_scale) / 2.0f) * obj_draw_scale;

    // Apply animation sprite offset and draw
    float anim_x = base_scaled_x + img->animation->sprite_offset_x;
    float anim_y = base_scaled_y + img->animation->sprite_offset_y;

    image_draw(image_id + new_animation, anim_x, anim_y, COLOR_MASK_NONE, obj_draw_scale);
}

static void image_draw_silh_scaled_centered(int image_id, int x, int y, color_t color, int draw_scale_percent)
{
    float obj_draw_scale = 100.0f / draw_scale_percent;
    const image *img = image_get(image_id);

    float scaled_x = (((x) +img->width / 2.0f) - (img->width / obj_draw_scale) / 2.0f) * obj_draw_scale;
    float scaled_y = (((y) +img->height / 2.0f) - (img->height / obj_draw_scale) / 2.0f) * obj_draw_scale;

    image_draw_silhouette(image_id, scaled_x, scaled_y, color, obj_draw_scale);
}

static void draw_invasion_warning(int x, int y, int image_id)
{
    image_draw(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, SCALE_NONE);
}

static void draw_map(void)
{
    // Recalculate inner bounds (same as draw_background)
    int map_clip_x_min = data.x_min + WIDTH_BORDER;
    int map_clip_y_min = data.y_min + WIDTH_BORDER;
    int map_clip_x_max = data.sidebar.x_min;  // Stop before sidebar starts
    int map_clip_y_max = data.y_max - BOTTOM_PANEL_HEIGHT;

    graphics_set_clip_rectangle(map_clip_x_min, map_clip_y_min, map_clip_x_max - map_clip_x_min, map_clip_y_max - map_clip_y_min);
    // Reset all edge drawn flags for this frame
    for (int i = 0; i < g_trade_edge_count; i++) {
        g_trade_edges[i].drawn = 0;
    }
    empire_set_viewport(map_clip_x_max - map_clip_x_min, map_clip_y_max - map_clip_y_min);

    data.x_draw_offset = map_clip_x_min;
    data.y_draw_offset = map_clip_y_min;
    empire_adjust_scroll(&data.x_draw_offset, &data.y_draw_offset);

    image_draw(empire_get_image_id(), data.x_draw_offset, data.y_draw_offset, COLOR_MASK_NONE, SCALE_NONE);
    if (data.trade_route_anim_start == 0) {
        data.trade_route_anim_start = time_get_millis();
    }

    empire_object_foreach(draw_empire_object);
    empire_object_foreach_of_type(empire_draw_object_trade_route, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    empire_object_foreach_of_type(empire_draw_object_trade_route, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_CITY);

    scenario_invasion_foreach_warning(draw_invasion_warning);

    graphics_reset_clip_rectangle();
}

static void draw_city_name(const empire_city *city)
{
    int image_base = image_group(GROUP_EMPIRE_PANELS);
    int draw_ornaments_outside = data.x_min - data.panel.x_min > 90;
    int base_x_min = draw_ornaments_outside ? data.panel.x_min : data.x_min;
    int base_x_max = draw_ornaments_outside ? data.panel.x_max : data.x_max;
    image_draw(image_base + 6, base_x_min + 2, data.y_max - 199, COLOR_MASK_NONE, SCALE_NONE);//left bird
    if (data.sidebar.border_btn.is_collapsed) {
        image_draw(image_base + 7, base_x_max - 84, data.y_max - 199, COLOR_MASK_NONE, SCALE_NONE);//right bird
    }
    image_draw(image_base + 8, (data.x_min + data.x_max - 332) / 2, data.y_max - 181, COLOR_MASK_NONE, SCALE_NONE); //city badge big
    if (city) {
        int x_offset = (data.panel.x_min + data.panel.x_max - 332) / 2 + 64;
        int y_offset = data.y_max - 118;
        const uint8_t *city_name = empire_city_get_name(city);
        text_draw_centered(city_name, x_offset, y_offset, 268, FONT_LARGE_BLACK, 0);
    }
}

static void draw_panel_buttons(void)
{
    image_buttons_draw(data.panel.x_min + 20, data.y_max - 44, image_button_help, 1);
    image_buttons_draw(data.panel.x_max - 44, data.y_max - 44, image_button_return_to_city, 1);
    image_buttons_draw(data.panel.x_max - 44, data.y_max - 100, image_button_advisor, 1);
    image_buttons_draw(data.panel.x_min + 24, data.y_max - 100, image_button_show_prices, 1);
    //image_buttons_draw(data.sidebar.x_min-32,data.y_min + 100, button_toggle_sidebar_width, 1);
    if (data.selected_button != NO_POSITION) {
        const trade_open_button *btn = &trade_open_buttons[data.selected_button];
        button_border_draw(btn->x - 1, btn->y - 1, btn->width + 2, btn->height + 2, 1);
    }
}

static void draw_sidebar_grid_box(void)
{
    graphics_set_clip_rectangle(
        data.sidebar.x_min,
        data.sidebar.y_min,
        data.sidebar.width,
        data.sidebar.height
    );
    grid_box_draw(&sidebar_grid_box);
    graphics_set_clip_rectangle(
        data.sidebar.x_min,
        data.sidebar.y_min,
        data.sidebar.width,
        data.sidebar.height
    );
    if (data.sidebar.width_percent > 5 && (!data.sidebar.dragging || data.sidebar.dragging_width > 5)) {
        window_empire_sidebar_sort_draw_expanding_buttons(data.sidebar.x_min, data.sidebar.y_min, data.sidebar.width, grid_box_has_scrollbar(&sidebar_grid_box));
    }

    graphics_reset_clip_rectangle();
}

static void draw_trade_button_highlights(void)
{
    for (int i = 0; i < resource_button_count; ++i) {
        const resource_button *btn = &resource_buttons[i];
        if (data.hovered_resource_button == i && btn->do_highlight) {
            button_border_draw(btn->x - 1, btn->y - 1, btn->width + 2, btn->height + 2, 1);
            continue;
        }
        if (data.focus_resource == btn->res) {
            time_millis elapsed = time_get_millis() - data.trade_route_anim_start;
            float time_seconds = elapsed / 1000.0f; // Convert to seconds
            float pulse = sinf(time_seconds * 1.0f * 3.14f); // 1 full cycle per second
            int alpha = 96 + (int) (pulse * 64); // Range: 32–160
            graphics_tint_rect(btn->x, btn->y, RESOURCE_ICON_WIDTH - 1, RESOURCE_ICON_HEIGHT - 1,
                COLOR_MASK_DARK_PINK, alpha
            );
        }

    }
}

// -------------------------------------------------------------------------------------------------------
//                                              DRAW FOREGROUND
// -------------------------------------------------------------------------------------------------------

static void draw_foreground(void)
{
    draw_map();
    setup_sidebar_gridbox();
    resource_button_count = 0;
    trade_open_button_count = 0;
    const empire_city *city = 0;
    int selected_object = empire_selected_object();

    if (selected_object) {
        const empire_object *object = empire_object_get(selected_object - 1); // it should be -1 , thats correct
        if (object->type == EMPIRE_OBJECT_CITY) {
            data.selected_city = empire_city_get_for_object(object->id);
            city = empire_city_get(data.selected_city);
        }
    } else {
        data.selected_city = 0;
    }
    draw_paneling();
    if (!data.sidebar.border_btn.is_collapsed) {
        draw_sidebar_grid_box();  // grid_box uses usable_sidebar dimensions
        grid_box_request_refresh(&sidebar_grid_box);
    }
    draw_city_name(city);
    draw_object_info();
    draw_panel_buttons();
    draw_trade_button_highlights();
}

static void determine_selected_object(const mouse *m)
{
    if (is_map(m)) {
        if (!m->left.went_up || data.finished_scroll) {
            int hovered_obj_id = empire_get_hovered_object(m->x - data.x_min - 16, m->y - data.y_min - 16);
            data.hovered_object = hovered_obj_id;
            return;
        } else {
            empire_select_object(m->x - data.x_min - 16, m->y - data.y_min - 16);
            window_invalidate();
        }
    } else if (is_sidebar(m)) {
        if (sidebar_grid_box.focused_item.index == NO_POSITION) {
            data.hovered_object = NO_POSITION;
        }
    } else {
        data.finished_scroll = 0;
        data.hovered_object = NO_POSITION;
        return;
    }
}

static void process_selection(void)
{
    int selected_object = empire_selected_object();
    if (selected_object) {
        data.selected_city = empire_city_get_for_object(selected_object - 1);
        //data.selected_city is array index of the empire object from the array of cities 
    } else {
        data.selected_city = 0;
    }
}

// -------------------------------------------------------------------------------------------------------
//                                              HANDLE INPUT
// -------------------------------------------------------------------------------------------------------

// Mouse position helper functions
static int is_sidebar(const mouse *m)
{
    if (m->x >= data.sidebar.x_min &&
        m->x < data.sidebar.x_max &&
        m->y >= data.sidebar.y_min &&
        m->y < data.sidebar.y_max) {
        return 1;
    }
    return 0;
}

static int is_sidebar_border(const mouse *m)
{
    if (m->x >= data.sidebar.border_btn.x_min &&
        m->x <= data.sidebar.border_btn.x_max &&
        m->y >= data.sidebar.border_btn.y_min &&
        m->y <= data.sidebar.border_btn.y_max) {
        return 1;
    }
    return 0;
}

static int is_map(const mouse *m)
{
    if (m->x >= data.x_min + WIDTH_BORDER &&
        m->x < data.sidebar.x_min &&
        m->y >= data.y_min + WIDTH_BORDER &&
        m->y < data.y_max - BOTTOM_PANEL_HEIGHT - WIDTH_BORDER) {
        return 1;
    }
    return 0;
}

static int is_outside_map(int x, int y)
{
    return (x < data.x_min + 16 || x >= data.sidebar.x_min ||
        y < data.y_min + 16 || y >= data.y_max - BOTTOM_PANEL_HEIGHT);
}

static void handle_sidebar_border(const mouse *m)
{
    // Set hover state
    data.sidebar.border_btn.is_hovered = is_sidebar_border(m);

    // Early exit if mouse not on sidebar border
    if (!data.sidebar.border_btn.is_hovered) {
        return;
    }

    data.hovered_object = 0; //clear hovers from sidebar

    // Handle expand/collapse toggle on left mouse release
    if (m->left.went_up) {
        if (data.sidebar.border_btn.is_collapsed) {
            sidebar_expand();
        } else {
            data.sidebar.previous_width = data.sidebar.width_percent;
            data.sidebar.dragging_width = data.sidebar.width_percent;
            data.sidebar.dragging = 1;
        }
    }
}

static void on_sidebar_city_click(const grid_box_item *item)
{
    if (data.hovered_resource_button != NO_POSITION) {    // Priority: resource buttons take precedence
        return;
    }
    int index = item->index;    // Get actual index
    if (index < 0 || index >= sidebar_city_count) return;
    sidebar_city_entry *entry = &sidebar_cities[index];
    empire_city *city = empire_city_get(entry->city_id);

    if (!city) return;
    data.selected_city = entry->city_id;
    empire_select_object_by_id(city->empire_object_id);
    grid_box_request_refresh(&sidebar_grid_box);
    window_invalidate();
}

void handle_sidebar_dragging(const mouse *m)
{
    if (m->left.went_up) {
        data.sidebar.dragging = 0; // stopped dragging
        if (data.sidebar.dragging_width <= 5) {
            sidebar_collapse();
        } else {
            data.sidebar.width_percent = data.sidebar.dragging_width; // save the width percent
            config_set(CONFIG_UI_EMPIRE_SIDEBAR_WIDTH, data.sidebar.width_percent);
        }
        return;
    }
    if (m->right.went_up) {
        data.sidebar.dragging = 0;
        data.sidebar.width_percent = data.sidebar.previous_width;
        window_invalidate(); // reset to previous width
        return;
    }
    const int map_draw_x_max = data.x_max - WIDTH_BORDER;                 // right edge of usable map
    const int map_draw_x_min = data.x_min + WIDTH_BORDER;                 // left edge of usable map

    // Ignore if mouse isn't over the map horizontally
    if (m->x < map_draw_x_min || m->x > map_draw_x_max || data.usable_map_width <= 0) {
        return;
    }

    // Percent position measured FROM THE RIGHT EDGE (0 at right edge, 100 at left edge)
    const int dist_from_right_px = map_draw_x_max - m->x;
    int mouse_percent_from_right = (dist_from_right_px * 100) / data.usable_map_width;

    // Snap to 2% strips (each strip = 2% of map width)
    const int strip = 2;
    int strip_index = mouse_percent_from_right / strip;
    if (strip_index < 0) strip_index = 0;
    int new_width = (strip_index * strip) + 1; // +1 to center in strip
    if (new_width < 10) {
        data.sidebar.dragging_width = 0;      // collapse preview
    } else if (new_width < 20) {
        data.sidebar.dragging_width = 20;     // minimum visible width (20%)
    } else if (new_width > 70) {
        data.sidebar.dragging_width = 70;     // maximum width (70%)
    } else {
        data.sidebar.dragging_width = new_width;
    }

    // Immediate layout update for live feedback
    int raw = (data.usable_map_width * data.sidebar.dragging_width) / 100;
    data.sidebar.width =
        ((raw + (BLOCK_SIZE / 2)) / BLOCK_SIZE) * BLOCK_SIZE
        + data.sidebar.margin_left + data.sidebar.margin_right;

    data.sidebar.x_max = map_draw_x_max;
    data.sidebar.x_min = data.sidebar.x_max - data.sidebar.width;
}


static void handle_input(const mouse *m, const hotkeys *h)
{
    pixel_offset position;
    if (data.sidebar.dragging) {
        handle_sidebar_dragging(m);
        return; //block other input handling if the sidebar is being dragged
    }
    if (scroll_get_delta(m, &position, SCROLL_TYPE_EMPIRE)) {
        empire_scroll_map(position.x, position.y);
    }

    // Only let the grid‐box process clicks if the sidebar is actually expanded:
    if (!data.sidebar.border_btn.is_collapsed) {
        if (window_empire_sidebar_sort_handle_expanding_buttons_input(m)) {
            return; //block other input handling if the expanding buttons are active
        }
        grid_box_handle_input(&sidebar_grid_box, m, 1);

    }

    if (m->is_touch) {
        const touch *t = touch_get_earliest();
        if (!is_outside_map(t->current_point.x, t->current_point.y) && !is_sidebar(m)) { // disable dragging on sidebar
            if (t->has_started) {
                data.is_scrolling = 1;
                scroll_drag_start(1);
            }
        }
        if (t->has_ended) {
            data.is_scrolling = 0;
            data.finished_scroll = !touch_was_click(t);
            scroll_drag_end();
        }
    }
    data.focus_button_id = 0;
    data.focus_resource = 0;
    unsigned int button_id;
    image_buttons_handle_mouse(m, data.panel.x_min + 20, data.y_max - 44, image_button_help, 1, &button_id);
    if (button_id) {
        data.focus_button_id = 1;
    }
    image_buttons_handle_mouse(m, data.panel.x_max - 44, data.y_max - 44, image_button_return_to_city, 1, &button_id);
    if (button_id) {
        data.focus_button_id = 2;
    }
    image_buttons_handle_mouse(m, data.panel.x_max - 44, data.y_max - 100, image_button_advisor, 1, &button_id);
    if (button_id) {
        data.focus_button_id = 3;
    }
    image_buttons_handle_mouse(m, data.panel.x_min + 24, data.y_max - 100, image_button_show_prices, 1, &button_id);
    if (button_id) {
        data.focus_button_id = 4;
    }


    button_id = 0;
    determine_selected_object(m);
    handle_sidebar_border(m);
    process_selection();
    int selected_object = empire_selected_object();
    data.hovered_resource_button = NO_POSITION;
    for (int i = 0; i < resource_button_count; i++) { //moved out of the 'selected object is empire city' to process all resource buttons
        const resource_button *btn = &resource_buttons[i];
        if (m->x >= btn->x && m->x < btn->x + btn->width &&
            m->y >= btn->y && m->y < btn->y + btn->height) {
            data.hovered_resource_button = i;
            data.focus_resource = btn->res;
            if (m->left.went_up && btn->do_highlight) { //do_highlight distinguishes closed from open routes - dont show resource window if closed route resource is clicked
                button_show_resource_window(i);
                return;
            }

        }

    }
    data.selected_button = NO_POSITION;
    for (int i = 0; i < trade_open_button_count; i++) {
        const trade_open_button *btn = &trade_open_buttons[i];

        if (m->x >= btn->x && m->x < btn->x + btn->width &&
            m->y >= btn->y && m->y < btn->y + btn->height) {
            data.selected_button = i;
            if (m->left.went_up) {

                button_open_trade_by_route(btn->route_id);  // <-- Trigger popup
                data.selected_button = NO_POSITION; //reset to get rid of the highlight
            }
            break;  // Only process one button at a time
        }
    }

    if (selected_object) {

        const empire_object *obj = empire_object_get(selected_object - 1);
        // allow de-selection only for objects that are currently selected/drawn, otherwise exit empire map
        if (input_go_back_requested(m, h)) {

            switch (obj->type) {
                case EMPIRE_OBJECT_CITY:

                    empire_clear_selected_object();
                    window_invalidate();
                    break;
                case EMPIRE_OBJECT_ROMAN_ARMY:

                    if (city_military_distant_battle_roman_army_is_traveling()) {
                        if (city_military_distant_battle_roman_months_traveled() == obj->distant_battle_travel_months) {
                            empire_clear_selected_object();
                            window_invalidate();
                        }
                    }
                    break;
                case EMPIRE_OBJECT_ENEMY_ARMY:
                    if (city_military_months_until_distant_battle() > 0) {
                        if (city_military_distant_battle_enemy_months_traveled() == obj->distant_battle_travel_months) {
                            empire_clear_selected_object();
                            window_invalidate();
                        }
                    }
                    break;
                default:
                    window_city_show();
                    break;
            }
        }

    } else {
        if (m->right.went_down) {
            scroll_drag_start(0);
        }
        if (m->right.went_up) {
            int has_scrolled = scroll_drag_end();
            if (!has_scrolled && input_go_back_requested(m, h)) {
                window_city_show();
            }
        }
        if (h->escape_pressed) { // handle escape
            window_city_show();
        }
    }
}

static void get_tooltip_trade_route_type(tooltip_context *c)
{
    int selected_object = empire_selected_object();
    if (!selected_object || empire_object_get(selected_object - 1)->type != EMPIRE_OBJECT_CITY) {
        return;
    }

    data.selected_city = empire_city_get_for_object(selected_object - 1);
    const empire_city *city = empire_city_get(data.selected_city);
    if (city->type != EMPIRE_CITY_TRADE || city->is_open) {
        return;
    }

    int x_offset = (data.panel.x_min + data.panel.x_max + 355) / 2;
    int y_offset = data.y_max - 41;
    int y_offset_max = y_offset + 22 - 2 * city->is_sea_trade;
    if (c->mouse_x >= x_offset && c->mouse_x < x_offset + 32 &&
        c->mouse_y >= y_offset && c->mouse_y < y_offset_max) {
        c->type = TOOLTIP_BUTTON;
        c->text_group = 44;
        c->text_id = 28 + city->is_sea_trade;
    }
}

static void get_tooltip(tooltip_context *c)
{
    int resource = data.focus_resource;
    if (resource) {
        c->type = TOOLTIP_BUTTON;
        c->precomposed_text = resource_get_data(resource)->text;
    } else if (data.focus_button_id) {
        c->type = TOOLTIP_BUTTON;
        switch (data.focus_button_id) {
            case 1: c->text_id = 1; break;
            case 2: c->text_id = 2; break;
            case 3: c->text_id = 69; break;
            case 4:
                c->text_group = 54;
                c->text_id = 2;
                break;
        }
    } else if (data.sidebar.border_btn.is_hovered) {
        c->type = TOOLTIP_BUTTON;
        c->text_group = CUSTOM_TRANSLATION;
        c->text_id = TR_TOOLTIP_CHANGE_SIDEBAR_WIDTH;
    } else if (window_empire_sidebar_sort_get_sorting_arrow_focused()) {
        c->type = TOOLTIP_BUTTON;
        c->text_group = CUSTOM_TRANSLATION;
        c->text_id = window_empire_sidebar_sort_get_sorting_arrow_is_down() ? TR_TOOLTIP_DESCENDING_ORDER : TR_TOOLTIP_ASCENDING_ORDER;
    } else {
        get_tooltip_trade_route_type(c);
    }
}
// -------------------------------------------------------------------------------------------------------
//                                              BUTTON HANDLERS     
// -------------------------------------------------------------------------------------------------------

static void button_help(int param1, int param2)
{
    window_message_dialog_show(MESSAGE_DIALOG_EMPIRE_MAP, 0);
}

static void button_return_to_city(int param1, int param2)
{
    window_city_show();
}
static void sorting_order(int is_down, int param2)
{
    window_empire_sidebar_sort_set_sorting_reversed(is_down);
}
static void button_advisor(int advisor, int param2)
{
    window_advisors_show_advisor(advisor);
}

static void button_show_prices(int param1, int param2)
{
    window_trade_prices_show(0, 0, screen_width(), screen_height());
}

static void button_show_resource_window(int resource_button_index)
{
    resource_button *btn = &resource_buttons[resource_button_index];
    window_resource_settings_show(btn->res);
}

static void confirmed_open_trade_by_route(int accepted, int checked)
{
    if (accepted) {
        int city_id = empire_city_get_for_trade_route(data.selected_trade_route);
        empire_city_open_trade(city_id, 1);
        building_menu_update();
        window_trade_opened_show(city_id);
    }
}

static void button_open_trade_by_route(int route_id)
{
    data.selected_trade_route = route_id;
    window_popup_dialog_show(POPUP_DIALOG_OPEN_TRADE, confirmed_open_trade_by_route, 2);
}

void register_resource_button(int x, int y, int width, int height, resource_type r, int highlight)
{
    if (resource_button_count >= MAX_RESOURCE_BUTTONS) return;
    resource_buttons[resource_button_count++] = (resource_button) { x, y, width, height, r, highlight };
}

void register_open_trade_button(int x, int y, int width, int height, int route_id, int highlight)
{
    if (trade_open_button_count >= MAX_TRADE_OPEN_BUTTONS) return;
    trade_open_buttons[trade_open_button_count++] = (trade_open_button) { x, y, width, height, route_id, highlight };
}


// -------------------------------------------------------------------------------------------------------
//                                              WINDOW SHOW 
// -------------------------------------------------------------------------------------------------------

void window_empire_show(void)
{

    init();
    setup_sidebar();
    setup_sidebar_gridbox();
    grid_box_init(&sidebar_grid_box, sidebar_city_count);
    window_type window = {
        WINDOW_EMPIRE,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}

int window_empire_is_dragging_sidebar(void)
{
    return data.sidebar.dragging;
}

void window_empire_show_checked(void)
{
    tutorial_availability avail = tutorial_advisor_empire_availability();
    if (avail == AVAILABLE) {
        window_empire_show();
    } else {
        city_warning_show(avail == NOT_AVAILABLE ? WARNING_NOT_AVAILABLE : WARNING_NOT_AVAILABLE_YET, NEW_WARNING_SLOT);
    }
}
