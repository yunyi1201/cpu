/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  char expr[256];  
  word_t ori_result;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
  Log("watchpoint: %s", MUXDEF(CONFIG_WATCHPOINT, ANSI_FMT("ON", ANSI_FG_GREEN), ANSI_FMT("OFF", ANSI_FG_RED)));
}


static WP* new_wp() {
  Assert(free_, "no more free watchpoint");
  WP *wp = free_;

  free_ = wp->next;
  wp->next = head;
  head = wp;
  return wp;
}

static void free_wp(WP* wp) {
  WP **curr = &head;
  WP *prev = NULL;

  for (; *curr != wp; curr = &(*curr)->next) {
    prev = *curr;
  }
  if (!prev) {
    *curr = (*curr)->next;
    return ;
  }
  prev->next = (*curr)->next;
}

void watchpoint_display() {
  printf("NO\texpression\t\n");
  WP *wp = head;
  while (wp) {
    printf("%d\t%s\n", wp->NO, wp->expr);
    wp = wp->next;
  } 
}


void new_watchpoint(char *e, word_t result) {
  WP *wp = new_wp();
  Assert(wp, "wp pool should have enough watchpoint");
  memcpy(wp->expr, e, strlen(e));
  wp->ori_result = result;
}


void delete_watchpoint(int num) {
  Assert(num < NR_WP, "watchpoint don't exist");
  free_wp(&wp_pool[num]);
}


void wp_is_toggle() {
  WP *wp = head;
  bool success = false;
  while (wp) {
    word_t result = expr(wp->expr, &success);
    Assert(success, "eval expression %s should success", wp->expr);
    if (wp->ori_result != result) {
      printf("watchpoit %d:\t %s\n", wp->NO, wp->expr);
      printf("old value = %lu\n", wp->ori_result);
      printf("new value = %lu\n", result);
      nemu_state.state = NEMU_STOP;
    }
    wp = wp->next;
  }
}

