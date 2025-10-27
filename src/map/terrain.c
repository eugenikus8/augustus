#include "terrain.h"

#include <string.h>
#include "building/building.h"
#include "city/map.h"
#include "core/image.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/ring.h"
#include "map/routing.h"
#include "map/sprite.h"

static grid_u32 terrain_grid;
static grid_u32 terrain_grid_backup;


const terrain_flags_array *map_terrain_to_array(int grid_offset)
{
    static const char *names[TERRAIN_NUM_FLAGS] = {
        "TREE", "ROCK", "WATER", "BUILDING", "SHRUB", "GARDEN", "ROAD", "RESERVOIR_R", "AQUEDUCT", "ELEVATION",
        "ACCESS_RAMP", "MEADOW", "RUBBLE", "FOUNTAIN_R", "WALL", "GATEHOUSE", "ORG_TREE", "HIGHWAY1", "HIGHWAY2",
        "HIGHWAY3", "HIGHWAY4"
    };
    static terrain_flags_array result;
    unsigned int terrain_value = terrain_grid.items[grid_offset];

    // Reset everything to zero to avoid stale data
    memset(&result, 0, sizeof(result));

    if (terrain_value == 0) {
        // No bits set: represent as CLEAR
        strncpy(result.key[0], "CLEAR", KEY_MAX_LEN - 1);
        result.key[0][KEY_MAX_LEN - 1] = '\0';
        result.count = 1;
        return &result;
    }

    for (int i = 0; i < TERRAIN_NUM_FLAGS; ++i) {
        if ((terrain_value >> i) & 1) {
            result.bits[i] = 1;

            strncpy(result.key[result.count], names[i], KEY_MAX_LEN - 1);
            result.key[result.count][KEY_MAX_LEN - 1] = '\0';

            result.count++;
        }
    }

    return &result;
}

int map_terrain_is(int grid_offset, int terrain)
{
    return map_grid_is_valid_offset(grid_offset) && terrain_grid.items[grid_offset] & terrain;
}

int map_terrain_is_roadblock(int grid_offset)
{
    int terrain = map_terrain_get(grid_offset);
    return (terrain & TERRAIN_BUILDING) && (terrain & TERRAIN_ROAD);
}

int map_terrain_is_superset(int grid_offset, unsigned int terrain_sum)
{
    return map_grid_is_valid_offset(grid_offset) && ((terrain_grid.items[grid_offset] & terrain_sum) == terrain_sum);
}

int map_terrain_get(int grid_offset)
{
    return terrain_grid.items[grid_offset];
}

int map_terrain_get_from_buffer_16(buffer *buf, int grid_offset)
{
    buffer_set(buf, grid_offset * sizeof(uint16_t));
    return buffer_read_u16(buf);
}

int map_terrain_get_from_buffer_32(buffer *buf, int grid_offset)
{
    buffer_set(buf, grid_offset * sizeof(uint32_t));
    return buffer_read_u32(buf);
}

void map_terrain_set(int grid_offset, int terrain)
{
    terrain_grid.items[grid_offset] = terrain;
}

void map_terrain_add(int grid_offset, int terrain)
{
    terrain_grid.items[grid_offset] |= terrain;
}

void map_terrain_remove(int grid_offset, int terrain)
{
    terrain_grid.items[grid_offset] &= ~terrain;
}

void map_terrain_add_with_radius(int x, int y, int size, int radius, int terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            map_terrain_add(map_grid_offset(xx, yy), terrain);
        }
    }
}

void map_terrain_remove_with_radius(int x, int y, int size, int radius, int terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            map_terrain_remove(map_grid_offset(xx, yy), terrain);
        }
    }
}

void map_terrain_remove_all(int terrain)
{
    map_grid_and_u32(terrain_grid.items, ~terrain);
}

