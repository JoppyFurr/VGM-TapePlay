/*
 * VGM-TapePlay
 *
 * A tool to play pre-processed VGM files on the SC-3000.
 * Joppy Furr 2024
 */

#include <stdbool.h>
#include <stdint.h>

#include "SGlib.h"
__sfr __at 0x7f psg_port;
__sfr __at 0xbf vdp_control_port;

#define TONE_0_BIT      0x01
#define TONE_1_BIT      0x02
#define TONE_2_BIT      0x04
#define NOISE_BIT       0x08
#define VOLUME_0_BIT    0x10
#define VOLUME_1_BIT    0x20
#define VOLUME_2_BIT    0x40
#define VOLUME_3_BIT    0x80

#include "../tile_data/pattern.h"
#include "../tile_data/pattern_index.h"
#include "../tile_data/colour_table.h"
#include "../music_data/music.h"

static const uint8_t underline [16] = {
    PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1,
    PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1,
    PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1,
    PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1, PATTERN_PLAYER + 1
};

static const uint8_t bar_black   [2] = { PATTERN_PLAYER +  0, PATTERN_PLAYER +  0 };
static const uint8_t bar_green_1 [2] = { PATTERN_PLAYER +  2, PATTERN_PLAYER +  2 };
static const uint8_t bar_green_2 [2] = { PATTERN_PLAYER +  3, PATTERN_PLAYER +  3 };
static const uint8_t bar_green_3 [2] = { PATTERN_PLAYER +  4, PATTERN_PLAYER +  4 };
static const uint8_t bar_amber_1 [2] = { PATTERN_PLAYER +  5, PATTERN_PLAYER +  5 };
static const uint8_t bar_amber_2 [2] = { PATTERN_PLAYER +  6, PATTERN_PLAYER +  6 };
static const uint8_t bar_amber_3 [2] = { PATTERN_PLAYER +  7, PATTERN_PLAYER +  7 };
static const uint8_t bar_red_1   [2] = { PATTERN_PLAYER +  8, PATTERN_PLAYER +  8 };
static const uint8_t bar_red_2   [2] = { PATTERN_PLAYER +  9, PATTERN_PLAYER +  9 };
static const uint8_t bar_red_3   [2] = { PATTERN_PLAYER + 10, PATTERN_PLAYER + 10 };

static uint16_t outer_index = 0; /* Index into the compressed index_data */
static uint16_t inner_index = 0; /* Index when expanding references into index_data */
static uint16_t frame_index = 0; /* Index into frame data */

/* Flag for 'is the next nibble to the high nibble of its byte?' */
static bool nibble_high = false;


/*
 * Write one byte of data to the sn76489.
 */
inline void psg_write (uint8_t data)
{
    psg_port = data;
}


/*
 * Fill the name table with tile-zero.
 */
void clear_screen (void)
{
    uint8_t blank_line [32] = { 0 };

    for (uint8_t row = 0; row < 24; row++)
    {
        SG_loadTileMap (0, row, blank_line, sizeof (blank_line));
    }
}


/*
 * Update a bar graph.
 *
 * TODO: Should the drop be limited to one or two bars per frame?
 *       Some music looks a bit flickery, particularly in the noise channel.
 */
static void bar_update (uint8_t bar, uint8_t value)
{
    static uint8_t previous [4] = { 15, 15, 15, 15 };
    uint8_t x = 9 + (bar << 2);

    /* Range of tiles that need to be updated */
    uint8_t first = (((value < previous [bar]) ? value : previous [bar]) + 1) >> 1;
    uint8_t last =   (((value > previous [bar]) ? value : previous [bar]) + 1) >> 1;

    /* Uses fall-through */
    switch (first)
    {
        case 0:
            SG_loadTileMap (x, 7, (value == 0) ? bar_red_3 : bar_black, 2);
            if (last == 0) break;
        case 1:
            SG_loadTileMap (x, 8, (value < 1) ? bar_red_2 :
                                  (value < 2) ? bar_red_1 :
                                  (value < 3) ? bar_amber_3 : bar_black, 2);
            if (last == 1) break;
        case 2:
            SG_loadTileMap (x, 9, (value < 3) ? bar_amber_2 :
                                  (value < 4) ? bar_amber_1 :
                                  (value < 5) ? bar_green_1 : bar_black, 2);
            if (last == 2) break;
        case 3:
            SG_loadTileMap (x, 10, (value < 5) ? bar_green_3 :
                                   (value < 6) ? bar_green_2 :
                                   (value < 7) ? bar_green_1 : bar_black, 2);
            if (last == 3) break;
        case 4:
            SG_loadTileMap (x, 11, (value < 7) ? bar_green_3 :
                                   (value < 8) ? bar_green_2 :
                                   (value < 9) ? bar_green_1 : bar_black, 2);
            if (last == 4) break;
        case 5:
            SG_loadTileMap (x, 12, (value < 9)  ? bar_green_3 :
                                   (value < 10) ? bar_green_2 :
                                   (value < 11) ? bar_green_1 : bar_black, 2);
            if (last == 5) break;
        case 6:
            SG_loadTileMap (x, 13, (value < 11) ? bar_green_3 :
                                   (value < 12) ? bar_green_2 :
                                   (value < 13) ? bar_green_1 : bar_black, 2);
            if (last == 6) break;
        case 7:
            SG_loadTileMap (x, 14, (value < 13) ? bar_green_3 :
                                   (value < 14) ? bar_green_2 :
                                   (value < 15) ? bar_green_1 : bar_black, 2);
        case 8:
            break;
    }

    previous [bar] = value;
}


