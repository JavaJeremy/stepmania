/*
 * SDL_sound -- An abstract sound format decoding API.
 * Copyright (C) 2001  Ryan C. Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * WAV decoder for SDL_sound.
 *
 * This file written by Ryan C. Gordon. (icculus@clutteredmind.org)
 */

#include <global.h>
#include "RageSoundReader_WAV.h"
#include "RageLog.h"
#include "RageUtil.h"

#include <stdio.h>
#include <errno.h>
#include <SDL_endian.h>

#define BAIL_IF_MACRO(c, e, r) if (c) { SetError(e); return r; }
#define RETURN_IF_MACRO(c, r) if (c) return r;

#define riffID 0x46464952  /* "RIFF", in ascii. */
#define waveID 0x45564157  /* "WAVE", in ascii. */
#define fmtID  0x20746D66  /* "fmt ", in ascii. */
#define dataID 0x61746164  /* "data", in ascii. */

enum
{
	FMT_NORMAL=1,     /* Uncompressed waveform data.     */
	FMT_ADPCM =2    /* ADPCM compressed waveform data. */
};

struct ADPCMCOEFSET
{
	Sint16 iCoef1, iCoef2;
};

struct ADPCMBLOCKHEADER
{
	Uint8 bPredictor;
	Uint16 iDelta;
	Sint16 iSamp[2];
};

/* Call this to convert milliseconds to an actual byte position, based on audio data characteristics. */
Uint32 RageSoundReader_WAV::ConvertMsToBytePos(int BytesPerSample, int channels, Uint32 ms) const
{
    const float frames_per_ms = ((float) SampleRate) / 1000.0f;
    const Uint32 frame_offset = (Uint32) (frames_per_ms * ((float) ms));
    const Uint32 frame_size = (Uint32) BytesPerSample * channels;
    return frame_offset * frame_size;
}

Uint32 RageSoundReader_WAV::ConvertBytePosToMs(int BytesPerSample, int channels, Uint32 pos) const
{
    const Uint32 frame_size = (Uint32) BytesPerSample * channels;
    const Uint32 frame_no = pos / frame_size;
    const float frames_per_ms = ((float) SampleRate) / 1000.0f;
    return (Uint32) (frame_no / frames_per_ms);
}

/* Better than SDL_ReadLE16, since you can detect i/o errors... */
bool RageSoundReader_WAV::read_le16(FILE *rw, Sint16 *si16) const
{
    int rc = fread( si16, sizeof (Sint16), 1, rw );
	if( rc != 1 )
	{
		SetError( feof(rw)? "end of file": strerror(errno) );
		return false;
	}
    *si16 = SDL_SwapLE16( *si16 );
    return true;
}

bool RageSoundReader_WAV::read_le16(FILE *rw, Uint16 *ui16) const
{
    int rc = fread( ui16, sizeof (Uint16), 1, rw );
	if( rc != 1 )
	{
		SetError( feof(rw)? "end of file": strerror(errno) );
		return false;
	}
    *ui16 = SDL_SwapLE16(*ui16);
    return true;
}


/* Better than SDL_ReadLE32, since you can detect i/o errors... */
bool RageSoundReader_WAV::read_le32(FILE *rw, Sint32 *si32) const
{
    int rc = fread( si32, sizeof (Sint32), 1, rw );
	if( rc != 1 )
	{
		SetError( feof(rw)? "end of file": strerror(errno) );
		return false;
	}
    *si32 = SDL_SwapLE32( *si32 );
    return true;
}

bool RageSoundReader_WAV::read_le32(FILE *rw, Uint32 *ui32) const
{
    int rc = fread( ui32, sizeof (Uint32), 1, rw );
	if( rc != 1 )
	{
		SetError( feof(rw)? "end of file": strerror(errno) );
		return false;
	}
    *ui32 = SDL_SwapLE32( *ui32 );
    return true;
}

bool RageSoundReader_WAV::read_uint8(FILE *rw, Uint8 *ui8) const
{
    int rc = fread( ui8, sizeof (Uint8), 1, rw );
	if( rc != 1 )
	{
		SetError( feof(rw)? "end of file": strerror(errno) );
		return false;
	}
    return true;
}