int map_terrain_count_directly_adjacent_with_type(int grid_offset, int terrain)
{
    int count = 0;
    if (map_terrain_is(grid_offset + map_grid_delta(0, -1), terrain)) {
        count++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(1, 0), terrain)) {
        count++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(0, 1), terrain)) {
        count++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(-1, 0), terrain)) {
        count++;
    }
    return count;
}

int map_terrain_count_directly_adjacent_with_types(int grid_offset, int terrain_sum)
{
    int count = 0;
    if (map_terrain_is_superset(grid_offset + map_grid_delta(0, -1), terrain_sum)) {
        count++;
    }
    if (map_terrain_is_superset(grid_offset + map_grid_delta(1, 0), terrain_sum)) {
        count++;
    }
    if (map_terrain_is_superset(grid_offset + map_grid_delta(0, 1), terrain_sum)) {
        count++;
    }
    if (map_terrain_is_superset(grid_offset + map_grid_delta(-1, 0), terrain_sum)) {
        count++;
    }
    return count;
}


int map_terrain_count_diagonally_adjacent_with_type(int grid_offset, int terrain)
{
    int count = 0;
    if (map_terrain_is(grid_offset + map_grid_delta(1, -1), terrain)) {
        count++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(1, 1), terrain)) {
        count++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(-1, 1), terrain)) {
        count++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(-1, -1), terrain)) {
        count++;
    }
    return count;
}

int map_terrain_has_adjacent_x_with_type(int grid_offset, int terrain)
{
    if (map_terrain_is(grid_offset + map_grid_delta(0, -1), terrain) ||
        map_terrain_is(grid_offset + map_grid_delta(0, 1), terrain)) {
        return 1;
    }
    return 0;
}

int map_terrain_has_adjacent_y_with_type(int grid_offset, int terrain)
{
    if (map_terrain_is(grid_offset + map_grid_delta(-1, 0), terrain) ||
        map_terrain_is(grid_offset + map_grid_delta(1, 0), terrain)) {
        return 1;
    }
    return 0;
}

int map_terrain_exists_tile_in_area_with_type(int x, int y, int size, int terrain)
{
    for (int yy = y; yy < y + size; yy++) {
        for (int xx = x; xx < x + size; xx++) {
            if (map_grid_is_inside(xx, yy, 1) && terrain_grid.items[map_grid_offset(xx, yy)] & terrain) {
                return 1;
            }
        }
    }
    return 0;
}

int map_terrain_exists_tile_in_radius_with_type(int x, int y, int size, int radius, int terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (map_terrain_is(map_grid_offset(xx, yy), terrain)) {
                return 1;
            }
        }
    }
    return 0;
}

int map_terrain_exists_rock_in_radius(int x, int y, int size, int radius)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    int entry_flag_offset = map_grid_offset(city_map_entry_flag()->x, city_map_entry_flag()->y);
    int exit_flag_offset = map_grid_offset(city_map_exit_flag()->x, city_map_exit_flag()->y);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int offset = map_grid_offset(xx, yy);
            if (offset == entry_flag_offset || offset == exit_flag_offset) {
                continue;
            }
            if (map_terrain_is(offset, TERRAIN_ROCK)) {
                return 1;
            }
        }
    }
    return 0;
}

int map_terrain_exists_clear_tile_in_radius(int x, int y, int size, int radius, int except_grid_offset,
    int *x_tile, int *y_tile)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (grid_offset != except_grid_offset && !(terrain_grid.items[grid_offset] & TERRAIN_NOT_CLEAR)) {
                *x_tile = xx;
                *y_tile = yy;
                return 1;
            }
        }
    }
    *x_tile = x_max;
    *y_tile = y_max;
    return 0;
}

int map_terrain_all_tiles_in_radius_are(int x, int y, int size, int radius, int terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (!map_terrain_is(map_grid_offset(xx, yy), terrain)) {
                return 0;
            }
        }
    }
    return 1;
}

