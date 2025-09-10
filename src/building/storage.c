#include "storage.h"

#include "building/building.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "building/type.h"
#include "city/resource.h"
#include "core/array.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/log.h"
#include "game/resource.h"
#include "game/save_version.h"

#define STORAGE_ARRAY_SIZE_STEP 200

#define STORAGE_ORIGINAL_BUFFER_SIZE 32
#define STORAGE_STATIC_BUFFER_SIZE 10
#define STORAGE_CURRENT_BUFFER_SIZE (STORAGE_STATIC_BUFFER_SIZE + RESOURCE_MAX * 2)

static array(data_storage) storages;

static void storage_create(data_storage *storage, unsigned int position)
{
    storage->id = position;
}

static int storage_in_use(const data_storage *storage)
{
    return storage->in_use;
}

void building_storage_clear_all(void)
{
    if (!array_init(storages, STORAGE_ARRAY_SIZE_STEP, storage_create, storage_in_use) ||
        !array_next(storages)) { // Ignore first storage
        log_error("Unable to create storages. The game will likely crash.", 0, 0);
    }
}

int building_storage_get_array_size(void)
{
    return storages.size;
}

int building_storage_try_add_resource(building *b, int resource, int amount, int is_produced)
{
    if (b->type == BUILDING_GRANARY) {
        return building_granary_try_add_resource(b, resource, amount, is_produced, 1);
    } else if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_try_add_resource(b, resource, amount, 1);
    }
    return 0;
}

void building_storage_reset_building_ids(void)
{
    data_storage *storage;
    array_foreach(storages, storage)
    {
        storage->building_id = 0;
    }

    static const building_type types[] = { BUILDING_GRANARY, BUILDING_WAREHOUSE };

    for (int i = 0; i < 2; i++) {
        building_type type = types[i];
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_UNUSED) {
                continue;
            }
            if (b->storage_id) {
                if (array_item(storages, b->storage_id)->building_id) {
                    // storage is already connected to a building: corrupt, create new
                    b->storage_id = building_storage_create(b->id);
                } else {
                    array_item(storages, b->storage_id)->building_id = b->id;
                }
            }
        }
    }
}

int building_storage_create(int building_id)
{
    data_storage *storage;
    array_new_item_after_index(storages, 1, storage);
    if (!storage) {
        return 0;
    }
    storage->in_use = 1;
    storage->building_id = building_id;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        //default the storage quantity to max
        storage->storage.resource_state[r].quantity = BUILDING_STORAGE_QUANTITY_MAX;
    }

    if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
        building_storage_accept_none(storage->id);
    } else {
        building_storage_accept_all(storage->id);
    }
    return storage->id;
}

int building_storage_get_building_id(int storage_id)
{
    if (storage_id < 0 || storage_id >= storages.size) {
        return 0;
    }
    return array_item(storages, storage_id)->building_id;
}

int building_storage_restore(int storage_id)
{
    if (array_item(storages, storage_id)->in_use) {
        return 0;
    }
    array_item(storages, storage_id)->in_use = 1;
    if (storage_id >= storages.size) {
        storages.size = storage_id + 1;
    }
    return storage_id;
}

void building_storage_delete(int storage_id)
{
    array_item(storages, storage_id)->in_use = 0;
    array_trim(storages);
}

const building_storage *building_storage_get(int storage_id)
{
    return &array_item(storages, storage_id)->storage;
}

const data_storage *building_storage_get_array_entry(int storage_id)
{
    return array_item(storages, storage_id);
}

void building_storage_set_data(int storage_id, building_storage new_data)
{
    array_item(storages, storage_id)->storage = new_data;
}

void building_storage_toggle_empty_all(int storage_id)
{
    array_item(storages, storage_id)->storage.empty_all ^= 1;
}

int building_storage_get_empty_all(int building_id)
{
    building *b = building_get(building_id);
    int storage_id = b->storage_id;
    if (storage_id < 0 || storage_id >= storages.size) {
        return 0;
    }
    return array_item(storages, storage_id)->storage.empty_all;
}