bool RageSoundReader_WAV::read_fmt_chunk()
{
    RETURN_IF_MACRO(!read_le16(rw, &fmt.wFormatTag), false);
    RETURN_IF_MACRO(!read_le16(rw, &fmt.wChannels), false);
    RETURN_IF_MACRO(!read_le32(rw, &SampleRate), false);
    RETURN_IF_MACRO(!read_le32(rw, &fmt.dwAvgBytesPerSec), false);
    RETURN_IF_MACRO(!read_le16(rw, &fmt.wBlockAlign), false);
    RETURN_IF_MACRO(!read_le16(rw, &fmt.wBitsPerSample), false);

    if( fmt.wFormatTag == FMT_ADPCM )
    {
		memset(&adpcm, '\0', sizeof (adpcm));

		RETURN_IF_MACRO(!read_le16(rw, &adpcm.cbSize), false);
		RETURN_IF_MACRO(!read_le16(rw, &adpcm.wSamplesPerBlock), false);
		RETURN_IF_MACRO(!read_le16(rw, &adpcm.wNumCoef), false);

		adpcm.aCoef = new ADPCMCOEFSET[adpcm.wNumCoef];

		for (int i = 0; i < adpcm.wNumCoef; i++)
		{
			RETURN_IF_MACRO(!read_le16(rw, &adpcm.aCoef[i].iCoef1), false);
			RETURN_IF_MACRO(!read_le16(rw, &adpcm.aCoef[i].iCoef2), false);
		}

		adpcm.blockheaders = new ADPCMBLOCKHEADER[fmt.wChannels];
	}

    return true;
}


int RageSoundReader_WAV::read_sample_fmt_normal(char *buf, unsigned len)
{
    int ret = fread( buf, 1, len, this->rw );
	if (ret == -1)
	{
		SetError( strerror(errno) );
		return -1;
	}

    return ret;
}


int RageSoundReader_WAV::seek_sample_fmt_normal( Uint32 ms )
{
    const int offset = ConvertMsToBytePos( BytesPerSample, Channels, ms);
    const int pos = (int) (this->fmt.data_starting_offset + offset);

	int rc = fseek( this->rw, pos, SEEK_SET );
    if( rc != pos )
		return -1;

    return ms;
}

int RageSoundReader_WAV::get_length_fmt_adpcm() const
{
	int offset = fseek(this->rw, 0, SEEK_END);
    BAIL_IF_MACRO( offset == -1, strerror(errno), -1 );

    offset -= fmt.data_starting_offset;

	/* pcm bytes per block */
	const int bpb = (adpcm.wSamplesPerBlock * fmt.adpcm_sample_frame_size);
    const int blockno = offset / fmt.wBlockAlign;
    const int byteno = blockno * bpb;

    /* Seek back to the beginning of the last frame and find out how long it really is. */
	fseek( this->rw, blockno * fmt.wBlockAlign + fmt.data_starting_offset, SEEK_SET );

	/* Don't mess up this->adpcm; we'll put the cursor back as if nothing happened. */
	struct adpcm_t tmp_adpcm;
	if ( !read_adpcm_block_headers(tmp_adpcm) )
		return 0;

	return ConvertBytePosToMs( BytesPerSample, Channels, byteno) + 
		   ConvertBytePosToMs( BytesPerSample, Channels, tmp_adpcm.samples_left_in_block * fmt.adpcm_sample_frame_size);
}


int RageSoundReader_WAV::get_length_fmt_normal() const
{
    int ret = fseek( this->rw, 0, SEEK_END );
    BAIL_IF_MACRO( ret == -1, strerror(errno), -1 );
    int offset = ftell( this->rw );

	LOG->Trace("offs %i, st %i, pos %i, bps %i, chan %i, ret %i", 
		offset, this->fmt.data_starting_offset,
		offset - this->fmt.data_starting_offset, BytesPerSample, Channels,
		ConvertBytePosToMs( BytesPerSample, Channels, offset - this->fmt.data_starting_offset));
    return ConvertBytePosToMs( BytesPerSample, Channels, offset - this->fmt.data_starting_offset);
}

#define FIXED_POINT_COEF_BASE      256
#define FIXED_POINT_ADAPTION_BASE  256
#define SMALLEST_ADPCM_DELTA       16

bool RageSoundReader_WAV::read_adpcm_block_headers( adpcm_t &out ) const
{
    ADPCMBLOCKHEADER *headers = out.blockheaders;

    int i;
    for (i = 0; i < fmt.wChannels; i++)
        RETURN_IF_MACRO(!read_uint8(rw, &headers[i].bPredictor), false);

    for (i = 0; i < fmt.wChannels; i++)
        RETURN_IF_MACRO(!read_le16(rw, &headers[i].iDelta), false);

    for (i = 0; i < fmt.wChannels; i++)
        RETURN_IF_MACRO(!read_le16(rw, &headers[i].iSamp[0]), false);

    for (i = 0; i < fmt.wChannels; i++)
        RETURN_IF_MACRO(!read_le16(rw, &headers[i].iSamp[1]), false);

    out.samples_left_in_block = out.wSamplesPerBlock;
    out.nibble_state = 0;
    return true;
}