int map_terrain_has_only_rocks_trees_in_ring(int x, int y, int distance)
{
    int start = map_ring_start(1, distance);
    int end = map_ring_end(1, distance);
    int base_offset = map_grid_offset(x, y);
    for (int i = start; i < end; i++) {
        const ring_tile *tile = map_ring_tile(i);
        if (map_ring_is_inside_map(x + tile->x, y + tile->y)) {
            if (!map_terrain_is(base_offset + tile->grid_offset,
                TERRAIN_ROCK | TERRAIN_TREE | TERRAIN_ORIGINALLY_TREE)) {
                return 0;
            }
        }
    }
    return 1;
}

int map_terrain_has_only_meadow_in_ring(int x, int y, int distance)
{
    int start = map_ring_start(1, distance);
    int end = map_ring_end(1, distance);
    int base_offset = map_grid_offset(x, y);
    for (int i = start; i < end; i++) {
        const ring_tile *tile = map_ring_tile(i);
        if (map_ring_is_inside_map(x + tile->x, y + tile->y)) {
            if (!map_terrain_is(base_offset + tile->grid_offset, TERRAIN_MEADOW)) {
                return 0;
            }
        }
    }
    return 1;
}

int map_terrain_is_adjacent_to_wall(int x, int y, int size)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        if (map_terrain_is(base_offset + *tile_delta, TERRAIN_WALL)) {
            return 1;
        }
    }
    return 0;
}

int map_terrain_is_adjacent_to_water(int x, int y, int size)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        if (map_terrain_is(base_offset + *tile_delta, TERRAIN_WATER)) {
            return 1;
        }
    }
    return 0;
}

int map_terrain_is_adjacent_to_open_water(int x, int y, int size)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        if (map_terrain_is(base_offset + *tile_delta, TERRAIN_WATER) &&
            map_routing_distance(base_offset + *tile_delta) > 0) {
            return 1;
        }
    }
    return 0;
}

int map_terrain_get_adjacent_road_or_clear_land(int x, int y, int size, int *x_tile, int *y_tile)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        int grid_offset = base_offset + *tile_delta;
        if (map_terrain_is(grid_offset, TERRAIN_ROAD) ||
            !map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
            *x_tile = map_grid_offset_to_x(grid_offset);
            *y_tile = map_grid_offset_to_y(grid_offset);
            return 1;
        }
    }
    return 0;
}

static void add_road_if_no_highway(int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        map_terrain_add(grid_offset, TERRAIN_ROAD);
    }
}

static void add_road_if_clear(int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
        map_terrain_add(grid_offset, TERRAIN_ROAD);
    }
}

void map_terrain_add_roadblock_road(int x, int y)
{
    // roads under roadblock
    map_terrain_add(map_grid_offset(x, y), TERRAIN_ROAD);
}

void map_terrain_add_gatehouse_roads(int x, int y, int orientation)
{
    // roads under gatehouse
    add_road_if_no_highway(map_grid_offset(x, y));
    add_road_if_no_highway(map_grid_offset(x + 1, y));
    add_road_if_no_highway(map_grid_offset(x, y + 1));
    add_road_if_no_highway(map_grid_offset(x + 1, y + 1));

    // free roads before/after gate
    if (orientation == 1) {
        add_road_if_clear(map_grid_offset(x, y - 1));
        add_road_if_clear(map_grid_offset(x + 1, y - 1));
        add_road_if_clear(map_grid_offset(x, y + 2));
        add_road_if_clear(map_grid_offset(x + 1, y + 2));
    } else if (orientation == 2) {
        add_road_if_clear(map_grid_offset(x - 1, y));
        add_road_if_clear(map_grid_offset(x - 1, y + 1));
        add_road_if_clear(map_grid_offset(x + 2, y));
        add_road_if_clear(map_grid_offset(x + 2, y + 1));
    }
}

