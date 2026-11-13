/*
  RouteWise C backend
  - Stores locations and roads in CSV files.
  - Represents the map as a weighted, undirected graph.
  - Implements BFS and Dijkstra (using a min-heap).
  - Serves the HTML/CSS/JS frontend through Mongoose.

  Location names: use letters, numbers, spaces, and underscores only.
  This keeps the simple CSV storage format safe and easy to understand.
*/

#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LOCATIONS 60
#define MAX_ROADS 200
#define NAME_LEN 64
#define INF 1000000000

typedef struct {
  char name[NAME_LEN];
  int x, y;
} Location;

typedef struct {
  int a, b;
  int distance;
  int blocked;
} Road;

typedef struct {
  int to;
  int distance;
} Edge;

typedef struct {
  int vertex;
  int distance;
} HeapItem;

static Location locations[MAX_LOCATIONS];
static Road roads[MAX_ROADS];
static int location_count = 0, road_count = 0;

/* ---------- Small utility functions ---------- */

static int safe_name(const char *s) {
  int i;
  if (s == NULL || s[0] == '\0' || strlen(s) >= NAME_LEN) return 0;
  for (i = 0; s[i]; i++) {
    if (!(isalnum((unsigned char)s[i]) || s[i] == ' ' || s[i] == '_'))
      return 0;
  }
  return 1;
}

static int find_location(const char *name) {
  int i;
  for (i = 0; i < location_count; i++)
    if (strcmp(locations[i].name, name) == 0) return i;
  return -1;
}

static void save_locations(void) {
  FILE *f = fopen("locations.csv", "w");
  int i;
  if (!f) return;
  for (i = 0; i < location_count; i++)
    fprintf(f, "%s,%d,%d\n", locations[i].name, locations[i].x, locations[i].y);
  fclose(f);
}

static void save_roads(void) {
  FILE *f = fopen("roads.csv", "w");
  int i;
  if (!f) return;
  for (i = 0; i < road_count; i++) {
    fprintf(f, "%s,%s,%d,%d\n",
      locations[roads[i].a].name, locations[roads[i].b].name,
      roads[i].distance, roads[i].blocked);
  }
  fclose(f);
}

static void load_data(void) {
  FILE *f;
  char line[256], name[NAME_LEN], other[NAME_LEN];
  int x, y, d, blocked;

  location_count = road_count = 0;

  f = fopen("locations.csv", "r");
  if (f) {
    while (fgets(line, sizeof(line), f) && location_count < MAX_LOCATIONS) {
      if (sscanf(line, "%63[^,],%d,%d", name, &x, &y) == 3) {
        if (safe_name(name)) {
          strcpy(locations[location_count].name, name);
          locations[location_count].x = x;
          locations[location_count].y = y;
          location_count++;
        }
      }
    }
    fclose(f);
  }

  f = fopen("roads.csv", "r");
  if (f) {
    while (fgets(line, sizeof(line), f) && road_count < MAX_ROADS) {
      if (sscanf(line, "%63[^,],%63[^,],%d,%d", name, other, &d, &blocked) == 4) {
        int a = find_location(name), b = find_location(other);
        if (a >= 0 && b >= 0 && a != b && d > 0) {
          roads[road_count].a = a;
          roads[road_count].b = b;
          roads[road_count].distance = d;
          roads[road_count].blocked = blocked ? 1 : 0;
          road_count++;
        }
      }
    }
    fclose(f);
  }
}

static void json_error(struct mg_connection *c, const char *message) {
  mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                "{\"ok\":false,\"error\":\"%s\"}\n", message);
}

static void get_param(struct mg_http_message *hm, const char *key,
                      char *out, size_t size) {
  if (mg_http_get_var(&hm->query, key, out, size) < 0) out[0] = '\0';
}

/* ---------- Graph and data API ---------- */

