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

#define TONE_0_BIT      0x01
#define TONE_1_BIT      0x02
#define TONE_2_BIT      0x04
#define NOISE_BIT       0x08
#define VOLUME_0_BIT    0x10
#define VOLUME_1_BIT    0x20
#define VOLUME_2_BIT    0x40
#define VOLUME_3_BIT    0x80

#include "../music/aqua_lake.h"

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
        }
        if (frame & VOLUME_1_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x30 | data);
        }
        if (frame & VOLUME_2_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x50 | data);
        }
        if (frame & VOLUME_3_BIT)
        {
            data = nibble_read ();
            psg_write (0x80 | 0x70 | data);
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
    /* Default register values */
    psg_write (0x80 | 0x1f); /* Mute Tone0 */
    psg_write (0x80 | 0x3f); /* Mute Tone1 */
    psg_write (0x80 | 0x5f); /* Mute Tone2 */
    psg_write (0x80 | 0x7f); /* Mute Noise */

    SG_displayOn ();

    while (true)
    {
        SG_waitForVBlank ();
        tick ();
    }
}
