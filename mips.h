/* File: mips.h
 * ------------
 * The Mips class defines an object capable of emitting MIPS
 * instructions and managing the allocation and use of registers.
 * It is used by the Tac instruction classes to convert each
 * instruction to the appropriate MIPS equivalent.
 *
 * You can scan this code to see it works and get a sense of what
 * it does.  You will not need to modify this class unless
 * you're attempting some machine-specific optimizations. 
 *
 * It comments the emitted assembly but the commenting for the code
 * in the class itself is pretty sparse. The SPIM manual (see link
 * from other materials on our web site) has more detailed documentation
 * on the MIPS architecture, instruction set, calling conventions, etc.
 */

#ifndef _H_mips
#define _H_mips

#include "tac.h"
#include "list.h"
#include "cfg.h"
#include <map> 
class Location;


class Mips {
  private:
    typedef enum {
            zero, at, v0, v1, a0, a1, a2, a3, 
			s0, s1, s2, s3, s4, s5, s6, s7,
			t0, t1, t2, t3, t4, t5, t6, t7,
			t8, t9, k0, k1, gp, sp, fp, ra, 
            f0, f1, f2, f3, f4, f5, f6, f7,
            f8, f9, f10, f11, f12, f13, f14, 
            f15, f16, f17, f18, f19, f20, f21, 
            f22, f23, f24, f25, f26, f27, f28,
            f29, f30, f31 } Register;
    // 31 regular 32 fp
    struct RegContents {
	bool isDirty;
	Location *var;
	const char *name;
	bool isGeneralPurpose;
    bool mutexLocked;
    bool canDiscard;
    } regs[64];

    Register rs, rt, rd;

    std::map<Register, Location*> register_descriptor;
    //std::map<Location*, Register> copyOf_register_descriptor;
    int oldestTmpReg;

    typedef enum { ForRead, ForWrite } Reason;
    
    /* Fill and Spill */
    void FillRegister(Location *src, Register reg);
    void SpillRegister(Location *dst, Register reg);
    void DiscardValueInRegister(Location *dst, Register reg);

    /* Save + Restore to s registers*/
    // (removed, see previous commits for src)
    //void saveRegisters();
    //void restoreRegisters();
      
    /* Register maintenence + fetching */
    int regs_indexOfNextClean_T();
    int regs_selectRandom_T(); //uses a mutex.
    int regs_pickRegForVar_T(Location *varLoc, bool copyRequired);
    void regs_cleanRegister_T(Register reg);
    void regs_cleanForBranch();

    void regs_discardForBranch();

    /* FPU analogues (might not need all these)
    int regs_indexOfNextClean_F();
    int regs_selectRandom_F(); //uses a mutex.
    int regs_pickRegForVar_F(Location *varLoc);
    void regs_cleanRegister_F(Register reg);
     */

    /*
    int nextCleanRegIndex();
    int getBackupRegister();
    void cleanTempRegisters();
    void cleanRegister(int index);
    */

    /* register descriptor manipulation and access */
    void RD_insert(Location *varLoc, Register reg);
    void RD_remove(Location *varLoc, Register reg);
    void RD_updateRegister(Location *varLoc, Register reg);
    int RD_lookup_RegisterForVar(Location *varLoc);
    Location* RD_getRegContents(Register reg);
    std::map<Register, Location*>::iterator RD_lookupIterForReg(Location *varLoc);


    /* everything else */
    void EmitCallInstr(Location *dst, const char *fn, bool isL);
    
    static const char *mipsName[BinaryOp::NumOps];
    static const char *NameForTac(BinaryOp::OpCode code);

    Instruction* currentInstruction;
 public:
    Mips();

    static void Emit(const char *fmt, ...);
    
    void EmitDiscardValue(Location *dst);

    void EmitLoadConstant(Location *dst, int val);
    void EmitLoadStringConstant(Location *dst, const char *str);
    void EmitLoadLabel(Location *dst, const char *label);

    void EmitLoad(Location *dst, Location *reference, int offset);
    void EmitStore(Location *reference, Location *value, int offset);
    void EmitCopy(Location *dst, Location *src);

    void EmitBinaryOp(BinaryOp::OpCode code, Location *dst, 
			    Location *op1, Location *op2);
    void FP_EmitBinaryOp(const char *operation, Location *dst, 
			    Location *op1, Location *op2);

    void EmitLabel(const char *label);
    void EmitGoto(const char *label);
    void EmitIfZ(Location *test, const char*label);
    void EmitReturn(Location *returnVal);

    void EmitBeginFunction(int frameSize);
    void EmitEndFunction();

    void EmitParam(Location *arg);
    void EmitLCall(Location *result, const char* label);
    void EmitACall(Location *result, Location *fnAddr);
    void EmitPopParams(int bytes);

    void EmitVTable(const char *label, List<const char*> *methodLabels);

    void EmitPreamble();

  
    class CurrentInstruction;
};


// Adds CurrentInstruction to the Mips object
class Mips::CurrentInstruction
{
public:
  CurrentInstruction(Mips& mips, Instruction* instr)
    : mips( mips )
  {
    mips.currentInstruction= instr;
  }

  ~CurrentInstruction()
  {
    mips.currentInstruction= NULL;
  }

private:
  Mips& mips;
};


#endif
 
