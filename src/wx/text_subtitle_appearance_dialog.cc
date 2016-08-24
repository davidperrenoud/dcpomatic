/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

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

#include "text_subtitle_appearance_dialog.h"
#include "lib/text_subtitle_content.h"
#include "lib/subtitle_content.h"
#include <wx/wx.h>
#include <wx/clrpicker.h>

using boost::shared_ptr;

TextSubtitleAppearanceDialog::TextSubtitleAppearanceDialog (wxWindow* parent, shared_ptr<Content> content)
	: TableDialog (parent, _("Subtitle appearance"), 2, 1, true)
	, _content (content)
{
	add (_("Colour"), true);
	_colour = new wxColourPickerCtrl (this, wxID_ANY);
	add (_colour);

	add (_("Effect"), true);
	add (_effect = new wxChoice (this, wxID_ANY));

	add (_("Outline / shadow colour"), true);
	_effect_colour = new wxColourPickerCtrl (this, wxID_ANY);
	add (_effect_colour);

	add (_("Fade in time"), true);
	_fade_in = new Timecode<ContentTime> (this);
	add (_fade_in);

	add (_("Fade out time"), true);
	_fade_out = new Timecode<ContentTime> (this);
	add (_fade_out);

	layout ();

	_effect->Append (_("None"));
	_effect->Append (_("Outline"));
	_effect->Append (_("Shadow"));;

	_colour->SetColour (wxColour (_content->subtitle->colour().r, _content->subtitle->colour().g, _content->subtitle->colour().b));
	if (_content->subtitle->outline()) {
		_effect->SetSelection (1);
	} else if (_content->subtitle->shadow()) {
		_effect->SetSelection (2);
	} else {
		_effect->SetSelection (0);
	}
	_effect_colour->SetColour (
		wxColour (_content->subtitle->effect_colour().r, _content->subtitle->effect_colour().g, _content->subtitle->effect_colour().b)
		);
	_fade_in->set (_content->subtitle->fade_in(), _content->active_video_frame_rate ());
	_fade_out->set (_content->subtitle->fade_out(), _content->active_video_frame_rate ());
}

void
TextSubtitleAppearanceDialog::apply ()
{
	wxColour const c = _colour->GetColour ();
	_content->subtitle->set_colour (dcp::Colour (c.Red(), c.Green(), c.Blue()));
	_content->subtitle->set_outline (_effect->GetSelection() == 1);
	_content->subtitle->set_shadow (_effect->GetSelection() == 2);
	wxColour const ec = _effect_colour->GetColour ();
	_content->subtitle->set_effect_colour (dcp::Colour (ec.Red(), ec.Green(), ec.Blue()));
	_content->subtitle->set_fade_in (_fade_in->get (_content->active_video_frame_rate ()));
	_content->subtitle->set_fade_out (_fade_out->get (_content->active_video_frame_rate ()));
}
