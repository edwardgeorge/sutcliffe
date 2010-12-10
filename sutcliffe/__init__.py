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