int building_storage_count_stored_resource_types(int building_id)
{
    building *b = building_get(building_id);
    if (!b->storage_id) {
        return 0;
    }
    int stored_types_count = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (b->resources[r] > 0) {
            stored_types_count++;
        }
    }
    return stored_types_count;
}

int building_storage_get_amount(building *b, resource_type resource)
{
    if (b->type == BUILDING_GRANARY) {
        return building_granary_get_amount(b, resource);
    } else if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_get_amount(b, resource);
    }
    return 0;
}

int building_storage_get_storage_state_quantity(building *b, resource_type resource)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];
    return entry->quantity;
}

const building_storage_state building_storage_get_state(building *b, int resource, int relative)
{
    if (b->has_plague || b->state != BUILDING_STATE_IN_USE) {
        return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
    if (b->type != BUILDING_GRANARY && b->type != BUILDING_WAREHOUSE) { //safeguard
        return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (!relative) {
        // If relative is 0, return raw state without checking amounts
        return entry->state;
    }
    int amount = (b->type == BUILDING_WAREHOUSE) ?
        building_warehouse_get_amount(b, resource) : building_granary_get_amount(b, resource);

    switch (entry->state) {
        case BUILDING_STORAGE_STATE_ACCEPTING:
            if (amount < entry->quantity) {
                return BUILDING_STORAGE_STATE_ACCEPTING;
            }
            break;

        case BUILDING_STORAGE_STATE_GETTING:
            if (amount <= entry->quantity) {
                return BUILDING_STORAGE_STATE_GETTING;
            }
            break;

        case BUILDING_STORAGE_STATE_MAINTAINING:
            if (amount <= entry->quantity) {
                return BUILDING_STORAGE_STATE_MAINTAINING;
            }
            break;

        default:
            break;
    }
    return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
}

resource_type building_storage_get_highest_quantity_resource(building *b)
{
    unsigned char i;
    unsigned char highest_resource = RESOURCE_NONE;
    if (b->type == BUILDING_WAREHOUSE) {
        building_warehouse_recount_resources(b);
    }
    for (i = RESOURCE_NONE + 1; i < RESOURCE_MAX; i++) { //not interested in RESOURCE_NONE
        if (b->resources[i] > highest_resource) {
            highest_resource = i;
        }
    }
    return highest_resource;
}

void building_storage_cycle_resource_state(int storage_id, resource_type resource_id)
{
    resource_storage_entry *entry = &array_item(storages, storage_id)->storage.resource_state[resource_id];

    switch (entry->state) {
        case BUILDING_STORAGE_STATE_ACCEPTING:
            entry->state = BUILDING_STORAGE_STATE_GETTING;
            break;
        case BUILDING_STORAGE_STATE_GETTING:
            entry->state = BUILDING_STORAGE_STATE_MAINTAINING;
            break;
        case BUILDING_STORAGE_STATE_MAINTAINING:
            entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
            break;
        case BUILDING_STORAGE_STATE_NOT_ACCEPTING:
            entry->state = BUILDING_STORAGE_STATE_ACCEPTING;
            break;
        default:
            entry->state = BUILDING_STORAGE_STATE_ACCEPTING;
            break;
    }
}

void building_storage_toggle_permission(building_storage_permission_states p, building *b)
{
    int permission_bit = 1 << p;
    array_item(storages, b->storage_id)->storage.permissions ^= permission_bit;
}

int building_storage_get_permission(building_storage_permission_states p, building *b)
{
    const building_storage *s = building_storage_get(b->storage_id);
    int permission_bit = 1 << p;
    return !(s->permissions & permission_bit);
}

void building_storage_set_permission(building_storage_permission_states p, building *b, int enable)
{
    int permission_bit = 1 << p;
    int *permissions = &array_item(storages, b->storage_id)->storage.permissions;

    if (enable) {
        *permissions |= permission_bit;
    } else {
        *permissions &= ~permission_bit;
    }
}

void building_storage_cycle_partial_resource_state(int storage_id, resource_type resource_id, int reverse_order)
{
    resource_storage_entry *entry = &array_item(storages, storage_id)->storage.resource_state[resource_id];

    if (entry->state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return; // not accepting is always MAX
    }

    int step = config_get(CONFIG_GP_STORAGE_INCREMENT_4) ? 4 : 8;

    int current = entry->quantity;

    // If current quantity is out of bounds, reset it.
    if (current > BUILDING_STORAGE_QUANTITY_MAX || current < step) {
        entry->quantity = BUILDING_STORAGE_QUANTITY_MAX;
        return;
    }

    if (reverse_order) {
        current -= step;
        if (current < step) {
            current = BUILDING_STORAGE_QUANTITY_MAX;
        }
    } else {
        current += step;
        if (current > BUILDING_STORAGE_QUANTITY_MAX) {
            current = step;
        }
    }

    entry->quantity = (building_storage_quantity) current;
}

int building_storage_accepts_storage(building *b, resource_type resource, int *understaffed)
{
    if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_accepts_storage(b, resource, understaffed);
    } else if (b->type == BUILDING_GRANARY) {
        return building_granary_accepts_storage(b, resource, understaffed);
    }
}

void building_storage_accept_none(int storage_id)
{
    data_storage *s = array_item(storages, storage_id);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        s->storage.resource_state[r].state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
}

void building_storage_accept_all(int storage_id)
{
    data_storage *s = array_item(storages, storage_id);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        s->storage.resource_state[r].state = BUILDING_STORAGE_STATE_ACCEPTING;
    }
}