static void api_data(struct mg_connection *c) {
  char json[24000];
  size_t used = 0;
  int i;

  used += (size_t) snprintf(json + used, sizeof(json) - used,
                            "{\"locations\":[");
  for (i = 0; i < location_count; i++) {
    used += (size_t) snprintf(json + used, sizeof(json) - used,
      "%s{\"name\":\"%s\",\"x\":%d,\"y\":%d}",
      i ? "," : "", locations[i].name, locations[i].x, locations[i].y);
  }
  used += (size_t) snprintf(json + used, sizeof(json) - used, "],\"roads\":[");
  for (i = 0; i < road_count; i++) {
    used += (size_t) snprintf(json + used, sizeof(json) - used,
      "%s{\"from\":\"%s\",\"to\":\"%s\",\"distance\":%d,\"blocked\":%s}",
      i ? "," : "", locations[roads[i].a].name, locations[roads[i].b].name,
      roads[i].distance, roads[i].blocked ? "true" : "false");
  }
  snprintf(json + used, sizeof(json) - used, "]}");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", json);
}

static void api_add_location(struct mg_connection *c, struct mg_http_message *hm) {
  char name[NAME_LEN], sx[24], sy[24];
  int x, y;

  get_param(hm, "name", name, sizeof(name));
  get_param(hm, "x", sx, sizeof(sx));
  get_param(hm, "y", sy, sizeof(sy));

  if (!safe_name(name)) return json_error(c, "Use a short name with letters, numbers, spaces or underscores.");
  if (find_location(name) >= 0) return json_error(c, "That location already exists.");
  if (location_count >= MAX_LOCATIONS) return json_error(c, "Location limit reached.");

  x = atoi(sx); y = atoi(sy);
  if (x < 20 || x > 580 || y < 20 || y > 320)
    return json_error(c, "Map coordinates must be within x: 20-580 and y: 20-320.");

  strcpy(locations[location_count].name, name);
  locations[location_count].x = x;
  locations[location_count].y = y;
  location_count++;
  save_locations();
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"ok\":true}\n");
}

static void api_add_road(struct mg_connection *c, struct mg_http_message *hm) {
  char from[NAME_LEN], to[NAME_LEN], sd[24];
  int a, b, d, i;

  get_param(hm, "from", from, sizeof(from));
  get_param(hm, "to", to, sizeof(to));
  get_param(hm, "distance", sd, sizeof(sd));
  a = find_location(from); b = find_location(to); d = atoi(sd);

  if (a < 0 || b < 0) return json_error(c, "Choose two existing locations.");
  if (a == b) return json_error(c, "A road must connect two different locations.");
  if (d <= 0 || d > 100000) return json_error(c, "Distance must be a positive number.");
  if (road_count >= MAX_ROADS) return json_error(c, "Road limit reached.");

  for (i = 0; i < road_count; i++) {
    if ((roads[i].a == a && roads[i].b == b) ||
        (roads[i].a == b && roads[i].b == a))
      return json_error(c, "A road already connects these locations.");
  }

  roads[road_count].a = a;
  roads[road_count].b = b;
  roads[road_count].distance = d;
  roads[road_count].blocked = 0;
  road_count++;
  save_roads();
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"ok\":true}\n");
}

static void api_toggle_road(struct mg_connection *c, struct mg_http_message *hm) {
  char from[NAME_LEN], to[NAME_LEN], state[16];
  int a, b, i, blocked;

  get_param(hm, "from", from, sizeof(from));
  get_param(hm, "to", to, sizeof(to));
  get_param(hm, "blocked", state, sizeof(state));
  a = find_location(from); b = find_location(to);
  blocked = atoi(state) ? 1 : 0;

  for (i = 0; i < road_count; i++) {
    if ((roads[i].a == a && roads[i].b == b) ||
        (roads[i].a == b && roads[i].b == a)) {
      roads[i].blocked = blocked;
      save_roads();
      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"ok\":true}\n");
      return;
    }
  }
  json_error(c, "Road not found.");
}

/* ---------- Queue for BFS ---------- */

typedef struct {
  int items[MAX_LOCATIONS];
  int front, rear;
} Queue;

static void enqueue(Queue *q, int value) { q->items[q->rear++] = value; }
static int dequeue(Queue *q) { return q->items[q->front++]; }
static int queue_empty(Queue *q) { return q->front == q->rear; }

/* ---------- Min-heap for Dijkstra ---------- */

typedef struct {
  HeapItem items[10000];
  int size;
} MinHeap;

static void heap_push(MinHeap *h, int vertex, int distance) {
  int i = ++h->size;
  while (i > 1 && h->items[i / 2].distance > distance) {
    h->items[i] = h->items[i / 2];
    i /= 2;
  }
  h->items[i].vertex = vertex;
  h->items[i].distance = distance;
}

