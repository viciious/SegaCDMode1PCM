/*
 * SEGA CD Mode 1 PCM Player
 * by Victor Luchits
 * and Chilly Willy
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hw_md.h"
#include "scd_pcm.h"

#define WHITE_TEXT 0x0000
#define GREEN_TEXT 0x2000
#define RED_TEXT   0x4000

extern uint32_t vblank_vector;
extern uint32_t gen_lvl2;
extern uint32_t Sub_Start;
extern uint32_t Sub_End;

extern volatile uint32_t gTicks;             /* incremented every vblank */

extern void Kos_Decomp(uint8_t *src, uint8_t *dst);

extern uint8_t stereo_test_u8_wav;
extern int stereo_test_u8_wav_len;
extern uint8_t macabre_ima_wav;
extern int macabre_ima_wav_len;

extern uint8_t macabre_sb4_wav;
extern int macabre_sb4_wav_len;

int main(void)
{
    uint16_t buttons = 0, previous = 0;
    uint8_t *bios;
    char text[44];
    uint8_t last_src = 0;
    int16_t pan = 128, vol = 255;
    uint8_t src_paused[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    clear_screen();

    /*
     * Check for CD BIOS
     * When a cart is inserted in the MD, the CD hardware is mapped to
     * 0x400000 instead of 0x000000. So the BIOS ROM is at 0x400000, the
     * Program RAM bank is at 0x420000, and the Word RAM is at 0x600000.
     */
    bios = (uint8_t *)0x415800;
    if (memcmp(bios + 0x6D, "SEGA", 4))
    {
        bios = (uint8_t *)0x416000;
        if (memcmp(bios + 0x6D, "SEGA", 4))
        {
            // check for WonderMega/X'Eye
            if (memcmp(bios + 0x6D, "WONDER", 6))
            {
                bios = (uint8_t *)0x41AD00; // might also be 0x40D500
                // check for LaserActive
                if (memcmp(bios + 0x6D, "SEGA", 4))
                {
                    put_str("No CD detected!", RED_TEXT, 20-7, 12);
                    while (1) ;
                }
            }
        }
    }
    sprintf(text, "CD Sub-CPU BIOS detected at 0x%6X", (uint32_t)bios);
    put_str(text, GREEN_TEXT, 2, 2);

	/*
	 * Reset the Gate Array - this specific sequence of writes is recognized by
	 * the gate array as a reset sequence, clearing the entire internal state -
	 * this is needed for the LaserActive
	 */
	write_word(0xA12002, 0xFF00);
	write_byte(0xA12001, 0x03);
	write_byte(0xA12001, 0x02);
	write_byte(0xA12001, 0x00);

    /*
     * Reset the Sub-CPU, request the bus
     */
    write_byte(0xA12001, 0x02);
    while (!(read_byte(0xA12001) & 2)) write_byte(0xA12001, 0x02); // wait on bus acknowledge

    /*
     * Decompress Sub-CPU BIOS to Program RAM at 0x00000
     */
    put_str("Decompressing Sub-CPU BIOS", GREEN_TEXT, 2, 3);
    write_word(0xA12002, 0x0002); // no write-protection, bank 0, 2M mode, Word RAM assigned to Sub-CPU
    memset((void *)0x420000, 0, 0x20000); // clear program ram first bank - needed for the LaserActive
    Kos_Decomp(bios, (uint8_t *)0x420000);

    /*
     * Copy Sub-CPU program to Program RAM at 0x06000
     */
    put_str("Copying Sub-CPU Program", GREEN_TEXT, 2, 4);
    memcpy((void *)0x426000, &Sub_Start, (int)&Sub_End - (int)&Sub_Start);
    if (memcmp((void *)0x426000, &Sub_Start, (int)&Sub_End - (int)&Sub_Start))
    {
        sprintf(text, "len %d!", (int)&Sub_End - (int)&Sub_Start);
        put_str("Failed writing Program RAM!", RED_TEXT, 20-13, 12);
        put_str(text, RED_TEXT, 20-13, 13);
        while (1) ;
    }

    write_byte(0xA1200E, 0x00); // clear main comm port
    write_byte(0xA12002, 0x2A); // write-protect up to 0x05400
    write_byte(0xA12001, 0x01); // clear bus request, deassert reset - allow CD Sub-CPU to run
    while (!(read_byte(0xA12001) & 1)) write_byte(0xA12001, 0x01); // wait on Sub-CPU running
    put_str("Sub-CPU started", GREEN_TEXT, 2, 5);

    /*
     * Set the vertical blank handler to generate Sub-CPU level 2 ints.
     * The Sub-CPU BIOS needs these in order to run.
     */
    write_long((uint32_t)&vblank_vector, (uint32_t)&gen_lvl2);
    set_sr(0x2000); // enable interrupts

    /*
     * Wait for Sub-CPU program to set sub comm port indicating it is running -
     * note that unless there's something wrong with the hardware, a timeout isn't
     * needed... just loop until the Sub-CPU program responds, but 2000000 is about
     * ten times what the LaserActive needs, and the LA is the slowest unit to
     * initialize
     */
    while (read_byte(0xA1200F) != 'I')
    {
        static int timeout = 0;
        timeout++;
        if (timeout > 2000000)
        {
            put_str("CD failed to start!", RED_TEXT, 20-9, 12);
            while (1) ;
        }
    }

    /*
     * Wait for Sub-CPU to indicate it is ready to receive commands
     */
    while (read_byte(0xA1200F) != 0x00) ;
    put_str("CD initialized and ready to go!", WHITE_TEXT, 20-15, 12);

    /*
    * Initialize the PCM driver
    */
    scd_init_pcm();

    put_str("Uploading samples...", WHITE_TEXT, 20-15, 13);

    /*
    * Upload test samples
    */
    scd_upload_buf(1, &macabre_ima_wav, macabre_ima_wav_len);
    scd_upload_buf(2, &macabre_sb4_wav, macabre_sb4_wav_len);
    scd_upload_buf(3, &stereo_test_u8_wav, stereo_test_u8_wav_len);

    clear_screen();

    put_str("Mode 1 PCM Player", WHITE_TEXT, 20-8, 2);

    put_str("Sample A: IMA ADPCM Mono", WHITE_TEXT, 2, 16);
    put_str("Sample B: SB4 ADPCM Mono", WHITE_TEXT, 2, 17);
    put_str("Sample C: 8bit PCM Stereo", WHITE_TEXT, 2, 18);

    put_str("START = Pause last source", WHITE_TEXT, 2, 20);
    put_str("A/B/C = Play Smpl A/B/C on src 1/2/3", WHITE_TEXT, 2, 21);
    put_str("X/Y/Z = Play Smpl A/B/C on free src", WHITE_TEXT, 2, 22);
    put_str("L/R   = pan left/right", WHITE_TEXT, 2, 23);
    put_str("U/D   = volume incr/decr", WHITE_TEXT, 2, 24);
    put_str("MODE  = clear", WHITE_TEXT, 2, 25);

    while (1)
    {
        int new_src = 0;

        delay(2);
        buttons = get_pad(0) & SEGA_CTRL_BUTTONS;

        if (buttons & SEGA_CTRL_LEFT)
            pan--;
        if (buttons & SEGA_CTRL_RIGHT)
            pan++;

        if (pan < 0)
            pan = 0;
        if (pan > 255)
            pan = 255;

        if (buttons & SEGA_CTRL_DOWN)
            vol--;
        if (buttons & SEGA_CTRL_UP)
            vol++;

        if (vol < 0)
            vol = 0;
        if (vol > 255)
            vol = 255;

        if (((buttons ^ previous) & SEGA_CTRL_A) && (buttons & SEGA_CTRL_A))
        {
            last_src = scd_play_src(1, 1, 0, pan, vol, 0);
            src_paused[last_src] = 0;
        }
        if (((buttons ^ previous) & SEGA_CTRL_B) && (buttons & SEGA_CTRL_B))
        {
            last_src = scd_play_src(2, 2, 0, pan, vol, 0);
            src_paused[last_src] = 0;
        }
        if (((buttons ^ previous) & SEGA_CTRL_C) && (buttons & SEGA_CTRL_C))
        {
            last_src = scd_play_src(3, 3, 0, pan, vol, 0);
            src_paused[last_src] = 0;
        }

        if (((buttons ^ previous) & SEGA_CTRL_X) && (buttons & SEGA_CTRL_X))
        {
            new_src = scd_play_src(255, 1, 0, pan, vol, 0);
        }
        if (((buttons ^ previous) & SEGA_CTRL_Y) && (buttons & SEGA_CTRL_Y))
        {
            new_src = scd_play_src(255, 2, 0, pan, vol, 0);
        }
        if (((buttons ^ previous) & SEGA_CTRL_Z) && (buttons & SEGA_CTRL_Z))
        {
            new_src = scd_play_src(255, 3, 0, pan, vol, 0);
        }

        if (new_src != 0) {
            last_src = new_src;
            src_paused[last_src] = 0;
        }

        if (((buttons ^ previous) & SEGA_CTRL_START) && (buttons & SEGA_CTRL_START))
        {
            scd_punpause_src(last_src, !src_paused[last_src]);
            src_paused[last_src] = !src_paused[last_src];
        }

        if (((buttons ^ previous) & SEGA_CTRL_MODE) && (buttons & SEGA_CTRL_MODE))
        {
            scd_clear_pcm();
        }

        scd_update_src(last_src, 0, pan, vol, 0);

        sprintf(text, "%d", last_src);
        put_str("Last Source:   ", GREEN_TEXT, 2, 6);
        put_str(text, WHITE_TEXT, 15, 6);
        sprintf(text, "%04X", scd_getpos_for_src(last_src));
        put_str("Position:    ", GREEN_TEXT, 2, 7);
        put_str(text, WHITE_TEXT, 15, 7);

        sprintf(text, "%03d", (int)pan);
        put_str("Panning:       ", GREEN_TEXT, 2, 9);
        put_str(text, WHITE_TEXT, 15, 9);
        sprintf(text, "%03d", (int)vol);
        put_str("Volume:        ", GREEN_TEXT, 2, 10);
        put_str(text, WHITE_TEXT, 15, 10);

        previous = buttons;
    }

    /*
     * Should never reach here due to while condition
     */
    clear_screen ();
    return 0;
}
