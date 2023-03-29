#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Vtop.h"
#include "verilated.h"

#ifdef TRACE
#include "verilated_vcd_c.h"
#endif

vluint64_t trace_time = 0;

int main(int argc, char *argv[]) {
  VerilatedContext* contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  Vtop* top = new Vtop{contextp};
  #ifdef TRACE
  VerilatedVcdC *tfp = new VerilatedVcdC;
  Verilated::traceEverOn(true);
  top->trace(tfp, 0);
  tfp->open("wave.vcd");
  #endif

  while (1) {
    int a = rand() & 1;
    int b = rand() & 1;
    top->a = a;
    top->b = b;
    top->eval();
    printf("a = %d, b = %d, f = %d\n", a, b, top->f);
    assert(top->f == (a ^ b));
    #ifdef TRACE
    tfp->dump(trace_time);
    trace_time ++;
    if (trace_time > 2000) break;
    #endif
  }

  #ifdef TRACE
  tfp->close();
  #endif
  delete top;
  delete contextp;
  return 0;
}