static HeapItem heap_pop(MinHeap *h) {
  HeapItem result = h->items[1], last = h->items[h->size--];
  int i = 1, child;
  while (i * 2 <= h->size) {
    child = i * 2;
    if (child + 1 <= h->size &&
        h->items[child + 1].distance < h->items[child].distance) child++;
    if (last.distance <= h->items[child].distance) break;
    h->items[i] = h->items[child];
    i = child;
  }
  if (h->size > 0) h->items[i] = last;
  return result;
}

/* Build an adjacency list from the saved road list. Blocked roads are skipped. */
static int build_graph(Edge graph[MAX_LOCATIONS][MAX_ROADS], int counts[MAX_LOCATIONS]) {
  int i;
  for (i = 0; i < location_count; i++) counts[i] = 0;
  for (i = 0; i < road_count; i++) {
    int a = roads[i].a, b = roads[i].b;
    if (!roads[i].blocked) {
      graph[a][counts[a]].to = b;
      graph[a][counts[a]++].distance = roads[i].distance;
      graph[b][counts[b]].to = a;
      graph[b][counts[b]++].distance = roads[i].distance;
    }
  }
  return 1;
}

static void save_history(const char *source, const char *destination,
                         const char *algorithm, int distance, const char *path) {
  FILE *f = fopen("history.csv", "a");
  if (!f) return;
  fprintf(f, "%s,%s,%s,%d,%s\n", source, destination, algorithm, distance, path);
  fclose(f);
}

static void api_route(struct mg_connection *c, struct mg_http_message *hm) {
  char source[NAME_LEN], destination[NAME_LEN], algorithm[24];
  int start, goal, i, found = 0, total = 0;
  int dist[MAX_LOCATIONS], previous[MAX_LOCATIONS], visited[MAX_LOCATIONS] = {0};
  int path[MAX_LOCATIONS], path_count = 0;
  Edge graph[MAX_LOCATIONS][MAX_ROADS];
  int counts[MAX_LOCATIONS];
  char path_text[5000] = "", json[8000];
  size_t used = 0;

  get_param(hm, "source", source, sizeof(source));
  get_param(hm, "destination", destination, sizeof(destination));
  get_param(hm, "algorithm", algorithm, sizeof(algorithm));
  start = find_location(source);
  goal = find_location(destination);

  if (start < 0 || goal < 0) return json_error(c, "Choose valid starting and destination locations.");
  if (start == goal) return json_error(c, "Choose two different locations.");
  if (strcmp(algorithm, "bfs") != 0 && strcmp(algorithm, "dijkstra") != 0)
    return json_error(c, "Algorithm must be bfs or dijkstra.");

  build_graph(graph, counts);
  for (i = 0; i < location_count; i++) {
    dist[i] = INF; previous[i] = -1;
  }
  dist[start] = 0;

  if (strcmp(algorithm, "bfs") == 0) {
    Queue q = {{0}, 0, 0};
    enqueue(&q, start);
    visited[start] = 1;
    while (!queue_empty(&q)) {
      int u = dequeue(&q);
      if (u == goal) break;
      for (i = 0; i < counts[u]; i++) {
        int v = graph[u][i].to;
        if (!visited[v]) {
          visited[v] = 1;
          previous[v] = u;
          dist[v] = dist[u] + 1; /* BFS counts number of roads, not kilometres. */
          enqueue(&q, v);
        }
      }
    }
    found = visited[goal];
    total = dist[goal];
  } else {
    MinHeap heap = {{{0, 0}}, 0};
    heap_push(&heap, start, 0);
    while (heap.size > 0) {
      HeapItem item = heap_pop(&heap);
      int u = item.vertex;
      if (visited[u]) continue;
      visited[u] = 1;
      if (u == goal) break;
      for (i = 0; i < counts[u]; i++) {
        int v = graph[u][i].to, nd = dist[u] + graph[u][i].distance;
        if (!visited[v] && nd < dist[v]) {
          dist[v] = nd;
          previous[v] = u;
          heap_push(&heap, v, nd);
        }
      }
    }
    found = visited[goal];
    total = dist[goal];
  }

  if (!found) {
    mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                  "{\"ok\":true,\"found\":false,\"message\":\"No route available. Check blocked roads.\"}\n");
    return;
  }

  /* Reconstruct the route by walking backwards through previous[]. */
  for (i = goal; i != -1 && path_count < MAX_LOCATIONS; i = previous[i])
    path[path_count++] = i;

  for (i = path_count - 1; i >= 0; i--) {
    if (path_text[0]) strncat(path_text, " -> ", sizeof(path_text) - strlen(path_text) - 1);
    strncat(path_text, locations[path[i]].name,
            sizeof(path_text) - strlen(path_text) - 1);
  }

  save_history(source, destination, algorithm, total, path_text);

  used += (size_t) snprintf(json + used, sizeof(json) - used,
    "{\"ok\":true,\"found\":true,\"algorithm\":\"%s\",\"distance\":%d,\"path\":[",
    algorithm, total);
  for (i = path_count - 1; i >= 0; i--) {
    used += (size_t) snprintf(json + used, sizeof(json) - used,
      "%s\"%s\"", i == path_count - 1 ? "" : ",", locations[path[i]].name);
  }
  snprintf(json + used, sizeof(json) - used, "]}");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", json);
}

