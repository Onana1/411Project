#include "mips-small-pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/************************************************************/
int main(int argc, char *argv[]) {
  short i;
  char line[MAXLINELENGTH];
  state_t state;
  FILE *filePtr;

  if (argc != 2) {
    printf("error: usage: %s <machine-code file>\n", argv[0]);
    return 1;
  }

  memset(&state, 0, sizeof(state_t));

  state.pc = state.cycles = 0;
  state.IFID.instr = state.IDEX.instr = state.EXMEM.instr = state.MEMWB.instr =
      state.WBEND.instr = NOPINSTRUCTION; /* nop */

  /* read machine-code file into instruction/data memory (starting at address 0)
   */

  filePtr = fopen(argv[1], "r");
  if (filePtr == NULL) {
    printf("error: can't open file %s\n", argv[1]);
    perror("fopen");
    exit(1);
  }

  for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL;
       state.numMemory++) {
    if (sscanf(line, "%x", &state.dataMem[state.numMemory]) != 1) {
      printf("error in reading address %d\n", state.numMemory);
      exit(1);
    }
    state.instrMem[state.numMemory] = state.dataMem[state.numMemory];
    printf("memory[%d]=%x\n", state.numMemory, state.dataMem[state.numMemory]);
  }

  printf("%d memory words\n", state.numMemory);

  printf("\tinstruction memory:\n");
  for (i = 0; i < state.numMemory; i++) {
    printf("\t\tinstrMem[ %d ] = ", i);
    printInstruction(state.instrMem[i]);
  }

  run(&state);

  return 0;
}
/************************************************************/

