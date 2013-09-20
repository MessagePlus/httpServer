/*
 * processXML.c
 *
 *  Created on: Nov 3, 2012
 *      Author: rxvallejoc
 */

#include <stdio.h>
#include <stdlib.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

//#include <utils/util.h>

#include "abyss.h"
#include "mysqlDB.h"

typedef struct _service{
	Octstr *id;
	Octstr *name;
	Octstr *shortNumber;
	Octstr *connectionId;
	Octstr *carrierId;
	Octstr *integratorId;
	Octstr *integratorQueueId;
	Octstr *errorCode;
	Octstr *errorText;
} Service;


static Service *serviceCreate(){
	Service *service;
	service= gw_malloc(sizeof(Service));
	service->id=NULL;
	service->name=NULL;
	service->shortNumber=NULL;
	service->connectionId=NULL;
	service->carrierId=NULL;
	service->integratorId=NULL;
	service->integratorQueueId=NULL;
	service->errorCode=NULL;
	service->errorText=NULL;
	return service;
}

static void serviceDestroy(Service *service) {
	if (service == NULL)
		return;
	if (service->id)
		octstr_destroy(service->id);
	if (service->name)
		octstr_destroy(service->name);
	if (service->shortNumber)
		octstr_destroy(service->shortNumber);
	if (service->connectionId)
		octstr_destroy(service->connectionId);
	if (service->carrierId)
		octstr_destroy(service->carrierId);
	if (service->integratorId)
		octstr_destroy(service->integratorId);
	if (service->integratorQueueId)
		octstr_destroy(service->integratorQueueId);
	if (service->errorCode)
		octstr_destroy(service->errorCode);
	if (service->errorText)
		octstr_destroy(service->errorText);

	gw_free(service);
}

void initRowMO(SMSmsg *msg) {
	memset(msg, 0, sizeof(SMSmsg));
}

void decodeUrl(urlentry *entries, char *cl, int *m) {
	int i;
	*m = 0;

	if (cl == NULL) {
		return;
	}

	for (i = 0; cl[0] != '\0'; i++) {
		getword(entries[i].val, cl, '&');
		plustospace(entries[i].val);
		unescape_url(entries[i].val);
		getword(entries[i].name, entries[i].val, '=');
	}
	*m = i;
}

char x2c(char *what) {
	char digit;
	digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
	digit *= 16;
	digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
	return (digit);
}

void unescape_url(char *url) {
	int i, j;

	for (i = 0, j = 0; url[j]; ++i, ++j) {
		if ((url[i] = url[j]) == '%') {
			url[i] = x2c(&url[j + 1]);
			j += 2;
		}
	}
	url[i] = '\0';
}

void getword(char *word, char *line, char stop) {
	int i, j;
	for (i = 0; ((line[i]) && (line[i] != stop)); i++) {
		word[i] = line[i];
	}

	word[i] = '\0';
	if (line[i])
		++i;
	j = 0;
	while (line[j++] = line[i++])
		;
}

void plustospace(char *str) {
	int i;
	for (i = 0; str[i]; i++) {
		if (str[i] == '+')
			str[i] = ' ';
	}
}

int ltrim(char *txt) {
	int i;
	if (txt == NULL) {
		return 0;
	}

	i = 0;
	while (i < strlen(txt)) {
		if (txt[i] == ' ' || txt[i] == '\n' || txt[i] == '\r' || txt[i] == '\t') {
			i++;
		} else {
			break;
		}
	}
	if (i > 0) {
		int j;
		for (j = 0; j < strlen(txt) - i; j++) {
			txt[j] = txt[i + j];
		}
		txt[j] = '\0';
	}

	return 0;
}

int rtrim(char *txt) {
	int i = 0;

	if (txt == NULL) {
		return 0;
	}

	i = strlen(txt) - 1;
	while (i >= 0) {
		if (txt[i] == ' ' || txt[i] == '\n' || txt[i] == '\r' || txt[i] == '\t') {
			i--;
		} else {
			break;
		}
	}

	txt[i + 1] = '\0';

	return 0;
}

