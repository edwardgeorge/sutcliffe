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
    PyObject *tuple;
    MSFObject *result;
    static char *kwlist[] = {"minute", "second", "frame", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "III", kwlist,
        &minute, &second, &frame)) return NULL;
    if(!(tuple = Py_BuildValue("((III))", minute, second, frame)))
        return NULL;
    if(!(result = (MSFObject*)PyTuple_Type.tp_new(self, tuple, NULL)))
        return NULL;
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
_get_toc_callback(trackinfo *track, void *user_data)
{
    PyObject *trackdict = PyDict_New();
    PyObject *i;
    i = PyInt_FromLong((long)track->session);
    PyDict_SetItemString(trackdict, "session", i);
    Py_XDECREF(i);
    i = PyInt_FromLong((long)track->number);
    PyDict_SetItemString(trackdict, "number", i);
    Py_XDECREF(i);
    i = PyInt_FromLong((long)track->first_sector);
    PyDict_SetItemString(trackdict, "first_sector", i);
    Py_XDECREF(i);
    PyList_Append((PyObject*)user_data, trackdict);
    Py_DECREF(trackdict);
}

static PyObject *
S_get_toc(PyObject *self, PyObject *args)
{
    int fd;
    char *device_nodename;
    PyObject *tracks = NULL;
    PyArg_ParseTuple(args, "s", &device_nodename);
    fd = opendev(device_nodename, O_RDONLY | O_NONBLOCK, 0, &device_nodename);
    if(fd == -1)
    {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, device_nodename);
        goto end;
    }
    tracks = PyList_New(0);
    if(read_toc(fd, _get_toc_callback, (void*)tracks) == -1)
    {
        PyErr_SetFromErrnoWithFilename(PyExc_IOError, device_nodename);
        goto error;
    }
    // catch exceptions from callbacks
    if(PyErr_Occurred() != NULL) goto error;
    end:
    close(fd);
    return tracks;
    error:
    Py_XDECREF(tracks);
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

    m = Py_InitModule("_sutcliffe", SMethods);
    if(m == NULL) return;

    Py_INCREF(&MSFType);
    PyModule_AddObject(m, "MSF", (PyObject *)&MSFType);
}
