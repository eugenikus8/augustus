#ifndef GRAPHICS_COLOR_H
#define GRAPHICS_COLOR_H

#include <stdint.h>

typedef uint32_t color_t;

#define COLOR_BLACK 0xff000000
#define COLOR_BLUE 0xff0055ff
#define COLOR_RED 0xffff0000
#define COLOR_WHITE 0xffffffff

#define COLOR_GRID 0xff180800

#define COLOR_SG2_TRANSPARENT 0xfff700ff
#define COLOR_TOOLTIP 0xff424242
#define COLOR_SIDEBAR 0xffbdb592

#define COLOR_BORDER_GREEN 0xfffae094   //light yellow
#define COLOR_BORDER_RED 0xffac5446

#define COLOR_FONT_RED COLOR_RED
#define COLOR_FONT_BLUE 0xff0055ff
#define COLOR_FONT_YELLOW 0xffe7e75a
#define COLOR_FONT_ORANGE 0xffff5a08
#define COLOR_FONT_ORANGE_LIGHT 0xffffa500
#define COLOR_FONT_LIGHT_GRAY 0xffb3b3b3
#define COLOR_FONT_GRAY 0xff888888
#define COLOR_FONT_PLAIN 0xff180800
#define COLOR_FONT_GREEN 0xff00cc00

#define COLOR_INSET_LIGHT 0xffffffff
#define COLOR_INSET_DARK 0xff848484

#define COLOR_RISK_ICON_BORDER_DARK 0xf0000000
#define COLOR_RISK_ICON_BORDER_LIGHT 0xf0ffffff
#define COLOR_RISK_ICON_LOW 0xffcaae6f
#define COLOR_RISK_ICON_MEDIUM 0xffe7b951
#define COLOR_RISK_ICON_HIGH 0xfff7a142
#define COLOR_RISK_ICON_EXTREME 0xffff603a

#define COLOR_MAINTAIN_ICON 0xfffbf0f5

#define COLOR_MASK_NONE 0xffffffff
#define COLOR_MASK_RED 0xffff0818
#define COLOR_MASK_GREEN 0xff18ff18
#define COLOR_MASK_PURPLE 0xff7f0000
#define COLOR_MASK_DARK_GREEN 0xff005100
#define COLOR_MASK_DARK_BLUE 0x66001199
#define COLOR_MASK_BLUE 0x663377ff
#define COLOR_MASK_GREY 0x66aaaaaa
#define COLOR_MASK_PINK 0x99fa94ff
#define COLOR_MASK_LEGION_HIGHLIGHT 0xffff6633
#define COLOR_MASK_FOOTPRINT_GHOST 0x22ffffff
#define COLOR_MASK_BUILDING_GHOST 0xa7ffffff
#define COLOR_MASK_BUILDING_GHOST_RED 0xa7ff8878

#define COLOR_MASK_ORANGE_GOLD 0x66ffcc33
#define COLOR_MASK_AMBER 0x66ffb300  
#define COLOR_MASK_DIMMED_RED 0x66ff4444 
#define COLOR_MASK_LIME_GREEN 0x6600ff00 
#define COLOR_MASK_COBALT_BLUE 0x660066ff  
#define COLOR_MASK_TEAL 0x6600ffff  
#define COLOR_MASK_DIMMED_PURPLE 0x669933cc  
#define COLOR_MASK_SOFT_WHITE 0x66ffffff  
#define COLOR_MASK_IMPERIAL_RED 0x66a00000  
#define COLOR_MASK_OLIVE_GREEN 0x663a5f00

#define COLOR_MASK_TYRIAN_PURPLE 0xff66023C
#define COLOR_MASK_LIGHT_BLUE 0xff3552ff
#define COLOR_MASK_SKY_BLUE 0xff90aaff
#define COLOR_MASK_POSITIVE_RANGE 0x80005100    //50% transparent DARK_GREEN
#define COLOR_MASK_NEGATIVE_RANGE 0x40ff0000    //25% transparent COLOR_RED

#define COLOR_MINIMAP_VIEWPORT 0xffe7e75a
#define COLOR_MINIMAP_DARK 0xff424242
#define COLOR_MINIMAP_LIGHT 0xffc6c6c6
#define COLOR_MINIMAP_SOLDIER 0xfff70000
#define COLOR_MINIMAP_SELECTED_SOLDIER 0xffffffff
#define COLOR_MINIMAP_ENEMY_CENTRAL 0xff7b0000
#define COLOR_MINIMAP_ENEMY_NORTHERN 0xff1800ff
#define COLOR_MINIMAP_ENEMY_DESERT 0xff08007b
#define COLOR_MINIMAP_WOLF COLOR_FONT_LIGHT_GRAY
#define COLOR_MINIMAP_TRADE_CARAVAN COLOR_FONT_ORANGE_LIGHT
#define COLOR_MINIMAP_TRADE_SHIP COLOR_FONT_ORANGE_LIGHT

