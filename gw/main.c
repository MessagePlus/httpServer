/*******************************************************************************
 **
 ** main.c
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
/* Must check this
 #include <sys/io.h>
 */
#endif	/* _WIN32 */


#include <sys/signal.h>
#include <sys/wait.h>


#include "abyss.h"
#include "gwlib/gwlib.h"
#include "mysqlDB.h"

/* our config */
static Cfg *cfg;

void AnswerInit(TSession *r, uint16 statuscode)
{
        ResponseChunked(r);

        ResponseStatus(r,statuscode);

        ResponseContentType(r,"text/html");

        ResponseWrite(r);

        HTTPWrite(r,"<HTML>",6);
        HTTPWrite(r,"<HEAD>",6);
        HTTPWrite(r,"<TITLE>Message Plus Web Server</TITLE>",38);
        HTTPWrite(r,"</HEAD>",7);
        HTTPWrite(r,"<BODY bgColor=#FFFFFF text=#000000>",35);
        HTTPWrite(r,"<H3 align=center>Message Plus Web Server</H3>",45);
        HTTPWrite(r,"<BR>",4);
        HTTPWrite(r,"<BR>",4);

        HTTPWrite(r,"</P>",4);
        HTTPWrite(r,"<hr>",4);
        HTTPWrite(r,"<P>",3);
        HTTPWrite(r,"<CENTER><FONT size=-1>",22);
        HTTPWrite(r,"Copyright &copy; 2007 . All rights reserved.",44);
        HTTPWrite(r,"</FONT>",7);
        HTTPWrite(r,"<BR><BR></center></P>",21);
        HTTPWrite(r,"</BODY></HTML>",14);
        HTTPWriteEnd(r);


}

void AnswerXML(TSession *r, uint16 statuscode, char *buffer)
{
       ResponseChunked(r);
       ResponseStatus(r,statuscode);
       ResponseContentType(r,"text/xml");

       ResponseWrite(r);
       HTTPWrite(r,buffer,strlen(buffer));
       HTTPWriteEnd(r);
}

bool HandleMO(TSession *r) {
	int ret;
	char z[4096];
	char errText[1024];
	if (strcmp(r->uri, "/recepcion/mt") != 0)
			return FALSE;

	debug("httpserver",0, "***************** **************** ************");

	ret = processXML(r, errText, 500);

	switch (ret) {
	case 0:
		sprintf(z,"<?xml version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"yes\"?>\n<sms-Response>\n<status>0</status>\n<mensaje>SMS Recibido</mensaje>\n</sms-Response>\n");
		AnswerXML(r, 200, z);
		break;
	default:
		debug("httpServer",0, "ERROR: [%s] %s", HTTPReasonByStatus(ret), errText);
		sprintf(z,"<?xml version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"yes\"?>\n<sms-Response>\n<status>%d</status>\n<mensaje>%s</mensaje>\n</sms-Response>\n",ret, errText);
		debug("httpServer",0, " %s", z);
		AnswerXML(r, 200, z);
		break;

	}
	debug("httpServer",0, "***************** **************** ************");

	return TRUE;

}

void Answer(TSession *r, uint16 statuscode, char *buffer)
{
	ResponseChunked(r);

	ResponseStatus(r, statuscode);

	ResponseContentType(r, "text/html");

	ResponseWrite(r);

	HTTPWrite(r, "<HTML><BODY>", 12);

	HTTPWrite(r, buffer, strlen(buffer));

	HTTPWrite(r, "</BODY></HTML>", 14);

	HTTPWriteEnd(r);
}

bool HandleInit(TSession *r)
{
        char z[50];
        time_t ltime;
        TDate date;

        if (strcmp(r->uri,"/")!=0)
                return FALSE;


        AnswerInit(r,200);

        return TRUE;
}


bool HandleTime(TSession *r)
{
	char z[50];
	time_t ltime;
	TDate date;

	if (strcmp(r->uri, "/time") != 0)
		return FALSE;

	if (!RequestAuth(r, "Mot de passe", "moez", "hello"))
		return TRUE;

	time(&ltime);
	DateFromGMT(&date, ltime);

	strcpy(z, "The time is ");
	DateToString(&date, z + strlen(z));

	Answer(r, 200, z);

	return TRUE;
}

bool HandleDump(TSession *r)
{
	char z[50];

	if (strcmp(r->uri, "/name") != 0)
		return FALSE;

	sprintf(z, "Server name is %s", (r->server)->name);
	Answer(r, 200, z);

	return TRUE;
}

bool HandleStatus(TSession *r)
{
	uint32 status;

	if (sscanf(r->uri, "/status/%d", &status) <= 0)
		return FALSE;

	ResponseStatus(r, (uint16) status);

	return TRUE;
}

