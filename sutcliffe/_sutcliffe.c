#include "Python.h"
#include <fcntl.h>
#include <util.h>
#include "osx.c"

// TYPES BEGIN

typedef struct {
    PyTypeObject tuple;
    CDMSF msf;
    unsigned int sector;
} MSFObject;

static PyObject *
MSF_new(PyTypeObject *self, PyObject *args, PyObject *kwds)
{
    unsigned int minute, second, frame;
    PyObject *tuple, *newargs;
    MSFObject *result;
    static char *kwlist[] = {"minute", "second", "frame", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "III", kwlist,
        &minute, &second, &frame)) return NULL;
    if(!(tuple = Py_BuildValue("III", minute, second, frame)))
        return NULL;
    if(!(newargs = Py_BuildValue("(O)", tuple)))
    {
        Py_DECREF(tuple);
        return NULL;
    }
    result = (MSFObject*)PyTuple_Type.tp_new(self, newargs, NULL);
    Py_DECREF(tuple);
    Py_DECREF(newargs);
    if(!result) return NULL;
    result->msf.minute = minute;
    result->msf.second = second;
    result->msf.frame = frame;
    result->sector = CDConvertMSFToLBA(result->msf);
    return (PyObject*)result;
}

static PyObject *
MSF_getmsf(MSFObject *self, void *closure)
{
    PyObject *item = PyTuple_GetItem((PyObject*)self, (int)closure);
    Py_XINCREF(item);
    return item;
}

static PyObject *
MSF_getsector(MSFObject *self, void *closure)
{
    return Py_BuildValue("I", self->sector);
}

static PyGetSetDef MSF_getset[] = {
    {"minute", (getter)MSF_getmsf, NULL, "minute", (void*)0},
    {"second", (getter)MSF_getmsf, NULL, "second", (void*)1},
    {"frame", (getter)MSF_getmsf, NULL, "frame", (void*)2},
    {"sector", (getter)MSF_getsector, NULL, "sector", NULL},
    {NULL}
};

static PyTypeObject MSFType = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "sutcliffe.MSF",
    .tp_basicsize = sizeof(MSFObject),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_getset = MSF_getset,
    .tp_new = (newfunc)MSF_new,
};

static PyObject *
new_msf(CDMSF msf)
{
    PyObject *args = Py_BuildValue("III",
        msf.minute, msf.second, msf.frame);
    PyObject *MSFobj = PyObject_CallObject((PyObject*)&MSFType, args);
    Py_XDECREF(args);
    return MSFobj;
}

typedef struct {
    PyObject_HEAD
    unsigned int point;
    unsigned int session;
    unsigned int tno;
    unsigned int adr;
    unsigned int control;
    CDMSF p;
    CDMSF address;
} TrackObject;

static void
Track_dealloc(TrackObject *self)
{
    /*Py_XDECREF(self->p);*/
    /*Py_XDECREF(self->address);*/
    self->ob_type->tp_free((PyObject *)self);
}

static PyObject *
Track_get_point(TrackObject *self, void *closure)
{
    return Py_BuildValue("I", self->point);
}

static PyObject *
Track_get_session(TrackObject *self, void *closure)
{
    return Py_BuildValue("I", self->session);
}

static PyObject *
Track_get_control(TrackObject *self, void *closure)
{
    return Py_BuildValue("I", self->control);
}

static PyObject *
Track_get_p(TrackObject *self, void *closure)
{
    return new_msf(self->p);
}

static PyObject *
Track_get_address(TrackObject *self, void *closure)
{
    return new_msf(self->address);
}

static PyGetSetDef Track_getset[] = {
    {"session", (getter)Track_get_session, NULL, "session", NULL},
    {"point", (getter)Track_get_point, NULL, "point", NULL},
    {"control", (getter)Track_get_control, NULL, "control", NULL},
    {"p", (getter)Track_get_p, NULL, "p", NULL},
    {"address", (getter)Track_get_address, NULL, "address", NULL},
    {NULL}
};

