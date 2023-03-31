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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>


word_t vaddr_read(vaddr_t addr, int len);

enum {
  TK_NOTYPE = 256, TK_EQ,
  TK_NUM, TK_REG, TK_NEQ, TK_REF,

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"-", '-'},
  {"\\*", '*'},
  {"/", '/'},
  {"0x[0-9a-fA-F]{1,16}", TK_NUM},
  {"[0-9]{1,20}", TK_NUM},
  {"\\(", '('},
  {"\\)", ')'},
  {"\\$[a-z0-9]{1,31}", TK_REG},
  {"!=", TK_NEQ},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE: break;
          case TK_NUM:
          case TK_REG: sprintf(tokens[nr_token].str, "%.*s", substr_len, substr_start);
          default:  tokens[nr_token].type = rules[i].token_type;
                    nr_token++;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}


static int op_prec(int t) {
  switch (t)
  {
		case TK_REF: return 0;
 		case '*': case '/': return 1;
 		case '+': case '-': return 2;
		case TK_EQ: case TK_NEQ: return 3;
 		default:
    	Assert(0, "Not recongized operator: %d", t);
  }
}


static inline int op_prec_cmp(int t1, int t2) {
  return op_prec(t1) - op_prec(t2);
}



static int find_dominated_op(int p, int q, bool *success) {
  int i;
  int bracket_level = 0;
  int dominated_op = -1;
  for (i = p; i <= q; i++) {
    switch (tokens[i].type)
    {
      case TK_NUM: case TK_REG: break;

      case '(': 
        bracket_level ++;
        break; 

      case ')':
        bracket_level --;
        if (bracket_level < 0) {
          *success = false;
          return -1;
        }
        break;

      default:
        if (bracket_level == 0) {
          if (dominated_op == -1 || op_prec_cmp(tokens[dominated_op].type, tokens[i].type) < 0 || 
							(op_prec_cmp(tokens[dominated_op].type, tokens[i].type) == 0 && 
							tokens[i].type != TK_REF)) {
            dominated_op = i;
          }
        }
        break;
    }
  }

  *success = (dominated_op == -1) ? false: true;
  return dominated_op;
}

// evaluate expression value
// p: expression start posion
// q: expression end posion
static word_t eval(int p, int q, bool *success) {
  // bad expression
  if (p > q) {
    *success = false;
    return 0; 
  } else if (p == q) { // signal element, directly return.
		word_t val;
		switch (tokens[p].type)
		{
			case TK_REG: val = isa_reg_str2val(tokens[p].str+1, success);
								if (!*success) return 0; 
								break;
			case TK_NUM: val = strtol(tokens[p].str, NULL, 0); break;
			default: assert(0);
			*success = true;
		}
		return val;
  } else if (tokens[p].type == '(' && tokens[q].type == ')') {
    return eval(p+1, q-1, success);
  } else {
    int dominated_op = find_dominated_op(p, q, success);
    // bad expression
    if (!(*success)) 
      return 0;
    Assert(dominated_op >= 0 && dominated_op < nr_token, "error occured");

		int op_type = tokens[dominated_op].type;
		if (op_type == TK_REF) {
			word_t val = eval(dominated_op+1, q, success);
			if (!*success) return 0;
			switch (op_type)
			{
				case TK_REF: return vaddr_read(val, 8);
				default: Assert(0, "Not reconization operator");
			}
		}

    word_t right = eval(dominated_op+1, q, success);  
    if (!(*success)) return 0;
    
    word_t left = eval(p, dominated_op-1, success);
    if (!(*success)) return 0;
    
    *success = true;
    switch (tokens[dominated_op].type)
    {
      case '+': 
        return right + left;
      case '-':
        return left - right;
      case '*': 
        return left * right;
      case '/':
        Assert(right != 0, "divide by zero error");
        return left / right;
			case TK_EQ: return right == left;
			case TK_NEQ: return right != left;
      default:
        panic("Not reconization operator!");
    }
  }
	return 0;
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  int prev_type;
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '*') {
      if (i == 0) {
        tokens[i].type = TK_REF;
        continue;
      }
      prev_type = tokens[i-1].type;
      if (!(prev_type == ')' || prev_type == TK_NUM || prev_type == TK_REG)) {
        tokens[i].type = TK_REF;
      }
    }
  }
  return eval(0, nr_token-1, success);
}
