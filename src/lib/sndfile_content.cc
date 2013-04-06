/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <libcxml/cxml.h>
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "compose.hpp"
#include "job.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using boost::shared_ptr;
using boost::lexical_cast;

SndfileContent::SndfileContent (boost::filesystem::path f)
	: Content (f)
	, AudioContent (f)
{

}

SndfileContent::SndfileContent (shared_ptr<const cxml::Node> node)
	: Content (node)
	, AudioContent (node)
{
	_audio_channels = node->number_child<int> ("AudioChannels");
	_audio_length = node->number_child<ContentAudioFrame> ("AudioLength");
	_audio_frame_rate = node->number_child<int> ("AudioFrameRate");
}

string
SndfileContent::summary () const
{
	return String::compose (_("Sound file: %1"), file().filename ());
}

string
SndfileContent::information () const
{
	if (_audio_frame_rate == 0) {
		return "";
	}
	
	stringstream s;

	s << String::compose (
		_("%1 channels, %2kHz, %3 samples"),
		audio_channels(),
		audio_frame_rate() / 1000.0,
		audio_length()
		);
	
	return s.str ();
}

bool
SndfileContent::valid_file (boost::filesystem::path f)
{
	/* XXX: more extensions */
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".aif" || ext == ".aiff");
}

shared_ptr<Content>
SndfileContent::clone () const
{
	return shared_ptr<Content> (new SndfileContent (*this));
}

void
SndfileContent::examine (shared_ptr<Film> film, shared_ptr<Job> job, bool quick)
{
	job->set_progress_unknown ();
	Content::examine (film, job, quick);

	SndfileDecoder dec (film, shared_from_this());

	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_channels = dec.audio_channels ();
		_audio_length = dec.audio_length ();
		_audio_frame_rate = dec.audio_frame_rate ();
	}

	signal_changed (AudioContentProperty::AUDIO_CHANNELS);
	signal_changed (AudioContentProperty::AUDIO_LENGTH);
	signal_changed (AudioContentProperty::AUDIO_FRAME_RATE);
}

void
SndfileContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("Sndfile");
	Content::as_xml (node);
	node->add_child("AudioChannels")->add_child_text (lexical_cast<string> (_audio_channels));
	node->add_child("AudioLength")->add_child_text (lexical_cast<string> (_audio_length));
	node->add_child("AudioFrameRate")->add_child_text (lexical_cast<string> (_audio_frame_rate));
}