void RageSoundReader_WAV::do_adpcm_nibble(Uint8 nib, ADPCMBLOCKHEADER *header, Sint32 lPredSamp)
{
	static const Sint32 max_audioval = ((1<<(16-1))-1);
	static const Sint32 min_audioval = -(1<<(16-1));
	static const Sint32 AdaptionTable[] =
    {
		230, 230, 230, 230, 307, 409, 512, 614,
		768, 614, 512, 409, 307, 230, 230, 230
	};

    Sint32 lNewSamp = lPredSamp;

    if (nib & 0x08)
        lNewSamp += header->iDelta * (nib - 0x10);
	else
        lNewSamp += header->iDelta * nib;

	lNewSamp = clamp(lNewSamp, min_audioval, max_audioval);

    Sint32 delta = ((Sint32) header->iDelta * AdaptionTable[nib]) /
              FIXED_POINT_ADAPTION_BASE;

	delta = max( delta, SMALLEST_ADPCM_DELTA );

    header->iDelta = Sint16(delta);
	header->iSamp[1] = header->iSamp[0];
	header->iSamp[0] = Sint16(lNewSamp);
}


bool RageSoundReader_WAV::decode_adpcm_sample_frame()
{
	ADPCMBLOCKHEADER *headers = adpcm.blockheaders;

	Uint8 nib = adpcm.nibble;
	for (int i = 0; i < this->fmt.wChannels; i++)
	{
		const Sint16 iCoef1 = adpcm.aCoef[headers[i].bPredictor].iCoef1;
		const Sint16 iCoef2 = adpcm.aCoef[headers[i].bPredictor].iCoef2;
		const Sint32 lPredSamp = ((headers[i].iSamp[0] * iCoef1) +
			(headers[i].iSamp[1] * iCoef2)) / FIXED_POINT_COEF_BASE;

		if (adpcm.nibble_state == 0)
		{
			if( !read_uint8(this->rw, &nib) )
				return false;
			adpcm.nibble_state = 1;
			do_adpcm_nibble(nib >> 4, &headers[i], lPredSamp);
		}
		else
		{
			adpcm.nibble_state = 0;
			do_adpcm_nibble(nib & 0x0F, &headers[i], lPredSamp);
		}
	}

	adpcm.nibble = nib;
	return true;
}


void RageSoundReader_WAV::put_adpcm_sample_frame( Uint16 *buf, int frame )
{
    ADPCMBLOCKHEADER *headers = adpcm.blockheaders;
    for (int i = 0; i < fmt.wChannels; i++)
        *(buf++) = headers[i].iSamp[frame];
}


Uint32 RageSoundReader_WAV::read_sample_fmt_adpcm(char *buf, unsigned len)
{
    Uint32 bw = 0;

    while (bw < len)
    {
        /* write ongoing sample frame before reading more data... */
        switch (this->adpcm.samples_left_in_block)
        {
        case 0:  /* need to read a new block... */
            if (!read_adpcm_block_headers(adpcm))
                return bw;

            /* only write first sample frame for now. */
            put_adpcm_sample_frame( (Uint16 *) (buf + bw), 1 );
            adpcm.samples_left_in_block--;
            bw += this->fmt.adpcm_sample_frame_size;
            break;

        case 1:  /* output last sample frame of block... */
            put_adpcm_sample_frame( (Uint16 *) (buf + bw), 0 );
            this->adpcm.samples_left_in_block--;
            bw += this->fmt.adpcm_sample_frame_size;
            break;

        default: /* output latest sample frame and read a new one... */
            put_adpcm_sample_frame( (Uint16 *) (buf + bw), 0 );
            this->adpcm.samples_left_in_block--;
            bw += this->fmt.adpcm_sample_frame_size;

            if (!decode_adpcm_sample_frame())
                return bw;
        }
    }

    return bw;
}



