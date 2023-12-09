#include <WebSocket.hpp>

#include <stdexcept>

WebSocket::WebSocket(std::string url, uint16_t port) {

  // Create Context
  struct lws_context_creation_info contextCreateInfo = {};

  struct lws_protocols protocols[] = {{"", callback, 0, 0, 0, this},
                                      {NULL, NULL, 0, 0}};

  contextCreateInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  contextCreateInfo.port = CONTEXT_PORT_NO_LISTEN; // we don't run the server
  contextCreateInfo.protocols = protocols;
  contextCreateInfo.fd_limit_per_thread = 1 + 1 + 1;

  struct lws_context *context = lws_create_context(&contextCreateInfo);
  if (!context)
    throw std::runtime_error("Failed to init WebSocket");

  // Load Config Fil

  struct lws_client_connect_info i = {};

  i.context = context;
  i.address = url.c_str();
  i.port = port;
  i.path = "/";
  i.host = i.address;
  i.origin = i.address;
  i.protocol = "ws";

  // no need for ssl in case of localhost
  // if (strcmp(i.address, "localhost"))
  i.ssl_connection = LCCSCF_USE_SSL;

  char token[10] = {};
  struct lws *wsi = lws_client_connect_via_info(&i);

  // Start Service Thread
  thread = std::thread(service, this);
}

WebSocket::~WebSocket() {
  thread.join();

  if (wsi)
    lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE, 1);

  if (context)
    lws_context_destroy(context);
}

int WebSocket::callback(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len) {
  WebSocket *ws = (WebSocket *)user;

  switch (reason) {
  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    ws->isConnected = true;
    printf("asd\n");

    // // Attempt to Authenticate
    // const char *token = session->config.token;
    // size_t length = strlen(token);

    // char buf[LWS_PRE + 128];
    // memcpy(&buf[LWS_PRE], token, length);
    // lws_write(wsi, (void *)&buf[LWS_PRE], length, LWS_WRITE_TEXT);
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE:
    // if (!strcmp(in, "authenticated"))
    //   session->isAuthenticated = 1;
    break;

  case LWS_CALLBACK_CLIENT_CLOSED:
    ws->isConnected = false;
    break;

  default:
    break;
  }

  return 0;
}

void *WebSocket::service(void *user) {
  WebSocket *ws = (WebSocket *)user;
  // TODO

  while (true) {
    lws_service(ws->context, 1000);
  }

  pthread_exit(NULL);
  return NULL;
}