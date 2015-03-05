/*

Copyright (c) 2015, Arvid Norberg
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

#include <boost/cstdint.hpp>
#include <boost/system/error_code.hpp>
#include <vector>
#include <string>
#include "libtorrent/assert.hpp"

#ifndef TORRENT_BDECODE_HPP
#define TORRENT_BDECODE_HPP

/*

This is an efficient bdecoder. It decodes into a flat memory buffer of tokens.

Each token has an offset into the bencoded buffer where the token came from
and a next pointer, which is a relative number of tokens to skip forward to
get to the logical next item in a container.

strings and ints offset pointers point to the first character of the length
prefix or the 'i' character. This is to maintain uniformity with other types
and to allow easily calculating the span of a node by subtracting its offset
by the offset of the next node.

example layout:

{
	"a": { "b": 1, "c": "abcd" },
	"d": 3
}

  /---------------------------------------------------------------------------------------\
  |                                                                                       |
  |                                                                                       |
  |                  /--------------------------------------------\                       |
  |                  |                                            |                       |
  |                  |                                            |                       |
  |          /-----\ |       /----\  /----\  /----\  /----\       |  /----\  /----\       |
  | next     |     | |       |    |  |    |  |    |  |    |       |  |    |  |    |       |
  | pointers |     v |       |    v  |    v  |    v  |    v       v  |    v  |    v       v
+-+-----+----+--+----+--+----+--+----+--+----+--+----+--+-------+----+--+----+--+------+  X
| dict  | str   | dict  | str   | int   | str   | str   | end   | str   | int   | end  |
|       |       |       |       |       |       |       |       |       |       |      |
|       |       |       |       |       |       |       |       |       |       |      |
+-+-----+-+-----+-+-----+-+-----+-+-----+-+-----+-+-----+-+-----+-+-----+-+-----+-+----+
  | offset|       |       |       |       |       |       |       |       |       |
  |       |       |       |       |       |       |       |       |       |       |
  |/------/       |       |       |       |       |       |       |       |       |
  ||  /-----------/       |       |       |       |       |       |       |       |
  ||  |/------------------/       |       |       |       |       |       |       |
  ||  ||  /-----------------------/       |       |       |       |       |       |
  ||  ||  |  /----------------------------/       |       |       |       |       |
  ||  ||  |  |  /---------------------------------/       |       |       |       |
  ||  ||  |  |  |     /-----------------------------------/       |       |       |
  ||  ||  |  |  |     |/------------------------------------------/       |       |
  ||  ||  |  |  |     ||  /-----------------------------------------------/       |
  ||  ||  |  |  |     ||  |  /----------------------------------------------------/
  ||  ||  |  |  |     ||  |  |
  vv  vv  v  v  v     vv  v  v
``d1:ad1:bi1e1:c4:abcde1:di3ee``

*/

namespace libtorrent {

TORRENT_EXPORT boost::system::error_category& get_bdecode_category();

namespace bdecode_errors
{
	// libtorrent uses boost.system's ``error_code`` class to represent
	// errors. libtorrent has its own error category get_bdecode_category()
	// whith the error codes defined by error_code_enum.
	enum error_code_enum
	{
		// Not an error
		no_error = 0,
		// expected string in bencoded string
		expected_digit,
		// expected colon in bencoded string
		expected_colon,
		// unexpected end of file in bencoded string
		unexpected_eof,
		// expected value (list, dict, int or string) in bencoded string
		expected_value,
		// bencoded recursion depth limit exceeded
		depth_exceeded,
		// bencoded item count limit exceeded
		limit_exceeded,
		// integer overflow
		overflow,

		// the number of error codes
		error_code_max
	};

	// hidden
	TORRENT_EXPORT boost::system::error_code make_error_code(error_code_enum e);
}
} // namespace libtorrent

#if BOOST_VERSION >= 103500

namespace boost { namespace system {

	template<> struct is_error_code_enum<libtorrent::bdecode_errors::error_code_enum>
	{ static const bool value = true; };

	template<> struct is_error_condition_enum<libtorrent::bdecode_errors::error_code_enum>
	{ static const bool value = true; };
} }

#endif

namespace libtorrent {

