/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "dcpomatic_assert.h"
#include "font.h"
#include "font_config.h"
#include "image.h"
#include "render_text.h"
#include "types.h"
#include "util.h"
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
#include <cairomm/cairomm.h>
LIBDCP_DISABLE_WARNINGS
#include <pangomm.h>
LIBDCP_ENABLE_WARNINGS
#include <pango/pangocairo.h>
#include <boost/algorithm/string.hpp>
#include <iostream>


using std::cerr;
using std::cout;
using std::list;
using std::make_pair;
using std::make_shared;
using std::max;
using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using namespace dcpomatic;


/** Create a Pango layout using a dummy context which we can use to calculate the size
 *  of the text we will render.  Then we can transfer the layout over to the real context
 *  for the actual render.
 */
static Glib::RefPtr<Pango::Layout>
create_layout()
{
	auto c_font_map = pango_cairo_font_map_new ();
	DCPOMATIC_ASSERT (c_font_map);
	auto font_map = Glib::wrap (c_font_map);
	auto c_context = pango_font_map_create_context (c_font_map);
	DCPOMATIC_ASSERT (c_context);
	auto context = Glib::wrap (c_context);
	return Pango::Layout::create (context);
}


static void
setup_layout (Glib::RefPtr<Pango::Layout> layout, string font_name, string markup)
{
	layout->set_alignment (Pango::ALIGN_LEFT);
	Pango::FontDescription font (font_name);
	layout->set_font_description (font);
	layout->set_markup (markup);
}


string
marked_up (list<StringText> subtitles, int target_height, float fade_factor, string font_name)
{
	auto constexpr pixels_to_1024ths_point = 72 * 1024 / 96;

	auto make_span = [target_height, fade_factor](StringText const& subtitle, string text, string extra_attribute) {
		string span;
		span += "<span ";
		if (subtitle.italic()) {
			span += "style=\"italic\" ";
		}
		if (subtitle.bold()) {
			span += "weight=\"bold\" ";
		}
		if (subtitle.underline()) {
			span += "underline=\"single\" ";
		}
		span += "size=\"" + dcp::raw_convert<string>(lrintf(subtitle.size_in_pixels(target_height) * pixels_to_1024ths_point)) + "\" ";
		/* Between 1-65535 inclusive, apparently... */
		span += "alpha=\"" + dcp::raw_convert<string>(int(floor(fade_factor * 65534)) + 1) + "\" ";
		span += "color=\"#" + subtitle.colour().to_rgb_string() + "\"";
		if (!extra_attribute.empty()) {
			span += " " + extra_attribute;
		}
		span += ">";
		span += text;
		span += "</span>";
		return span;
	};

	string out;
	for (auto const& i: subtitles) {
		if (std::abs(i.space_before()) > dcp::SPACE_BEFORE_EPSILON) {
			/* We need to insert some horizontal space into the layout.  The only way I can find to do this
			 * is to write a " " with some special letter_spacing.  As far as I can see, such a space will
			 * be written with letter_spacing either side.  This means that to get a horizontal space x we
			 * need to write a " " with letter spacing (x - s) / 2, where s is the width of the " ".
			 */
			auto layout = create_layout();
			setup_layout(layout, font_name, make_span(i, " ", {}));
			int space_width;
			int dummy;
			layout->get_pixel_size(space_width, dummy);
			auto spacing = ((i.space_before() * i.size_in_pixels(target_height) - space_width) / 2) * pixels_to_1024ths_point;
			out += make_span(i, " ", "letter_spacing=\"" + dcp::raw_convert<string>(spacing) + "\"");
		}

		out += make_span(i, i.text(), {});
	}

	return out;
}


static void
set_source_rgba (Cairo::RefPtr<Cairo::Context> context, dcp::Colour colour, float fade_factor)
{
	context->set_source_rgba (float(colour.r) / 255, float(colour.g) / 255, float(colour.b) / 255, fade_factor);
}


static shared_ptr<Image>
create_image (dcp::Size size)
{
	/* FFmpeg BGRA means first byte blue, second byte green, third byte red, fourth byte alpha.
	 * This must be COMPACT as we're using it with Cairo::ImageSurface::create
	 */
	auto image = make_shared<Image>(AV_PIX_FMT_BGRA, size, Image::Alignment::COMPACT);
	image->make_black ();
	return image;
}