#define COLOR_OVERLAY_NEUTRAL 0xccffffff
#define COLOR_OVERLAY_NEGATIVE_STEP 0x00040505
#define COLOR_OVERLAY_POSITIVE_STEP 0x00050504

#define COLOR_MOUSE_DARK_GRAY 0xff3f3f3f
#define COLOR_MOUSE_MEDIUM_GRAY 0xff737373
#define COLOR_MOUSE_LIGHT_GRAY 0xffb3b3b3

#define ALPHA_OPAQUE 0xff000000
#define ALPHA_FONT_SEMI_TRANSPARENT 0x99ffffff
#define ALPHA_MASK_SEMI_TRANSPARENT 0x48ffffff
#define ALPHA_TRANSPARENT 0x00000000

#define COLOR_BITSHIFT_ALPHA 24
#define COLOR_BITSHIFT_RED 16
#define COLOR_BITSHIFT_GREEN 8
#define COLOR_BITSHIFT_BLUE 0

#define COLOR_WEATHER_SAND 0xffe6b85c
#define COLOR_WEATHER_SNOW 0xffddeeff
#define COLOR_WEATHER_RAIN 0xff000000
#define COLOR_WEATHER_SNOWFLAKE 0x88ffffff
#define COLOR_WEATHER_SAND_PARTICLE 0xe1a65e2e
#define COLOR_WEATHER_DROPS 0xccffffff

#define COLOR_CHANNEL_ALPHA 0xff000000
#define COLOR_CHANNEL_RED 0x00ff0000
#define COLOR_CHANNEL_GREEN 0x0000ff00
#define COLOR_CHANNEL_BLUE 0x000000ff
#define COLOR_CHANNEL_RGB (COLOR_CHANNEL_RED | COLOR_CHANNEL_GREEN | COLOR_CHANNEL_BLUE)
#define COLOR_CHANNEL_RB (COLOR_CHANNEL_RED | COLOR_CHANNEL_BLUE)

#define COLOR_COMPONENT(c, shift) (((c) >> (shift)) & 0xff)

// Note: for the blending functions to work properly, variables must be of type color_t

#define COLOR_MIX_ALPHA(alpha_src, alpha_dst) ((alpha_src) + (((alpha_dst) * (0xff - (alpha_src))) >> 8))
#define COLOR_BLEND_CHANNEL_TO_OPAQUE(src, dst, alpha, channel) \
        (((((src) & (channel)) * (alpha) + ((dst) & (channel)) * (0xff - (alpha))) >> 8) & (channel))
#define COLOR_BLEND_CHANNEL(src, dst, alpha_src, alpha_dst, alpha_mix, channel) \
        ((((((src) & (channel)) * (alpha_src) + ((((dst) & (channel)) * (0xff - (alpha_src))) >> 8) * (alpha_dst))) / (alpha_mix)) & (channel))

#define COLOR_BLEND_ALPHA_TO_OPAQUE(src, dst, alpha) \
        (ALPHA_OPAQUE | \
        COLOR_BLEND_CHANNEL_TO_OPAQUE(src, dst, alpha, COLOR_CHANNEL_RED | COLOR_CHANNEL_BLUE) | \
        COLOR_BLEND_CHANNEL_TO_OPAQUE(src, dst, alpha, COLOR_CHANNEL_GREEN))

#define COLOR_BLEND_ALPHAS(src, dst, alpha_src, alpha_dst, alpha_mix) \
        ((alpha_mix) << COLOR_BITSHIFT_ALPHA | \
        COLOR_BLEND_CHANNEL(src, dst, alpha_src, alpha_dst, alpha_mix, COLOR_CHANNEL_RED) | \
        COLOR_BLEND_CHANNEL(src, dst, alpha_src, alpha_dst, alpha_mix, COLOR_CHANNEL_GREEN) | \
        COLOR_BLEND_CHANNEL(src, dst, alpha_src, alpha_dst, alpha_mix, COLOR_CHANNEL_BLUE))

#endif // GRAPHICS_COLOR_H