int trim(char *txt) {
	ltrim(txt);
	rtrim(txt);
	return 0;
}

xmlNodePtr getNodePtr(xmlDocPtr doc, xmlNodePtr current, const char* tag)
{
   while (current != NULL)
   {
       if ( ! xmlStrcmp(current->name, (const xmlChar*) tag) )
       {
          //return current->xmlChildrenNode;
	  return current;
       }
       current = current->next;
    }
    return NULL;
}

xmlChar* getNodeValue(xmlDocPtr doc, xmlNodePtr current, const char * tag)
{
   while (current != NULL)
   {
       if ( ! xmlStrcmp(current->name, (const xmlChar*) tag) )
       {
           return xmlNodeListGetString(doc, current->xmlChildrenNode, 1);
       }
       current = current->next;
   }
   return NULL;
}
int parseXML(char *buf, char *recipientId, char *service,char *messageId, msgSMS **msgText, int *msgParts, char *errText) {
	xmlDocPtr document;
	xmlNodePtr root, current;
	xmlChar *clave;

	while (buf[0] != '\0' && buf[0] != '<') {
		buf++;
	}

	if ((document = xmlParseMemory(buf, strlen(buf))) == NULL) {
		strcpy(errText, "ERROR: Bad document.");
		return 8000;
	}

	if ((root = xmlDocGetRootElement(document)) == NULL) {
		xmlFreeDoc(document);
		strcpy(errText, "ERROR: Bad document. Root element not found.");
		return 8000;
	}

	if (xmlStrcmp(root->name, (const xmlChar*) "sms-Request")) {
		xmlFreeDoc(document);
		strcpy(errText, "ERROR: Bad document. Node 'sms-Request' not found.");
		return 8000;
	}

	current = root;

	/*** Process multi text ******************/
	{
		xmlNodePtr textList;
		int i = 0;

		*msgParts = 1;
		*msgText = (msgSMS*) gw_malloc(sizeof(msgSMS) * 1);
		textList = current->xmlChildrenNode;
		while (textList != NULL) {
			if (!xmlStrcmp(textList->name, (const xmlChar*) "mensaje")) {
				clave = xmlNodeListGetString(document, textList->xmlChildrenNode, 1);
				/*
				 if (clave == NULL)
				 {
				 xmlFreeDoc (document);
				 strcpy(errText, "ERROR: Bad document.-- Node 'RequestText' not found.");
				 return 8000;
				 }
				 */
				i++;
				xmlFree(clave);
			}
			textList = textList->next;
		}

		*msgParts = i;
		*msgText = (msgSMS*) gw_malloc(sizeof(msgSMS) * (*msgParts));
		i = 0;
		textList = current->xmlChildrenNode;
		while (textList != NULL) {
			if (!xmlStrcmp(textList->name, (const xmlChar*) "mensaje")) {
				clave = xmlNodeListGetString(document, textList->xmlChildrenNode, 1);
				/*
				 if (clave == NULL)
				 {
				 xmlFreeDoc (document);
				 strcpy(errText, "ERROR: Bad document. Node 'RequestText' not found.");
				 return 8000;
				 }
				 */
				sprintf((*msgText)[i], "%s", clave);
				trim((*msgText)[i]);
				i++;
				xmlFree(clave);
			}
			textList = textList->next;
		}
	}
	/*****************************************/

	clave = getNodeValue(document, current->xmlChildrenNode, "celular");
	if (clave == NULL) {
		xmlFreeDoc(document);
		strcpy(errText, "ERROR: Bad document. Node 'celular' not found.");
		return 8000;
	} else {
		sprintf(recipientId, "%s", clave);
		trim(recipientId);
		//getFormatedPhone(procConfig.prefix, recipientId);
	}

	clave = getNodeValue(document, current->xmlChildrenNode, "servicio");
	if (clave == NULL) {
		xmlFreeDoc(document);
		strcpy(errText, "ERROR: Bad document. Node 'servicio' not found.");
		return 8000;
	} else {
		sprintf(service, "%s", clave);
		trim(recipientId);
		//getFormatedPhone(procConfig.prefix, recipientId);
	}

	clave = getNodeValue(document, current->xmlChildrenNode, "id_mensaje");
	if (clave == NULL) {
		xmlFreeDoc(document);
		strcpy(errText, "ERROR: Bad document. Node 'id_mensaje' not found.");
		return 8000;
	} else {
		sprintf(messageId, "%s", clave);
		trim(recipientId);
		//getFormatedPhone(procConfig.prefix, recipientId);
	}

	xmlFree(clave);
	xmlFreeDoc(document);
	strcpy(errText, "");
	return 0;
}

