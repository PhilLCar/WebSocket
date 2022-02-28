#include <http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void httpreqstr(HttpRequest *request, char *buffer) {
  char *method;
  switch(request->method) {
    case HTTP_GET:
      method = "GET ";
      break;
    case HTTP_HEAD:
      method = "HEAD ";
      break;
    case HTTP_PUT:
      method = "PUT ";
      break;
    case HTTP_POST:
      method = "POST ";
      break;
    case HTTP_DELETE:
      method = "DELETE ";
      break;
  }
  sprintf(buffer, "%s %s %s\r\n%s\r\n%s", method, 
                                          request->file, 
                                          request->version,
                                          request->header,
                                          request->body);
}

void httprespstr(HttpResponse *response, char *buffer) {
  sprintf(buffer, "%s %d %s\r\n%s\r\n%s", response->version,
                                          response->status, 
                                          response->message,
                                          response->header,
                                          response->body);
}

void buildhttpreq(HttpRequest *request, char *field, char *value) {
  int i;
  for (i = 0; request->header[i]; i++);
  sprintf(&request->header[i], "%s: %s\r\n", field, value);
}

void buildhttpresp(HttpResponse *response, char *field, char *value) {
  int i;
  for (i = 0; response->header[i]; i++);
  sprintf(&response->header[i], "%s: %s\r\n", field, value);
}

void freehttpreq(HttpRequest *request) {
  free(request->body);
  free(request);
}

void freehttpresp(HttpResponse *response) {
  free(response->body);
  free(response);
}

void httpreqfromstr(HttpRequest *request, char *str) {
  int i = 0;
  char buffer[32];
  memset(buffer, 0, 32 * sizeof(char));
  for (; str[i] != ' '; i++) {
    buffer[i] = str[i];
  }
  if      (!strcmp(buffer, "GET"))    request->method = HTTP_GET;
  else if (!strcmp(buffer, "HEAD"))   request->method = HTTP_HEAD;
  else if (!strcmp(buffer, "PUT"))    request->method = HTTP_PUT;
  else if (!strcmp(buffer, "POST"))   request->method = HTTP_POST;
  else if (!strcmp(buffer, "DELETE")) request->method = HTTP_DELETE;
  memset(request->file, 0, 512 * sizeof(char));
  ++i;
  for (int j = 0; str[i] != ' '; i++, j++) {
    request->file[j] = str[i];
  }
  memset(request->version, 0, 32 * sizeof(char));
  ++i;
  for (int j = 0; str[i] != '\n'; i++, j++) {
    if (str[i] != '\r') request->version[j] = str[i];
    else j--;
  }
  memset(request->header, 0, 2048 * sizeof(char));
  ++i;
  for (int j = 0, l = 0; str[i] != '\n' || !l; i++, j++) {
    if (str[i] != '\r') request->header[j] = str[i];
    else j--;
    if (str[i] == '\n')      l = 1;
    else if (str[i] != '\r') l = 0;
  }
  ++i;
  if (request->body) free(request->body);
  request->body = malloc((strlen(&str[i]) + 1) * sizeof(char));
  sprintf(request->body, "%s", &str[i]);
}

void httprespfromstr(HttpResponse *response, char *str) {
  int i = 0;
  char buffer[32];
  memset(response->version, 0, 32 * sizeof(char));
  for (; str[i] != ' '; i++) {
    response->version[i] = str[i];
  }
  ++i;
  memset(buffer, 0, 32 * sizeof(char));
  for (int j = 0; str[i] != ' '; i++, j++) {
    buffer[j] = str[i];
  }
  ++i;
  response->status = atoi(buffer);
  memset(response->message, 0, 128 * sizeof(char));
  for (int j = 0; str[i] != '\n'; i++, j++) {
    if (str[i] != '\r') response->message[j] = str[i];
    else j--;
  }
  ++i;
  memset(response->header, 0, 2048 & sizeof(char));
  for (int j = 0, l = 0; str[i] != '\n' || !l; i++, j++) {
    if (str[i] != '\r') response->header[j] = str[i];
    else j--;
    if (str[i] == '\n')      l = 1;
    else if (str[i] != '\r') l = 0;
  }
  ++i;
  if (response->body) free(response->body);
  response->body = malloc((strlen(&str[i]) + 1) * sizeof(char));
  sprintf(response->body, "%s", &str[i]);
}

void getfield(char *text, char *field, char *buffer) {
  for (int i = 0; text[i]; i++) {
    for (int j = 0;; j++) {
      if (!field[j] && text[i + j] == ':' && text[i + j + 1] == ' ') {
        int l = 0;
        for (int k = i + j + 2; text[k] != '\n' && text[k] != '\r'; k++) buffer[l++] = text[k];
        buffer[l] = 0;
        return;
      }
      if (text[i + j] != field[j]) break;
    }
  }
}