bool HandleMIMEType(TSession *r)
{
	char *m;

	if (strncmp(r->uri, "/mime/", 6) != 0)
		return FALSE;

	m = MIMETypeFromExt(r->uri + 6);
	if (!m)
		m = "(none)";

	Answer(r, 200, m);

	return TRUE;
}


void copyright()
{
	printf("ABYSS Web Server version "SERVER_VERSION"\n(C) Moez Mahfoudh - 2000\n\n");
}

void help(char *name)
{
	copyright();
	printf("Usage: %s [-h] [-c configuration file]\n\n", name);
}

static int check_args(int i, int argc, char **argv)
{
	if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--tryhttp") == 0)
	{
		//only_try_http = 1;
	}
	else
		return -1;

	return 0;
}

/*
 * Adding hooks to kannel check config
 *
 * Martin Conte.
 */

static int smppbox_is_allowed_in_group(Octstr *group, Octstr *variable)
{
	Octstr *groupstr;

	groupstr = octstr_imm("group");

#define OCTSTR(name) \
        if (octstr_compare(octstr_imm(#name), variable) == 0) \
        return 1;
#define SINGLE_GROUP(name, fields) \
        if (octstr_compare(octstr_imm(#name), group) == 0) { \
        if (octstr_compare(groupstr, variable) == 0) \
        return 1; \
        fields \
        return 0; \
    }
#define MULTI_GROUP(name, fields) \
        if (octstr_compare(octstr_imm(#name), group) == 0) { \
        if (octstr_compare(groupstr, variable) == 0) \
        return 1; \
        fields \
        return 0; \
    }
#include "httpServer-cfg.def"

	return 0;
}

#undef OCTSTR
#undef SINGLE_GROUP
#undef MULTI_GROUP

static int smppbox_is_single_group(Octstr *query)
{
#define OCTSTR(name)
#define SINGLE_GROUP(name, fields) \
        if (octstr_compare(octstr_imm(#name), query) == 0) \
        return 1;
#define MULTI_GROUP(name, fields) \
        if (octstr_compare(octstr_imm(#name), query) == 0) \
        return 0;
#include "httpServer-cfg.def"
	return 0;
}

static void signal_handler(int signum)
{
	/* On some implementations (i.e. linuxthreads), signals are delivered
	 * to all threads.  We only want to handle each signal once for the
	 * entire box, and we let the gwthread wrapper take care of choosing
	 * one.
	 */
	if (!gwthread_shouldhandlesignal(signum))
		return;

	switch (signum)
	{
	case SIGINT:
	case SIGTERM:

			error(0, "SIGINT received, aborting program...");
			gwthread_wakeup_all();
			gwlib_shutdown();
			exit(1);
		break;

	case SIGHUP:
		warning(0, "SIGHUP received, catching and re-opening logs");
		log_reopen();
		alog_reopen();
		break;

		/*
		 * It would be more proper to use SIGUSR1 for this, but on some
		 * platforms that's reserved by the pthread support.
		 */
	case SIGQUIT:
		warning(0, "SIGQUIT received, reporting memory usage.");
		gw_check_leaks();
		break;
	}
}

static void setup_signal_handlers(void)
{
	struct sigaction act;

	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
}
int main(int argc, char **argv)
{
	TServer srv;
	char *p, *conffile = DEFAULT_CONF_FILE;
	bool err = FALSE;
	int cf_index;
	Octstr *filename, *version;

	gwlib_init();
	cf_index = get_and_set_debugs(argc, argv, check_args);
	setup_signal_handlers();
	if (argv[cf_index] == NULL)
		filename = octstr_create("httpServer.conf");
	else
		filename = octstr_create(argv[cf_index]);

	cfg = cfg_create(filename);

	/* Adding cfg-checks to core */
	cfg_add_hooks(smppbox_is_allowed_in_group, smppbox_is_single_group);



	if (cfg_read(cfg) == -1)
		panic(0, "Couldn't read configuration from `%s'.", octstr_get_cstr(filename));

	octstr_destroy(filename);

	version = octstr_format("HttpServer version %s gwlib", GW_VERSION);
	report_versions(octstr_get_cstr(version));
	octstr_destroy(version);

	struct server_type *res = NULL;
	res = sqlbox_init_mysql(cfg);
	sqlbox_configure_mysql(cfg);

	DateInit();

	MIMETypeInit();

	ServerCreate(&srv, "HTTPServer", 80, DEFAULT_DOCS, NULL);

	ConfReadServerFile(cfg, &srv);

	ServerAddHandler(&srv, HandleMO);
	ServerAddHandler(&srv, HandleTime);
	ServerAddHandler(&srv, HandleDump);
	ServerAddHandler(&srv, HandleStatus);
	ServerAddHandler(&srv, HandleMIMEType);
	ServerAddHandler(&srv, HandleInit);
	ServerInit(&srv);

	ServerRun(&srv);
	cfg_destroy(cfg);

	return 0;
}
