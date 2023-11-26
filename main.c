#include <libwebsockets.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

static struct my_conn {
  lws_sorted_usec_list_t sul; /* schedule connection retry */
  struct lws *wsi;            /* related wsi if any */
  uint16_t retry_count;       /* count of consequetive retries */
} mco;

// Config Info
static struct {
  char server[64];
  int port;
  char token[64];
} config = {};

static struct lws_context *context;
static int interrupted;
/*
 * The retry and backoff policy we want to use for our client connections
 */
static const uint32_t backoff_ms[] = {1000, 2000, 3000, 4000, 5000};
static const lws_retry_bo_t retry = {
    .retry_ms_table = backoff_ms,
    .retry_ms_table_count = LWS_ARRAY_SIZE(backoff_ms),
    .conceal_count = LWS_ARRAY_SIZE(backoff_ms),
    .secs_since_valid_ping = 3,    /* force PINGs after secs idle */
    .secs_since_valid_hangup = 10, /* hangup after secs idle */
    .jitter_percent = 20,
};
/*
 * Scheduled sul callback that starts the connection attempt
 */
static void connect_client(lws_sorted_usec_list_t *sul) {
  struct my_conn *mco = lws_container_of(sul, struct my_conn, sul);
  struct lws_client_connect_info i;
  memset(&i, 0, sizeof(i));
  i.context = context;
  i.port = config.port;
  i.address = config.server;
  i.path = "/";
  i.host = i.address;
  i.origin = i.address;
  i.ssl_connection = LCCSCF_ALLOW_INSECURE;
  i.protocol = "ws";
  i.local_protocol_name = "lws-minimal-client";
  i.pwsi = &mco->wsi;
  i.retry_and_idle_policy = &retry;
  i.userdata = mco;
  if (!lws_client_connect_via_info(&i))
    /*
     * Failed... schedule a retry... we can't use the _retry_wsi()
     * convenience wrapper api here because no valid wsi at this
     * point.
     */
    if (lws_retry_sul_schedule(context, 0, sul, &retry, connect_client,
                               &mco->retry_count)) {
      lwsl_err("%s: connection attempts exhausted\n", __func__);
      interrupted = 1;
    }
}

static int callback_minimal(struct lws *wsi, enum lws_callback_reasons reason,
                            void *user, void *in, size_t len) {
  struct my_conn *mco = (struct my_conn *)user;
  switch (reason) {
  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
    goto do_retry;
    break;
  case LWS_CALLBACK_CLIENT_RECEIVE:
    lwsl_hexdump_notice(in, len);
    break;
  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    lwsl_user("%s: established\n", __func__);
    char buf[LWS_PRE + 128];

    const char *token = config.token;
    memcpy(&buf[LWS_PRE], token, strlen(token));
    lws_write(wsi, (void *)&buf[LWS_PRE], strlen(token), LWS_WRITE_TEXT);

    const char *testmsg = "{\"table\":\"asd\", \"value\":\"asd\"} ";
    memcpy(&buf[LWS_PRE], testmsg, strlen(testmsg));
    lws_write(wsi, (void *)&buf[LWS_PRE], strlen(testmsg), LWS_WRITE_TEXT);

    break;
  case LWS_CALLBACK_CLIENT_CLOSED:
    goto do_retry;
  default:
    break;
  }
  return lws_callback_http_dummy(wsi, reason, user, in, len);
do_retry:
  /*
   * retry the connection to keep it nailed up
   *
   * For this example, we try to conceal any problem for one set of
   * backoff retries and then exit the app.
   *
   * If you set retry.conceal_count to be larger than the number of
   * elements in the backoff table, it will never give up and keep
   * retrying at the last backoff delay plus the random jitter amount.
   */
  if (lws_retry_sul_schedule_retry_wsi(wsi, &mco->sul, connect_client,
                                       &mco->retry_count)) {
    lwsl_err("%s: connection attempts exhausted\n", __func__);
    interrupted = 1;
  }
  return 0;
}

static void sigint_handler(int sig) { interrupted = 1; }

int validateToken(const char *token) {
  static const char validChars[] =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  while (*token != '\0') {
    int valid = 0;
    for (int i = 0; i < sizeof(validChars); i++)
      if (*token == validChars[i])
        valid = 1;
    if (!valid)
      return 0;
    token++;
  }
  return 1;
}

int main(int argc, const char **argv) {

  /* Read Config File */ {
    printf("-------------------------------- Loading Config File\n");

    FILE *f = fopen("../config.txt", "r");

    if (!f) {
      printf("ERROR: Config file was not found!\n");
      return 1;
    }

    char line[32 + 64];
    while (fgets(line, sizeof(line), f)) {
      char param[32], value[64];
      int n = sscanf(line, "%31[^:]: %63[^\n]", param, value);
      if (n == 2) {
        if (!strcmp(param, "server")) {
          // Set Server Address
          strcpy(config.server, value);
          printf("- server address is set to `%s`\n", config.server);
        } else if (!strcmp(param, "port")) {
          // Set Server Port
          sscanf(value, "%i", &config.port);
          printf("- server port is set to `%i`\n", config.port);
        } else if (!strcmp(param, "token")) {
          // Set Token
          if (strlen(config.token) == 0) {
            if (strlen(value) != 0)
              strcpy(config.token, value);
          } else {
            printf("WARNING: Attempted to set token twice! (original:`%s`, "
                   "attempted:`%s`)\n",
                   config.token, value);
          }
        } else if (!strcmp(param, "tokenFile")) {
          // Set Token from File
          char tpath[32];
          sprintf(tpath, "../%s", value);

          FILE *tf = fopen(tpath, "r");
          if (tf) {
            char line[64];
            while (fgets(line, sizeof(line), tf)) {
              char token[64];
              if (sscanf(line, "%64s", token) == 1) {
                if (strlen(token) != 0) {
                  if (strlen(config.token) != 0)
                    printf("WARNING: Modifying Token `%s` to `%s`\n",
                           config.token, token);
                  else
                    printf("- token is set to '%s'\n", token);

                  strcpy(config.token, token);
                  break;
                }
              }
            }
          } else {
            printf("WARNING: Failed to open Token File `%s`\n", tpath);
          }

          fclose(tf);
        }
      }
    }

    fclose(f);

    if (strlen(config.server) * config.port * strlen(config.token) == 0) {
      // One of them is 0 (aka unset), stop executing
      printf("ERROR: Incomplete Config File\n");
      return 1;
    }

    if (!validateToken(config.token)) {
      printf("ERROR: Invalid Token!\n");
      return 1;
    }

    printf("-------------------------------- Load Complete\n\n");
  }
  /* Init Websocket */

  static const struct lws_protocols protocols[] = {{
                                                       "dataStream",
                                                       callback_minimal,
                                                       0,
                                                       0,
                                                   },
                                                   {}};

  // Init Websocket
  struct lws_context_creation_info info = {};
  const char *p;
  int n = 0;
  signal(SIGINT, sigint_handler);
  lws_cmdline_option_handle_builtin(argc, argv, &info);
  lwsl_user("LWS minimal ws client\n");
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
  info.protocols = protocols;
  //   info.user = Cant;

  info.fd_limit_per_thread = 1 + 1 + 1;
  context = lws_create_context(&info);
  if (!context) {
    lwsl_err("lws init failed\n");
    return 1;
  }
  /* schedule the first client connection attempt to happen immediately */
  lws_sul_schedule(context, 0, &mco.sul, connect_client, 1);
  while (n >= 0 && !interrupted)
    n = lws_service(context, 0);
  lws_context_destroy(context);
  lwsl_user("Completed\n");
  return 0;
}