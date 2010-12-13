import base64
import hashlib

from _sutcliffe import *


class Session(object):
    def __init__(self, num):
        self.num = num
        self.tracks = []
        self.first_track = None
        self.last_track = None
        self.disc_type = 0
        self.lead_out = None

    def __len__(self):
        return len(self.tracks)

    def __iter__(self):
        return iter(self.tracks)

    @property
    def cdda(self):
        return self.disc_type == 0

    def append(self, track):
        if 1 <= track.num <= 99 and track.adr == 1:
            self.tracks.append(track)
        elif track.num == 160 and track.adr == 1:
            self.first_track = track.p.minute
            self.disc_type = track.p.second
        elif track.num == 161 and track.adr == 1:
            self.last_track = track.p.minute
        elif track.num == 162 and track.adr == 1:
            self.lead_out = track.p

    def get_track(self, num):
        for i in self.tracks:
            if i.num == num:
                return i
        raise IndexError()

    def __getitem__(self, item):
        if isinstance(item, slice):
            b, e = item.start, item.stop
            if b is None:
                b = self.first_track
            if e is None:
                e = self.last_track
        elif isinstance(item, int):
            b, e = item, item
        else:
            raise TypeError()

        start_sector = self.get_track(b).p.sector
        if e + 1 > self.last_track:
            end_sector = self.lead_out.sector - 1
        else:
            end_sector = self.get_track(e + 1).p.sector - 1

        return start_sector, end_sector


class TOC(object):
    def __init__(self):
        self.sessions = []

    def __len__(self):
        return len(self.sessions)

    def __iter__(self):
        return iter(self.sessions)

    @property
    def cdda_sessions(self):
        for i in self:
            if i.cdda:
                yield i

    def append(self, track):
        session = track.session
        i = session - len(self.sessions)
        if i > 0:
            self.sessions.extend([None] * i)
            self.sessions[session - 1] = Session(session)
        self.sessions[session - 1].append(track)

    @property
    def cddb_discid(self):
        s = list(self.cdda_sessions)[0]
        #csum = lambda i: ((i % 10) + csum(i // 10)) if i > 0 else 0
        csum = lambda a, b: csum(a + (b % 10), b // 10) if b > 0 else a
        #nums = lambda msf: (msf.minute * 60) + msf.second
        nums = lambda msf: (msf.sector + 150) // 75
        calc = lambda i: nums(i.p)
        checksum = reduce(csum, map(calc, s), 0)
        length = (((s.lead_out.sector + 150) / 75) -
            ((s.get_track(s.first_track).p.sector + 150) / 75))
        res = ((checksum % 0xff) << 24 | length << 8 | s.last_track)
        res = "%08x" % (res, )
        return res

    @property
    def musicbrainz_discid(self):
        s = list(self.cdda_sessions)[0]
        h = hashlib.sha1()
        h.update("%02X" % s.first_track)
        h.update("%02X" % s.last_track)
        h.update("%08X" % (s.lead_out.sector + 150))
        for i in range(99):
            try:
                t = s.get_track(i + 1)
            except IndexError, e:
                h.update("%08X" % 0)
            else:
                h.update("%08X" % (t.p.sector + 150))
        return (base64.b64encode(h.digest())
                    .replace('+', '.')
                    .replace('/', '_')
                    .replace('=', '-'))