int processXML(TSession *r, char *errText, int readTimeout) {
	int ret = 0;
	int m, i = 0;
	char *content_type, *content_length;
	char Id[30 + 1];
	char Pwd[30 + 1];

	msgSMS *msgText;
	int msgParts;
	char sender[20 + 1];
	char service[100 + 1];
	char messageId[200 + 1];


	urlentry entries[100];
	int input_len = 0;
	char *outBuf = NULL;
	char filebuf[4096];
	char *ptrFilebuf = filebuf;

	int len = 0;
	time_t startTime;

	int bytesReaded;
	SMSmsg rowMO;
	initRowMO(&rowMO);

	content_type = RequestHeaderValue(r, "content-type");

	if (r->method != m_post) {
		strcpy(errText, "Invalid method. It must be 'post'.");
		return 405;
	}

	content_type = RequestHeaderValue(r, "content-type");
	if (content_type == NULL) {
		debug("httpserver", 0, "ERROR: Undefined content type. It must be 'text/xml'.");
		strcpy(errText, "Undefined content type. It must be 'text/xml'.");
		return 400;
	};
	if (strcmp(content_type, "text/xml") != 0) {
		debug("httpserver", 0, "WARNING: Invalid content type (%s). It must be 'text/xml'.", content_type);
		strcpy(content_type, "text/xml");
		return 400;
	};

	decodeUrl(entries, r->query, &m);

	for (i = 0; i <= m; i++) {
		if (strcasecmp(entries[i].name, "id") == 0) {
			strcpy(Id, entries[i].val);
		}
		if (strcasecmp(entries[i].name, "pwd") == 0) {
			strcpy(Pwd, entries[i].val);
		}
	}

	content_length = RequestHeaderValue(r, "content-length");
	if (content_length == NULL) {
		strcpy(errText, "No 'content_length' header was found.");
		return 411;
	};

	input_len = strtol(content_length, (char **) NULL, 10);
	if (input_len == 0) {
		sprintf(errText, "Invalid 'content_length' value (%s).", content_length);
		return 400;
	};


	debug("httpserver", 0, "Id: %s", Id);
	debug("httpserver", 0, "Pwd: %s", Pwd);
	debug("httpserver", 0, "content_type: %s", content_type);
	debug("httpserver", 0, "content_length: %s", content_length);
	debug("httpserver", 0, "input_len: %d", input_len);

	ConnReadInit(r->conn);
	memset(filebuf, 0, sizeof(filebuf));
	bytesReaded = 0;
	startTime = time(NULL);

	while (bytesReaded <= input_len) {
		char *aux;
		int l_aux;

		if (!ConnRead(r->conn, 0)) {
		};
		aux = &(r->conn->buffer[r->conn->bufferpos]);
		l_aux = r->conn->buffersize - r->conn->bufferpos;
		r->conn->bufferpos += l_aux;
		bytesReaded += l_aux;
		ptrFilebuf = memcpy(ptrFilebuf, aux, l_aux);
		ptrFilebuf += l_aux;
		if (time(NULL) - startTime > readTimeout) {
			debug("httpserver", 0, "WARNING: ------------->> Read Timeout <<-------------");
			break;
		}
	}

	ptrFilebuf = 0;
	len = strlen(filebuf);
	debug("httpserver", 0, "filebuf: %s", filebuf);

	ret = parseXML(filebuf, sender, &service,&messageId, &msgText, &msgParts, errText);
	if (ret != 0) {
		debug("httpserver",0, "ERROR: Failed to parse XML content. %s", errText);
		return ret;
	};

	debug("httpserver",0, "ret parseXML: %d", ret);
	debug("httpserver",0, "sender      : %s", sender);
	debug("httpserver",0, "servicio    : %s", service);
	debug("httpserver",0, "id_mensaje  : %s", messageId);
	for (i = 0; i < msgParts; i++) {
		debug("httpserver",0, "msgText[%d]  : %s", i, msgText[i]);
	}

	strcpy(rowMO.phone, sender);
	strcpy(rowMO.text, msgText[0]);
	strcpy(rowMO.service, service);
	strcpy(rowMO.msgId, messageId);
	/* Parametros para verificaciones posteriores */
	rowMO.errCode = 0;
	strcpy(rowMO.errText, "");

	Service *serviceRoute;
	serviceRoute=serviceCreate();
	if (searchService(serviceRoute, octstr_imm(service),octstr_imm(Id),octstr_imm(Pwd))) {
		//Buscar Servicio
		mysql_update(
				octstr_format(SQL_INSERT_MT, octstr_imm(sender), serviceRoute->shortNumber, "0",octstr_imm( msgText[0]), "0", octstr_imm(messageId), "QUEUED", serviceRoute->errorCode, serviceRoute->errorText, serviceRoute->id, serviceRoute->name, serviceRoute->carrierId,
						serviceRoute->integratorId, serviceRoute->integratorQueueId, serviceRoute->connectionId));
		serviceDestroy(serviceRoute);
	} else {
		debug("smppServer", 0, "No service was found.");
//		mysql_update(
//				octstr_format(SQL_INSERT_MT, msg->sms.receiver, msg->sms.sender, "0", msg->sms.msgdata, "0", msgid, "NOTDISPATCHED", route->errorCode, route->errorText, service->id, service->name,
//						route->id, integratorId, integratorQueueId, service->connectionId));
		mysql_update(octstr_format(SQL_INSERT_MT, octstr_imm(sender), serviceRoute->shortNumber, "0",octstr_imm(msgText[0]), "0", octstr_imm(messageId), "NOTDISPATCHED", serviceRoute->errorCode, serviceRoute->errorText, serviceRoute->id, serviceRoute->name, serviceRoute->carrierId,
								serviceRoute->integratorId, serviceRoute->integratorQueueId, serviceRoute->connectionId));
		rowMO.errCode = 1020;
		strcpy(rowMO.errText, "No service was found.");
		ret=1020;
		strcpy(errText, "No service was found.");
		serviceDestroy(serviceRoute);
	}

	//putRowMO(&rowMO);

	gw_free(msgText);
	return ret;

}