/*
 * Read the next nibble from the frame data.
 */
static uint8_t nibble_read (void)
{
    if (nibble_high)
    {
        nibble_high = false;
        return frame_data[frame_index++] >> 4;
    }
    else
    {
        nibble_high = true;
        return frame_data[frame_index] & 0x0f;
    }
}


/*
 * Advance the index if we end half way through a byte.
 */
static void nibble_done (void)
{
    if (nibble_high)
    {
        nibble_high = false;
        frame_index++;
    }
}


/*
 * Called every 1/60s to apply the next set of register writes.
 */
static void tick (void)
{
    static uint8_t delay = 0;
    static uint16_t segment_end = 0;

    /* Read and process the next frame */
    if (delay == 0)
    {
        uint16_t element;
        uint8_t frame;
        uint8_t data;

        /* If we are not already processing a segment of referenced
         * data, read a new element from the compressed index_data */
        if (inner_index == segment_end)
        {
            element = index_data[outer_index++];

            if (element & 0x8000)
            {
                /* Segment */
                inner_index = element & 0x0fff;
                segment_end = inner_index + ((element >> 12) & 0x0007) + 2;
            }
            else
            {
                /* Single index */
                inner_index = outer_index - 1;
                segment_end = outer_index;
            }
        }

        /* Read the delay and frame_index from the index_data */
        frame_index = index_data[inner_index++];
        delay = ((frame_index >> 12) & 0x0007) + 1;
        frame_index &= 0x0fff;

        /* Read the frame header from the frame_data */
        frame = frame_data[frame_index++];

        if (frame & TONE_0_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x00 | data);

            data = nibble_read ();
            data |= nibble_read () << 4;
            psg_write (data);
        }
        if (frame & TONE_1_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x20 | data);

            data = nibble_read ();
            data |= nibble_read () << 4;
            psg_write (data);
        }
        if (frame & TONE_2_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x40 | data);

            data = nibble_read ();
            data |= nibble_read () << 4;
            psg_write (data);
        }
        if (frame & NOISE_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x60 | data);
        }
        if (frame & VOLUME_0_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x10 | data);
            bar_update (0, data);
        }
        if (frame & VOLUME_1_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x30 | data);
            bar_update (1, data);
        }
        if (frame & VOLUME_2_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x50 | data);
            bar_update (2, data);
        }
        if (frame & VOLUME_3_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x70 | data);
            bar_update (3, data);
        }

        nibble_done ();
    }

    /* Check for end of data and loop */
    if (outer_index == END_FRAME_INDEX)
    {
        outer_index = LOOP_FRAME_INDEX_OUTER;
        inner_index = LOOP_FRAME_INDEX_INNER;
        segment_end = LOOP_FRAME_SEGMENT_END;
    }

    /* Decrement the delay counter */
    if (delay > 0)
    {
        delay--;
    }
}


/*
 * Entry point.
 */
int main (void)
{
    /* Default PSG register values */
    psg_write (0x80 | 0x1f); /* Mute Tone0 */
    psg_write (0x80 | 0x3f); /* Mute Tone1 */
    psg_write (0x80 | 0x5f); /* Mute Tone2 */
    psg_write (0x80 | 0x7f); /* Mute Noise */

    /* Load tiles for all three screen-slices */
    for (uint16_t slice = 0x000; slice < 0x300; slice += 0x100)
    {
        SG_loadTilePatterns (patterns, slice, sizeof (patterns));
        SG_loadTileColours (colour_table, slice, sizeof (colour_table));
    }
    SG_setBackdropColor (1); /* Black */

    /* Set up the display */
    clear_screen ();
    SG_loadTileMap (8, 16, underline, sizeof (underline));

    SG_displayOn ();

    while (true)
    {
        SG_waitForVBlank ();
        tick ();
    }
}
