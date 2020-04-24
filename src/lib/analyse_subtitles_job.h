/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "job.h"
#include "types.h"
#include "player_text.h"

class Film;
class Content;

class AnalyseSubtitlesJob : public Job
{
public:
	AnalyseSubtitlesJob (boost::shared_ptr<const Film> film, boost::shared_ptr<Content> content);

	std::string name () const;
	std::string json_name () const;
	void run ();

	boost::filesystem::path path () const {
		return _path;
	}

private:
	void analyse (PlayerText text, TextType type);

	boost::weak_ptr<Content> _content;
	boost::filesystem::path _path;
	boost::optional<dcpomatic::Rect<double> > _bounding_box;
};
