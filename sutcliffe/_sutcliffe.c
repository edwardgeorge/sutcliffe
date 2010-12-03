#include "Python.h"
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

static PyMethodDef SMethods[] = {
    {"get_devices", S_get_devices, METH_NOARGS},
    {NULL, NULL, 0, NULL} /* sentinel */
};

PyMODINIT_FUNC
init_sutcliffe(void)
{
    PyObject *m;
    m = Py_InitModule("_sutcliffe", SMethods);
    if(m == NULL) return;
}
