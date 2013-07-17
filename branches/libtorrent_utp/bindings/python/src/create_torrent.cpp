// Copyright Daniel Wallin & Arvid Norberg 2009. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/python.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include "libtorrent/intrusive_ptr_base.hpp"

using namespace boost::python;
using namespace libtorrent;

namespace
{
    void set_hash(create_torrent& c, int p, char const* hash)
    {
        c.set_hash(p, sha1_hash(hash));
    }

    void call_python_object(boost::python::object const& obj, int i)
    {
       obj(i);
    }

#ifndef BOOST_NO_EXCEPTIONS
    void set_piece_hashes_callback(create_torrent& c, std::string const& p
        , boost::python::object cb)
    {
        set_piece_hashes(c, p, boost::bind(call_python_object, cb, _1));
    }
#else
    void set_piece_hashes_callback(create_torrent& c, std::string const& p
        , boost::python::object cb)
    {
        error_code ec;
        set_piece_hashes(c, p, boost::bind(call_python_object, cb, _1), ec);
    }

    void set_piece_hashes0(create_torrent& c, std::string const & s)
    {
        error_code ec;
        set_piece_hashes(c, s, ec);
    }
#endif

    void add_node(create_torrent& ct, std::string const& addr, int port)
    {
        ct.add_node(std::make_pair(addr, port));
    }

    void add_file(file_storage& ct, file_entry const& fe
       , std::string const& hash, std::string const& linkpath)
    {
       ct.add_file(fe, hash.empty() ? 0 : hash.c_str()
          , linkpath.empty() ? 0 : &linkpath);
    }
}

void bind_create_torrent()
{
    void (file_storage::*add_file0)(std::string const&, size_type, int, std::time_t, std::string const&) = &file_storage::add_file;
#if TORRENT_USE_WSTRING
    void (file_storage::*add_file1)(std::wstring const&, size_type, int, std::time_t, std::string const&) = &file_storage::add_file;
#endif

    void (file_storage::*set_name0)(std::string const&) = &file_storage::set_name;
#if TORRENT_USE_WSTRING
    void (file_storage::*set_name1)(std::wstring const&) = &file_storage::set_name;
#endif

#ifndef BOOST_NO_EXCEPTIONS
    void (*set_piece_hashes0)(create_torrent&, std::string const&) = &set_piece_hashes;
#endif
    void (*add_files0)(file_storage&, std::string const&, boost::uint32_t) = add_files;

    class_<file_storage>("file_storage")
        .def("is_valid", &file_storage::is_valid)
        .def("add_file", add_file, (arg("entry"), arg("hash") = std::string(), arg("symlink") = std::string()))
        .def("add_file", add_file0, (arg("path"), arg("size"), arg("flags") = 0, arg("mtime") = 0, arg("linkpath") = ""))
#if TORRENT_USE_WSTRING
        .def("add_file", add_file1, (arg("path"), arg("size"), arg("flags") = 0, arg("mtime") = 0, arg("linkpath") = ""))
#endif
        .def("num_files", &file_storage::num_files)
        .def("at", &file_storage::at, return_internal_reference<>())
        .def("hash", &file_storage::hash)
        .def("symlink", &file_storage::symlink, return_internal_reference<>())
        .def("file_index", &file_storage::file_index)
        .def("file_base", &file_storage::file_base)
        .def("set_file_base", &file_storage::set_file_base)
        .def("file_path", &file_storage::file_path)
        .def("total_size", &file_storage::total_size)
        .def("set_num_pieces", &file_storage::set_num_pieces)
        .def("num_pieces", &file_storage::num_pieces)
        .def("set_piece_length", &file_storage::set_piece_length)
        .def("piece_length", &file_storage::piece_length)
        .def("piece_size", &file_storage::piece_size)
        .def("set_name", set_name0)
#if TORRENT_USE_WSTRING
        .def("set_name", set_name1)
#endif
        .def("name", &file_storage::name, return_internal_reference<>())
        ;

    class_<create_torrent>("create_torrent", no_init)
        .def(init<file_storage&>())
        .def(init<file_storage&, int>())

        .def("generate", &create_torrent::generate)

        .def("files", &create_torrent::files, return_internal_reference<>())
        .def("set_comment", &create_torrent::set_comment)
        .def("set_creator", &create_torrent::set_creator)
        .def("set_hash", &set_hash)
        .def("add_url_seed", &create_torrent::add_url_seed)
        .def("add_node", &add_node)
        .def("add_tracker", &create_torrent::add_tracker)
        .def("set_priv", &create_torrent::set_priv)
        .def("num_pieces", &create_torrent::num_pieces)
        .def("piece_length", &create_torrent::piece_length)
        .def("piece_size", &create_torrent::piece_size)
        .def("priv", &create_torrent::priv)
        ;

    def("add_files", add_files0, (arg("fs"), arg("path"), arg("flags") = 0));
    def("set_piece_hashes", set_piece_hashes0);
    def("set_piece_hashes", set_piece_hashes_callback);

}