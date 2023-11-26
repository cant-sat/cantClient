#include "ConfigFile.h"
#include <libwebsockets.h>
#include <math.h>

typedef struct {
  int isConnected;
  int isAuthenticated;

  ConfigFile config;
} Session;

static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                    void *user, void *in, size_t len) {
  Session *session = lws_wsi_user(wsi);

  switch (reason) {
  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    printf("Connected to server\n");
    session->isConnected = 1;

    // Attempt to Authenticate
    const char *token = session->config.token;
    size_t length = strlen(token);

    char buf[LWS_PRE + 128];
    memcpy(&buf[LWS_PRE], token, length);
    lws_write(wsi, (void *)&buf[LWS_PRE], length, LWS_WRITE_TEXT);
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE:
    // printf("Received data: %s\n", (char *)in);
    if (!strcmp(in, "authenticated")) {
      printf("Authenticated Successfully!\n");
      session->isAuthenticated = 1;
    } else {
      printf("New Message: %s\n", (char *)in);
    }
    break;

  case LWS_CALLBACK_CLIENT_CLOSED:
    printf("Disconnected from server\n");
    session->isConnected = 0;
    session->isAuthenticated = 0;
    break;

  default:
    break;
  }

  return 0;
}

int main() {
  int ret = 0;

  // Create Context
  struct lws_context_creation_info contextCreateInfo = {};

  struct lws_protocols protocols[] = {{"", callback, 0, 0}, {NULL, NULL, 0, 0}};

  contextCreateInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  contextCreateInfo.port = CONTEXT_PORT_NO_LISTEN; // we don't run the server
  contextCreateInfo.protocols = protocols;
  contextCreateInfo.fd_limit_per_thread = 1 + 1 + 1;

  struct lws_context *context = lws_create_context(&contextCreateInfo);
  if (!context) {
    printf("Failed to init WebSocket\n");
    ret = 1;
    goto quit;
  }

  // Load Config File
  Session session;
  if (loadConfig("config.txt", &session.config)) {
    printf("Failed load Config File\n");
    ret = 1;
    goto destroy_context;
  }

  struct lws_client_connect_info i = {};

  i.context = context;
  i.address = session.config.address;
  i.port = session.config.port;
  i.path = "/";
  i.host = i.address;
  i.origin = i.address;
  i.protocol = "ws";
  i.userdata = &session;

  i.ssl_connection = LCCSCF_USE_SSL;

  struct lws *wsi = lws_client_connect_via_info(&i);

  while (1) {
    lws_service(context, 1000);
    // Add your own logic here

    if (session.isAuthenticated) {
      char msg[128];
      struct timeval tv;

      struct timespec res = {};
      clock_gettime(CLOCK_MONOTONIC, &res);
      float s =
          (float)(1000.0f * (float)res.tv_sec + (float)res.tv_nsec / 1e6) /
          1000.0f;
      sprintf(msg, "{\"table\":\"%s\", \"value\":%i}", "temperature",
              (int)(sinf(s) * 100.0));

      char buf[LWS_PRE + 128];
      memcpy(&buf[LWS_PRE], msg, strlen(msg));
      lws_write(wsi, (void *)&buf[LWS_PRE], strlen(msg), LWS_WRITE_TEXT);
    }
  }

close_wsi:
  lws_wsi_close(wsi, 1);
destroy_context:
  lws_context_destroy(context);
quit:
  return ret;
}
