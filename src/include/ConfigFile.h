#ifndef __CONFIGFILE_H
#define __CONFIGFILE_H

#include <stdio.h>
#include <string.h>

typedef struct {
  char address[64];
  int port;
  char token[64];
} ConfigFile;

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

int loadConfig(const char *path, ConfigFile *config) {
  FILE *f = fopen(path, "r");

  if (!f) {
    printf("ERROR: Config file was not found!\n");
    return 1;
  }

  char line[32 + 64 + 1];
  while (fgets(line, sizeof(line), f)) {
    char param[33], value[65];
    int n = sscanf(line, "%32[^:]: %64[^\n]", param, value);
    if (n == 2) {
      if (!strcmp(param, "server")) {
        strcpy(config->address, value);
      } else if (!strcmp(param, "port")) {
        sscanf(value, "%i", &config->port);
      } else if (!strcmp(param, "token")) {
        if (strlen(config->token) == 0) {
          if (strlen(value) != 0)
            strcpy(config->token, value);
        }
      } else if (!strcmp(param, "tokenFile")) {
        // Set Token from File
        FILE *tf = fopen(value, "r");
        if (tf) {
          char line[65];
          while (fgets(line, sizeof(line), tf)) {
            char token[65];
            if (sscanf(line, "%64s", token) == 1) {
              if (strlen(token) != 0) {
                strcpy(config->token, token);
                break;
              }
            }
          }

          fclose(tf);
        }
      }
    }
  }

  fclose(f);

  if (strlen(config->address) * config->port * strlen(config->token) == 0) {
    // One of them is 0 (aka unset), stop executing
    return 1;
  }

  // TODO: validate token when loading
  if (!validateToken(config->token)) {
    return 1;
  }

  return 0;
}

#endif