static Cairo::RefPtr<Cairo::ImageSurface>
create_surface (shared_ptr<Image> image)
{
	/* XXX: I don't think it's guaranteed that format_stride_for_width will return a stride without any padding,
	 * so it's lucky that this works.
	 */
	DCPOMATIC_ASSERT (image->alignment() == Image::Alignment::COMPACT);
	DCPOMATIC_ASSERT (image->pixel_format() == AV_PIX_FMT_BGRA);
	return Cairo::ImageSurface::create (
		image->data()[0],
		Cairo::FORMAT_ARGB32,
		image->size().width,
		image->size().height,
		/* Cairo ARGB32 means first byte blue, second byte green, third byte red, fourth byte alpha */
		Cairo::ImageSurface::format_stride_for_width (Cairo::FORMAT_ARGB32, image->size().width)
		);
}


static string
setup_font (StringText const& subtitle)
{
	auto font_file = default_font_file ();

	if (subtitle.font && subtitle.font->file()) {
		font_file = *subtitle.font->file();
	}

	return FontConfig::instance()->make_font_available(font_file);
}


static float
calculate_fade_factor (StringText const& first, DCPTime time, int frame_rate)
{
	float fade_factor = 1;

	/* Round the fade start/end to the nearest frame start.  Otherwise if a subtitle starts just after
	   the start of a frame it will be faded out.
	*/
	auto const fade_in_start = DCPTime::from_seconds(first.in().as_seconds()).round(frame_rate);
	auto const fade_in_end = fade_in_start + DCPTime::from_seconds (first.fade_up_time().as_seconds ());

	if (fade_in_start <= time && time <= fade_in_end && fade_in_start != fade_in_end) {
		fade_factor *= DCPTime(time - fade_in_start).seconds() / DCPTime(fade_in_end - fade_in_start).seconds();
	}

	if (time < fade_in_start) {
		fade_factor = 0;
	}

	/* first.out() may be zero if we don't know when this subtitle will finish.  We can only think about
	 * fading out if we _do_ know when it will finish.
	 */
	if (first.out() != dcp::Time()) {
		auto const fade_out_end = DCPTime::from_seconds (first.out().as_seconds()).round(frame_rate);
		auto const fade_out_start = fade_out_end - DCPTime::from_seconds(first.fade_down_time().as_seconds());

		if (fade_out_start <= time && time <= fade_out_end && fade_out_start != fade_out_end) {
			fade_factor *= 1 - DCPTime(time - fade_out_start).seconds() / DCPTime(fade_out_end - fade_out_start).seconds();
		}
		if (time > fade_out_end) {
			fade_factor = 0;
		}
	}

	return fade_factor;
}


static int
x_position (StringText const& first, int target_width, int layout_width)
{
	int x = 0;
	switch (first.h_align()) {
	case dcp::HAlign::LEFT:
		/* h_position is distance between left of frame and left of subtitle */
		x = first.h_position() * target_width;
		break;
	case dcp::HAlign::CENTER:
		/* h_position is distance between centre of frame and centre of subtitle */
		x = (0.5 + first.h_position()) * target_width - layout_width / 2;
		break;
	case dcp::HAlign::RIGHT:
		/* h_position is distance between right of frame and right of subtitle */
		x = (1.0 - first.h_position()) * target_width - layout_width;
		break;
	}

	return x;
}


static int
y_position (StringText const& first, int target_height, int baseline_to_bottom, int layout_height)
{
	int y = 0;
	switch (first.valign_standard) {
	case dcp::Standard::INTEROP:
		switch (first.v_align()) {
		case dcp::VAlign::TOP:
			/* v_position is distance from top of frame to subtitle baseline */
			y = first.v_position() * target_height - (layout_height - baseline_to_bottom);
			break;
		case dcp::VAlign::CENTER:
			/* v_position is distance from centre of frame to subtitle baseline */
			y = (0.5 + first.v_position()) * target_height - (layout_height - baseline_to_bottom);
			break;
		case dcp::VAlign::BOTTOM:
			/* v_position is distance from bottom of frame to subtitle baseline */
			y = (1.0 - first.v_position()) * target_height - (layout_height - baseline_to_bottom);
			break;
		}
		break;
	case dcp::Standard::SMPTE:
		switch (first.v_align()) {
		case dcp::VAlign::TOP:
			/* v_position is distance from top of frame to top of subtitle */
			y = first.v_position() * target_height;
			break;
		case dcp::VAlign::CENTER:
			/* v_position is distance from centre of frame to centre of subtitle */
			y = (0.5 + first.v_position()) * target_height - layout_height / 2;
			break;
		case dcp::VAlign::BOTTOM:
			/* v_position is distance from bottom of frame to bottom of subtitle */
			y = (1.0 - first.v_position()) * target_height - layout_height;
			break;
		}
	}

	return y;
}


