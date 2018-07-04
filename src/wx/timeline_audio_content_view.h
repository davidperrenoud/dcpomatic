/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "timeline_content_view.h"

/** @class TimelineAudioContentView
 *  @brief Timeline view for AudioContent.
 */
class TimelineAudioContentView : public TimelineContentView
{
public:
	TimelineAudioContentView (Timeline& tl, boost::shared_ptr<Content> c);

private:
	bool active () const {
		return true;
	}
	wxColour background_colour () const;
	wxColour foreground_colour () const;
	wxString label () const;
};
