/* playlist_python.c
 * - Interpreter functions for python
 * Copyright (c) 2000 Alexander Haväng
 * Copyright (c) 2001-3 Brendan Cully
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "definitions.h"
#ifdef _REENTRANT
#undef _REENTRANT
#endif
#include <Python.h>

extern ices_config_t ices_config;

static PyObject *python_module;
static char* python_path = NULL;

static char* pl_init_hook;
static char* pl_shutdown_hook;
static char* pl_get_next_hook;
static char* pl_get_metadata_hook;
static char* pl_get_lineno_hook;

/* -- local prototypes -- */
static int playlist_python_get_lineno(void);
static char* playlist_python_get_next(void);
static char* playlist_python_get_metadata(void);
static int playlist_python_reload(void);
static void playlist_python_shutdown(void);

static int python_init(void);
static void python_shutdown(void);
static int python_setup_path(void);
static PyObject* python_eval(char *functionname);
static char* python_find_attr(PyObject* module, char* f1, char* f2);

/* Call python function to initialize the python script */
int ices_playlist_python_initialize(playlist_module_t* pm) {
	PyObject* res;
	int rc = 1;

	pm->get_next = playlist_python_get_next;
	pm->get_metadata = playlist_python_get_metadata;
	pm->get_lineno = playlist_python_get_lineno;
	pm->reload = playlist_python_reload;
	pm->shutdown = playlist_python_shutdown;

	if (python_init() < 0)
		return -1;

	if (pl_init_hook) {
		if ((res = python_eval(pl_init_hook)) && PyInt_Check(res))
			rc = PyInt_AsLong(res);
		else
			ices_log_error("ices_init failed");

		Py_XDECREF(res);
	}

	return rc;
}

/* Call the python function to get the current line number */
static int playlist_python_get_lineno(void) {
	PyObject* res;
	int rc = 0;

	if (pl_get_lineno_hook) {
		if ((res = python_eval(pl_get_lineno_hook)) && PyInt_Check(res))
			rc = PyInt_AsLong(res);
		else
			ices_log_error("ices_get_lineno failed");

		Py_XDECREF(res);
	}

	return rc;
}

/* Call python function to get next file to play */
static char *playlist_python_get_next(void) {
	PyObject* res;
	char* rc = NULL;

	if ((res = python_eval(pl_get_next_hook)) && PyString_Check(res))
		rc = ices_util_strdup(PyString_AsString(res));
	else
		ices_log_error("ices_get_next failed");

	Py_XDECREF(res);

	return rc;
}

static char*playlist_python_get_metadata(void) {
	PyObject* res;
	char* rc = NULL;

	if (pl_get_metadata_hook) {
		if ((res = python_eval(pl_get_metadata_hook)) && PyString_Check(res))
			rc = ices_util_strdup(PyString_AsString(res));
		else
			ices_log_error("ices_get_metadata failed");

		Py_XDECREF(res);
	}

	return rc;
}

/* Attempt to reload the playlist module */
static int playlist_python_reload(void) {
	PyObject* new_module;

	if (!(new_module = PyImport_ReloadModule(python_module))) {
		ices_log_error("Playlist module reload failed");
		PyErr_Print();

		return -1;
	}

	python_module = new_module;
	ices_log_debug("Playlist module reloaded");

	return 0;
}

/* Call python function to shutdown the script */
static void playlist_python_shutdown(void) {
	PyObject* res;

	if (pl_shutdown_hook) {
		if (!((res = python_eval(pl_shutdown_hook)) && PyInt_Check(res)))
			ices_log_error("ices_shutdown failed");

		Py_XDECREF(res);
	}

	python_shutdown();
}

/* -- Python interpreter management -- */

/* Function to initialize the python interpreter */
static int python_init(void) {
	/* For some reason, python refuses to look in the
	 * current directory for modules */
	if (python_setup_path() < 0)
		return -1;

	Py_Initialize();

	ices_log_debug("Importing %s.py module...", ices_config.pm.module);

	/* Call the python api code to import the module */
	if (!(python_module = PyImport_ImportModule(ices_config.pm.module))) {
		ices_log("Error: Could not import module %s", ices_config.pm.module);
		PyErr_Print();
		return -1;
	}

	/* Find defined methods */
	pl_init_hook = python_find_attr(python_module, "ices_init",
					"ices_python_initialize");
	pl_shutdown_hook = python_find_attr(python_module, "ices_shutdown",
					    "ices_python_shutdown");
	pl_get_next_hook = python_find_attr(python_module, "ices_get_next",
					    "ices_python_get_next");
	pl_get_metadata_hook = python_find_attr(python_module, "ices_get_metadata",
						"ices_python_get_metadata");
	pl_get_lineno_hook = python_find_attr(python_module, "ices_get_lineno",
					      "ices_python_get_current_lineno");

	if (!pl_get_next_hook) {
		ices_log_error("The playlist module must define at least the ices_get_next method");
		return -1;
	}

	return 0;
}

/* Force the python interpreter to look in our module path
 * and in the current directory for modules */
static int python_setup_path(void) {
	char *oldpath = getenv("PYTHONPATH");

	if (oldpath && (python_path = (char*) malloc(strlen(oldpath) + strlen(ICES_MODULEDIR) + 15)))
		sprintf(python_path, "PYTHONPATH=%s:%s:.", oldpath, ICES_MODULEDIR);
	else if ((python_path = (char*) malloc(strlen(ICES_MODULEDIR) + 14)))
		sprintf(python_path, "PYTHONPATH=%s:.", ICES_MODULEDIR);
	else {
		ices_log_error("Could not allocate memory for python environment");
		return -1;
	}

	putenv(python_path);

	return 0;
}

/* Shutdown the python interpreter */
static void python_shutdown(void) {
	Py_Finalize();
	ices_util_free(python_path);
}

static PyObject*python_eval(char *functionname) {
	PyObject *ret;

	ices_log_debug("Interpreting [%s]", functionname);

	/* Call the python function */
	ret = PyObject_CallMethod(python_module, functionname, NULL);
	if (!ret)
		PyErr_Print();

	ices_log_debug("Done interpreting [%s]", functionname);
	return ret;
}

static char*python_find_attr(PyObject* module, char* f1, char* f2) {
	char* rc;

	if (PyObject_HasAttrString(module, f1))
		rc = f1;
	else if (PyObject_HasAttrString(module, f2))
		rc = f2;
	else
		rc = NULL;

	if (rc)
		ices_log_debug("Found method: %s", rc);

	return rc;
}
