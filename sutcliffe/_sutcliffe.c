#include "Python.h"
#include "osx.c"

static PyMethodDef SMethods[] = {
    {NULL, NULL, 0, NULL} /* sentinel */
};

PyMODINIT_FUNC
init_sutcliffe(void)
{
    PyObject *m;
    m = Py_InitModule("_sutcliffe", SMethods);
    if(m == NULL) return;
}
