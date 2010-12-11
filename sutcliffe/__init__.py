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
