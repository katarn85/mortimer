/*
 * BSD LICENSE
 *
 * Copyright (c) 2011-2012, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LGPL LICENSE
 *
 * tinycompress library for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation.
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to
 * the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef __TINYCOMPRESS_H
#define __TINYCOMPRESS_H

#if defined(__cplusplus)
extern "C" {
#endif
#include <stdbool.h>
#include "spdif_compress_offload.h"

#define COMPR_ERR_MAX 128
#define COMPRESS_OUT        0x20000000
#define COMPRESS_IN         0x10000000


/*
 * struct compr_config: config structure, needs to be filled by app
 * If fragment_size or fragments are zero, this means "don't care"
 * and tinycompress will choose values that the driver supports
 *
 * @fragment_size: size of fragment requested, in bytes
 * @fragments: number of fragments
 * @codec: codec type and parameters requested
 */
struct compr_config {
	__u32 fragment_size;
	__u32 fragments;
	struct snd_codec *codec;
};

struct compr_gapless_mdata {
	__u32 encoder_delay;
	__u32 encoder_padding;
};

struct compr_sdp_time_stamp {
	__u32 low_pts;
	__u32 high_pts;

};

struct compress {
	int fd;
	unsigned int flags;
	char error[COMPR_ERR_MAX];
	struct compr_config *config;
	int running;
	int max_poll_wait_ms;
	int nonblocking;
	unsigned int gapless_metadata;
	unsigned int next_track;
};




//struct compress;
struct snd_compr_tstamp;

/*
 * compress_open: open a new compress stream
 * returns the valid struct compress on success, NULL on failure
 * If config does not specify a requested fragment size, on return
 * it will be updated with the size and number of fragments that
 * were configured
 *
 * @card: sound card number
 * @device: device number
 * @flags: device flags can be COMPRESS_OUT or COMPRESS_IN
 * @config: stream config requested. Returns actual fragment config
 */
struct compress *compress_open(unsigned int card, unsigned int device,
		unsigned int flags, struct compr_config *config);

/*
 * compress_close: close the compress stream
 *
 * @compress: compress stream to be closed
 */
void compress_close(struct compress *compress);

/*
 * compress_get_hpointer: get the hw timestamp
 * return 0 on success, negative on error
 *
 * @compress: compress stream on which query is made
 * @avail: buffer availble for write/read, in bytes
 * @tstamp: hw time
 */
int compress_get_hpointer(struct compress *compress,
		unsigned int *avail, struct timespec *tstamp);


/*
 * compress_get_tstamp: get the raw hw timestamp
 * return 0 on success, negative on error
 *
 * @compress: compress stream on which query is made
 * @samples: number of decoded samples played
 * @sampling_rate: sampling rate of decoded samples
 */
int compress_get_tstamp(struct compress *compress,
		unsigned int *samples, unsigned int *sampling_rate);

/*
 * compress_write: write data to the compress stream
 * return bytes written on success, negative on error
 * By default this is a blocking call and will not return
 * until all bytes have been written or there was a
 * write error.
 * If non-blocking mode has been enabled with compress_nonblock(),
 * this function will write all bytes that can be written without
 * blocking and will then return the number of bytes successfully
 * written. If the return value is not an error and is < size
 * the caller can use compress_wait() to block until the driver
 * is ready for more data.
 *
 * @compress: compress stream to be written to
 * @buf: pointer to data
 * @size: number of bytes to be written
 */
int compress_write(struct compress *compress, const void *buf, unsigned int size);

/*
 * compress_read: read data from the compress stream
 * return bytes read on success, negative on error
 * By default this is a blocking call and will block until
 * size bytes have been written or there was a read error.
 * If non-blocking mode was enabled using compress_nonblock()
 * the behaviour will change to read only as many bytes as
 * are currently available (if no bytes are available it
 * will return immediately). The caller can then use
 * compress_wait() to block until more bytes are available.
 *
 * @compress: compress stream from where data is to be read
 * @buf: pointer to data buffer
 * @size: size of given buffer
 */
int compress_read(struct compress *compress, void *buf, unsigned int size);

/*
 * compress_start: start the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be started
 */
int compress_start(struct compress *compress);

/*
 * compress_stop: stop the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be stopped
 */
int compress_stop(struct compress *compress);

/*
 * compress_pause: pause the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be paused
 */
int compress_pause(struct compress *compress);

/*
 * compress_resume: resume the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be resumed
 */
int compress_resume(struct compress *compress);

/*
 * compress_drain: drain the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be drain
 */
int compress_drain(struct compress *compress);

/*
 * compress_next_track: set the next track for stream
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be transistioned to next track
 */
int compress_next_track(struct compress *compress);

/*
 * compress_partial_drain: drain will return after the last frame is decoded
 * by DSP and will play the , All the data written into compressed
 * ring buffer is decoded
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be drain
 */
int compress_partial_drain(struct compress *compress);

/*
 * compress_set_gapless_metadata: set gapless metadata of a compress strem
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream for which metadata has to set
 * @mdata: metadata encoder delay and  padding
 */

int compress_set_gapless_metadata(struct compress *compress,
			struct compr_gapless_mdata *mdata);

/*
 * is_codec_supported:check if the given codec is supported
 * returns true when supported, false if not
 *
 * @card: sound card number
 * @device: device number
 * @flags: stream flags
 * @codec: codec type and parameters to be checked
 */
bool is_codec_supported(unsigned int card, unsigned int device,
	       unsigned int flags, struct snd_codec *codec);

/*
 * compress_set_max_poll_wait: set the maximum time tinycompress
 * will wait for driver to signal a poll(). Interval is in
 * milliseconds.
 * Pass interval of -1 to disable timeout and make poll() wait
 * until driver signals.
 * If this function is not used the timeout defaults to 20 seconds.
 */
void compress_set_max_poll_wait(struct compress *compress, int milliseconds);

/* Enable or disable non-blocking mode for write and read */
void compress_nonblock(struct compress *compress, int nonblock);

/* Wait for ring buffer to ready for next read or write */
int compress_wait(struct compress *compress, int timeout_ms);

int is_compress_running(struct compress *compress);

int is_compress_ready(struct compress *compress);

/* Returns a human readable reason for the last error */
const char *compress_get_error(struct compress *compress);

/* Returns a codec capabilitiesr */

int compress_get_codec_caps(struct compress *compress, struct snd_compr_codec_caps *codec_caps);

int compress_get_pts(struct compress *compress, 
	struct compr_sdp_time_stamp *mdata);

int compress_set_pts(struct compress *compress, 
	struct compr_sdp_time_stamp *mdata);

int compress_set_tz_handle(struct compress *compress, unsigned int tz_dec);

int compress_get_tz_handle(struct compress *compress, void *tz_dec);
int compress_get_tz_phys_addr(struct compress *compress, unsigned int *tz_phy_addr);
int compress_pcm_read(struct compress *compress, void *buf, unsigned int size);



#endif