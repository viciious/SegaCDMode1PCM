#include <stdint.h>

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
void scd_upload_buf(uint16_t buf_id, const uint8_t *data, uint32_t data_len);

// scd_upload_buf starts playback on source from the start of the buffer
// source is a virtual playback channel, one or two hardware channels can be mapped
// to a single source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// value range for buf_id: [1, 256]
// values for freq: [0, 32767], a value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255], 0 is full right, 128 is center, and 255 is full left
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
//
// returned value: if the passed src_id is 255, then thew newly allocated source id,
// if the allocation has failed, a value of 0 is returned
// otherwise the originally passed value of src_id is returned
uint8_t scd_play_src(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);

// scd_punpause_src pauses or unpauses the source
//
// value range for src_id: [1, 8]
uint8_t scd_punpause_src(uint8_t src_id, uint8_t paused);

// scd_update_src updates the frequency, panning, volume and autoloop property for the source
//
// value range for src_id: [1, 8] and a special value of 255, which allocates a new free source id
// values for freq: [0, 32767], a value of 0 means "use frequency derived from the WAVE file"
// values for pan: [0, 255], 0 is full right, 128 is center, and 255 is full left
// values for vol: [0, 255]
// values for autoloop: [0, 255], a boolean: the source will automatically loopf from the start after
// reaching the end of the playback buffer
void scd_update_src(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);

// scd_stop_src stops playback on the given source
//
// value range for src_id: [1, 8]
void scd_stop_src(uint8_t src_id);

// scd_rewind_src sets position for the given source to the start of the playback buffer
//
// value range for src_id: [1, 8]
void scd_rewind_src(uint8_t src_id);

// scd_getpos_for_src returns playback position for the given source
//
// value range for src_id: [1, 8]
//
// returned value: current read position in PCM memory of the ricoh chip for the first channel of the source
uint16_t scd_getpos_for_src(uint8_t src_id);

// scd_clear_pcm stops playback on all channels
void scd_clear_pcm(void);
