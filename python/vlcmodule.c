#include <Python.h>
#include <vlc/vlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


static PyObject *vlc_create(PyObject *self, PyObject *args)
{
    int iRc;

    iRc = VLC_Create();
    return Py_BuildValue("i", iRc);
}


static PyObject *vlc_init(PyObject *self, PyObject *args)
{
    int iVlc;
    char *pArgv[] = { "vlc", "--sout", NULL };
    int iRc;

    if (!PyArg_ParseTuple(args, "iss", &iVlc, &pArgv[2]))
        return NULL;
    iRc = VLC_Init(iVlc, 3, pArgv);
    return Py_BuildValue("i", iRc);
}


static PyObject *vlc_addTarget(PyObject *self, PyObject *args)
{
    int iVlc;
    char *file;
    int iRc;

    if (!PyArg_ParseTuple(args, "is", &iVlc, &file))
        return NULL;
    iRc = VLC_AddTarget(iVlc, file, PLAYLIST_APPEND, PLAYLIST_END);
    return Py_BuildValue("i", iRc);
}


static PyObject *vlc_play(PyObject *self, PyObject *args)
{
    int iVlc;
    int iRc;

    if (!PyArg_ParseTuple(args, "i", &iVlc))
        return NULL;
    iRc = VLC_Play(iVlc);
    return Py_BuildValue("i", iRc);
}


static PyObject *vlc_stop(PyObject *self, PyObject *args)
{
    int iVlc;
    int iRc;

    if (!PyArg_ParseTuple(args, "i", &iVlc))
        return NULL;
    iRc = VLC_Stop(iVlc);
    return Py_BuildValue("i", iRc);
}


static PyObject *vlc_pause(PyObject *self, PyObject *args)
{
    int iVlc;
    int iRc;

    if (!PyArg_ParseTuple(args, "i", &iVlc))
        return NULL;
    iRc = VLC_Pause(iVlc);
    return Py_BuildValue("i", iRc);
}


static PyMethodDef VlcMethods[] = {
    {"create", vlc_create, METH_VARARGS, "Create a vlc thread."},
    {"init", vlc_init, METH_VARARGS, "Initialize a vlc thread."},
    {"addTarget", vlc_addTarget, METH_VARARGS, "Add a target in the playlist."},
    {"play", vlc_play, METH_VARARGS, "Play"},
    {"stop", vlc_stop, METH_VARARGS, "Stop"},
    {"pause", vlc_pause, METH_VARARGS, "Pause"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


void initvlc(void)
{
    Py_InitModule("vlc", VlcMethods);
}

