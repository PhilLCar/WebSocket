#ifndef HTTP_H
#define HTTP_H

#define HTTP_GET    0
#define HTTP_HEAD   1
#define HTTP_PUT    2
#define HTTP_POST   3
#define HTTP_DELETE 4

// Information
#define HTTP_CONTINUE         100
#define HTTP_CONTINUE_M       "Continue"
#define HTTP_SWITCH           101
#define HTTP_SWITCH_M         "Switching Protocols"
#define HTTP_CHECKPOINT       103
#define HTTP_CHECKPOINT_M     "Checkpoint"

// Successful
#define HTTP_OK               200
#define HTTP_OK_M             "OK"
#define HTTP_CREATED          201
#define HTTP_CREATED_M        "Created"
#define HTTP_ACCEPTED         202
#define HTTP_ACCEPTED_M       "Accepted"
#define HTTP_NAI              203
#define HTTP_NAI_M            "Non-Authoritative Information"
#define HTTP_NOCONTENT        204
#define HTTP_NOCONTENT_M      "No Content"
#define HTTP_RESET            205
#define HTTP_RESET_M          "Reset Content"
#define HTTP_PARTIAL          206
#define HTTP_PARTICAL_M       "Partial Content"

// Redirection
#define HTTP_MULTIPLE         300
#define HTTP_MULTIPLE_M       "Multiple Choices"
#define HTTP_MOVED            301
#define HTTP_MOVED_M          "Moved Permanently"
#define HTTP_FOUND            302
#define HTTP_FOUND_M          "Found"
#define HTTP_SEEOTHER         303
#define HTTP_SEEOTHER_M       "See Other"
#define HTTP_NOTMODIFIED      304
#define HTTP_NOTMODIFIED_M    "Not Modified"
#define HTTP_SWITCHPROXY      306
#define HTTP_SWITCHPROXY_M    "Switch Proxy"
#define HTTP_REDIRECT         307
#define HTTP_REDIRECT_M       "Temporary Redirect"
#define HTTP_RESUME           308
#define HTTP_RESUME_M         "Resumme Incomplete"

// Client Error
#define HTTP_BADREQUEST       400
#define HTTP_BADREQUEST_M     "Bad Request"
#define HTTP_UNAUTHORIZED     401
#define HTTP_UNAUTHORIZED_M   "Unauthorized"
#define HTTP_PAYMENT          402
#define HTTP_PAYMENT_M        "Payment Required"
#define HTTP_FORBIDDEN        403
#define HTTP_FORBIDDEN_M      "Forbidden"
#define HTTP_NOTFOUND         404
#define HTTP_NOTFOUND_M       "Not Found"
#define HTTP_NOTALLOWED       405
#define HTTP_NOTALLOWED_M     "Method Not Allowed"
#define HTTP_NOTACCEPTABLE    406
#define HTTP_NOTACCEPTABLE_M  "Not Acceptable"
#define HTTP_AUTHENTICATION   407
#define HTTP_AUTHENTICATION_M "Proxy Authentication Required"
#define HTTP_RTIMEOUT         408
#define HTTP_RTIMEOUT_M       "Request Timeout"
#define HTTP_CONFLICT         409
#define HTTP_CONFLICT_M       "Conflict"
#define HTTP_GONE             410
#define HTTP_GONE_M           "Gone"
#define HTTP_LENGTH           411
#define HTTP_LENGTH_M         "Length Required"
#define HTTP_PRECONDITION     412
#define HTTP_PRECONDITION_M   "Precondition Failed"
#define HTTP_TOOLARGE         413
#define HTTP_TOOLARGE_M       "Request Entity Too Large"
#define HTTP_TOOLONG          414
#define HTTP_TOOLONG_M        "Request-URI Too Long"
#define HTTP_UNSUPPORTED      415
#define HTTP_UNSUPPORTED_M    "Unsupported Media Type"
#define HTTP_UNSAT            416
#define HTTP_UNSAT_M          "Request Range Not Satisfiable"
#define HTTP_FAILED           417
#define HTTP_FAILED_M         "Expectation Failed"

// Server Error
#define HTTP_INTERNAL         500
#define HTTP_INTERNAL_M       "Internal Server Error"
#define HTTP_NOTIMPLEMENTED   501
#define HTTP_NOTIMPLEMENTED_M "Not Implemented"
#define HTTP_BADGATEWAY       502
#define HTTP_BADGATEWAY_M     "Found"
#define HTTP_UNAVAILABLE      503
#define HTTP_UNAVAILABLE_M    "Service Unavailable"
#define HTTP_GTIMEOUT         504
#define HTTP_GTIMEOUT_M       "Gateway Timeout"
#define HTTP_VERSION          505
#define HTTP_VERSION_M        "HTTP Version Not Supported"
#define HTTP_NETAUTHREQ       511
#define HTTP_NETAUTHREQ_M     "Network Authentication Required"


typedef struct http_request {
  int   method;
  char  file[512];
  char  version[32];
  char  header[2048];
  char *body;
} HttpRequest;

typedef struct http_response {
  char  version[32];
  int   status;
  char  message[128];
  char  header[2048];
  char *body;
} HttpResponse;

void httpreqstr(HttpRequest*, char*);
void httprespstr(HttpResponse*, char*);
void buildhttpreq(HttpRequest*, char*, char*);
void buildhttpresp(HttpResponse*, char*, char*);
void freehttpreq(HttpRequest*);
void freehttpresp(HttpResponse*);
void httpreqfromstr(HttpRequest*, char*);
void httprespfromstr(HttpResponse*, char*);
void getfield(char*, char*, char*);

#endif