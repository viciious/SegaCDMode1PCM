#include "s_sources.h"
#include "s_channels.h"
#include "s_buffers.h"
#include "pcm.h"

#define S_PAINT_CHUNK   128 // the number of samples to paint in a single call of S_Src_Paint

sfx_source_t s_sources[ S_MAX_SOURCES ] = { { 0 } };

void S_Src_Init(sfx_source_t *src)
{
    src->buf = NULL;
    src->num_channels = 0;
    src->channels[0] = src->channels[1] = 0;
    src->rem = 0;
    src->eof = 0;
}

void S_Src_Stop(sfx_source_t *src)
{
    int i;
    for (i = 0; i < src->num_channels; i++) {
        S_Chan_Clear( &s_channels[ src->channels[ i ] ] );
    }
    src->buf = NULL;
    src->freq = 0;
    src->painted = 0;
    src->num_channels = 0;
    src->rem = 0;
    src->eof = 1;
}

void S_Src_Rewind(sfx_source_t *src)
{
    sfx_buffer_t *buf = src->buf;

    if (!buf) {
        return;
    }

    src->data_pos = 0;

    switch (buf->format) {
        case S_FORMAT_WAV_ADPCM:
            src->adpcm.data = buf->data;
            src->adpcm.data_end = buf->data; // force block read
            src->adpcm.remaining_bytes = buf->data_len;
            break;
    }
}

void S_Src_SetPause(sfx_source_t *src, uint8_t paused)
{
    sfx_buffer_t *buf = src->buf;

    if (!buf) {
        return;
    }
    src->paused = paused;
}

uint16_t S_Buf_LoadMonoSamples(sfx_source_t *src, uint16_t *pos, uint16_t len)
{
    sfx_buffer_t *buf = src->buf;

    switch (buf->format) {
        case S_FORMAT_RAW_U8:
            if (src->data_pos + len > buf->data_len) {
                len = buf->data_len - src->data_pos;
                if (len == 0) {
                    return 0;
                }
            }
            pcm_load_samples_u8(*pos, buf->data + src->data_pos, len);
            src->data_pos += len;
            return len;

        case S_FORMAT_WAV_ADPCM:
            return adpcm_load_samples(&src->adpcm, *pos, len);
    }

    return 0;
}

uint16_t S_Buf_LoadStereoSamples(sfx_source_t *src, uint16_t *pos, uint16_t len)
{
    sfx_buffer_t *buf = src->buf;

    switch (buf->format) {
        case S_FORMAT_RAW_U8:
            if (src->data_pos + len*2 > buf->data_len) {
                len = (buf->data_len - src->data_pos) / 2;
                if (len == 0) {
                    return 0;
                }
            }
            pcm_load_stereo_samples_u8(pos[0], pos[1], buf->data + src->data_pos, len);
            src->data_pos += len*2;
            return len;

        case S_FORMAT_WAV_ADPCM:
            return 0;
    }

    return 0;
}

uint16_t S_Src_LoadSamples(sfx_source_t *src, uint16_t *pos, uint16_t len)
{
    uint16_t painted = 0;

    switch (src->num_channels) {
        case 2:
            painted = S_Buf_LoadStereoSamples(src, pos, len);
            pos[0] += painted;
            pos[1] += painted;
            break;
        case 1:
        default:
            painted = S_Buf_LoadMonoSamples(src, pos, len);
            pos[0] += painted;
            break;
    }

    return painted;
}

