#!/usr/bin/python

# Copyright Daniel Wallin 2006. Use, modification and distribution is
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

import sys
import libtorrent as lt
import time
import os.path
import sys

class WindowsConsole:
    def __init__(self):
        self.console = Console.getconsole()

    def clear(self):
        self.console.page()

    def write(self, str):
        self.console.write(str)

    def sleep_and_input(self, seconds):
        time.sleep(seconds)
        if msvcrt.kbhit():
            return msvcrt.getch()
        return None

class UnixConsole:
    def __init__(self):
        self.fd = sys.stdin
        self.old = termios.tcgetattr(self.fd.fileno())
        new = termios.tcgetattr(self.fd.fileno())
        new[3] = new[3] & ~termios.ICANON
        new[6][termios.VTIME] = 0
        new[6][termios.VMIN] = 1
        termios.tcsetattr(self.fd.fileno(), termios.TCSADRAIN, new)

        sys.exitfunc = self._onexit

    def _onexit(self):
        termios.tcsetattr(self.fd.fileno(), termios.TCSADRAIN, self.old)

    def clear(self):
        sys.stdout.write('\033[2J\033[0;0H')
        sys.stdout.flush()

    def write(self, str):
        sys.stdout.write(str)
        sys.stdout.flush()

    def sleep_and_input(self, seconds):
        read,_,_ = select.select([self.fd.fileno()], [], [], seconds)
        if len(read) > 0:
            return self.fd.read(1)
        return None

if os.name == 'nt':
    import Console
    import msvcrt
else:
    import termios
    import select

class PythonExtension(lt.torrent_plugin):
    def __init__(self, alerts):
        lt.torrent_plugin.__init__(self)
        self.alerts = alerts
        self.alerts.append('PythonExtension')
        self.count = 0

    def on_piece_pass(self, index):
        self.alerts.append('got piece %d' % index)

    def on_piece_failed(self, index):
        self.alerts.append('failed piece %d' % index)

    def tick(self):
        self.count += 1
        if self.count >= 10:
            self.count = 0
            self.alerts.append('PythonExtension tick')

def write_line(console, line):
    console.write(line)

def add_suffix(val):
    prefix = ['B', 'kB', 'MB', 'GB', 'TB']
    for i in range(len(prefix)):
        if abs(val) < 1000:
            if i == 0:
                return '%5.3g%s' % (val, prefix[i])
            else:
                return '%4.3g%s' % (val, prefix[i])
        val /= 1000

    return '%6.3gPB' % val

def progress_bar(progress, width):
    progress_chars = int(progress * width + 0.5)
    return progress_chars * '#' + (width - progress_chars) * '-'

def print_peer_info(console, peers):

    out = ' down    (total )   up     (total )  q  r flags  block progress  client\n'

    for p in peers:

        out += '%s/s ' % add_suffix(p.down_speed)
        out += '(%s) ' % add_suffix(p.total_download)
        out += '%s/s ' % add_suffix(p.up_speed)
        out += '(%s) ' % add_suffix(p.total_upload)
        out += '%2d ' % p.download_queue_length
        out += '%2d ' % p.upload_queue_length

        if p.flags & lt.peer_info.interesting: out += 'I'
        else: out += '.'
        if p.flags & lt.peer_info.choked: out += 'C'
        else: out += '.'
        if p.flags & lt.peer_info.remote_interested: out += 'i'
        else: out += '.'
        if p.flags & lt.peer_info.remote_choked: out += 'c'
        else: out += '.'
        if p.flags & lt.peer_info.supports_extensions: out += 'e'
        else: out += '.'
        if p.flags & lt.peer_info.local_connection: out += 'l'
        else: out += 'r'
        out += ' '

        if p.downloading_piece_index >= 0:
            out += progress_bar(float(p.downloading_progress) / p.downloading_total, 15)
        else:
            out += progress_bar(0, 15)
        out += ' '

        if p.flags & lt.peer_info.handshake: 
            id = 'waiting for handshake'
        elif p.flags & lt.peer_info.connecting: 
            id =  'connecting to peer'
        elif p.flags & lt.peer_info.queued: 
            id =  'queued'
        else:
            id = p.client

        out += '%s\n' % id[:10]

    write_line(console, out)


def print_download_queue(console, download_queue):

    out = ""

    for e in download_queue:
        out += '%4d: [' % e['piece_index'];
        for fin, req in zip(e['finished_blocks'], e['requested_blocks']):
            if fin:
                out += '#'
            elif req:
                out += '+'
            else:
                out += '-'
        out += ']\n'

    write_line(console, out)

