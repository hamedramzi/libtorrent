/*

Copyright (c) 2013, Arvid Norberg
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

#include "test.hpp"
#include "setup_transfer.hpp"

#include "libtorrent/file_storage.hpp"
#include "libtorrent/file.hpp"

using namespace libtorrent;

void setup_test_storage(file_storage& st)
{
	st.add_file(combine_path("test", "a"), 10000);
	st.add_file(combine_path("test", "b"), 20000);
	st.add_file(combine_path("test", combine_path("c", "a")), 30000);
	st.add_file(combine_path("test", combine_path("c", "b")), 40000);

	st.set_piece_length(0x4000);
	st.set_num_pieces((st.total_size() + st.piece_length() - 1) / 0x4000);

	TEST_EQUAL(st.file_name(0), "a");
	TEST_EQUAL(st.file_name(1), "b");
	TEST_EQUAL(st.file_name(2), "a");
	TEST_EQUAL(st.file_name(3), "b");
	TEST_EQUAL(st.name(), "test");

	TEST_EQUAL(st.file_path(0), combine_path("test", "a"));
	TEST_EQUAL(st.file_path(1), combine_path("test", "b"));
	TEST_EQUAL(st.file_path(2), combine_path("test", combine_path("c", "a")));
	TEST_EQUAL(st.file_path(3), combine_path("test", combine_path("c", "b")));

	TEST_EQUAL(st.file_size(0), 10000);
	TEST_EQUAL(st.file_size(1), 20000);
	TEST_EQUAL(st.file_size(2), 30000);
	TEST_EQUAL(st.file_size(3), 40000);

	TEST_EQUAL(st.file_offset(0), 0);
	TEST_EQUAL(st.file_offset(1), 10000);
	TEST_EQUAL(st.file_offset(2), 30000);
	TEST_EQUAL(st.file_offset(3), 60000);

	TEST_EQUAL(st.total_size(), 100000);
	TEST_EQUAL(st.piece_length(), 0x4000);
	printf("%d\n", st.num_pieces());
	TEST_EQUAL(st.num_pieces(), (100000 + 0x3fff) / 0x4000);
}

int test_main()
{
	{
		// test rename_file
		file_storage st;
		setup_test_storage(st);
		
		st.rename_file(0, combine_path("test", combine_path("c", "d")));
		TEST_EQUAL(st.file_path(0, "."), combine_path(".", combine_path("test"
			, combine_path("c", "d"))));
		TEST_EQUAL(st.file_path(0, ""), combine_path("test"
			, combine_path("c", "d")));

		// files with absolute paths should ignore the save_path argument
		// passed in to file_path()
#ifdef TORRENT_WINDOWS
		st.rename_file(0, "c:\\tmp\\a");
		TEST_EQUAL(st.file_path(0, "."), "c:\\tmp\\a");
#else
		st.rename_file(0, "/tmp/a");
		TEST_EQUAL(st.file_path(0, "."), "/tmp/a");
#endif
	}

	{
		// test set_name. Make sure the name of the torrent is not encoded
		// in the paths of each individual file. When changing the name of the
		// torrent, the path of the files should change too
		file_storage st;
		setup_test_storage(st);
		
		st.set_name("test_2");
		TEST_EQUAL(st.file_path(0, "."), combine_path(".", combine_path("test_2"
			, "a")));
	}

	{
		// test rename_file
		file_storage st;
		st.add_file("a", 10000);
		TEST_EQUAL(st.file_path(0, ""), "a");

		st.rename_file(0, combine_path("test", combine_path("c", "d")));
		TEST_EQUAL(st.file_path(0, "."), combine_path(".", combine_path("test", combine_path("c", "d"))));
		TEST_EQUAL(st.file_path(0, ""), combine_path("test", combine_path("c", "d")));

#ifdef TORRENT_WINDOWS
		st.rename_file(0, "c:\\tmp\\a");
		TEST_EQUAL(st.file_path(0, "."), "c:\\tmp\\a");
		TEST_EQUAL(st.file_path(0, "c:\\test-1\\test2"), "c:\\tmp\\a");
#else
		st.rename_file(0, "/tmp/a");
		TEST_EQUAL(st.file_path(0, "."), "/tmp/a");
		TEST_EQUAL(st.file_path(0, "/usr/local/temp"), "/tmp/a");
#endif

		st.rename_file(0, combine_path("tmp", "a"));
		TEST_EQUAL(st.file_path(0, "."), combine_path("tmp", "a"));
	}

	{
		// test applying pointer offset
		file_storage st;
		char const filename[] = "test1fooba";

		st.add_file_borrow(filename, 5, combine_path("test-torrent-1", "test1")
			, 10);

		// test filename_ptr and filename_len
		TEST_EQUAL(st.file_name_ptr(0), filename);
		TEST_EQUAL(st.file_name_len(0), 5);

		TEST_EQUAL(st.file_path(0, ""), combine_path("test-torrent-1", "test1"));
		TEST_EQUAL(st.file_path(0, "tmp"), combine_path("tmp"
			, combine_path("test-torrent-1", "test1")));

		// apply a pointer offset of 5 bytes. The name of the file should
		// change to "fooba".

		st.apply_pointer_offset(5);

		TEST_EQUAL(st.file_path(0, ""), combine_path("test-torrent-1", "fooba"));
		TEST_EQUAL(st.file_path(0, "tmp"), combine_path("tmp"
			, combine_path("test-torrent-1", "fooba")));

		// test filename_ptr and filename_len
		TEST_EQUAL(st.file_name_ptr(0), filename + 5);
		TEST_EQUAL(st.file_name_len(0), 5);
	}

	{
		// test map_file
		file_storage fs;
		fs.set_piece_length(512);
		fs.add_file(combine_path("temp_storage", "test1.tmp"), 17);
		fs.add_file(combine_path("temp_storage", "test2.tmp"), 612);
		fs.add_file(combine_path("temp_storage", "test3.tmp"), 0);
		fs.add_file(combine_path("temp_storage", "test4.tmp"), 0);
		fs.add_file(combine_path("temp_storage", "test5.tmp"), 3253);
		// size: 3882
		fs.add_file(combine_path("temp_storage", "test6.tmp"), 841);
		// size: 4723

		peer_request rq = fs.map_file(0, 0, 10);
		TEST_EQUAL(rq.piece, 0);
		TEST_EQUAL(rq.start, 0);
		TEST_EQUAL(rq.length, 10);
		rq = fs.map_file(5, 0, 10);
		TEST_EQUAL(rq.piece, 7);
		TEST_EQUAL(rq.start, 298);
		TEST_EQUAL(rq.length, 10);
		rq = fs.map_file(5, 0, 1000);
		TEST_EQUAL(rq.piece, 7);
		TEST_EQUAL(rq.start, 298);
		TEST_EQUAL(rq.length, 841);
	}

	{
		// test file_path_hash and path_hash. Make sure we can detect a path
		// whose name collides with
		file_storage fs;
		fs.set_piece_length(512);
		fs.add_file(combine_path("temp_storage", combine_path("foo", "test1")), 17);
		fs.add_file(combine_path("temp_storage", "foo"), 612);

		fprintf(stderr, "path: %s\n", fs.paths()[0].c_str());
		fprintf(stderr, "file: %s\n", fs.file_path(1).c_str());
		boost::uint32_t file_hash = fs.file_path_hash(1, "a");
		boost::uint32_t path_hash = fs.path_hash(0, "a");
		TEST_EQUAL(file_hash, path_hash);
	}

	// TODO: test file_storage::optimize too
	// TODO: test map_block
	// TODO: test piece_size(int piece)
	// TODO: test file_index_at_offset
	// TODO: test file attributes
	// TODO: test symlinks
	// TODO: test pad_files
	// TODO: test reorder_file (make sure internal_file_entry::swap() is used)

	return 0;
}

