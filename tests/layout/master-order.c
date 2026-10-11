#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#define MAX_CLIENTS 99
#define NUM_WORKSPACES 9
#define MIN(a, b) ((a) < (b) ? (a) : (b))
typedef unsigned long Window;
typedef struct Client Client;
struct Client {
	Client *next;
	int mapped, floating, fullscreen, mon;
	Window win;
};
static Client *workspaces[NUM_WORKSPACES];
static Client *focused;
static int current_ws, current_mon;
static int master_count = 2;
static struct { int warp_cursor; } user_config;
static int client_is_visible_on_monitor(const Client *c, int mon) { return c->mon == mon; }
static int workspace_nmaster_for(int ws) { (void)ws; return master_count; }
static void tile(void) {}
static void warp_cursor(Client *c) { (void)c; }
static void send_wm_take_focus(Window w) { (void)w; }
static void update_borders(void) {}
void move_master_next(void);
void move_master_prev(void);
#include "../../src/master.c"

static void check_order(Client *clients, int count, int swapped)
{
	Client *c = workspaces[0];
	for (int i = 0; i < count; i++) {
		int expected = swapped && i < 2 ? 1 - i : i;
		assert(c == &clients[expected]);
		c = c->next;
	}
	assert(c == NULL);
}

int main(void)
{
	Client clients[MAX_CLIENTS + 4] = {0};
	const int counts[] = {0, 1, 2, MAX_CLIENTS, MAX_CLIENTS + 1, MAX_CLIENTS + 4};
	for (size_t n = 0; n < sizeof(counts) / sizeof(counts[0]); n++) {
		int count = counts[n];
		for (int i = 0; i < count; i++) {
			clients[i].next = i + 1 < count ? &clients[i + 1] : NULL;
			clients[i].mapped = 1;
		}
		workspaces[0] = count ? clients : NULL;
		move_master_next();
		check_order(clients, count, count >= 2);
		move_master_prev();
		check_order(clients, count, 0);
		move_master_prev();
		check_order(clients, count, count >= 2);
		move_master_next();
		check_order(clients, count, 0);
	}
	/* Floating, fullscreen, hidden and other-monitor clients keep their slots. */
	int count = MAX_CLIENTS + 4;
	for (int i = 0; i < count; i++) {
		clients[i].next = i + 1 < count ? &clients[i + 1] : NULL;
		clients[i].mapped = 1;
	}
	workspaces[0] = clients;
	clients[1].floating = 1;
	clients[2].fullscreen = 1;
	clients[3].mapped = 0;
	clients[4].mon = 1;
	master_count = 3;
	move_master_next();
	Client *c = workspaces[0];
	for (int i = 0; i < count; i++) {
		int expected = i == 0 ? 5 : i == 5 ? 6 : i == 6 ? 0 : i;
		assert(c == &clients[expected]);
		c = c->next;
	}
	assert(c == NULL);
	move_master_prev();
	check_order(clients, count, 0);
	current_ws = -1;
	move_master_next();
	move_master_prev();
	current_ws = NUM_WORKSPACES;
	move_master_next();
	move_master_prev();
	check_order(clients, count, 0);
	puts("master client retention tests passed");
	return 0;
}