static void api_history(struct mg_connection *c) {
  FILE *f = fopen("history.csv", "r");
  char line[1024], json[16000];
  size_t used = 0;
  int first = 1;
  if (!f) {
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "[]\n");
    return;
  }
  fgets(line, sizeof(line), f); /* Skip CSV heading. */
  used += (size_t) snprintf(json + used, sizeof(json) - used, "[");
  while (fgets(line, sizeof(line), f)) {
    char src[NAME_LEN], dst[NAME_LEN], alg[24], path[500];
    int distance;
    if (sscanf(line, "%63[^,],%63[^,],%23[^,],%d,%499[^\n]",
               src, dst, alg, &distance, path) == 5) {
      used += (size_t) snprintf(json + used, sizeof(json) - used,
        "%s{\"source\":\"%s\",\"destination\":\"%s\",\"algorithm\":\"%s\",\"distance\":%d,\"path\":\"%s\"}",
        first ? "" : ",", src, dst, alg, distance, path);
      first = 0;
    }
  }
  fclose(f);
  snprintf(json + used, sizeof(json) - used, "]");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", json);
}

static void api_clear_history(struct mg_connection *c) {
  FILE *f = fopen("history.csv", "w");
  if (!f) return json_error(c, "Could not clear history file.");
  fprintf(f, "source,destination,algorithm,distance,path\n");
  fclose(f);
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"ok\":true}\n");
}

/* ---------- HTTP request handler ---------- */

static void handle_request(struct mg_connection *c, int ev, void *ev_data) {
  struct mg_http_message *hm = (struct mg_http_message *) ev_data;
  struct mg_http_serve_opts opts = {.root_dir = "."};

  if (ev != MG_EV_HTTP_MSG) return;

  if (mg_strcmp(hm->uri, mg_str("/api/data")) == 0) {
    api_data(c);
  } else if (mg_strcmp(hm->uri, mg_str("/api/add-location")) == 0) {
    api_add_location(c, hm);
  } else if (mg_strcmp(hm->uri, mg_str("/api/add-road")) == 0) {
    api_add_road(c, hm);
  } else if (mg_strcmp(hm->uri, mg_str("/api/toggle-road")) == 0) {
    api_toggle_road(c, hm);
  } else if (mg_strcmp(hm->uri, mg_str("/api/route")) == 0) {
    api_route(c, hm);
  } else if (mg_strcmp(hm->uri, mg_str("/api/history")) == 0) {
    api_history(c);
  } else if (mg_strcmp(hm->uri, mg_str("/api/clear-history")) == 0) {
    api_clear_history(c);
  } else {
    mg_http_serve_dir(c, hm, &opts);
  }
}

int main(void) {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  load_data();

  if (mg_http_listen(&mgr, "http://0.0.0.0:8000", handle_request, NULL) == NULL) {
    fprintf(stderr, "Could not start RouteWise server on port 8000.\n");
    mg_mgr_free(&mgr);
    return 1;
  }

  printf("RouteWise is running at http://localhost:8000\n");
  printf("Keep this terminal open while using the application.\n");

  for (;;) mg_mgr_poll(&mgr, 1000);

  mg_mgr_free(&mgr);
  return 0;
}
