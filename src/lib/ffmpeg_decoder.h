/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/ffmpeg_decoder.h
 *  @brief A decoder using FFmpeg to decode content.
 */

#include <vector>
#include <string>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libpostproc/postprocess.h>
}
#include "util.h"
#include "decoder.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "film.h"

struct AVFilterGraph;
struct AVCodecContext;
struct AVFilterContext;
struct AVFormatContext;
struct AVFrame;
struct AVBufferContext;
struct AVCodec;
struct AVStream;
class Job;
class Options;
class Image;
class Log;

class FFmpegAudioStream : public AudioStream
{
public:
	FFmpegAudioStream (std::string n, int i, int s, int64_t c)
		: AudioStream (s, c)
		, _name (n)
		, _id (i)
	{}

	std::string to_string () const;

	std::string name () const {
		return _name;
	}

	int id () const {
		return _id;
	}

	static boost::shared_ptr<FFmpegAudioStream> create (std::string t, boost::optional<int> v);

private:
	friend class stream_test;
	
	FFmpegAudioStream (std::string t, boost::optional<int> v);
	
	std::string _name;
	int _id;
};

/** @class FFmpegDecoder
 *  @brief A decoder using FFmpeg to decode content.
 */
class FFmpegDecoder : public VideoDecoder, public AudioDecoder
{
public:
	FFmpegDecoder (boost::shared_ptr<Film>, DecodeOptions);
	~FFmpegDecoder ();

	float frames_per_second () const;
	libdcp::Size native_size () const;
	SourceFrame length () const;
	int time_base_numerator () const;
	int time_base_denominator () const;
	int sample_aspect_ratio_numerator () const;
	int sample_aspect_ratio_denominator () const;

	void set_audio_stream (boost::shared_ptr<AudioStream>);
	void set_subtitle_stream (boost::shared_ptr<SubtitleStream>);

	bool seek (double);
	bool seek_to_last ();
	void seek_forward ();
	void seek_back ();

private:

	bool pass ();
	bool do_seek (double p, bool, bool);
	PixelFormat pixel_format () const;
	AVSampleFormat audio_sample_format () const;
	int bytes_per_audio_sample () const;

	void filter_and_emit_video ();

	void setup_general ();
	void setup_video ();
	void setup_audio ();
	void setup_subtitle ();

	void decode_audio_packet ();

	void maybe_add_subtitle ();
	boost::shared_ptr<AudioBuffers> deinterleave_audio (uint8_t** data, int size);

	void film_changed (Film::Property);

	std::string stream_name (AVStream* s) const;

	AVFormatContext* _format_context;
	int _video_stream;
	
	AVFrame* _frame;

	AVCodecContext* _video_codec_context;
	AVCodec* _video_codec;
	AVCodecContext* _audio_codec_context;    ///< may be 0 if there is no audio
	AVCodec* _audio_codec;                   ///< may be 0 if there is no audio
	AVCodecContext* _subtitle_codec_context; ///< may be 0 if there is no subtitle
	AVCodec* _subtitle_codec;                ///< may be 0 if there is no subtitle

	AVPacket _packet;

	std::list<boost::shared_ptr<FilterGraph> > _filter_graphs;
	boost::mutex _filter_graphs_mutex;
};
