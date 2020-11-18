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


#include "dcpomatic_button.h"
#include "language_tag_dialog.h"
#include "language_tag_widget.h"
#include "wx_util.h"
#include <wx/wx.h>


LanguageTagWidget::LanguageTagWidget (wxWindow* parent, wxString tooltip, dcp::LanguageTag tag)
	: _parent (parent)
	, _sizer (new wxBoxSizer(wxHORIZONTAL))
{
	_language = new wxStaticText (parent, wxID_ANY, wxT(""));
	_language->SetToolTip (tooltip);
	set (tag);
	_sizer->Add (_language, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	_edit = new Button (parent, _("Edit..."));
	_sizer->Add (_edit, 0, wxLEFT, DCPOMATIC_SIZER_GAP);

	_edit->Bind (wxEVT_BUTTON, boost::bind(&LanguageTagWidget::edit, this));
}


void
LanguageTagWidget::edit ()
{
	LanguageTagDialog* d = new LanguageTagDialog(_parent, _tag);
	d->ShowModal ();
	set (d->get());
	Changed (d->get());
	d->Destroy ();
}


void
LanguageTagWidget::set (dcp::LanguageTag tag)
{
	_tag = tag;
	checked_set (_language, std_to_wx(tag.to_string()));
}


void
LanguageTagWidget::enable (bool e)
{
	_language->Enable (e);
	_edit->Enable (e);
}