void map_terrain_add_triumphal_arch_roads(int x, int y, int orientation)
{
    if (orientation == 1) {
        // road in the middle
        map_terrain_add(map_grid_offset(x + 1, y), TERRAIN_ROAD);
        map_terrain_add(map_grid_offset(x + 1, y + 1), TERRAIN_ROAD);
        map_terrain_add(map_grid_offset(x + 1, y + 2), TERRAIN_ROAD);
        // no roads on other tiles
        map_terrain_remove(map_grid_offset(x, y), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x, y + 1), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x, y + 2), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 2, y), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 2, y + 1), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 2, y + 2), TERRAIN_ROAD);
    } else if (orientation == 2) {
        // road in the middle
        map_terrain_add(map_grid_offset(x, y + 1), TERRAIN_ROAD);
        map_terrain_add(map_grid_offset(x + 1, y + 1), TERRAIN_ROAD);
        map_terrain_add(map_grid_offset(x + 2, y + 1), TERRAIN_ROAD);
        // no roads on other tiles
        map_terrain_remove(map_grid_offset(x, y), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 1, y), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 2, y), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x, y + 2), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 1, y + 2), TERRAIN_ROAD);
        map_terrain_remove(map_grid_offset(x + 2, y + 2), TERRAIN_ROAD);
    }
}

void map_terrain_backup(void)
{
    map_grid_copy_u32(terrain_grid.items, terrain_grid_backup.items);
}

void map_terrain_restore(void)
{
    map_grid_copy_u32(terrain_grid_backup.items, terrain_grid.items);
}

void map_terrain_clear(void)
{
    map_grid_clear_u32(terrain_grid.items);
}

void map_terrain_init_outside_map(void)
{
    int map_width, map_height;
    map_grid_size(&map_width, &map_height);
    int y_start = (GRID_SIZE - map_height) / 2;
    int x_start = (GRID_SIZE - map_width) / 2;
    for (int y = 0; y < GRID_SIZE; y++) {
        int y_outside_map = y < y_start || y >= y_start + map_height;
        for (int x = 0; x < GRID_SIZE; x++) {
            if (y_outside_map || x < x_start || x >= x_start + map_width) {
                terrain_grid.items[x + GRID_SIZE * y] = TERRAIN_MAP_EDGE;
            }
        }
    }
}

void map_terrain_save_state(buffer *buf)
{
    map_grid_save_state_u32(terrain_grid.items, buf);
}

void map_terrain_save_state_legacy(buffer *buf)
{
    map_grid_save_state_u32_to_u16(terrain_grid.items, buf);
}

static void determine_original_trees(buffer *images, int legacy_buffer)
{
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (terrain_grid.items[x + GRID_SIZE * y] & TERRAIN_TREE &&
                !(terrain_grid.items[x + GRID_SIZE * y] & TERRAIN_WATER)) {
                terrain_grid.items[x + GRID_SIZE * y] |= TERRAIN_ORIGINALLY_TREE;
                if (images) {
                    buffer_set(images, (x + GRID_SIZE * y) * (legacy_buffer ? 2 : 4));
                    int image_id = legacy_buffer ? buffer_read_u16(images) : buffer_read_u32(images);
                    int image_tree_group = image_group(GROUP_TERRAIN_TREE);
                    int ring;
                    if (image_id >= image_tree_group + 8 && image_id < image_tree_group + 16) {
                        ring = 1;
                    } else if (image_id >= image_tree_group + 16 && image_id < image_tree_group + 24) {
                        ring = 2;
                    } else if (image_id >= image_tree_group + 24 && image_id < image_tree_group + 32) {
                        ring = 3;
                    } else {
                        continue;
                    }
                    int start = map_ring_start(1, ring);
                    int end = map_ring_end(1, ring);
                    int base_offset = x + GRID_SIZE * y;
                    for (int i = start; i < end; i++) {
                        int current_offset = base_offset + map_ring_tile(i)->grid_offset;
                        if (map_grid_is_valid_offset(current_offset) &&
                            !map_terrain_is_superset(current_offset, TERRAIN_MAP_EDGE)) {
                            map_terrain_add(current_offset, TERRAIN_ORIGINALLY_TREE);
                        }
                    }
                }
            }
        }
    }
}

