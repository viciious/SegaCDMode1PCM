#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "scd_pcm.h"

extern void write_byte(unsigned int dst, unsigned char val);
extern void write_word(unsigned int dst, unsigned short val);
extern void write_long(unsigned int dst, unsigned int val);
extern unsigned char read_byte(unsigned int src);
extern unsigned short read_word(unsigned int src);
extern unsigned int read_long(unsigned int src);

static char wait_cmd_ack(void)
{
    char ack = 0;

    while (!ack)
        ack = read_byte(0xA1200F); // wait for acknowledge byte in sub comm port

    return ack;
}

static void wait_do_cmd(char cmd)
{
    while (read_byte(0xA1200F)) ; // wait until Sub-CPU is ready to receive command
    write_byte(0xA1200E, cmd); // set main comm port to command
}

void scd_init_pcm(void)
{
    /*
    * Initialize the PCM driver
    */
    wait_do_cmd('I');
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_upload_buf(uint16_t buf_id, const uint8_t *data, uint32_t data_len)
{
    uint8_t *scdWordRam = (uint8_t *)0x600000;

    memcpy(scdWordRam, data, data_len);

    write_word(0xA12010, buf_id); /* buf_id */
    write_long(0xA12014, 0x0C0000); /* word ram on CD side (in 1M mode) */
    write_long(0xA12018, data_len); /* sample length */
    wait_do_cmd('B'); // SfxCopyBuffer command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

uint8_t scd_play_src(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    write_long(0xA12010, ((unsigned)src_id<<16)|buf_id); /* src|buf_id */
    write_long(0xA12014, ((unsigned)freq<<16)|pan); /* freq|pan */
    write_long(0xA12018, ((unsigned)vol<<16)|autoloop); /* vol|autoloop */
    wait_do_cmd('A'); // SfxPlaySource command
    wait_cmd_ack();
    src_id = read_byte(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return src_id;
}

uint8_t scd_punpause_src(uint8_t src_id, uint8_t paused)
{
    write_long(0xA12010, ((unsigned)src_id<<16)|paused); /* src|paused */
    wait_do_cmd('N'); // SfxPUnPSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return src_id;
}

void scd_update_src(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    write_long(0xA12014, ((unsigned)freq<<16)|pan); /* freq|pan */
    write_long(0xA12018, ((unsigned)vol<<16)|autoloop); /* vol|autoloop */
    wait_do_cmd('U'); // SfxPlaySource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

uint16_t scd_getpos_for_src(uint8_t src_id)
{
    uint16_t pos;
    write_long(0xA12010, src_id<<16);
    wait_do_cmd('G'); // SfxPUnPSource command
    wait_cmd_ack();
    pos = read_word(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return pos;
}

void scd_stop_src(uint8_t src_id)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    wait_do_cmd('O'); // SfxStopSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_rewind_src(uint8_t src_id)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    wait_do_cmd('W'); // SfxRewindSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_clear_pcm(void)
{
    wait_do_cmd('L'); // SfxClear command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}
