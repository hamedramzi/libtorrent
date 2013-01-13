/*

Copyright (c) 2008, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include "libtorrent/session.hpp"
#include "libtorrent/session_settings.hpp"
#include "libtorrent/time.hpp"
#include "libtorrent/hasher.hpp"
#include "libtorrent/create_torrent.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/thread.hpp"
#include <boost/tuple/tuple.hpp>
#include <iostream>

#include "test.hpp"
#include "setup_transfer.hpp"

using namespace libtorrent;

void test_running_torrent(boost::intrusive_ptr<torrent_info> info, size_type file_size)
{
	session ses(fingerprint("LT", 0, 1, 0, 0), std::make_pair(48130, 48140), "0.0.0.0", 0);
	ses.set_alert_mask(alert::storage_notification);

	add_torrent_params p;
	p.ti = info;
	p.save_path = ".";
	error_code ec;
	torrent_handle h = ses.add_torrent(p, ec);
	if (ec)
	{
		fprintf(stderr, "add_torrent: %s\n", ec.message().c_str());
		return;
	}

//	test_sleep(500);
	torrent_status st = h.status();

	TEST_EQUAL(st.total_wanted, file_size * 3);
	TEST_EQUAL(st.total_wanted_done, 0);

	std::vector<int> prio(info->num_files(), 1);
	prio[0] = 0;
	h.prioritize_files(prio);
	st = h.status();

	TEST_EQUAL(st.total_wanted, file_size * 2);
	TEST_EQUAL(st.total_wanted_done, 0);
	TEST_EQUAL(h.file_priorities().size(), info->num_files());
	if (!st.is_seeding)
	{
		TEST_EQUAL(h.file_priorities()[0], 0);
		if (info->num_files() > 1)
			TEST_EQUAL(h.file_priorities()[1], 1);
		if (info->num_files() > 2)
			TEST_EQUAL(h.file_priorities()[2], 1);
	}

	if (info->num_files() > 1)
	{
		prio[1] = 0;
		h.prioritize_files(prio);
		st = h.status();

		TEST_EQUAL(st.total_wanted, file_size);
		TEST_EQUAL(st.total_wanted_done, 0);
		if (!st.is_seeding)
		{
			TEST_EQUAL(h.file_priorities().size(), info->num_files());
			TEST_EQUAL(h.file_priorities()[0], 0);
			if (info->num_files() > 1)
				TEST_EQUAL(h.file_priorities()[1], 0);
			if (info->num_files() > 2)
				TEST_EQUAL(h.file_priorities()[2], 1);
		}
	}

	if (info->num_pieces() > 0)
	{
		h.piece_priority(0, 1);
		st = h.status();
		TEST_CHECK(st.pieces.size() > 0 && st.pieces[0] == false);
		std::vector<char> piece(info->piece_length());
		for (int i = 0; i < int(piece.size()); ++i)
			piece[i] = (i % 26) + 'A';
		h.add_piece(0, &piece[0]);

		// wait until the piece is done writing and hashing
		// TODO: wait for an alert rather than just waiting 10 seconds. This is kind of silly
		test_sleep(2000);
		st = h.status();
		TEST_CHECK(st.pieces.size() > 0 && st.pieces[0] == true);

		std::cout << "reading piece 0" << std::endl;
		h.read_piece(0);
		alert const* a = ses.wait_for_alert(seconds(10));
		bool passed = false;
		while (a)
		{
			std::auto_ptr<alert> al = ses.pop_alert();
			assert(al.get());
			std::cout << "  " << al->message() << std::endl;
			if (read_piece_alert* rpa = alert_cast<read_piece_alert>(al.get()))
			{
				std::cout << "SUCCEEDED!" << std::endl;
				passed = true;
				TEST_CHECK(memcmp(&piece[0], rpa->buffer.get(), piece.size()) == 0);
				TEST_CHECK(rpa->size == info->piece_size(0));
				TEST_CHECK(rpa->piece == 0);
				TEST_CHECK(hasher(&piece[0], piece.size()).final() == info->hash_for_piece(0));
				break;
			}
			a = ses.wait_for_alert(seconds(10));
			TEST_CHECK(a);
		}
		TEST_CHECK(passed);
	}
}

int test_main()
{
/*	{
		remove("test_torrent_dir2/tmp1");
		remove("test_torrent_dir2/tmp2");
		remove("test_torrent_dir2/tmp3");
		file_storage fs;
		size_type file_size = 1 * 1024 * 1024 * 1024;
		fs.add_file("test_torrent_dir2/tmp1", file_size);
		fs.add_file("test_torrent_dir2/tmp2", file_size);
		fs.add_file("test_torrent_dir2/tmp3", file_size);
		libtorrent::create_torrent t(fs, 4 * 1024 * 1024);
		t.add_tracker("http://non-existing.com/announce");

		std::vector<char> piece(4 * 1024 * 1024);
		for (int i = 0; i < int(piece.size()); ++i)
			piece[i] = (i % 26) + 'A';
		
		// calculate the hash for all pieces
		sha1_hash ph = hasher(&piece[0], piece.size()).final();
		int num = t.num_pieces();
		TEST_CHECK(t.num_pieces() > 0);
		for (int i = 0; i < num; ++i)
			t.set_hash(i, ph);

		std::vector<char> tmp;
		std::back_insert_iterator<std::vector<char> > out(tmp);
		bencode(out, t.generate());
		error_code ec;
		boost::intrusive_ptr<torrent_info> info(new torrent_info(&tmp[0], tmp.size(), ec));
		TEST_CHECK(info->num_pieces() > 0);

		test_running_torrent(info, file_size);
	}
*/
	{
		file_storage fs;

		fs.add_file("test_torrent_dir2/tmp1", 0);
		libtorrent::create_torrent t(fs, 4 * 1024 * 1024);
		t.add_tracker("http://non-existing.com/announce");

		std::vector<char> tmp;
		std::back_insert_iterator<std::vector<char> > out(tmp);
		bencode(out, t.generate());
		error_code ec;
		boost::intrusive_ptr<torrent_info> info(new torrent_info(&tmp[0], tmp.size(), ec));
		test_running_torrent(info, 0);
	}

	return 0;
}


