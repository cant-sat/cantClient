#pragma once

#include <libwebsockets.h>

#include <atomic>
#include <string>
#include <thread>

class WebSocket {
public:
  WebSocket(std::string url, uint16_t port = 443);
  ~WebSocket();

public:
  std::atomic<bool> isConnected = false;

private:
  std::atomic<struct lws_context *> context = nullptr;
  std::atomic<struct lws *> wsi = nullptr;

  std::thread thread;

private:
  static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len);
  static void *service(void *user);
};