/************************************************************/
void run(Pstate state) {
  state_t new;
  int instrCode, funcOperation, instrMemCode, instrWBCode, exVarA, exVarB;
  memset(&new, 0, sizeof(state_t));
  

  while (1) {

    printState(state);

    /* copy everything so all we have to do is make changes.
       (this is primarily for the memory and reg arrays) */
    memcpy(&new, state, sizeof(state_t));

    new.cycles++;
    /*if (new.cycles > 15) {
       exit(0);
    }*/

    /* --------------------- IF stage --------------------- */
    new.IFID.instr = state->instrMem[state->pc >> 2];

    if ((opcode(state->instrMem[state->pc >> 2]) == BEQZ_OP) && offset(field_imm(state->instrMem[state->pc >> 2])) < 0) {
      new.IFID.pcPlus1 = state->pc + 4;
      new.pc = state->pc + 4 + offset(field_imm(state->instrMem[state->pc >> 2]));
      new.IFID.instr = state->instrMem[state->pc >> 2];
    }
    /*if ((opcode(state->EXMEM.instr) == BEQZ_OP) && (offset(new.IFID.instr) < 0))
    if ((opcode(state->EXMEM.instr) == BEQZ_OP) && (offset(new.IFID.instr) < 0)) {
    
    }
     if ((opcode(state->EXMEM.instr) == BEQZ_OP)){ */
    /*if ((opcode(new.IFID.instr) == BEQZ_OP) && (offset(new.IFID.instr) < 0)) {
        new.IFID.pcPlus1 = new.pc = state->pc + offset(new.IFID.instr);
    }*/ else {
      new.IFID.pcPlus1 = new.pc += 4;
    }
    
    /* --------------------- ID stage --------------------- */
    new.IDEX.readRegA = state->reg[field_r1(state->IFID.instr)];
    new.IDEX.readRegB = state->reg[field_r2(state->IFID.instr)];
    new.IDEX.pcPlus1 = state->IFID.pcPlus1;
    new.IDEX.instr = state->IFID.instr;
    new.IDEX.offset = offset(field_imm(state->IFID.instr));

    /*stall if load precedes and uses source*/
   if (opcode(state->IDEX.instr) == LW_OP && field_r2(state->IDEX.instr) != 0) {
     if (field_r2(state->IDEX.instr) == field_r1(state->IFID.instr)) {
        new.IDEX.readRegA = state->reg[field_r1(NOPINSTRUCTION)];
        new.IDEX.readRegB = state->reg[field_r2(NOPINSTRUCTION)];
        new.IDEX.pcPlus1 = 0;
        new.IDEX.instr = NOPINSTRUCTION;
        new.IDEX.offset = offset(field_imm(NOPINSTRUCTION));
        new.IFID = state->IFID;
        new.pc = state->pc;
     } else if (opcode(state->IFID.instr) == REG_REG_OP && (field_r2(state->IDEX.instr) == field_r2(state->IFID.instr))) {
        new.IDEX.readRegA = state->reg[field_r1(NOPINSTRUCTION)];
        new.IDEX.readRegB = state->reg[field_r2(NOPINSTRUCTION)];
        new.IDEX.pcPlus1 = 0;
        new.IDEX.instr = NOPINSTRUCTION;
        new.IDEX.offset = offset(field_imm(NOPINSTRUCTION));
        new.IFID = state->IFID;
        new.pc = state->pc;
     }
   }

    
    /*instrOp = opcode(state->IFID.instr);
    if (instrOp == REG_REG_OP) {
      new.IDEX.offset = state->reg[field_r1(state->IFID.instr)];
    } else {
      new.IDEX.offset = offset(field_imm(state->IFID.instr));
    }*/
    
    /* --------------------- EX stage --------------------- */
    instrCode = opcode(state->IDEX.instr);
    /*exVarA = state->reg[state->IDEX.readRegA];
    exVarB = state->reg[state->IDEX.readRegB];*/
    /*printf("\t varAreg: %d\n", state->reg[field_r1(state->IFID.instr)]);*/
    exVarA = state->IDEX.readRegA;
    exVarB = state->IDEX.readRegB;

    /*forwarding check start*/
    if (opcode(state->WBEND.instr) == REG_REG_OP) {
      if (field_r3(state->WBEND.instr) == field_r1(state->IDEX.instr)) {
        exVarA = state->WBEND.writeData;
      }

      if (field_r3(state->WBEND.instr) == field_r2(state->IDEX.instr)) {
        exVarB = state->WBEND.writeData;
      }
    }

    if (opcode(state->MEMWB.instr) == REG_REG_OP) {
      if (field_r3(state->MEMWB.instr) == field_r1(state->IDEX.instr)) {
        exVarA = state->MEMWB.writeData;
      }

      if (field_r3(state->MEMWB.instr) == field_r2(state->IDEX.instr)) {
        exVarB = state->MEMWB.writeData;
      }
    }

    if (opcode(state->EXMEM.instr) == REG_REG_OP) {
      if (field_r3(state->EXMEM.instr) == field_r1(state->IDEX.instr)) {
        exVarA = state->EXMEM.aluResult;
      }

      if (field_r3(state->EXMEM.instr) == field_r2(state->IDEX.instr)) {
        exVarB = state->EXMEM.aluResult;
      }

    }

    if (((opcode(state->WBEND.instr) == ADDI_OP) || (opcode(state->WBEND.instr) == LW_OP)) && (field_r2(state->WBEND.instr) != 0)){
      /*printf("\t wbend %d\n", field_r2(state->WBEND.instr));*/
      if (field_r2(state->WBEND.instr) == field_r1(state->IDEX.instr)) {
        exVarA = state->WBEND.writeData;
      }

      if (field_r2(state->WBEND.instr) == field_r2(state->IDEX.instr)) {
        exVarB = state->WBEND.writeData;
      }
    }
    
    if (((opcode(state->MEMWB.instr) == ADDI_OP) || (opcode(state->MEMWB.instr) == LW_OP)) && (field_r2(state->MEMWB.instr) != 0)){
      /*printf("\t memwb %d\n", field_r2(state->MEMWB.instr));*/
      if (field_r2(state->MEMWB.instr) == field_r1(state->IDEX.instr)) {
        exVarA = state->MEMWB.writeData;
      }

      if (field_r2(state->MEMWB.instr) == field_r2(state->IDEX.instr)) {
        exVarB = state->MEMWB.writeData;
      }
    }

    if (((opcode(state->EXMEM.instr) == ADDI_OP) || (opcode(state->EXMEM.instr) == LW_OP)) && (field_r2(state->EXMEM.instr) != 0)){
      if (field_r2(state->EXMEM.instr) == field_r1(state->IDEX.instr)) {
        exVarA = state->EXMEM.aluResult;
      }

      if (field_r2(state->EXMEM.instr) == field_r2(state->IDEX.instr)) {
        exVarB = state->EXMEM.aluResult;
      }
    }

    /*forwarding check end*/

    if (instrCode == REG_REG_OP){
      new.EXMEM.instr = state->IDEX.instr;
      funcOperation = func(state->IDEX.instr);
      new.EXMEM.readRegB = exVarB;

      if (funcOperation == ADD_FUNC) {
        new.EXMEM.aluResult = exVarA + exVarB;
      } else if (funcOperation == SUB_FUNC) {
        new.EXMEM.aluResult = exVarA - exVarB;
      } else if (funcOperation == SLL_FUNC) {
        new.EXMEM.aluResult = exVarA << exVarB;
      } else if (funcOperation == SRL_FUNC) {
        new.EXMEM.aluResult = ((unsigned int) exVarA) >> exVarB;
      } else if (funcOperation == AND_FUNC) {
        new.EXMEM.aluResult = exVarA & exVarB;
      } else if (funcOperation == OR_FUNC) {
        new.EXMEM.aluResult = exVarA | exVarB;
      }

    if (field_r3(state->IDEX.instr) == 0) {
      new.EXMEM.aluResult = 0;
    }
    } else if (instrCode == ADDI_OP) {
      new.EXMEM.instr = state->IDEX.instr;
      new.EXMEM.aluResult = exVarA + state->IDEX.offset;
      new.EXMEM.readRegB = state->IDEX.readRegB;
      if (field_r2(state->IDEX.instr) == 0) {
        new.EXMEM.aluResult = 0;
      }
    } else if (instrCode == LW_OP) {
      new.EXMEM.instr = state->IDEX.instr;
      new.EXMEM.aluResult = (exVarA + state->IDEX.offset);
      new.EXMEM.readRegB = state->IDEX.readRegB;
      
    } else if (instrCode == SW_OP) {
      new.EXMEM.instr = state->IDEX.instr;
      new.EXMEM.aluResult = (exVarA + state->IDEX.offset);
      new.EXMEM.readRegB = exVarB;
    } else if (instrCode == BEQZ_OP) {
      new.EXMEM.instr = state->IDEX.instr;
      new.EXMEM.aluResult = state->IDEX.pcPlus1 + state->IDEX.offset;
      /*new.pc = state->IDEX.pcPlus1 + state->IDEX.offset;*/
      if (exVarA == 0) {
        /* should have predicted taken */
        /* check if prediction was correct */
        if (state->IDEX.offset >= 0) {
          new.pc = state->IDEX.pcPlus1 + state->IDEX.offset;

          new.IFID.instr = NOPINSTRUCTION;
          new.IFID.pcPlus1 = 0;

          new.IDEX.instr = NOPINSTRUCTION;
          new.IDEX.pcPlus1 = 0;
          new.IDEX.readRegA = 0;
          new.IDEX.readRegB = 0;
          new.IDEX.offset = offset(field_imm(NOPINSTRUCTION));
        }
      } else {
          if (state->IDEX.offset < 0) {
            new.pc = state->IDEX.pcPlus1 + state->IDEX.offset + 4;

            new.IFID.instr = NOPINSTRUCTION;
            new.IFID.pcPlus1 = 0;

            new.IDEX.instr = NOPINSTRUCTION;
            new.IDEX.pcPlus1 = 0;
            new.IDEX.readRegA = 0;
            new.IDEX.readRegB = 0;
            new.IDEX.offset = offset(field_imm(NOPINSTRUCTION));
          }
      }
      new.EXMEM.readRegB = exVarB;

    } else if (instrCode == HALT_OP || instrCode == NOPINSTRUCTION) {
      new.EXMEM.instr = state->IDEX.instr;
      new.EXMEM.aluResult = exVarA + state->IDEX.offset;
      new.EXMEM.readRegB = state->IDEX.readRegB;
    } else {
      printf("error: illegal opcode %x\n", instrCode);
            exit(1);
    }

    /* --------------------- MEM stage --------------------- */
    instrMemCode = opcode(state->EXMEM.instr);
    new.MEMWB.instr = state->EXMEM.instr;
    
    if ((instrMemCode == REG_REG_OP) || (instrMemCode == ADDI_OP)){
      new.MEMWB.writeData = state->EXMEM.aluResult;
    } else if (instrMemCode == LW_OP) {
      new.MEMWB.writeData = new.dataMem[state->EXMEM.aluResult >> 2];
      if (field_r2(state->EXMEM.instr) == 0) {
        new.MEMWB.writeData = 0;
      }
      /*new.dataMem[state->EXMEM.aluResult] = state->EXMEM.readRegB;*/
    } else if (instrMemCode == SW_OP) {
      new.MEMWB.writeData = state->EXMEM.readRegB;
      new.dataMem[state->EXMEM.aluResult >> 2] = state->EXMEM.readRegB;
    } else if (instrMemCode == HALT_OP || instrMemCode == BEQZ_OP) {
      new.MEMWB.writeData = state->EXMEM.aluResult;
    }
    
    
    /* --------------------- WB stage --------------------- */
    instrWBCode = opcode(state->MEMWB.instr);
    if (instrWBCode == REG_REG_OP){
      new.reg[field_r3(state->MEMWB.instr)] = state->MEMWB.writeData;
    } else if (instrWBCode == ADDI_OP || instrWBCode == LW_OP) {
      new.reg[field_r2(state->MEMWB.instr)] = state->MEMWB.writeData;
    } else if (instrWBCode == HALT_OP) {
      printf("machine halted\n");
      printf("total of %d cycles executed\n", state->cycles);
      exit(0);
    }

    /* --------------------- end stage --------------------- */
    new.WBEND.instr = state->MEMWB.instr;
    new.WBEND.writeData = state->MEMWB.writeData;

    /* transfer new state into current state */
    memcpy(state, &new, sizeof(state_t));
  }
}
/************************************************************/