	typedef boost::system::error_code error_code;

TORRENT_EXTRA_EXPORT char const* parse_int(char const* start
	, char const* end, char delimiter, boost::int64_t& val
	, bdecode_errors::error_code_enum& ec);

namespace detail
{
// internal
struct bdecode_token
{
	// the node with type 'end' is a logical node, pointing to the end
	// of the bencoded buffer.
	enum type_t
	{ none, dict, list, string, integer, end };

	enum limits_t
	{
		max_offset = (1 << 29) - 1,
		max_next_item = (1 << 29) - 1
	};

	bdecode_token(boost::uint32_t off, bdecode_token::type_t t)
		: offset(off)
		, type(t)
		, next_item(0)
		, header(0)
	{
		TORRENT_ASSERT(off <= max_offset);
		TORRENT_ASSERT(t >= 0 && t <= end);
	}

	bdecode_token(boost::uint32_t off, boost::uint32_t next
		, bdecode_token::type_t t, boost::uint8_t header_size = 0)
		: offset(off)
		, type(t)
		, next_item(next)
		, header(header_size)
	{
		TORRENT_ASSERT(off <= max_offset);
		TORRENT_ASSERT(next <= max_next_item);
		TORRENT_ASSERT(header_size < 8);
		TORRENT_ASSERT(t >= 0 && t <= end);
	}

	// offset into the bdecoded buffer where this node is
	boost::uint32_t offset:29;

	// one of type_t enums
	boost::uint32_t type:3;

	// if this node is a member of a list, 'next_item' is the number of nodes
	// to jump forward in th node array to get to the next item in the list.
	// if it's a key in a dictionary, it's the number of step forwards to get
	// to its corresponding value. If it's a value in a dictionary, it's the
	// number of steps to the next key, or to the end node.
	// this is the _relative_ offset to the next node
	boost::uint32_t next_item:29;

	// this is the number of bytes to skip forward from the offset to get to
	// the value of this typ. For a string, this is the length of the length
	// prefix and the colon. For an integer, this is just to skip the 'i'
	// character.
	boost::uint32_t header:3;
};
}

// TODO: 4 document this class. Can probably borrow a lot from lazy_entry
struct bdecode_node
{
	friend int bdecode(char const* start, char const* end, bdecode_node& ret
		, error_code& ec, int* error_pos, int depth_limit, int token_limit);

	bdecode_node();

	enum type_t
	{ none_t, dict_t, list_t, string_t, int_t };

	type_t type() const;

	std::pair<char const*, int> data_section() const;

	bdecode_node list_at(int i) const;
	boost::int64_t list_int_value_at(int i);
	int list_size() const;

	// dictionary operations
	bdecode_node dict_find(std::string key) const;
	bdecode_node dict_find(char const* key) const;
	std::pair<std::string, bdecode_node> dict_at(int i) const;
	std::string dict_find_string_value(char const* key) const;
	boost::int64_t dict_find_int_value(char const* key) const;
	int dict_size() const;

	// integer operations
	boost::int64_t int_value() const;

	// string oeprations
	std::string string_value() const;
	char const* string_ptr() const;
	int string_length() const;

	void clear();

private:
	bdecode_node(std::vector<detail::bdecode_token> const& tokens, char const* buf
		, int len, int idx);

	// if this is the root node, that owns all the tokens, they live in this
	// vector. If this is a sub-node, this field is not used, instead the
	// m_root_tokens pointer points to the root node's token.
	std::vector<detail::bdecode_token> m_tokens;

	// this points to the root nodes token vector
	// for the root node, this points to its own m_tokens member
	std::vector<detail::bdecode_token> const* m_root_tokens;

	// this points to the original buffer that was parsed
	char const* m_buffer;
	int m_buffer_size;

	// this is the index into m_root_tokens that this node refers to
	// for the root node, it's 0. -1 means uninitialized.
	int m_token_idx;

	// this is a cache of the last element index looked up. This only applies
	// to lists and dictionaries. If the next lookup is at m_last_index or
	// greater, we can start iterating the tokens at m_last_token.
	mutable int m_last_index;
	mutable int m_last_token;

	// the number of elements in this list or dict (computed on the first
	// call to dict_size() or list_size())
	mutable int m_size;
};

int bdecode(char const* start, char const* end, bdecode_node& ret
	, error_code& ec, int* error_pos = 0, int depth_limit = 1000
	, int token_limit = 1000000);

}

#endif // TORRENT_BDECODE_HPP