int building_storage_check_if_accepts_nothing(int storage_id)
{
    data_storage *s = array_item(storages, storage_id);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (s->storage.resource_state[r].state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
            return 0;
        }
    }
    return 1;
}


int building_storage_resource_max_storable(building *b, resource_type resource_id)
{
    if (b->type == BUILDING_GRANARY && resource_id >= RESOURCE_MIN_NON_FOOD) {
        return 0;
    }

    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource_id];

    if (entry->state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return entry->quantity;
    }

    return 0;
}

void decode_legacy_storage_state(uint8_t legacy, resource_storage_entry *entry)
{
    // helper function for the legacy 12 storage states
    switch (legacy) {
        case 0: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 1: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 2: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 3: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 4: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 5: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 6: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 7: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        case 8: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        case 9: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 10: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 11: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        default:
            entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
            entry->quantity = BUILDING_STORAGE_QUANTITY_MAX;
            break;
    }
}


void building_storage_save_state(buffer *buf)
{
    int buf_size = 4 + storages.size * STORAGE_CURRENT_BUFFER_SIZE;
    uint8_t *buf_data = malloc(buf_size);
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, STORAGE_CURRENT_BUFFER_SIZE);

    data_storage *s;
    array_foreach(storages, s)
    {
        buffer_write_i32(buf, s->storage.permissions); // Originally unused
        buffer_write_i32(buf, s->building_id);
        buffer_write_u8(buf, (uint8_t) s->in_use);
        buffer_write_u8(buf, (uint8_t) s->storage.empty_all);

        for (int r = 0; r < RESOURCE_MAX; r++) {
            buffer_write_u8(buf, (uint8_t) s->storage.resource_state[r].state);
            buffer_write_u8(buf, (uint8_t) s->storage.resource_state[r].quantity);
        }
    }
}

building_storage_permission_states building_storage_get_permission_from_building_type(building_type type)
{
    switch (type) {
        case BUILDING_MARKET:
            return BUILDING_STORAGE_PERMISSION_MARKET;
        case BUILDING_MESS_HALL:
            return BUILDING_STORAGE_PERMISSION_QUARTERMASTER;
        case BUILDING_TAVERN:
            return BUILDING_STORAGE_PERMISSION_BARKEEP;
        case BUILDING_CARAVANSERAI:
            return BUILDING_STORAGE_PERMISSION_CARAVANSERAI;
        case BUILDING_LIGHTHOUSE:
            return BUILDING_STORAGE_PERMISSION_LIGHTHOUSE;
        case BUILDING_ARMOURY:
            return BUILDING_STORAGE_PERMISSION_ARMOURY;
        default:
            return BUILDING_STORAGE_PERMISSION_MARKET; // assume market for other types (usually priests)
    }
}

