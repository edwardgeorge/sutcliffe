#include "Python.h"
#include <fcntl.h>
#include <util.h>
#include "osx.c"

static void
_get_devices_callback(char *nodename, io_object_t io, void *user_data)
{
    PyList_Append((PyObject*)user_data, PyString_FromString(nodename));
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
    PyDict_SetItemString(trackdict, "session",
        PyInt_FromLong((long)track->session));
    PyDict_SetItemString(trackdict, "number",
        PyInt_FromLong((long)track->number));
    PyDict_SetItemString(trackdict, "first_sector",
        PyInt_FromLong((long)track->first_sector));
    PyList_Append((PyObject*)user_data, trackdict);
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
        Py_XDECREF(tracks);
        tracks = NULL;
        goto end;
    }
    if(PyErr_Occurred() != NULL)
    {
        Py_XDECREF(tracks);
        tracks = NULL;
        goto end;
    }
    end:
    close(fd);
    return tracks;
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
    m = Py_InitModule("_sutcliffe", SMethods);
    if(m == NULL) return;
}
