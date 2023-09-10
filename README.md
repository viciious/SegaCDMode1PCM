# Sega CD Mode 1 PCM driver

## Description

This driver supports simultaneous playback of up to 8 mono PCM streams using the Ricoh RF5C164 chip on the Sega CD. The samples can be uploaded from cartridge ROM to SegaCD program RAM and playback can be controlled by the main Sega Genesis/MegaDrive CPU.

The supported formats for sound samples are: WAV IMA ADPCM, WAV SB4 ADPCM and raw 8-bit PCM.

The demo project that comes with the driver showcases an example of how the driver can be used to start and control playback of multiple PCM streams. The code is based on the SEGA CD Mode 1 CD Player by Chilly Willy.

![image](https://github.com/viciious/SegaCDMode1PCM/assets/1173058/40cade84-177b-4683-b512-7efce4e237f3)

A couple of important notes:
* Each sample must be under 128KiB
* The total amount of memory reserved for sound samples is around 460KiB
* For ADPCM, only mono samples supported
* Stereo samples require 2 hardware channels
* IMA ADPCM decoding is taxing on the Sub-CPU, so realistically up to 7 IMA ADPCM streams can be played back simultaneously without degradation
* The driver also includes CDDA music support

## Notes on SB4 ADPCM
SB4 ADPCM codec based on the Creative Labs's old 8-bit ADPCM spec: https://wiki.multimedia.cx/index.php/Creative_8_bits_ADPCM

WAVE codec id of 0x0200 is used for identification and is typically unsupported by modern software players. These files can "sorta" be played back like so: `ffplay -acodec adpcm_sbpro_4 sb4.wav`

The decoding process of SB4 stream is much lighter than that of IMA, so the driver has no problem decoding up 8 SB4 streams at once. However, the quality can be objectively worse, so your mileage may vary.

The encoder is still a WIP and will be avilable sometime later :P

## Sega MD API for the Driver

```
// scd_init_pcm initializes the PCM driver
void scd_init_pcm(void);

// scd_upload_buf copies data to word RAM and sends a request to the SegaCD
// to copy it to an internal buffer in program RAM
//
// value range for buf_id: [1, 256]
// the sample must be under 128KiB due to word RAM limitations in 1M mode
// the passed data can be a WAV file, for which unsigned 8-bit PCM, IMA ADPCM (codec id: 0x11) 
// or SB4 ADPCM (codec id: 0x0200) formats are supported, otherwise raw unsigned 8-bit PCM 
// data is assumed
//
// replacing data in a previously initialized buffer of sufficient size is supported
// otherwise a new memory block will be allocated from the available memory pool
// once the driver runs out of memory, no further allocations will be possible and
// the driver will have to be re-initialized by calling scd_init_pcm 
void scd_upload_buf(uint16_t buf_id, const uint8_t *data, uint32_t data_len) SCD_CODE_ATTR;

// scd_upload_buf starts playback on source from the start of the buffer
// source is a virtual playback channel, one or two hardware channels can be mapped
// to a single source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// value range for buf_id: [1, 256]
// values for freq: [0, 32767] value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255] value of 255 disables panning, 0 is full left, 128 is center, and 254 is full right
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
//
// returned value: if the passed src_id is 255, then thew newly allocated source id,
// if the allocation has failed, a value of 0 is returned
// otherwise the originally passed value of src_id is returned
uint8_t scd_play_src(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop) SCD_CODE_ATTR;

// scd_punpause_src pauses or unpauses the source
//
// value range for src_id: [1, 8]
uint8_t scd_punpause_src(uint8_t src_id, uint8_t paused) SCD_CODE_ATTR;

// scd_update_src updates the frequency, panning, volume and autoloop property for the source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// values for freq: [0, 32767] value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255] value of 255 disables panning, 0 is full left, 128 is center, and 254 is full right
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
void scd_update_src(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop) SCD_CODE_ATTR;

// scd_stop_src stops playback on the given source
//
// value range for src_id: [1, 8]
void scd_stop_src(uint8_t src_id) SCD_CODE_ATTR;

// scd_rewind_src sets position for the given source to the start of the playback buffer
//
// value range for src_id: [1, 8]
void scd_rewind_src(uint8_t src_id) SCD_CODE_ATTR;

// scd_getpos_for_src returns playback position for the given source
//
// value range for src_id: [1, 8]
//
// returned value: current read position in PCM memory of the ricoh chip for the first channel of the source
uint16_t scd_getpos_for_src(uint8_t src_id) SCD_CODE_ATTR;

// scd_clear_pcm stops playback on all channels
void scd_clear_pcm(void) SCD_CODE_ATTR;

// returns playback status mask for all sources
// if a source is active, it will have its bit set to 1 in the mask:
// bit 0 for source id 1, bit 1 for source id 2, etc
int scd_get_playback_status(void) SCD_CODE_ATTR;

// queues a scd_play_src call, always returns 0
uint8_t scd_queue_play_src(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop) SCD_CODE_ATTR;

// queues a scd_update_src call
void scd_queue_update_src(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop) SCD_CODE_ATTR;

// queues a scd_stop_src call
void scd_queue_stop_src(uint8_t src_id) SCD_CODE_ATTR;

// queues a scd_clear_pcm call
void scd_queue_clear_pcm(void) SCD_CODE_ATTR;

// flushes the command queue
int scd_flush_cmd_queue(void) SCD_CODE_ATTR;
```

## Credits
* Programming : Victor Luchits
* Testing: Barone & Chilly Willy

## "SEGA CD Mode 1 CD Player" Credits
* Programming : Chilly Willy

## Original IMA ADPCM 68000 assembler
* Programming : Mikael Kalms

## Links
https://github.com/Kalmalyzer/adpcm-68k

## License
All original code is available under the MIT license.