void building_storage_load_state(buffer *buf, int version)
{
    int storage_buf_size;
    size_t buf_size = buf->size;
    int storages_to_load;
    int highest_id_in_use = 0;

    //  state + quantity stored separately
    if (version > SAVE_GAME_LAST_STORAGE_STATE_AND_QUANTITY_TOGETHER) {
        storage_buf_size = buffer_read_i32(buf);
        buf_size -= 4;

        int num_resources = (storage_buf_size - STORAGE_STATIC_BUFFER_SIZE) / 2;
        storages_to_load = (int) buf_size / storage_buf_size;

        if (!array_init(storages, STORAGE_ARRAY_SIZE_STEP, storage_create, storage_in_use) ||
            !array_expand(storages, storages_to_load)) {
            log_error("Unable to create storages. The game will likely crash.", 0, 0);
        }

        for (int i = 0; i < storages_to_load; i++) {
            data_storage *s = array_next(storages);

            s->storage.permissions = buffer_read_i32(buf);
            s->building_id = buffer_read_i32(buf);
            s->in_use = buffer_read_u8(buf);
            s->storage.empty_all = buffer_read_u8(buf);

            if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
                building_storage_accept_none(s->id);
            }

            for (int r = 0; r < num_resources; r++) {
                int remapped = resource_remap(r);
                s->storage.resource_state[remapped].state = buffer_read_u8(buf);
                s->storage.resource_state[remapped].quantity = buffer_read_u8(buf);
            }

            if (storage_buf_size > STORAGE_CURRENT_BUFFER_SIZE) {
                buffer_skip(buf, storage_buf_size - STORAGE_CURRENT_BUFFER_SIZE);
            }

            if (s->in_use) {
                highest_id_in_use = i;
            }
        }

        storages.size = highest_id_in_use + 1;
        return;
    }


    int includes_storage_size = version > SAVE_GAME_LAST_STATIC_VERSION;
    int num_resources = RESOURCE_MAX_LEGACY;

    if (includes_storage_size) { // Augustus 4.0 
        storage_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
        num_resources = storage_buf_size - STORAGE_STATIC_BUFFER_SIZE;
    } else {
        storage_buf_size = STORAGE_ORIGINAL_BUFFER_SIZE; //Julius and vanilla
    }

    storages_to_load = (int) buf_size / storage_buf_size;

    if (!array_init(storages, STORAGE_ARRAY_SIZE_STEP, storage_create, storage_in_use) ||
        !array_expand(storages, storages_to_load)) {
        log_error("Unable to create storages. The game will likely crash.", 0, 0);
    }

    for (int i = 0; i < storages_to_load; i++) {
        data_storage *s = array_next(storages);

        s->storage.permissions = buffer_read_i32(buf);
        s->building_id = buffer_read_i32(buf);
        s->in_use = buffer_read_u8(buf);
        s->storage.empty_all = buffer_read_u8(buf);

        if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
            building_storage_accept_none(s->id);
        }

        for (int r = 0; r < num_resources; r++) {
            int remapped = resource_remap(r);
            uint8_t legacy = buffer_read_u8(buf);
            decode_legacy_storage_state(legacy, &s->storage.resource_state[remapped]);
        }

        if (!includes_storage_size) {
            buffer_skip(buf, 6); // hardcoded old unused bytes
        } else if (storage_buf_size > STORAGE_CURRENT_BUFFER_SIZE) {
            buffer_skip(buf, storage_buf_size - STORAGE_CURRENT_BUFFER_SIZE);
        }

        if (s->in_use) {
            highest_id_in_use = i;
        }
    }

    // fix granary storage
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        int granary_free_space = BUILDING_STORAGE_QUANTITY_MAX;
        for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            b->resources[r] /= 100;
            granary_free_space -= b->resources[r];
        }
        b->resources[RESOURCE_NONE] = granary_free_space;
    }

    storages.size = highest_id_in_use + 1;
}