int RageSoundReader_WAV::seek_sample_fmt_adpcm( Uint32 ms )
{
	const int offset = ConvertMsToBytePos( BytesPerSample, Channels, ms );
	const int bpb = (adpcm.wSamplesPerBlock * this->fmt.adpcm_sample_frame_size);
	int skipsize = (offset / bpb) * this->fmt.wBlockAlign;

	const int pos = skipsize + this->fmt.data_starting_offset;
	int rc = fseek(this->rw, pos, SEEK_SET);
	BAIL_IF_MACRO(rc == -1, strerror(errno), 0);
	BAIL_IF_MACRO(rc != pos, "end of file", 0);

	/* The offset we need is in this block, so we need to decode to there. */
	skipsize += (offset % bpb);
	rc = (offset % bpb);  /* bytes into this block we need to decode */
	if (!read_adpcm_block_headers(adpcm))
	{
		fseek(this->rw, 0, SEEK_SET);
		adpcm.samples_left_in_block = 0;
		return 0;
	}

	/* first sample frame of block is a freebie. :) */
	adpcm.samples_left_in_block--;
	rc -= this->fmt.adpcm_sample_frame_size;
	while (rc > 0)
	{
		if (!decode_adpcm_sample_frame())
		{
			fseek(this->rw, 0, SEEK_SET);
			adpcm.samples_left_in_block = 0;
			return 0;
		}

		adpcm.samples_left_in_block--;
		rc -= this->fmt.adpcm_sample_frame_size;
	}

	return ms;
}


/* Locate a chunk by ID. */
int RageSoundReader_WAV::find_chunk( Uint32 id, Sint32 &size )
{
	Uint32 pos = ftell(rw);
	while (1)
	{
		Uint32 id_ = 0;
		if( !read_le32(rw, &id_) )
			return false;
		if( !read_le32(rw, &size) )
			return false;

		if (id_ == id)
			return true;

		if(size < 0)
			return false;

		pos += (sizeof (Uint32) * 2) + size;
		int ret = fseek(rw, pos, SEEK_SET);
		if( ret == -1 )
		{
			SetError( strerror(errno) );
			return false;
		}
	}
}


SoundReader_FileReader::OpenResult RageSoundReader_WAV::WAV_open_internal()
{
	Uint32 magic1;
	if( !read_le32(rw, &magic1) || magic1 != riffID )
	{
		SetError( "WAV: Not a RIFF file." );
		return OPEN_NO_MATCH;
	}

	Uint32 ignore;
	read_le32(rw, &ignore); /* throw the length away; we get this info later. */

	Uint32 magic2;
	if( !read_le32( rw, &magic2 ) || magic2 != waveID )
	{
		SetError( "Not a WAVE file." );
		return OPEN_NO_MATCH;
	}

	Sint32 NextChunk;
    BAIL_IF_MACRO(!find_chunk(fmtID, NextChunk), "No format chunk.", OPEN_MATCH_BUT_FAIL);
	NextChunk += ftell(rw);
    BAIL_IF_MACRO(!read_fmt_chunk(), "Can't read format chunk.", OPEN_MATCH_BUT_FAIL);

	/* I think multi-channel WAVs are possible, but I've never even seen one. */
    Channels = (Uint8) fmt.wChannels;
	ASSERT( Channels <= 2 );

	if( fmt.wFormatTag != FMT_NORMAL &&
		fmt.wFormatTag != FMT_ADPCM )
	{
		SetError( ssprintf("Unsupported WAV format %i", fmt.wFormatTag) );

		/* It might be MP3 data in a WAV.  (Why do people *do* that?)  It's possible
		 * that the MAD decoder will figure that out, so let's return OPEN_NO_MATCH
		 * and keep searching for a decoder. */
		return OPEN_NO_MATCH;
	}

    if ( fmt.wBitsPerSample == 4 && this->fmt.wFormatTag == FMT_ADPCM )
	{
		Conversion = CONV_NONE;
		BytesPerSample = 2;
	}
    else if (fmt.wBitsPerSample == 8)
	{
		Conversion = CONV_8BIT_TO_16BIT;
		BytesPerSample = 1;
	}
    else if (fmt.wBitsPerSample == 16)
	{
		Conversion = CONV_16LSB_TO_16SYS;
		BytesPerSample = 2;
	}
    else
    {
		SetError( ssprintf("Unsupported sample size %i", fmt.wBitsPerSample) );
		return OPEN_MATCH_BUT_FAIL;
    }

	if( Conversion == CONV_8BIT_TO_16BIT )
		Input_Buffer_Ratio *= 2;
	if( Channels == 1 )
		Input_Buffer_Ratio *= 2;

	fseek(rw, NextChunk, SEEK_SET );

	Sint32 DataSize;
    BAIL_IF_MACRO(!find_chunk(dataID, DataSize), "No data chunk.", OPEN_MATCH_BUT_FAIL);

    fmt.data_starting_offset = ftell(rw);
    fmt.adpcm_sample_frame_size = BytesPerSample * Channels;

    return OPEN_OK;
}


