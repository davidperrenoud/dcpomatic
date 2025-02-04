/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/text_content.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE(full_dcp_subtitle_font_id_test)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film2("full_dcp_subtitle_font_id_test", { dcp });

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	auto text = content[0]->only_text();
	BOOST_REQUIRE(text);

	BOOST_REQUIRE_EQUAL(text->fonts().size(), 1U);
	auto font = text->fonts().front();
	BOOST_CHECK_EQUAL(font->id(), "0_theFontId");
	BOOST_REQUIRE(font->data());
	BOOST_CHECK_EQUAL(font->data()->size(), 367112);
}


BOOST_AUTO_TEST_CASE(dcp_subtitle_font_id_test)
{
	auto subs = content_factory(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV" / "8b48f6ae-c74b-4b80-b994-a8236bbbad74_sub.mxf");
	auto film = new_test_film2("dcp_subtitle_font_id_test", subs);

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	auto text = content[0]->only_text();
	BOOST_REQUIRE(text);

	BOOST_REQUIRE_EQUAL(text->fonts().size(), 1U);
	auto font = text->fonts().front();
	BOOST_CHECK_EQUAL(font->id(), "theFontId");
	BOOST_REQUIRE(font->data());
	BOOST_CHECK_EQUAL(font->data()->size(), 367112);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_interop_dcp)
{
	auto dcp = make_shared<DCPContent>("test/data/Iopsubs_FTR-1_F_XX-XX_MOS_2K_20220710_IOP_OV");
	auto film = new_test_film2("make_dcp_with_subs_from_interop_dcp", { dcp });
	dcp->text.front()->set_use(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		}
	);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_smpte_dcp)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film2("make_dcp_with_subs_from_smpte_dcp", { dcp });
	dcp->text.front()->set_use(true);
	make_and_verify_dcp(film);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_mkv)
{
	auto subs = content_factory(TestPaths::private_data() / "clapperboard_with_subs.mkv");
	auto film = new_test_film2("make_dcp_with_subs_from_mkv", subs);
	subs[0]->text.front()->set_use(true);
	subs[0]->text.front()->set_language(dcp::LanguageTag("en-US"));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_2K });
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_without_font_tag)
{
	auto subs = content_factory("test/data/no_font.xml");
	auto film = new_test_film2("make_dcp_with_subs_without_font_tag", { subs });
	subs[0]->text.front()->set_use(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	auto check_file = subtitle_file(film);
	dcp::SMPTESubtitleAsset check_asset(check_file);
	BOOST_CHECK_EQUAL(check_asset.load_font_nodes().size(), 1U);
	auto check_font_data = check_asset.font_data();
	BOOST_CHECK_EQUAL(check_font_data.size(), 1U);
	BOOST_CHECK(check_font_data.begin()->second == dcp::ArrayData(default_font_file()));
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_in_dcp_without_font_tag)
{
	/* Make a DCP with some subs in */
	auto source_subs = content_factory("test/data/short.srt");
	auto source = new_test_film2("make_dcp_with_subs_in_dcp_without_font_tag_source", { source_subs });
	source->set_interop(true);
	make_and_verify_dcp(
		source,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_STANDARD
		});

	/* Find the ID of the subs */
	dcp::DCP source_dcp(source->dir(source->dcp_name()));
	source_dcp.read();
	BOOST_REQUIRE(!source_dcp.cpls().empty());
	BOOST_REQUIRE(!source_dcp.cpls()[0]->reels().empty());
	BOOST_REQUIRE(source_dcp.cpls()[0]->reels()[0]->main_subtitle());
	auto const id = source_dcp.cpls()[0]->reels()[0]->main_subtitle()->asset()->id();

	/* Graft in some bad subs with no <Font> tag */
	auto source_subtitle_file = subtitle_file(source);
#if BOOST_VERSION >= 107400
	boost::filesystem::copy_file("test/data/no_font.xml", source_subtitle_file, boost::filesystem::copy_options::overwrite_existing);
#else
	boost::filesystem::copy_file("test/data/no_font.xml", source_subtitle_file, boost::filesystem::copy_option::overwrite_if_exists);
#endif

	/* Fix the <Id> tag */
	{
		Editor editor(source_subtitle_file);
		editor.replace("4dd8ee05-5986-4c67-a6f8-bbeac62e21db", id);
	}

	/* Now make a project which imports that DCP and makes another DCP from it */
	auto dcp_content = make_shared<DCPContent>(source->dir(source->dcp_name()));
	auto film = new_test_film2("make_dcp_with_subs_without_font_tag", { dcp_content });
	BOOST_REQUIRE(!dcp_content->text.empty());
	dcp_content->text.front()->set_use(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	auto check_file = subtitle_file(film);
	dcp::SMPTESubtitleAsset check_asset(check_file);
	BOOST_CHECK_EQUAL(check_asset.load_font_nodes().size(), 1U);
	auto check_font_data = check_asset.font_data();
	BOOST_CHECK_EQUAL(check_font_data.size(), 1U);
	BOOST_CHECK(check_font_data.begin()->second == dcp::ArrayData(default_font_file()));
}

