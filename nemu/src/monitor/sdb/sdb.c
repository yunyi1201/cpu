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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <memory/vaddr.h>
#include "sdb.h"

static int is_batch_mode = false;

void watchpoint_display();
void new_watchpoint(char *e, word_t result);
void delete_watchpoint(int num);
void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}


static int cmd_si(char *args) {
  int step = -1;
  if (!args) {
    step = 1;
  } else {
    int ret = sscanf(args, "%d", &step);
    if (ret != 1) {
      printf("oops, %s format is wrong\n", args);
      return 0;
    }
  }
  Assert(step > 0, "step: %d should great than 0", step);
  cpu_exec(step);
  return 0;
}

static int cmd_info(char *args) {
  if (!args) {
    printf("Usage: info [r/m]\n");
    return 0;
  }
  if (!strcmp(args, "r")) {
    isa_reg_display();
    return 0;
  } 

  if (!strcmp(args, "w")) {
    watchpoint_display();
    return 0;
  }

  Assert(0, "Not reconized argument %s", args);
}


static int cmd_x(char *args) {
  char *end = args + strlen(args);
  int n;
  vaddr_t addr;
  char *s1 = strtok(NULL, " ");
  if (!s1) {
    printf("Usage: x [n] [addr]\n");
    return 0;
  }   

  char *s2 = s1 + strlen(s1) + 1;
  if (s2 >= end) {
    printf("Usage: x [n] [addr]\n");
    return 0;
  }
  sscanf(s1, "%d", &n);
  sscanf(s2, "%lx",&addr);
  for (int i = 0; i < n; i++) {
    word_t value = vaddr_read(addr+i*4, 4);
    printf("Addr 0x%lx:\t  0x%lx\n", addr+i*4, value);
  }
  return 0;
}

static int cmp_p(char *args) {
  if (!args) {
    printf("Usage: p [expr]\n");
    return 0;
  }
  bool success = false;
  word_t value = expr(args, &success);
  
  if (success) {
    printf("eval %s, got %lx\n", args, value);
    return 0;
  }
  printf("eval invalid expression %s\n", args);
  return 0;
}


static int cmd_w(char *args) {
  if (!args) {
    printf("Usage: w [expr]\n");
    return 0;
  }
  bool success = false;
  word_t result = expr(args, &success);
  if (!success) {
    printf("eval an invalid expression %s\n", args);
    return 0;
  }
  new_watchpoint(args, result);
  return 0;
}

static int cmd_d(char *args) {
    if (!args) {
    printf("Usage: d [num]\n");
    return 0;
  }
  int num = -1;
  sscanf(args, "%d", &num);
  if (num == -1) {
    printf("can't read watchpoint num from %s\n", args);
    return 0;
  }
  delete_watchpoint(num); 
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "step exec [n] instructions", cmd_si},
  { "info", "display registers infomation", cmd_info},
  { "x", "display memory infomation", cmd_x},
  {"p", "eval expression value", cmp_p},
  {"w", "set a watchpoint", cmd_w},
  {"d", "delete an watchpoint", cmd_d},
};




#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