/** @param subtitles A list of subtitles that are all on the same line,
 *  at the same time and with the same fade in/out.
 */
static PositionImage
render_line (list<StringText> subtitles, dcp::Size target, DCPTime time, int frame_rate)
{
	/* XXX: this method can only handle italic / bold changes mid-line,
	   nothing else yet.
	*/

	DCPOMATIC_ASSERT (!subtitles.empty ());
	auto const& first = subtitles.front ();

	auto const font_name = setup_font (first);
	auto const fade_factor = calculate_fade_factor (first, time, frame_rate);
	auto const markup = marked_up (subtitles, target.height, fade_factor, font_name);
	auto layout = create_layout ();
	setup_layout (layout, font_name, markup);
	auto ink = layout->get_ink_extents();
	dcp::Size size{ink.get_width() / Pango::SCALE, ink.get_height() / Pango::SCALE};

	/* Calculate x and y scale factors.  These are only used to stretch
	   the font away from its normal aspect ratio.
	*/
	float x_scale = 1;
	float y_scale = 1;
	if (fabs (first.aspect_adjust() - 1.0) > dcp::ASPECT_ADJUST_EPSILON) {
		if (first.aspect_adjust() < 1) {
			x_scale = max (0.25f, first.aspect_adjust ());
			y_scale = 1;
		} else {
			x_scale = 1;
			y_scale = 1 / min (4.0f, first.aspect_adjust ());
		}
	}

	auto const border_width = first.effect() == dcp::Effect::BORDER ? (first.outline_width * target.width / 2048.0) : 0;
	size.width += 2 * ceil (border_width);
	size.height += 2 * ceil (border_width);

	size.width *= x_scale;
	size.height *= y_scale;

	/* Shuffle the subtitle over by the border width (if we have any) so it's not cut off */
	int const x_offset = (-ink.get_x() / Pango::SCALE) + ceil(border_width);
	int const y_offset = -ink.get_y() / Pango::SCALE + ceil(border_width);

	auto image = create_image (size);
	auto surface = create_surface (image);
	auto context = Cairo::Context::create (surface);

	context->set_line_width (1);
	context->scale (x_scale, y_scale);
	layout->update_from_cairo_context (context);

	if (first.effect() == dcp::Effect::SHADOW) {
		/* Drop-shadow effect */
		set_source_rgba (context, first.effect_colour(), fade_factor);
		context->move_to (x_offset + 4, y_offset + 4);
		layout->add_to_cairo_context (context);
		context->fill ();
	}

	if (first.effect() == dcp::Effect::BORDER) {
		/* Border effect */
		set_source_rgba (context, first.effect_colour(), fade_factor);
		context->set_line_width (border_width);
		context->set_line_join (Cairo::LINE_JOIN_ROUND);
		context->move_to (x_offset, y_offset);
		layout->add_to_cairo_context (context);
		context->stroke ();
	}

	/* The actual subtitle */

	set_source_rgba (context, first.colour(), fade_factor);

	context->move_to (x_offset, y_offset);
	layout->add_to_cairo_context (context);
	context->fill ();

	context->set_line_width (0.5);
	context->move_to (x_offset, y_offset);
	layout->add_to_cairo_context (context);
	context->stroke ();

	int const x = x_position (first, target.width, size.width);
	int const y = y_position (first, target.height, ink.get_y() / Pango::SCALE, size.height);
	return PositionImage (image, Position<int>(max (0, x), max(0, y)));
}


/** @param time Time of the frame that these subtitles are going on.
 *  @param target Size of the container that this subtitle will end up in.
 *  @param frame_rate DCP frame rate.
 */
list<PositionImage>
render_text (list<StringText> subtitles, dcp::Size target, DCPTime time, int frame_rate)
{
	list<StringText> pending;
	list<PositionImage> images;

	for (auto const& i: subtitles) {
		if (!pending.empty() && (i.v_align() != pending.back().v_align() || fabs(i.v_position() - pending.back().v_position()) > 1e-4)) {
			images.push_back(render_line(pending, target, time, frame_rate));
			pending.clear ();
		}
		pending.push_back (i);
	}

	if (!pending.empty()) {
		images.push_back(render_line(pending, target, time, frame_rate));
	}

	return images;
}
