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


/** @file  test/dcp_subtitle_test.cc
 *  @brief Test DCP subtitle content in various ways.
 *  @ingroup feature
 */


#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/dcp_content.h"
#include "lib/ratio.h"
#include "lib/dcp_decoder.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/text_content.h"
#include "lib/content_text.h"
#include "lib/font.h"
#include "lib/text_decoder.h"
#include "test.h"
#include <iostream>


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


optional<ContentStringText> stored;


static void
store (ContentStringText sub)
{
	if (!stored) {
		stored = sub;
	} else {
		for (auto i: sub.subs) {
			stored->subs.push_back (i);
		}
	}
}


/** Test pass-through of a very simple DCP subtitle file */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test)
{
	auto film = new_test_film ("dcp_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (false);
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub.xml");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	BOOST_CHECK_EQUAL (content->full_length(film).get(), DCPTime::from_seconds(2).get());

	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	make_and_verify_dcp (
		film,
	        {
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	check_dcp ("test/data/dcp_subtitle_test", film->dir(film->dcp_name()));
}


/** Test parsing of a subtitle within an existing DCP */
BOOST_AUTO_TEST_CASE (dcp_subtitle_within_dcp_test)
{
	auto film = new_test_film ("dcp_subtitle_within_dcp_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	auto content = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<DCPDecoder> decoder (new DCPDecoder (film, content, false, false, shared_ptr<DCPDecoder>()));
	decoder->only_text()->PlainStart.connect (bind (store, _1));

	stored = optional<ContentStringText> ();
	while (!decoder->pass() && !stored) {}

	BOOST_REQUIRE (stored);
	BOOST_REQUIRE_EQUAL (stored->subs.size(), 2U);
	BOOST_CHECK_EQUAL (stored->subs.front().text(), "Noch mal.");
	BOOST_CHECK_EQUAL (stored->subs.back().text(), "Encore une fois.");
}

/** Test subtitles whose text includes things like &lt;b&gt; */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test2)
{
	auto film = new_test_film ("dcp_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub2.xml");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	auto decoder = make_shared<DCPSubtitleDecoder>(film, content);
	decoder->only_text()->PlainStart.connect (bind (store, _1));

	stored = optional<ContentStringText> ();
	while (!decoder->pass()) {
		if (stored && stored->from() == ContentTime(0)) {
			BOOST_CHECK_EQUAL (stored->subs.front().text(), "&lt;b&gt;Hello world!&lt;/b&gt;");
		}
	}
}


/** Test a failure case */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test3)
{
	auto film = new_test_film ("dcp_subtitle_test3");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub3.xml");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	auto decoder = make_shared<DCPSubtitleDecoder>(film, content);
	stored = optional<ContentStringText> ();
	while (!decoder->pass ()) {
		decoder->only_text()->PlainStart.connect (bind (store, _1));
		if (stored && stored->from() == ContentTime::from_seconds(0.08)) {
			auto s = stored->subs;
			auto i = s.begin ();
			BOOST_CHECK_EQUAL (i->text(), "This");
			++i;
			BOOST_REQUIRE (i != s.end ());
			BOOST_CHECK_EQUAL (i->text(), " is ");
			++i;
			BOOST_REQUIRE (i != s.end ());
			BOOST_CHECK_EQUAL (i->text(), "wrong.");
			++i;
			BOOST_REQUIRE (i == s.end ());
		}
	}
}


/** Check that Interop DCPs aren't made with more than one <LoadFont> (#1273) */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test4)
{
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub3.xml");
	auto content2 = make_shared<DCPSubtitleContent>("test/data/dcp_sub3.xml");
	auto film = new_test_film2 ("dcp_subtitle_test4", {content, content2});
	film->set_interop (true);

	content->only_text()->add_font(make_shared<Font>("font1"));
	content2->only_text()->add_font(make_shared<Font>("font2"));

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	cxml::Document doc ("DCSubtitle");
	doc.read_file (subtitle_file (film));
	BOOST_REQUIRE_EQUAL (doc.node_children("LoadFont").size(), 1U);
}


static
void
check_font_tags (vector<cxml::NodePtr> nodes)
{
	for (auto i: nodes) {
		if (i->name() == "Font") {
			BOOST_CHECK (!i->optional_string_attribute("Id") || i->string_attribute("Id") != "");
		}
		check_font_tags (i->node_children());
	}
}


/** Check that imported <LoadFont> tags with empty IDs (or corresponding Font tags with empty IDs)
 *  are not passed through into the DCP.
 */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test5)
{
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub6.xml");
	auto film = new_test_film2 ("dcp_subtitle_test5", {content});
	film->set_interop (true);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	cxml::Document doc ("DCSubtitle");
	doc.read_file (subtitle_file(film));
	BOOST_REQUIRE_EQUAL (doc.node_children("LoadFont").size(), 1U);
	BOOST_CHECK (doc.node_children("LoadFont").front()->string_attribute("Id") != "");

	check_font_tags (doc.node_children());
}


/** Check that fonts specified in the DoM content are used in the output and not ignored (#2074) */
BOOST_AUTO_TEST_CASE (test_font_override)
{
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub4.xml");
	auto film = new_test_film2("test_font_override", {content});
	film->set_interop(true);

	BOOST_REQUIRE_EQUAL(content->text.size(), 1U);
	content->text.front()->get_font("theFontId")->set_file("test/data/Inconsolata-VF.ttf");

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });
	check_file (subtitle_file(film).parent_path() / "font_0.ttf", "test/data/Inconsolata-VF.ttf");
}

