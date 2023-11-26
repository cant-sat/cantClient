#include "ConfigFile.h"
#include <libwebsockets.h>

static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                    void *user, void *in, size_t len) {
  switch (reason) {
  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    printf("Connected to server\n");

    // Attempt to Authenticate
    char buf[LWS_PRE + 128];

    ConfigFile *config = lws_wsi_user(wsi);
    memcpy(&buf[LWS_PRE], config->token, strlen(config->token));
    lws_write(wsi, (void *)&buf[LWS_PRE], strlen(config->token),
              LWS_WRITE_TEXT);
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE:
    // printf("Received data: %s\n", (char *)in);
    if (!strcmp(in, "authenticated")) {
      printf("Authenticated Successfully!\n");
    } else {
      printf("New Message: %s\n", (char *)in);
    }
    break;

  default:
    break;
  }

  return 0;
}

static struct lws_protocols protocols[] = {{"", callback, 0, 0},
                                           {NULL, NULL, 0, 0}};

int main() {
  int ret = 0;

  // Create Context
  struct lws_context_creation_info contextCreateInfo = {};

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
  ConfigFile config;
  if (loadConfig("config.txt", &config)) {
    printf("Failed load Config File\n");
    ret = 1;
    goto destroy_context;
  }

  struct lws_client_connect_info i = {};

  i.context = context;
  i.address = config.address;
  i.port = config.port;
  i.path = "/";
  i.host = i.address;
  i.origin = i.address;
  i.protocol = "ws";
  i.userdata = &config;

  struct lws *wsi = lws_client_connect_via_info(&i);

  while (1) {
    lws_service(context, 1000);
    // Add your own logic here
  }

close_wsi:
  lws_wsi_close(wsi, 1);
destroy_context:
  lws_context_destroy(context);
quit:
  return ret;
}
