/******************************************************************************
 * disk/psygnosis_a.c
 * 
 * Custom format as used by various Psygnosis releases:
 *   Amnios
 *   Aquaventura (sync 0x4429)
 *   Betrayal
 *   Carrier Command
 *   Midwinter
 * 
 * Sometimes a single release will use both this and Psygnosis B.
 * 
 * Written in 2011 by Keir Fraser
 * 
 * RAW TRACK LAYOUT:
 *  u16 0x4489|0x4429
 *  u32 trk
 *  u32 csum
 *  u32 data[12*512/4]
 * MFM encoding of sectors:
 *  AmigaDOS style encoding and checksum.
 * 
 * TRKTYP_psygnosis_a data layout:
 *  u8 sector_data[12*512]
 *  u16 sync
 */

#include <libdisk/util.h>
#include "private.h"

#include <arpa/inet.h>

static void *psygnosis_a_write_mfm(
    struct disk *d, unsigned int tracknr, struct stream *s)
{
    struct track_info *ti = &d->di->track[tracknr];
    char *block = NULL;

    while ( (stream_next_bit(s) != -1) && !block )
    {
        uint32_t raw_dat[2*ti->len/4], hdr, csum;
        uint32_t idx_off = s->index_offset - 15;
        uint16_t sync = s->word;

        if ( (sync != 0x4489) && (sync != 0x4429) )
            continue;

        ti->data_bitoff = idx_off;

        if ( stream_next_bytes(s, raw_dat, 16) == -1 )
            goto done;
        mfm_decode_amigados(&raw_dat[0], 1);
        mfm_decode_amigados(&raw_dat[2], 1);
        hdr = ntohl(raw_dat[0]);
        csum = ntohl(raw_dat[2]);

        if ( hdr != (0xffffff00u | tracknr) )
            continue;

        if ( stream_next_bytes(s, raw_dat, sizeof(raw_dat)) == -1 )
            goto done;
        if ( (csum ^= mfm_decode_amigados(raw_dat, ti->len/4)) != 0 )
            continue;

        block = memalloc(ti->len + 2);
        *(uint16_t *)&block[ti->len] = htons(sync);
        memcpy(block, raw_dat, ti->len);
    }

done:
    if ( block )
        ti->valid_sectors = 1;
    ti->len += 2; /* for the sync mark */
    return block;
}

static void psygnosis_a_read_mfm(
    struct disk *d, unsigned int tracknr, struct track_buffer *tbuf)
{
    struct track_info *ti = &d->di->track[tracknr];
    uint32_t track, csum = 0, *dat = (uint32_t *)ti->dat;
    uint16_t sync;
    unsigned int i, dat_len = ti->len - 2;

    sync = ntohs(*(uint16_t *)&ti->dat[dat_len]);
    tbuf_bits(tbuf, SPEED_AVG, TB_raw, 16, sync);

    track = (~0u << 8) | tracknr;
    tbuf_bits(tbuf, SPEED_AVG, TB_even_odd, 32, track);

    for ( i = 0; i < dat_len/4; i++ )
        csum ^= ntohl(dat[i]);
    csum ^= csum >> 1;
    csum &= 0x55555555u;
    tbuf_bits(tbuf, SPEED_AVG, TB_even_odd, 32, csum);

    tbuf_bytes(tbuf, SPEED_AVG, TB_even_odd, dat_len, dat);
}

struct track_handler psygnosis_a_handler = {
    .type = TRKTYP_psygnosis_a,
    .bytes_per_sector = 12*512,
    .nr_sectors = 1,
    .write_mfm = psygnosis_a_write_mfm,
    .read_mfm = psygnosis_a_read_mfm
};