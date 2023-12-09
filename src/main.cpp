#include <WebSocket.hpp>

int main() {
  WebSocket ws("3.68.80.98");

  while (true) {
    printf("%i\n", ws.isConnected);
    sleep(1);
    // TODO
  }

  return 0;
}