static int legacy_map_is_bridge(int grid_offset)
{
    //old way for checking for bridges - check if it's sprite, and check if it's on water
    //checking just for sprites is misleading, as on land buildings also have sprites - it's their animation frame
    return (map_sprite_bridge_at(grid_offset)) && map_terrain_is(grid_offset, TERRAIN_WATER);
}

int map_bridge_find_start_and_direction_legacy(int grid_offset, int *axis, int *axis_direction)
{
    if (!legacy_map_is_bridge(grid_offset)) {
        return -1;
    }

    static const int dirs[4][2] = {
        {  0, -1 }, // north
        { +1,  0 }, // east
        {  0, +1 }, // south
        { -1,  0 }  // west
    };
    // Scan in all 4 directions until we find a ramp
    for (int i = 0; i < 4; ++i) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        int delta = map_grid_delta(dx, dy);

        int current = grid_offset;
        while (legacy_map_is_bridge(current)) {
            int sprite = map_sprite_bridge_at(current);
            if (map_bridge_is_ramp_sprite(sprite)) {
                int next = current + delta;
                int next_sprite = map_sprite_bridge_at(next);

                if (map_bridge_is_ramp_sprite(next_sprite)) {
                    // If both are ramps, check for low bridge
                    if (sprite <= 6 && next_sprite <= 6) { // ship bridge sprites > 6
                        *axis = (dx != 0) ? 0 : 1;
                        *axis_direction = (dx + dy);
                        return current;
                    }
                    break; // invalid for ship bridge
                }

                if (next_sprite >= 5 && next_sprite <= 15 && !map_bridge_is_ramp_sprite(next_sprite)) {
                    *axis = (dx != 0) ? 0 : 1;
                    *axis_direction = (dx + dy);
                    return current;
                }
            }

            current -= delta;
        }
    }

    return -1; // No valid bridge start found
}

void map_terrain_migrate_old_bridges(void)
{
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_grid_is_valid_offset(grid_offset)) {
                continue;
            }
            if (legacy_map_is_bridge(grid_offset) && !map_is_bridge(grid_offset)) {
                // Find true start of the old bridge
                // Only process tiles that are part of a legacy bridge and haven't been upgraded yet 
                int axis, dir;
                int start = map_bridge_find_start_and_direction_legacy(grid_offset, &axis, &dir);
                if (start < 0) {
                    continue;
                }
                int delta = (axis == 0)
                    ? map_grid_delta(dir, 0)
                    : map_grid_delta(0, dir);

                int is_ship_bridge = map_sprite_bridge_at(start) > 6 ? 1 : 0;
                int bridge_type = is_ship_bridge ? BUILDING_SHIP_BRIDGE : BUILDING_LOW_BRIDGE;

                int start_x = map_grid_offset_to_x(start);
                int start_y = map_grid_offset_to_y(start);
                building *b = building_create(bridge_type, start_x, start_y);

                int current = start;
                while (legacy_map_is_bridge(current)) {
                    map_terrain_add(current, TERRAIN_BUILDING);
                    map_building_set(current, b->id);
                    current += delta;
                }
            }
        }
    }
}

void map_terrain_migrate_old_walls(void)
{
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_grid_is_valid_offset(grid_offset)) {
                continue;
            }
            if (map_terrain_is(grid_offset, TERRAIN_WALL) && !map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
                // Create wall building for each wall tile
                building *b = building_create(BUILDING_WALL, x, y);
                map_building_set(grid_offset, b->id);
                map_terrain_add(grid_offset, TERRAIN_BUILDING);
            }
        }
    }
}

void map_terrain_load_state(buffer *buf, int expanded_terrain_data, buffer *images, int legacy_image_buffer)
{
    if (expanded_terrain_data) {
        map_grid_load_state_u32(terrain_grid.items, buf);
    } else {
        map_grid_load_state_u16_to_u32(terrain_grid.items, buf);
    }
    determine_original_trees(images, legacy_image_buffer);
}