/************************************************************/
int opcode(int instruction) { return (instruction >> OP_SHIFT) & OP_MASK; }
/************************************************************/

/************************************************************/
int func(int instruction) { return (instruction & FUNC_MASK); }
/************************************************************/

/************************************************************/
int field_r1(int instruction) { return (instruction >> R1_SHIFT) & REG_MASK; }
/************************************************************/

/************************************************************/
int field_r2(int instruction) { return (instruction >> R2_SHIFT) & REG_MASK; }
/************************************************************/

/************************************************************/
int field_r3(int instruction) { return (instruction >> R3_SHIFT) & REG_MASK; }
/************************************************************/

/************************************************************/
int field_imm(int instruction) { return (instruction & IMMEDIATE_MASK); }
/************************************************************/

/************************************************************/
int offset(int instruction) {
  /* only used for lw, sw, beqz */
  return convertNum(field_imm(instruction));
}
/************************************************************/

/************************************************************/
int convertNum(int num) {
  /* convert a 16 bit number into a 32-bit Sun number */
  if (num & 0x8000) {
    num -= 65536;
  }
  return (num);
}
/************************************************************/

/************************************************************/
void printState(Pstate state) {
  short i;
  printf("@@@\nstate before cycle %d starts\n", state->cycles);
  printf("\tpc %d\n", state->pc);

  printf("\tdata memory:\n");
  for (i = 0; i < state->numMemory; i++) {
    printf("\t\tdataMem[ %d ] %d\n", i, state->dataMem[i]);
  }
  printf("\tregisters:\n");
  for (i = 0; i < NUMREGS; i++) {
    printf("\t\treg[ %d ] %d\n", i, state->reg[i]);
  }
  printf("\tIFID:\n");
  printf("\t\tinstruction ");
  printInstruction(state->IFID.instr);
  printf("\t\tpcPlus1 %d\n", state->IFID.pcPlus1);
  printf("\tIDEX:\n");
  printf("\t\tinstruction ");
  printInstruction(state->IDEX.instr);
  printf("\t\tpcPlus1 %d\n", state->IDEX.pcPlus1);
  printf("\t\treadRegA %d\n", state->IDEX.readRegA);
  printf("\t\treadRegB %d\n", state->IDEX.readRegB);
  printf("\t\toffset %d\n", state->IDEX.offset);
  printf("\tEXMEM:\n");
  printf("\t\tinstruction ");
  printInstruction(state->EXMEM.instr);
  printf("\t\taluResult %d\n", state->EXMEM.aluResult);
  printf("\t\treadRegB %d\n", state->EXMEM.readRegB);
  printf("\tMEMWB:\n");
  printf("\t\tinstruction ");
  printInstruction(state->MEMWB.instr);
  printf("\t\twriteData %d\n", state->MEMWB.writeData);
  printf("\tWBEND:\n");
  printf("\t\tinstruction ");
  printInstruction(state->WBEND.instr);
  printf("\t\twriteData %d\n", state->WBEND.writeData);
}
/************************************************************/