static PyObject *
Track_repr(TrackObject *self)
{
    return PyString_FromFormat("<%s: %d (session:%d) %d:%d:%d [%d]>",
        self->ob_type->tp_name,
        self->point,
        self->session,
        self->p.minute,
        self->p.second,
        self->p.frame,
        CDConvertMSFToLBA(self->p));
}

static PyTypeObject TrackType = {
    PyObject_HEAD_INIT(NULL)
    .ob_size = 0,
    .tp_name = "sutcliffe.Track",
    .tp_basicsize = sizeof(TrackObject),
    .tp_new = NULL,  // prevent creation from python
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)Track_dealloc,
    .tp_getset = Track_getset,
    .tp_repr = (reprfunc)Track_repr,
};

static PyObject *
new_track(CDTOCDescriptor *descr)
{
    /*PyObject *tmp;*/
    TrackObject *self = (TrackObject*)PyType_GenericNew(&TrackType, NULL, NULL);
    if (!self) return NULL;
    self->point = descr->point;
    self->session = descr->session;
    self->tno = descr->tno;
    self->adr = descr->adr;
    self->control = descr->control;
    /*tmp = new_msf(descr->p);f*/
    /*if(!tmp) return NULL;*/
    /*self->p = tmp;*/
    /*tmp = new_msf(descr->p);f*/
    /*if(!tmp) return NULL;*/
    /*self->address = tmp;*/
    self->p = descr->p;
    self->control = descr->control;
    return self;
}

// TYPES END

static void
_get_devices_callback(char *nodename, io_object_t io, void *user_data)
{
    PyObject *s = PyString_FromString(nodename);
    PyList_Append((PyObject*)user_data, s);
    Py_XDECREF(s);
}

static PyObject *
S_get_devices(PyObject *self, PyObject *args)
{
    int num;
    PyObject *devices = PyList_New(0);

    num = iterate_devices(_get_devices_callback, (void*)devices);
    if(PyErr_Occurred() != NULL) return NULL;
    if(num == -1)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not get services.");
        return NULL;
    }
    return devices;
}

static void
_get_toc_callback(CDTOCDescriptor *track, void *user_data)
{
    PyObject *tobj = new_track(track);
    if(PyList_Check((PyObject*)user_data))
    {
        PyList_Append((PyObject*)user_data, tobj);
    }
    else
    {
        PyObject_CallMethod((PyObject*)user_data, "append", "O", tobj);
    }
    Py_XDECREF(tobj);
}

static PyObject *
S_get_toc(PyObject *self, PyObject *args)
{
    int fd;
    int newlist = 0;
    char *device_nodename;
    PyObject *tracks = Py_None;
    PyArg_ParseTuple(args, "s|O", &device_nodename, &tracks);
    fd = opendev(device_nodename, O_RDONLY | O_NONBLOCK, 0, &device_nodename);
    if(fd == -1)
    {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, device_nodename);
        goto end;
    }
    if (tracks == Py_None) {
        tracks = PyList_New(0);
        newlist = 1;
    }
    if(read_toc(fd, _get_toc_callback, (void*)tracks) == -1)
    {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, device_nodename);
        goto error;
    }
    // catch exceptions from callbacks
    if(PyErr_Occurred() != NULL) goto error;
    end:
    close(fd);
    if (!newlist){ Py_INCREF(tracks); }
    return tracks;
    error:
    if (newlist){ Py_XDECREF(tracks); }
    tracks = NULL;
    goto end;
}

static PyMethodDef SMethods[] = {
    {"get_devices", S_get_devices, METH_NOARGS},
    {"get_toc", S_get_toc, METH_VARARGS},
    {NULL, NULL, 0, NULL} /* sentinel */
};

PyMODINIT_FUNC
init_sutcliffe(void)
{
    PyObject *m;

    MSFType.tp_base = &PyTuple_Type;
    if (PyType_Ready(&MSFType) < 0) return;
    if (PyType_Ready(&TrackType) < 0) return;

    m = Py_InitModule("_sutcliffe", SMethods);
    if(m == NULL) return;

    Py_INCREF(&MSFType);
    PyModule_AddObject(m, "MSF", (PyObject *)&MSFType);
    Py_INCREF(&TrackType);
    PyModule_AddObject(m, "Track", (PyObject *)&TrackType);
}
