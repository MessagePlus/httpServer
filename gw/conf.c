/*******************************************************************************
 **
 ** conf.c
 **
 ** This file is part of the ABYSS Web server project.
 **
 ** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 ** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 ** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 ** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 ** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 ** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 ** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 ** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 ** SUCH DAMAGE.
 **
 *******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gwlib/gwlib.h"

/*  Check this
 #include <direct.h>
 */

#ifdef _UNIX
#include <pwd.h>
#endif

#include "abyss.h"

/*********************************************************************
 ** Configuration Files Parsing Functions
 *********************************************************************/

bool ConfReadLine(TFile *f, char *buffer, uint32 len)
{
	bool r = TRUE;
	char c, *p, *z = buffer;

	while ((--len) > 0)
	{
		if (FileRead(f, buffer, 1) < 1)
		{
			if (z == buffer)
				r = FALSE;
			break;
		};

		if ((*buffer == CR) || (*buffer == LF))
			break;

		buffer++;
	};

	if (len == 0)
		while (FileRead(f, &c, 1) == 1)
			if ((c == CR) || (c == LF))
				break;

	*buffer = '\0';

	/* Discard comments */
	if (p = strchr(z, '#'))
		*p = '\0';

	return r;
}

bool ConfNextToken(char **p)
{
	while (1)
		switch (**p)
		{
		case '\t':
		case ' ':
			(*p)++;
			break;
		case '\0':
			return FALSE;
		default:
			return TRUE;
		};
}

char *ConfGetToken(char **p)
{
	char *p0 = *p;

	while (1)
		switch (**p)
		{
		case '\t':
		case ' ':
		case CR:
		case LF:
		case '\0':
			if (p0 == *p)
				return NULL;

			if (**p)
			{
				**p = '\0';
				(*p)++;
			}
			;
			return p0;

		default:
			(*p)++;
		};
}

bool ConfReadInt(char *p, int32 *n, int32 min, int32 max)
{
	char *e;

	*n = strtol(p, &e, 10);

	if (min != max)
		return ((e != p) && (*n >= min) && (*n <= max));
	else
		return (e != p);
}

bool ConfReadBool(char *p, bool *b)
{
	if (strcasecmp(p, "yes") == 0)
	{
		*b = TRUE;
		return TRUE;
	};

	if (strcasecmp(p, "no") == 0)
	{
		*b = FALSE;
		return TRUE;
	};

	return FALSE;
}

/*********************************************************************
 ** MIME Types File
 *********************************************************************/

bool ConfReadMIMETypes(char *filename)
{
	TFile f;
	char z[512], *p;
	char *mimetype, *ext;

	if (!FileOpen(&f, filename, O_RDONLY))
		return FALSE;

	while (ConfReadLine(&f, z, 512))
	{
		p = z;

		if (ConfNextToken(&p))
			if (mimetype = ConfGetToken(&p))
				while (ConfNextToken(&p))
					if (ext = ConfGetToken(&p))
						MIMETypeAdd(mimetype, ext);
					else
						break;
	};

	FileClose(&f);
	return TRUE;
}

/*********************************************************************
 ** Server Configuration File
 *********************************************************************/

bool ConfReadServerFile(Cfg *cfg, TServer *srv)
{
	CfgGroup *cfg_group;
	Octstr *log_file;
	long log_level;
	long http_port;
	long keep_alive;
	long time_out;
	log_file = NULL;
	log_level = 0;
	cfg_group = cfg_get_single_group(cfg, octstr_imm("httpServer"));
	if (cfg_group == NULL)
		panic(0, "No 'httpServer' group in configuration");

	if (cfg_get_integer(&http_port, cfg_group, octstr_imm("httpServer-port")) == -1)
		http_port = 8080;
	srv->port = http_port;

	/* setup logfile stuff */
	log_file = cfg_get(cfg_group, octstr_imm("log-file"));

	cfg_get_integer(&log_level, cfg_group, octstr_imm("log-level"));
	if (log_file != NULL)
	{
		info(0, "Starting to log to file %s level %ld", octstr_get_cstr(log_file), log_level);
		log_open(octstr_get_cstr(log_file), log_level, GW_NON_EXCL);

	}
	cfg_get(cfg_group, octstr_imm("server-root"));
	cfg_get_integer(&keep_alive, cfg_group, octstr_imm("keep-alive"));
	srv->keepalivemaxconn = keep_alive;

	cfg_get_integer(&time_out, cfg_group, octstr_imm("time-out"));
	srv->keepalivetimeout = time_out;
	srv->timeout = time_out;

	debug("httpserver", 0, "==========Configuration Parameters============");
	debug("httpserver",0, "http-server-port: %ld", http_port);
	debug("httpserver",0, "log-file:         %s", octstr_get_cstr(log_file));
	debug("httpserver",0, "keep-alive:       %ld", keep_alive);
	debug("httpserver",0, "time-out:         %ld", time_out);
	debug("httpserver", 0, "==============================================");
	octstr_destroy(log_file);
	return TRUE;
}