/************************************************************/
void printInstruction(int instr) {

  if (opcode(instr) == REG_REG_OP) {

    if (func(instr) == ADD_FUNC) {
      print_rtype(instr, "add");
    } else if (func(instr) == SLL_FUNC) {
      print_rtype(instr, "sll");
    } else if (func(instr) == SRL_FUNC) {
      print_rtype(instr, "srl");
    } else if (func(instr) == SUB_FUNC) {
      print_rtype(instr, "sub");
    } else if (func(instr) == AND_FUNC) {
      print_rtype(instr, "and");
    } else if (func(instr) == OR_FUNC) {
      print_rtype(instr, "or");
    } else {
      printf("data: %d\n", instr);
    }

  } else if (opcode(instr) == ADDI_OP) {
    print_itype(instr, "addi");
  } else if (opcode(instr) == LW_OP) {
    print_itype(instr, "lw");
  } else if (opcode(instr) == SW_OP) {
    print_itype(instr, "sw");
  } else if (opcode(instr) == BEQZ_OP) {
    print_itype(instr, "beqz");
  } else if (opcode(instr) == HALT_OP) {
    printf("halt\n");
  } else {
    printf("data: %d\n", instr);
  }
}
/************************************************************/

/************************************************************/
void print_rtype(int instr, const char *name) {
  printf("%s %d %d %d\n", name, field_r3(instr), field_r1(instr),
         field_r2(instr));
}
/************************************************************/

/************************************************************/
void print_itype(int instr, const char *name) {
  printf("%s %d %d %d\n", name, field_r2(instr), field_r1(instr),
         offset(instr));
}
/************************************************************/