int searchService(Service *serviceRoute,Octstr *service,Octstr *id,Octstr *pwd) {
	Octstr *sql;
	MYSQL_RES *res;
	MYSQL_ROW row;

	int result;

	sql = octstr_format(SQL_SELECT_SERVICE,octstr_get_cstr(service),octstr_get_cstr(id),octstr_get_cstr(pwd));
	res = mysql_select(sql);

	if (res == NULL) {
		debug("SQLBOX", 0, "SQL statement failed: %s", octstr_get_cstr(sql));
	} else {
		if (mysql_num_rows(res) >= 1) {
			row = mysql_fetch_row(res);
			serviceRoute->id = octstr_imm(row[0]);
			serviceRoute->name = octstr_imm(row[1]);
			serviceRoute->shortNumber = octstr_imm(row[2]);
			serviceRoute->connectionId = octstr_imm(row[3]);
			serviceRoute->carrierId = octstr_imm(row[4]);
			serviceRoute->integratorId = octstr_imm(row[5]);
			serviceRoute->integratorQueueId = octstr_imm(row[6]);

			result = 1;
		} else {
			serviceRoute->errorCode = octstr_format("1020");
			serviceRoute->errorText = octstr_format("No service was found. to the message");
			result = 0;
		}
		mysql_free_result(res);
	}
	octstr_destroy(sql);
	return result;

}
