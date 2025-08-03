/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <glib.h>
#include <sys/types.h>

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _WIN32
#include <sys/wait.h>
#include <signal.h>
#endif

#include "file.h"
#include "debug.h"
#include "main.h"
#include "item.h"
#include "xmlconfig.h"
#include "coord.h"
#include "route.h"
#include "navigation.h"
#include "event.h"
#include "callback.h"
#include "navit_nls.h"
#include "util.h"
#ifdef HAVE_API_WIN32_BASE
#include <windows.h>
#include <winbase.h>
#endif

#ifdef HAVE_API_WIN32_CE
#include "libc.h"
#endif

struct map_data *map_data_default;

struct callback_list *cbl;

/*
 * @def environment_vars
 *
 * @brief Environment variables automatically added (and expanded) by navit at startup
 *
 * A NUL-terminated string array
 * environment_vars[0] is the name of the variable
 * environment_vars[1] is the value used when running from source dir
 * environment_vars[2] is the value used on Linux
 * environment_vars[3] is the value used on Windows CE (see main_init())
 * environment_vars[4] is the value used on Android
 * environment_vars[5] is the value used on Windows
 * ':'  is replaced with NAVIT_PREFIX
 * '::' is replaced with NAVIT_PREFIX and LIBDIR
 * '~'  is replaced with HOME on Linux, or USERPROFILE on Windows (not on Windows CE)
 */

static char *environment_vars[][6] = {
    {"NAVIT_LIBDIR", ":", ":/navit/", ":\\lib", ":/lib", ":\\lib"},
    {"NAVIT_SHAREDIR", ":", ":/", ":", ":/share", ":"},
    {"NAVIT_LOCALEDIR", ":/../locale", ":/", ":\\locale", ":/locale", ":\\locale"},
    {"NAVIT_USER_DATADIR", ":", "~/.navit", ":\\data", ":/home", "~\\navit"},
    {"NAVIT_LOGFILE", NULL, NULL, ":\\navit.log", NULL, ":\\navit.log"},
    {"NAVIT_LIBPREFIX", "*/.libs/", NULL, NULL, NULL, NULL},
    {"NAVIT_MAPS_DIR", "~/.navit/maps", "~/.navit/maps", NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL},
};

static void main_setup_environment(int mode)
{
    int i = 0;
    char *var, *val, *homedir;
    while ((var = environment_vars[i][0]))
    {
        val = environment_vars[i][mode + 1];
        if (val)
        {
            switch (val[0])
            {
            case ':':
                if (val[1] == ':')
                    val = g_strdup_printf("%s/%s%s", getenv("NAVIT_PREFIX"), LIBDIR + sizeof(PREFIX), val + 2);
                else
                    val = g_strdup_printf("%s%s", getenv("NAVIT_PREFIX"), val + 1);
                break;
            case '~':
                homedir = getenv("HOME");
                if (!homedir)
                    homedir = "./";
                val = g_strdup_printf("%s%s", homedir, val + 1);
                break;
            default:
                val = g_strdup(val);
                break;
            }
            setenv(var, val, 0);
            g_free(val);
        }
        i++;
    }
}

void main_init(const char *program)
{
    char *s;

    spawn_process_init();

    cbl = callback_list_new();
    setenv("LC_NUMERIC", "C", 1);
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    if (file_exists("navit.c") || file_exists("navit.o") || file_exists("navit.lo") || file_exists("THIS_IS_THE_NAVIT_WORKING_DIR") || file_exists("Makefile"))
    {
        char buffer[PATH_MAX];
        printf("%s", _("Running from source directory\n"));
        if (getcwd(buffer, PATH_MAX) == NULL)
        { /*libc of navit returns "dummy" */
            printf("%s", _("Error getting current path. use ./ \n"));
            buffer[0] = '.';
            buffer[1] = '/';
            buffer[2] = 0;
        }
        setenv("NAVIT_PREFIX", buffer, 0);
        main_setup_environment(0);
    }
    else
    {
        if (!getenv("NAVIT_PREFIX"))
        {
            int l;
            int progpath_len;
            char *progpath = "/bin/navit";
            l = strlen(program);
            progpath_len = strlen(progpath);
            if (l > progpath_len && !strcmp(program + l - progpath_len, progpath))
            {
                s = g_strdup(program);
                s[l - progpath_len] = '\0';
                if (strcmp(s, PREFIX))
                    printf(_("setting '%s' to '%s'\n"), "NAVIT_PREFIX", s);
                setenv("NAVIT_PREFIX", s, 0);
                g_free(s);
            }
            else
                setenv("NAVIT_PREFIX", PREFIX, 0);
        }
        main_setup_environment(1);
    }

    printf("NAVIT_LIBDIR : %s\nNAVIT_SHAREDIR : %s\nNAVIT_LOCALEDIR: %s\nNAVIT_USER_DATADIR: %s\nNAVIT_LOGFILE: %s\nNAVIT_LIBPREFIX: %s\nNAVIT_MAPS_DIR: %s\n",
           getenv("NAVIT_LIBDIR"),
           getenv("NAVIT_SHAREDIR"),
           getenv("NAVIT_LOCALEDIR"),
           getenv("NAVIT_USER_DATADIR"),
           getenv("NAVIT_LOGFILE"),
           getenv("NAVIT_LIBPREFIX"),
           getenv("NAVIT_MAPS_DIR"));
    s = getenv("NAVIT_WID");
    if (s)
    {
        setenv("SDL_WINDOWID", s, 0);
    }
}
