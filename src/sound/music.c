#include "music.h"

#include "core/dir.h"
#include "core/config.h"
#include "city/figures.h"
#include "city/population.h"
#include "game/settings.h"
#include "sound/device.h"

#include <stdlib.h>

#define TRACK_RANDOM_MIN TRACK_CITY_1
#define TRACK_RANDOM_MAX TRACK_CITY_5

enum {
    TRACK_NONE = 0,
    TRACK_CITY_1 = 1,
    TRACK_CITY_2 = 2,
    TRACK_CITY_3 = 3,
    TRACK_CITY_4 = 4,
    TRACK_CITY_5 = 5,
    TRACK_COMBAT_SHORT = 6,
    TRACK_COMBAT_LONG = 7,
    TRACK_INTRO = 8,
    TRACK_MAX = 9
};

static struct {
    int current_track;
    int next_check;
} data = { TRACK_NONE, 0 };

static const char tracks[][32] = {
    "",
    "wavs/ROME1.WAV",
    "wavs/ROME2.WAV",
    "wavs/ROME3.WAV",
    "wavs/ROME4.WAV",
    "wavs/ROME5.WAV",
    "wavs/Combat_Short.wav",
    "wavs/Combat_Long.wav",
    "wavs/setup.wav"
};

static const char mp3_tracks[][32] = {
    "",
    "mp3/ROME1.mp3",
    "mp3/ROME2.mp3",
    "mp3/ROME3.mp3",
    "mp3/ROME4.mp3",
    "mp3/ROME5.mp3",
    "mp3/Combat_Short.mp3",
    "mp3/Combat_Long.mp3",
    "mp3/setup.mp3"
};

void sound_music_set_volume(int percentage)
{
    sound_device_set_music_volume(percentage);
}
static void on_music_finished(void)
{
    data.current_track = TRACK_NONE; // Reset track to none
    sound_music_update(1); // Force track selection again
}

static void play_track(int track)
{
    sound_device_stop_music();
    if (track <= TRACK_NONE || track >= TRACK_MAX) {
        return;
    }
    const char *mp3_track = dir_get_file(mp3_tracks[track], NOT_LOCALIZED);

    int volume = setting_sound(SOUND_TYPE_MUSIC)->volume;
    if (!mp3_track || !sound_device_play_music(mp3_track, volume, 1)) {
        sound_device_play_music(dir_get_file(tracks[track], NOT_LOCALIZED), volume, 1);
    }
    data.current_track = track;
}
static void play_randomised_track(int track)
{
    const char *filename = dir_get_file(mp3_tracks[track], NOT_LOCALIZED);
    int volume = setting_sound(SOUND_TYPE_MUSIC)->volume;

    if (!filename || !sound_device_play_track(filename, volume, on_music_finished)) {
        sound_device_play_track(dir_get_file(tracks[track], NOT_LOCALIZED), volume, on_music_finished);
    }
    data.current_track = track;
}

void sound_music_play_intro(void)
{
    if (setting_sound(SOUND_TYPE_MUSIC)->enabled) {
        play_track(TRACK_INTRO);
    }
}

void sound_music_play_editor(void)
{
    if (setting_sound(SOUND_TYPE_MUSIC)->enabled) {
        play_track(TRACK_CITY_1);
    }
}

void sound_music_next_track(void)
{
    int randomise = config_get(CONFIG_GENERAL_ENABLE_MUSIC_RANDOMISE);
    if (!setting_sound(SOUND_TYPE_MUSIC)->enabled || !randomise) {
        return;
    }

    int track = data.current_track;
    while (track == data.current_track) {
        int range = TRACK_RANDOM_MAX - TRACK_RANDOM_MIN + 1;
        track = TRACK_CITY_1 + (rand() % range);
        if (track != data.current_track) {
            break;
        }
    }
    sound_music_stop();
    play_randomised_track(track);
    return;
}

void sound_music_update(int force)
{
    int randomise = config_get(CONFIG_GENERAL_ENABLE_MUSIC_RANDOMISE);
    if (sound_device_resume_music()) {

        data.next_check = 10;
        return;
    }

    if (data.next_check && !force) {
        --data.next_check;
        return;
    }
    if (!setting_sound(SOUND_TYPE_MUSIC)->enabled) {
        return;
    }

    int track;
    int population = city_population();
    int total_enemies = city_figures_total_invading_enemies();
    if (total_enemies >= 32) {
        track = TRACK_COMBAT_LONG;
    } else if (total_enemies > 0) {
        track = TRACK_COMBAT_SHORT;
    } else if (randomise) {
        int range = TRACK_RANDOM_MAX - TRACK_RANDOM_MIN + 1;
        track = TRACK_CITY_1 + (rand() % range);
    } else if (population < 1000) {
        track = TRACK_CITY_1;
    } else if (population < 2000) {
        track = TRACK_CITY_2;
    } else if (population < 5000) {
        track = TRACK_CITY_3;
    } else if (population < 7000) {
        track = TRACK_CITY_4;
    } else {
        track = TRACK_CITY_5;
    }

    // Case 1: exactly same track — maybe paused, try to resume
    if (track == data.current_track) {
        if (sound_device_resume_music()) {
            data.next_check = 10;
        }
        return;
    }

    // Case 2: both tracks are in randomised pool — don't switch
    if (randomise && track >= TRACK_RANDOM_MIN && track <= TRACK_RANDOM_MAX &&
        data.current_track >= TRACK_RANDOM_MIN && data.current_track <= TRACK_RANDOM_MAX) {
        return;
    }

    if (randomise) {
        play_randomised_track(track);
    } else {
        play_track(track);
    }

    data.next_check = 10;
}
void sound_music_pause(void)
{
    sound_device_pause_music();
}
void sound_music_resume(void)
{
    sound_device_resume_music();
    data.next_check = 10;
}
void sound_music_stop(void)
{
    sound_device_stop_music();
    data.current_track = TRACK_NONE;
    data.next_check = 0;
}