def main():
    from optparse import OptionParser

    parser = OptionParser()

    parser.add_option('-p', '--port', 
        type='int', help='set listening port')

    parser.add_option('-r', '--ratio', 
        type='float', help='set the preferred upload/download ratio. 0 means infinite. Values smaller than 1 are clamped to 1')

    parser.add_option('-d', '--max-download-rate', 
        type='float', help='the maximum download rate given in kB/s. 0 means infinite.')

    parser.add_option('-u', '--max-upload-rate', 
        type='float', help='the maximum upload rate given in kB/s. 0 means infinite.')

    parser.add_option('-s', '--save-path', 
        type='string', help='the path where the downloaded file/folder should be placed.')

    parser.add_option('-a', '--allocation-mode', 
        type='string', help='sets mode used for allocating the downloaded files on disk. Possible options are [full | compact]')

    parser.set_defaults(
        port=6881
      , ratio=0
      , max_download_rate=0
      , max_upload_rate=0
      , save_path='./'
      , allocation_mode='compact'
    )

    (options, args) = parser.parse_args()

    if options.port < 0 or options.port > 65525:
        options.port = 6881

    options.max_upload_rate *= 1000
    options.max_download_rate *= 1000

    if options.max_upload_rate <= 0:
        options.max_upload_rate = -1
    if options.max_download_rate <= 0:
        options.max_download_rate = -1

    compact_allocation = options.allocation_mode == 'compact'

    settings = lt.session_settings()
    settings.user_agent = 'python_client/' + lt.version

    ses = lt.session()
    ses.set_download_rate_limit(int(options.max_download_rate))
    ses.set_upload_rate_limit(int(options.max_upload_rate))
    ses.listen_on(options.port, options.port + 10)
    ses.set_settings(settings)
    ses.set_severity_level(lt.alert.severity_levels.info)
#    ses.add_extension(lt.create_ut_pex_plugin);
#    ses.add_extension(lt.create_metadata_plugin);

    handles = []
    alerts = []

    # Extensions
    # ses.add_extension(lambda x: PythonExtension(alerts))

    for f in args:
        e = lt.bdecode(open(f, 'rb').read())
        info = lt.torrent_info(e)
        print 'Adding \'%s\'...' % info.name()

        try:
            resume_data = lt.bdecode(open(
                os.path.join(options.save_path, info.name() + '.fastresume'), 'rb').read())
        except:
            resume_data = None

        h = ses.add_torrent(info, options.save_path, 
                resume_data=resume_data, compact_mode=compact_allocation)

        handles.append(h)

        h.set_max_connections(60)
        h.set_max_uploads(-1)
        h.set_ratio(options.ratio)
        h.set_sequenced_download_threshold(15)

    if os.name == 'nt':
        console = WindowsConsole()
    else:
        console = UnixConsole()

    alive = True
    while alive:
        console.clear()

        out = ''

        for h in handles:
            if h.has_metadata():
                name = h.torrent_info().name()[:40]
            else:
                name = '-'
            out += 'name: %-40s\n' % name

            s = h.status()

            if s.state != lt.torrent_status.seeding:
                state_str = ['queued', 'checking', 'connecting', 'downloading metadata', \
                             'downloading', 'finished', 'seeding', 'allocating']
                out += state_str[s.state] + ' '

                out += '%5.4f%% ' % (s.progress*100)
                out += progress_bar(s.progress, 49)
                out += '\n'

                out += 'total downloaded: %d Bytes\n' % s.total_done
                out += 'peers: %d seeds: %d distributed copies: %d\n' % \
                    (s.num_peers, s.num_seeds, s.distributed_copies)
                out += '\n'

            out += 'download: %s/s (%s) ' \
                % (add_suffix(s.download_rate), add_suffix(s.total_download))
            out += 'upload: %s/s (%s) ' \
                % (add_suffix(s.upload_rate), add_suffix(s.total_upload))
            out += 'ratio: %s\n' % '0'

            if s.state != lt.torrent_status.seeding:
                out += 'info-hash: %s\n' % h.info_hash()
                out += 'next announce: %s\n' % s.next_announce
                out += 'tracker: %s\n' % s.current_tracker

            write_line(console, out)

            print_peer_info(console, h.get_peer_info())
            print_download_queue(console, h.get_download_queue())

            if True and s.state != lt.torrent_status.seeding:
                out = '\n'
                fp = h.file_progress()
                ti = h.torrent_info()
                for f,p in zip(ti.files(), fp):
                    out += progress_bar(p, 20)
                    out += ' ' + f.path + '\n'
                write_line(console, out)

        write_line(console, 76 * '-' + '\n')
        write_line(console, '(q)uit), (p)ause), (u)npause), (r)eannounce\n')
        write_line(console, 76 * '-' + '\n')

        while 1:
            a = ses.pop_alert()
            if not a: break
            alerts.append(a)

        if len(alerts) > 8:
            del alerts[:len(alerts) - 8]

        for a in alerts:
            if type(a) == str:
                write_line(console, a + '\n')
            else:
                write_line(console, a.msg() + '\n')

        c = console.sleep_and_input(0.5)

        if not c:
            continue

        if c == 'r':
            for h in handles: h.force_reannounce()
        elif c == 'q':
            alive = False
        elif c == 'p':
            for h in handles: h.pause()
        elif c == 'u':
            for h in handles: h.resume()

    for h in handles:
        if not h.is_valid() or not h.has_metadata():
            continue
        h.pause()
        data = lt.bencode(h.write_resume_data())
        open(os.path.join(options.save_path, h.torrent_info().name() + '.fastresume'), 'wb').write(data)

main()