SoundReader_FileReader::OpenResult RageSoundReader_WAV::Open( CString filename_ )
{
	Close();
	Input_Buffer_Ratio = 1;
	filename = filename_;
    rw = fopen(filename, "rb");

    memset(&fmt, 0, sizeof(fmt));

    SoundReader_FileReader::OpenResult rc = WAV_open_internal();
    if ( rc != OPEN_OK )
		Close();

    return rc;
}


void RageSoundReader_WAV::Close()
{
	delete [] this->adpcm.aCoef;
	this->adpcm.aCoef = NULL;
	delete [] this->adpcm.blockheaders;
	this->adpcm.blockheaders = NULL;

	if( rw )
		fclose( rw );
	rw = NULL;
}


int RageSoundReader_WAV::Read(char *buf, unsigned len)
{
	/* Input_Buffer_Ratio is always 2 or 4.  Make sure len is always a multiple of
	 * Input_Buffer_Ratio; handling extra bytes is a pain and useless. */
	ASSERT( (len % Input_Buffer_Ratio) == 0);

	int ActualLen = len / Input_Buffer_Ratio;
	int ret = 0;
	switch (this->fmt.wFormatTag)
	{
	case FMT_NORMAL:
		ret = read_sample_fmt_normal( buf, ActualLen );
		break;
	case FMT_ADPCM:
		ret = read_sample_fmt_adpcm( buf, ActualLen );
		break;
	default: ASSERT(0); break;
	}

	if( ret <= 0 )
		return ret;

	if( Conversion == CONV_16LSB_TO_16SYS )
	{
		/* Do this in place. */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		const int cnt = len / sizeof(Sint16);
		Sint16 *tbuf = (Sint16 *) buf;
		for( int i = 0; i < cnt; ++i )
			tbuf[i] = SDL_Swap16( tbuf[i] );
#endif
	}

	static Sint16 *tmpbuf = NULL;
	static unsigned tmpbufsize = 0;
	if( len > tmpbufsize )
	{
		tmpbufsize = len;
		delete [] tmpbuf;
		tmpbuf = new Sint16[len];
	}
	if( Conversion == CONV_8BIT_TO_16BIT )
	{
		for( int s = 0; s < ret; ++s )
			tmpbuf[s] = (Sint16(buf[s])-128) << 8;
		memcpy( buf, tmpbuf, ret * sizeof(Sint16) );
		ret *= 2; /* 8-bit to 16-bit */
	}

	if( Channels == 1 )
	{
		Sint16 *in = (Sint16*) buf;
		for( int s = 0; s < ret/2; ++s )
			tmpbuf[s*2] = tmpbuf[s*2+1] = in[s];
		memcpy( buf, tmpbuf, ret * sizeof(Sint16) );
		ret *= 2; /* 1 channel -> 2 channels */
	}

	return ret;
}


int RageSoundReader_WAV::SetPosition(int ms)
{
	switch (this->fmt.wFormatTag)
	{
	case FMT_NORMAL:
		return seek_sample_fmt_normal( ms );
	case FMT_ADPCM:
		return seek_sample_fmt_adpcm( ms );
	}
	ASSERT(0);
	return -1;
}

int RageSoundReader_WAV::GetLength() const
{
    const int origpos = ftell( this->rw );
	
	int ret = 0;
	switch (this->fmt.wFormatTag)
	{
	case FMT_NORMAL:
		ret = get_length_fmt_normal();
		break;
	case FMT_ADPCM:
		ret = get_length_fmt_adpcm();
		break;
	}

	int rc = fseek( this->rw, origpos, SEEK_SET );
    BAIL_IF_MACRO( rc == -1, strerror(errno), -1 );

	return ret;
}

RageSoundReader_WAV::RageSoundReader_WAV()
{
	this->adpcm.aCoef = NULL;
	this->adpcm.blockheaders = NULL;
	rw = NULL;
}

SoundReader *RageSoundReader_WAV::Copy() const
{
	RageSoundReader_WAV *ret = new RageSoundReader_WAV;
	ret->Open( filename );
	return ret;
}

RageSoundReader_WAV::~RageSoundReader_WAV()
{
	Close();
}