int S_Src_Paint(sfx_source_t *src)
{
    int i;
    uint16_t painted, rem;
    sfx_channel_t *chan;
    sfx_channel_t *prichan;

    // stream data
    if (!src->buf || !src->num_channels) {
        S_Src_Stop(src);
        return 1;
    }

    // use position of the primary channel to determine backbuffer id
    prichan = &s_channels[ src->channels[ 0 ] ];

    int8_t backbuf = S_Chan_BackBuffer( prichan );
    if (src->backbuf != backbuf) {
        if (src->eof) {
            S_Src_Stop(src);
            return 1;
        }
        
        src->backbuf = backbuf;
        src->rem = CHBUF_SIZE;

        for (i = 0; i < src->num_channels; i++ ) {
            uint16_t startblock;

            chan = &s_channels[ src->channels[ i ] ];
            startblock = S_Chan_StartBlock( chan );
            src->bufpos[i] = CHBUF_POS(startblock + backbuf);
        }
    }
    else {
        if (src->rem == 0) {
            return 1;
        }
    }

    rem = src->rem;
    painted = 0;
    if (rem > S_PAINT_CHUNK) {
        rem = S_PAINT_CHUNK;
    }

paint:
    if (!src->paused) {
        if (!src->eof) {
            int len = rem - painted;
            int newpainted = S_Src_LoadSamples(src, src->bufpos, len);
            if (newpainted != len) {
                src->eof = 1;
            }
            painted += newpainted;
        }

        if (src->eof) {
            if (src->painted > 0 && src->autoloop) {
                // auto-restart only if we have previously painted at least 1 sample
                src->eof = 0;
                src->painted = 0;
                S_Src_Rewind(src);
                goto paint;
            }
        }
    }

    src->painted += painted;
    src->rem -= painted;

    if (painted < S_PAINT_CHUNK) {
        for (i = 0; i < src->num_channels; i++) {
            // pad remaining buffer data with silence
            int pad = src->rem;
            pcm_load_zero(src->bufpos[ i ], pad);
            src->bufpos[ i ] += src->rem;
        }
        src->rem = 0;
    }

    if (src->rem == 0) {
        // copy channel parameters from source and update
        for (i = 0; i < src->num_channels; i++) {
            chan = &s_channels[ src->channels[ i ] ];
            chan->freq = src->freq;
            chan->env = src->env;
            chan->pan = src->pan[i];
            S_Chan_Update(chan);
        }

        return 1;
    }

    return 0;
}

void S_Src_Play(sfx_source_t *src, sfx_buffer_t *buf, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    int i;
    sfx_channel_t *chan;

    src->buf = buf;
    src->backbuf = -1; // force data refresh on the next update
    src->pan[0] = S_Chan_MidiPan(pan);
    src->env = vol;
    src->autoloop = autoloop;
    src->channels[ 0 ] = 0;
    src->channels[ 1 ] = 0;
    src->freq = freq ? freq : buf->freq;
    src->paused = 0;
    src->eof = 0;

    if (!buf || !buf->num_channels || !buf->data || !src->freq) {
        goto noplay;
    }

    src->num_channels = buf->num_channels;
    src->adpcm.codec = buf->adpcm_codec;
    src->adpcm.block_size = buf->adpcm_block_size;

    // hard-pan stereo channels
    if (buf->num_channels == 2) {
        src->pan[0] = 0b00001000; // left
        src->pan[1] = 0b10000000; // right
    }

    for (i = 0; i < buf->num_channels; i++) {
        src->channels[ i ] = S_AllocChannel();
        if (!src->channels[ i ]) {
            // out of free channels
            break;
        }
        chan = &s_channels[ src->channels[ i ] ];
        chan->freq = src->freq;
    }

    if (i < buf->num_channels) {
        // deallocate channels, exit
        while (i-- > 0) {
            chan = &s_channels[ src->channels[ i ] ];
            chan->freq = 0;
        }
noplay:
        src->buf = NULL;
        src->num_channels = 0;
        return;
    }

    S_Src_Rewind(src);
}

void S_Src_Update(sfx_source_t *src, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    if (!src->buf) {
        return;
    }
    if (freq) {
        src->freq = freq;
    } else if (src->buf->freq) {
        src->freq = src->buf->freq;
    }
    if (src->num_channels == 1) {
        src->pan[0] = S_Chan_MidiPan(pan);
    }
    src->env = vol;
    src->autoloop = autoloop;
}

uint16_t S_Src_GetPosition(sfx_source_t *src)
{
    if (!src || !src->channels[0] ) {
        return 0xffff;
    }
    return S_Chan_GetPosition( &s_channels [ src->channels[0] ] );
}

void S_InitSources(void)
{
    int i;
    for (i = 0; i < S_MAX_SOURCES; i++) {
        S_Src_Init(&s_sources[ i ]);
    }
}

void S_StopSources(void)
{
    int i;

    pcm_reset();

    for (i = 0; i < S_MAX_SOURCES; i++) {
        S_Src_Stop(&s_sources[ i ]);
    }
}

int S_AllocSource(void)
{
    int i;
    for (i = 0; i < S_MAX_SOURCES; i++) {
        sfx_source_t *src = &s_sources[ i ];
        if (!src->buf) {
            return i + 1;
        }
    }
    return 0;
}
