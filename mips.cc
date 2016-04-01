/* File: mips.cc
 * -------------
 * Implementation of Mips class, which is responsible for TAC->MIPS
 * translation, register allocation, etc.
 *
 * Julie Zelenski academic year 2001-02 for CS143
 * Loosely based on some earlier work by Steve Freund
 *
 * A simple final code generator to translate Tac to MIPS.
 * It uses simplistic algorithms, in particular, its register handling
 * and spilling strategy is inefficient to the point of begin mocked
 * by elementary school children.
 *
 * Dan Bentley, April 2002
 * A simpler final code generator to translate Tac to MIPS.
 * It uses algorithms without loops or conditionals, to make there be a
 * very clear and obvious translation between one and the other.
 * Specifically, it always loads operands off stacks, and stores the
 * result back.  This breaks bad code immediately, theoretically helping
 * students.
 */

#include "mips.h"
#include <stdarg.h>
#include <cstring>


static bool LocationsAreSame(Location *var1, Location *var2)
{
    return (var1 == var2 ||
            (var1 && var2
             && !strcmp(var1->GetName(), var2->GetName())
             && var1->GetSegment()  == var2->GetSegment()
             && var1->GetOffset() == var2->GetOffset()));
}

void Mips::EmitDiscardValue(Location *dst)
{
    // last use of value dst.
    rd = (Register) regs_pickRegForVar_T(dst, false);
    Emit("\t\t#Last use of  %s. Discarding register_descriptor data for %s", 
          dst->GetName(), regs[rd].name); 
    regs[rd].canDiscard = true;
    //DiscardValueInRegister(dst, rd);
}

void Mips::RD_insert(Location *varLoc, Register reg) {

    // if location is already in the table
    if (RD_lookup_RegisterForVar(varLoc) != -1) {
        RD_updateRegister(varLoc, reg); // assume we want to update
        return;
    }
    if (RD_getRegContents(reg) != NULL) {
        printf("#Dbg: register already in use\n");
    }
    if (regs[reg].isDirty) {
        printf("#Dbg: Error, register is dirty.\n");
        return;
    }
    register_descriptor.insert(std::pair<Register, Location*>(reg, varLoc));
    regs[reg].isDirty = true;
}

void Mips::RD_remove(Location *varLoc, Register reg) {
    if (RD_lookup_RegisterForVar(varLoc) == -1) { // if value is not in RD
        //printf("Dbg: Error, tried to remove an unused register.\n");
        //printf("Dbg: register already removed... bad?.\n");
        return;
    }
    if (!regs[reg].isDirty) {
        // we might not need this, other things might be marking it as clean.
        printf("#Dbg: Error, register is not dirty.\n");
        return;
    }
    regs[reg].isDirty = false;
    std::map<Register, Location*>::iterator iter = RD_lookupIterForReg(varLoc);
    register_descriptor.erase(iter);
}

void Mips::RD_updateRegister(Location *varLoc, Register reg) {
    std::map<Register, Location*>::iterator iter = RD_lookupIterForReg(varLoc);
    iter->second = varLoc;
}

int Mips::RD_lookup_RegisterForVar(Location *varLoc) {
    std::map<Register, Location*>::iterator iter;
    Register selectedRegsiter = (Register)zero; // just to initialize it. 
    int found_count = 0;
    for (iter = register_descriptor.begin(); iter != register_descriptor.end(); ++iter) {
        if (LocationsAreSame(varLoc, iter->second)) {
                found_count++;
                selectedRegsiter = iter->first;
        }
    }
    if (found_count == 1) {
        return selectedRegsiter;
    }
    else if (found_count == 0) {
        // not found, return -1 
        return -1;
        // check the FPU ?
    }
    else {
        printf("Dbg: Error, we found multiple copies of varLoc.\n");
        return -10; // arbitrary, unused for now.
    }
}

Location* Mips::RD_getRegContents(Register reg) {
    std::map<Register, Location*>::iterator iter;
    iter = register_descriptor.find(reg);
    if (iter == register_descriptor.end()) {
        // element not found
        return (Location*) NULL;
    }
    else {
        return iter->second;
    }
}

std::map<Mips::Register, Location*>::iterator Mips::RD_lookupIterForReg(Location *varLoc) {
    std::map<Register, Location*>::iterator iter;
    std::map<Register, Location*>::iterator iterToReturn;
    int found_count = 0;

    for (iter = register_descriptor.begin(); iter != register_descriptor.end(); ++iter) {
        if (LocationsAreSame(varLoc, iter->second)) {
                found_count++;
                iterToReturn = iter;
        }
    }
    if (found_count == 1) {
        return iterToReturn;
    }
    else if (found_count == 0) {
        // not found, return -1 
        return register_descriptor.end();
    }
    else {
        printf("Dbg: Error, we found multiple copies of varLoc.\n");
        return register_descriptor.end();
    }
}

/* Method: FillRegister
 * --------------------
 * Fill a register from location src into reg.
 * Simply load a word into a register.
 */
void Mips::FillRegister(Location *src, Register reg) {
    Assert(src);
    const char *offsetFromWhere = src->GetSegment() == fpRelative? regs[fp].name : regs[gp].name;
    Assert(src->GetOffset() % 4 == 0); // all variables are 4 bytes in size

    Register previousReg = (Register) RD_lookup_RegisterForVar(src);
    if (reg > 31) {  // if filling to the fpu
        if ((int) previousReg == -1) { // if src is not already in a register
            // Load directly to coprocessor from ram
            // FRD_insert(); 
            Emit("l.s %s, %d(%s)\t# fill %s to %s from %s%+d", regs[reg].name,
                 src->GetOffset(), offsetFromWhere, src->GetName(), regs[reg].name,
                 offsetFromWhere,src->GetOffset());
        }
        else if (previousReg < 31) { // if src is in a regular register
            // Move to coprocessor:  mtcz Rsrc, CPdest 
            // FRD_insert(); 
            Emit("mtc1 %s, %s\t\t# move %s to %s", regs[previousReg].name, regs[reg].name,
                 regs[previousReg].name, regs[reg].name);
        }
        else { // src is already in coprocessor 
            if (previousReg == reg) { // if src in this register already
                // do nothing
            }
            else { // src is in different coprocessor reg
                // move from coprocessor reg to coprocessor reg
                Emit("mov.s %s, %s\t\t# move %s to %s", regs[rd].name, regs[f0].name,
                     regs[f0].name, regs[rd].name);
            }
        }
    }
    else { // filling to regular registers
        if ((int) previousReg == -1) { // src is not already in a register, load from ram
            Emit("lw %s, %d(%s)\t# fill %s to %s from %s%+d", regs[reg].name,
                 src->GetOffset(), offsetFromWhere, src->GetName(), regs[reg].name,
                 offsetFromWhere,src->GetOffset());
        }
        else if (reg != previousReg) { // if src in a different register
            if (previousReg > 31) { // src in coprocessor
                // mfcz Rdest, CPsrc
                Emit("mfc1 %s, %s\t\t# move %s to %s", regs[reg].name, regs[previousReg].name,
                     regs[previousReg].name, regs[reg].name);
            }
            else { // src in regular register
                Location *previousLoc = RD_getRegContents(previousReg);
                //Register copyFrom = (Register) regs_pickRegForVar_T(previousLoc, false);
                Emit("move %s, %s\t\t# move (fill copy) %s from %s to %s in %s", regs[reg].name, regs[previousReg].name,
                     previousLoc->GetName(), regs[previousReg].name, src->GetName(), regs[reg].name);
            }
        }
        else if (reg == previousReg) { // if already in correct register
            // do nothing
        }
    }
}


void Mips::SpillRegister(Location *dst, Register reg) {
    Assert(dst);
    const char *offsetFromWhere = dst->GetSegment() == fpRelative? regs[fp].name : regs[gp].name;
    Assert(dst->GetOffset() % 4 == 0); // all variables are 4 bytes in size

    if (!regs[reg].isDirty) {
        //printf("Dbg: spilling a clean register... bad\n");
    }

    Emit("sw %s, %d(%s)\t# spill %s from %s to %s%+d", regs[reg].name,
         dst->GetOffset(), offsetFromWhere, dst->GetName(), regs[reg].name,
         offsetFromWhere,dst->GetOffset());
    RD_remove(dst, reg);
    regs[reg].isDirty = false; // double assertion.
}

void Mips::DiscardValueInRegister(Location *dst, Register reg) {
    if (regs[reg].canDiscard == true) {
        Assert(dst);
        const char *offsetFromWhere = dst->GetSegment() == fpRelative? regs[fp].name : regs[gp].name;
        Assert(dst->GetOffset() % 4 == 0); // all variables are 4 bytes in size

        RD_remove(dst, reg);
        regs[reg].isDirty = false; // double assertion.
        regs[reg].canDiscard = false; // double assertion.
    }
}

int Mips::regs_pickRegForVar_T(Location *varLoc, bool copyRequired) {
    
    // if value is already in registers, keep it there.
    // copyRequired -> we *need* a new register, doesn't matter if already in regs.
    if (copyRequired == false) {
        if (RD_lookup_RegisterForVar(varLoc) != -1) { 
            return RD_lookup_RegisterForVar(varLoc);
        }
        else {
            return regs_indexOfNextClean_T();
        }
    }
    else {
        return regs_indexOfNextClean_T();
    }
}

int Mips::regs_indexOfNextClean_T() {

    int cleanRegIndex = -1;
    // Find a clean register
    for (int i = t0; i <= t9; i++) {
        if(regs[i].isDirty == false) {
            cleanRegIndex = i;
            break;
        }
    }
    // if none, clean one at random
    if (cleanRegIndex == -1) {
        // select a random (unlocked) register and clean it.
        cleanRegIndex = regs_selectRandom_T();
        regs_cleanRegister_T((Register)cleanRegIndex);
    }

    return cleanRegIndex;
}

int Mips::regs_selectRandom_T() {
    /*
     * Goal: Select a random t register that is:
     *      1. Not locked (they really should never all be locked)
     *      2. Dirty (by implication), we test anyway though.
     */
    bool done = false;
    int selection; 
    int safety_counter = 0;
    while(!done) {
        // gen random int from 0-10 because t0-t9
        selection = rand()%10 + 16; 
        if (regs[selection].mutexLocked == false) {
            if (regs[selection].isDirty == true) {
                done = true;
            }
        }
        // just in case everything is locked...
        safety_counter++;
        if (safety_counter >= 100) {
            selection = -100;
            break;
        }
    }
    return selection;
}

void Mips::regs_cleanRegister_T(Register reg) {
    /*
     * Goal: completely clean a register, good as new.
     *           (essentially spill register with "unknown" loc)
     *           (consider merging with SpillRegister)
     * Todo:
     *      [x] ensure register is dirty (double clean is bad...)
     *          [x] except
     *      [x] lookup reg in RD
     *          [x] except
     *      [x] spill to that location
     *      [-] remove RD entry -> SpillRegister does this
     *      [] (maybe) double check that removal worked.
     */
    Location *varLoc = RD_getRegContents(reg);
    if (varLoc == NULL) {
        //printf("Dbg: Error, register has no varLoc (but is still dirty?).\n");
        return; // return to avoid lookups with null data
    }
    /*
    if (!regs[reg].isDirty) {
        printf("Dbg: Error, register %s is already clean.\n", regs[reg].name);
    }
    if (RD_lookupIterForReg(varLoc) == register_descriptor.end()) {
        printf("Dbg: Error, register is not in RD (but is still dirty?).\n");
    }
    */

    /*
    if (regs[reg].canDiscard) {
        DiscardValueInRegister(varLoc, reg);
    }
    else {
        SpillRegister(varLoc, reg); // SpillRegister is in charge of updating RD!!
    }
    */
    SpillRegister(varLoc, reg); // SpillRegister is in charge of updating RD!!

    /*
    if (regs[reg].isDirty) {
        printf("Dbg: Error, register spilled but is dirty\n");
    }
    if (RD_getRegContents(reg) != NULL) {
        printf("Dbg: Error, register spilled but still has data.\n");
        if (RD_lookupIterForReg(varLoc) != register_descriptor.end()) {
            printf("Dbg: Error, register spilled but still in RD.\n");
        }
    }
    */

}

void Mips::regs_cleanForBranch() {
    for (int i = t0; i <= t9; i++) {
        if(regs[i].isDirty == true) {
            regs_cleanRegister_T((Register)i);
        }
    }
    register_descriptor.clear();
}
/*
int Mips::nextCleanRegIndex() {
    for (int i = t4; i < k0; i++) {
        if (regs[i].isDirty == false) {
            regs[i].isDirty = true;
            return i;
        }
    }
    return getBackupRegister();
}

int Mips::getBackupRegister() {
    if (oldestTmpReg != t8 && oldestTmpReg != t9) {
        oldestTmpReg = t8;
    }
    regs_cleanRegister_T(oldestTmpReg);
    if (oldestTmpReg == t8) {
        oldestTmpReg = t9;
    }
    else {
        oldestTmpReg = t8;
    }
    return (int) oldestTmpReg;
}

void Mips::cleanTempRegisters() {
    for (int i = 16; i < 26; i++) {
        if(regs[i].isDirty) {
            regs_cleanRegister_T(i);
        }
    }
    register_descriptor.clear();
}

void Mips::regs_cleanRegister_T(int index) {
    Location* dst = RD_getRegContents((Register)index);
    if (dst) {
        SpillRegister(dst,(Register)index);
    }
    else {
        regs[index].isDirty = false;
    }
}
*/

/* Method: Emit
 * ------------
 * General purpose helper used to emit assembly instructions in
 * a reasonable tidy manner.  Takes printf-style formatting strings
 * and variable arguments.
 */
void Mips::Emit(const char *fmt, ...)
{
    va_list args;
    char buf[1024];

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    if (buf[strlen(buf) - 1] != ':') printf("\t"); // don't tab in labels
    if (buf[0] != '#') printf("  ");   // outdent comments a little
    printf("%s", buf);
    if (buf[strlen(buf)-1] != '\n') printf("\n"); // end with a newline
}

/* Method: EmitLoadConstant
 * ------------------------
 * Used to assign variable an integer constant value.  Saves dst into
 * a register (using GetRegister above) and then emits an li (load
 * immediate) instruction with the constant value.
 */
void Mips::EmitLoadConstant(Location *dst, int val)
{

    /*
    // Use zero register:
    if (val == 0) {
        RD_insert(dst, (Register) 0);
        return;
    }
    */

    rd = (Register) regs_pickRegForVar_T(dst, false);
    regs[rd].mutexLocked = true;

    //FillRegister(dst,rd);
    Emit("li %s, %d\t\t# load constant value %d into %s", regs[rd].name,
         val, val, regs[rd].name);
    RD_insert(dst, rd);
    regs[rd].mutexLocked = false;
}

/* Method: EmitLoadStringConstant
 * ------------------------------
 * Used to assign a variable a pointer to string constant. Emits
 * assembly directives to create a new null-terminated string in the
 * data segment and assigns it a unique label. Slaves dst into a register
 * and loads that label address into the register.
 */
void Mips::EmitLoadStringConstant(Location *dst, const char *str)
{
    static int strNum = 1;
    char label[16];
    sprintf(label, "_string%d", strNum++);
    Emit(".data\t\t\t# create string constant marked with label");
    Emit("%s: .asciiz %s", label, str);
    Emit(".text");
    EmitLoadLabel(dst, label);
}


/* Method: EmitLoadLabel
 * ---------------------
 * Used to load a label (ie address in text/data segment) into a variable.
 * Saves dst into a register and emits an la (load address) instruction
 */
void Mips::EmitLoadLabel(Location *dst, const char *label)
{
    rd = (Register) regs_pickRegForVar_T(dst, false);
    regs[rd].mutexLocked = true;
    //FillRegister(dst,rd);
    Emit("la %s, %s\t# load label", regs[rd].name, label);
    RD_insert(dst, rd);
    regs[rd].mutexLocked = false;
}


/* Method: EmitCopy
 * ----------------
 * Used to copy the value of one variable to another.  Saves both
 * src and dst into registers and then emits a move instruction to
 * copy the contents from src to dst.
 */
void Mips::EmitCopy(Location *dst, Location *src)
{

    rs = (Register) regs_pickRegForVar_T(src, false);
    regs[rs].mutexLocked = true;
    FillRegister(src, rs);
    RD_insert(src, rs);

    // If rs will never be used again, send it to a black hold.
    if (regs[rs].canDiscard == true) {
        DiscardValueInRegister(src, rs);
    }

    rd = (Register) regs_pickRegForVar_T(dst, false);
    regs[rd].mutexLocked = true;

    //  FillRegister(src, rs);
    //  FillRegister(src, psuedo_rs);
    //  FillRegister(dst, rd);
    Emit("move %s, %s\t\t# move (copy) %s from %s to %s in %s", regs[rd].name, regs[rs].name,
         src->GetName(), regs[rs].name, dst->GetName(), regs[rd].name);

    RD_insert(dst, rd);
    regs[rs].mutexLocked = false;
    regs[rd].mutexLocked = false;

}


/* Method: EmitLoad
 * ----------------
 * Used to assign dst the contents of memory at the address in reference,
 * potentially with some positive/negative offset (defaults to 0).
 * Saves both ref and dst to registers, then emits a lw instruction
 * using constant-offset addressing mode y(rx) which accesses the address
 * at an offset of y bytes from the address currently contained in rx.
 */
void Mips::EmitLoad(Location *dst, Location *reference, int offset)
{
    rs = (Register) regs_pickRegForVar_T(reference, false);
    regs[rs].mutexLocked = true;
    FillRegister(reference, rs);
    RD_insert(reference, rs);

    rd = (Register) regs_pickRegForVar_T(dst, false);
    regs[rd].mutexLocked = true;

    
    Emit("lw %s, %d(%s) \t# load with offset", regs[rd].name,
         offset, regs[rs].name);

    RD_insert(dst, rd);

    regs_cleanRegister_T(rs);
    regs[rs].mutexLocked = false;
    regs[rd].mutexLocked = false;
}


/* Method: EmitStore
 * -----------------
 * Used to write value to  memory at the address in reference,
 * potentially with some positive/negative offset (defaults to 0).
 * Slaves both ref and dst to registers, then emits a sw instruction
 * using constant-offset addressing mode y(rx) which writes to the address
 * at an offset of y bytes from the address currently contained in rx.
 */
void Mips::EmitStore(Location *reference, Location *value, int offset)
{
    rs = (Register) regs_pickRegForVar_T(value, false);
    regs[rs].mutexLocked = true;
    FillRegister(value, rs);
    RD_insert(value, rs);


    rd = (Register) regs_pickRegForVar_T(reference, false);
    regs[rd].mutexLocked = true;
    FillRegister(reference, rd);
    RD_insert(reference, rd);

    Emit("sw %s, %d(%s) \t# store with offset",
         regs[rs].name, offset, regs[rd].name);
//  regs_cleanRegister_T(rs);

//    regs_cleanRegister_T(rd);
    regs[rd].mutexLocked = false;
    regs[rs].mutexLocked = false;
    /*
    Register reg = (Register) RD_lookup_RegisterForVar(reference);
    if ((int) reg == -1) {
        FillRegister(value, rs);
        FillRegister(reference, rd);
        Emit("sw %s, %d(%s) \t# store with offset",
           regs[rs].name, offset, regs[rd].name);
        regs_cleanRegister_T(rs);
        regs_cleanRegister_T(rd);
        //regs[rd].isDirty = false;
        }
        */
}


/* Method: EmitBinaryOp
 * --------------------
 * Used to perform a binary operation on 2 operands and store result
 * in dst. All binary forms for arithmetic, logical, relational, equality
 * use this method. Slaves both operands and dst to registers, then
 * emits the appropriate instruction by looking up the mips name
 * for the particular op code.
 */
void Mips::EmitBinaryOp(BinaryOp::OpCode code, Location *dst,
                        Location *op1, Location *op2)
{
    const char *operation = NameForTac(code);
    if (!strcmp(operation, "disable fpu")) {
        FP_EmitBinaryOp("add.s", dst, op1, op2);
    }
    /*
    if (!strcmp(operation, "add")) {
        FP_EmitBinaryOp("add.s", dst, op1, op2);
    }
    else if (!strcmp(operation, "sub")) {
        FP_EmitBinaryOp("sub.s", dst, op1, op2);
    }
    else if (!strcmp(operation, "mul")) {
        FP_EmitBinaryOp("mul.s", dst, op1, op2);
    }
    else if (!strcmp(operation, "div")) {
        FP_EmitBinaryOp("div.s", dst, op1, op2);
    }
    else if (!strcmp(operation, "neg")) {
        FP_EmitBinaryOp("neg.s", dst, op1, op2); // look at this later
    }
    */
    else {
        rs = (Register) regs_pickRegForVar_T(op1, false);
        regs[rs].mutexLocked = true;
        FillRegister(op1,rs);
        RD_insert(op1,rs);

        rt = (Register) regs_pickRegForVar_T(op2, false);
        regs[rt].mutexLocked = true;
        FillRegister(op2,rt);
        RD_insert(op2,rt);

        rd = (Register) regs_pickRegForVar_T(dst, false);
        regs[rd].mutexLocked = true;

        //FillRegister(dst,rd);
        Emit("%s %s, %s, %s\t", NameForTac(code), regs[rd].name,
             regs[rs].name, regs[rt].name);
        RD_insert(dst, rd);
        regs[rd].canDiscard = true;
        regs[rs].mutexLocked = false;
        regs[rt].mutexLocked = false;
        regs[rd].mutexLocked = false;
        //SpillRegister(dst, rd);
    }
}

void Mips::FP_EmitBinaryOp(const char* operation, Location *dst,
                           Location *op1, Location *op2)
{
    //fpu rs~f2, rt~f4, rd~f0 , fpu uses even regs.
    // note: neg is broken.
    rd = (Register) regs_pickRegForVar_T(dst, false);
    regs[rd].mutexLocked = true;
    regs[f0].mutexLocked = true;
    regs[f2].mutexLocked = true;
    regs[f4].mutexLocked = true;
    FillRegister(op1,f2);
    FillRegister(op2,f4);

    /*
    // convert data to integers, as per spec.
    Emit("cvt.w.s %s, %s\t\t# move %s to %s", regs[f2].name, regs[f2].name,
         regs[f2].name, regs[f2].name);
    Emit("cvt.w.s %s, %s\t\t# move %s to %s", regs[f4].name, regs[f4].name,
         regs[f4].name, regs[f4].name);
     */

    Emit("%s %s, %s, %s\t", operation, regs[f0].name,
         regs[f2].name, regs[f4].name);

    /*
    // convert result to integer
    Emit("cvt.w.s %s, %s\t\t# move %s to %s", regs[f0].name, regs[f0].name,
         regs[f0].name, regs[f0].name);
    // move result out of coprocessor
     */
    Emit("mfc1 %s, %s\t\t# move %s to %s", regs[rd].name, regs[f0].name,
         regs[f0].name, regs[rd].name);
    RD_insert(dst, rd);
    regs[rd].mutexLocked = false;
    regs[f0].mutexLocked = false;
    regs[f2].mutexLocked = false;
    regs[f4].mutexLocked = false;
}

/* Method: EmitLabel
 * -----------------
 * Used to emit label marker. Before a label, we spill all registers since
 * we can't be sure what the situation upon arriving at this label (ie
 * starts new basic block), and rather than try to be clever, we just
 * wipe the slate clean.
 */
void Mips::EmitLabel(const char *label)
{

    regs_cleanForBranch();
    Emit("%s:", label);
}


/* Method: EmitGoto
 * ----------------
 * Used for an unconditional transfer to a named label. Before a goto,
 * we spill all registers, since we don't know what the situation is
 * we are heading to (ie this ends current basic block) and rather than
 * try to be clever, we just wipe slate clean.
 */
void Mips::EmitGoto(const char *label)
{

    regs_cleanForBranch();
    Emit("b %s\t\t# unconditional branch", label);
}


/* Method: EmitIfZ
 * ---------------
 * Used for a conditional branch based on value of test variable.
 * We save test var to register and use in the emitted test instruction,
 * either beqz. See comments above on Goto for why we spill
 * all registers here.
 */
void Mips::EmitIfZ(Location *test, const char *label)
{
    /*
    rs = (Register) regs_pickRegForVar_T(test, false);
    regs[rs].mutexLocked = true;
    */
    FillRegister(test, v0);
    regs_cleanForBranch();
    Emit("beqz %s, %s\t# branch if %s is zero ", regs[v0].name, label,
         test->GetName());

//    regs[rs].mutexLocked = false;
}


/* Method: EmitParam
 * -----------------
 * Used to push a parameter on the stack in anticipation of upcoming
 * function call. Decrements the stack pointer by 4. Slaves argument into
 * register and then stores contents to location just made at end of
 * stack.
 */
void Mips::EmitParam(Location *arg)
{

    rs = (Register) regs_pickRegForVar_T(arg, false);
    regs[rs].mutexLocked = true;

    FillRegister(arg, rs);
    Emit("subu $sp, $sp, 4\t# decrement sp to make space for param");
    Emit("sw %s, 4($sp)\t# copy param value to stack", regs[rs].name);
    //RD_remove(arg, rs);
    regs[rs].mutexLocked = false;
}


/* Method: EmitCallInstr
 * ---------------------
 * Used to effect a function call. All necessary arguments should have
 * already been pushed on the stack, this is the last step that
 * transfers control from caller to callee.  See comments on Goto method
 * above for why we spill all registers before making the jump. We issue
 * jal for a label, a jalr if address in register. Both will save the
 * return address in $ra. If there is an expected result passed, we save
 * the var to a register and copy function return value from $v0 into that
 * register.
 */
void Mips::EmitCallInstr(Location *result, const char *fn, bool isLabel)
{
    regs_cleanForBranch();
    if (result != NULL) {
        rd = (Register) regs_pickRegForVar_T(result, false);
        regs[rd].mutexLocked = true;
        Emit("%s %-15s\t# jump to function", isLabel? "jal": "jalr", fn);
        Emit("move %s, %s\t\t# copy function return value from $v0",
             regs[rd].name, regs[v0].name);
        RD_insert(result, rd);
        regs[rd].mutexLocked = false;
    }
    else {
        Emit("%s %-15s\t# jump to function", isLabel? "jal": "jalr", fn);
    }
}


// Two covers for the above method for specific LCall/ACall variants
void Mips::EmitLCall(Location *dst, const char *label)
{
    EmitCallInstr(dst, label, true);
}

void Mips::EmitACall(Location *dst, Location *fn)
{
    FillRegister(fn, v0);
    EmitCallInstr(dst, regs[v0].name, false);
}

/*
 * We remove all parameters from the stack after a completed call
 * by adjusting the stack pointer upwards.
 */
void Mips::EmitPopParams(int bytes)
{
    if (bytes != 0)
        Emit("add $sp, $sp, %d\t# pop params off stack", bytes);
}


/* Method: EmitReturn
 * ------------------
 * Used to emit code for returning from a function (either from an
 * explicit return or falling off the end of the function body).
 * If there is an expression to return, we save that variable into
 * a register and move its contents to $v0 (the standard register for
 * function result).  Before exiting, we spill dirty registers (to
 * commit contents of saved registers to memory, necessary for
 * consistency, see comments at SpillForEndFunction above). We also
 * do the last part of the callee's job in function call protocol,
 * which is to remove our locals/temps from the stack, remove
 * saved registers ($fp and $ra) and restore previous values of
 * $fp and $ra so everything is returned to the state we entered.
 * We then emit jr to jump to the saved $ra.
 */
void Mips::EmitReturn(Location *returnVal)
{
    // need to spill as per directions above
    //regs_cleanForBranch();
    if (returnVal != NULL)
    {
        FillRegister(returnVal, v0);
        /*
        rs = (Register) regs_pickRegForVar_T(returnVal, false);
        Emit("move $v0, %s\t\t# assign return value into $v0",
             regs[rd].name);
             */
    }
    Emit("move $sp, $fp\t\t# pop callee frame off stack");
    Emit("lw $ra, -4($fp)\t# restore saved ra");
    Emit("lw $fp, 0($fp)\t# restore saved fp");
    Emit("jr $ra\t\t# return from function");
}


/* Method: EmitBeginFunction
 * -------------------------
 * Used to handle the callee's part of the function call protocol
 * upon entering a new function. We decrement the $sp to make space
 * and then save the current values of $fp and $ra (since we are
 * going to change them), then set up the $fp and bump the $sp down
 * to make space for all our locals/temps.
 */
void Mips::EmitBeginFunction(int stackFrameSize)
{
    Assert(stackFrameSize >= 0);
    Emit("subu $sp, $sp, 8\t# decrement sp to make space to save ra, fp");
    Emit("sw $fp, 8($sp)\t# save fp");
    Emit("sw $ra, 4($sp)\t# save ra");
    Emit("addiu $fp, $sp, 8\t# set up new fp");

    if (stackFrameSize != 0)
        Emit("subu $sp, $sp, %d\t# decrement sp to make space for locals/temps",
             stackFrameSize);
}


/* Method: EmitEndFunction
 * -----------------------
 * Used to end the body of a function. Does an implicit return in fall off
 * case to clean up stack frame, return to caller etc. See comments on
 * EmitReturn above.
 */
void Mips::EmitEndFunction()
{
    Emit("# (below handles reaching end of fn body with no explicit return)");
    EmitReturn(NULL);
}



/* Method: EmitVTable
 * ------------------
 * Used to layout a vtable. Uses assembly directives to set up new
 * entry in data segment, emits label, and lays out the function
 * labels one after another.
 */
void Mips::EmitVTable(const char *label, List<const char*> *methodLabels)
{
    Emit(".data");
    Emit(".align 2");
    Emit("%s:\t\t# label for class %s vtable", label, label);
    for (int i = 0; i < methodLabels->NumElements(); i++)
        Emit(".word %s\n", methodLabels->Nth(i));
    Emit(".text");
}


/* Method: EmitPreamble
 * --------------------
 * Used to emit the starting sequence needed for a program. Not much
 * here, but need to indicate what follows is in text segment and
 * needs to be aligned on word boundary. main is our only global symbol.
 */
void Mips::EmitPreamble()
{
    Emit("# standard Decaf preamble ");
    Emit(".text");
    Emit(".align 2");
    Emit(".globl main");
}


/* Method: NameForTac
 * ------------------
 * Returns the appropriate MIPS instruction (add, seq, etc.) for
 * a given BinaryOp:OpCode (BinaryOp::Add, BinaryOp:Equals, etc.).
 * Asserts if asked for name of an unset/out of bounds code.
 */
const char *Mips::NameForTac(BinaryOp::OpCode code)
{
    Assert(code >=0 && code < BinaryOp::NumOps);
    const char *name = mipsName[code];
    Assert(name != NULL);
    return name;
}

/* Constructor
 * ----------
 * Constructor sets up the mips names and register descriptors to
 * the initial starting state.
 */
Mips::Mips() {
    mipsName[BinaryOp::Add] = "add";
    mipsName[BinaryOp::Sub] = "sub";
    mipsName[BinaryOp::Mul] = "mul";
    mipsName[BinaryOp::Div] = "div";
    mipsName[BinaryOp::Mod] = "rem";
    mipsName[BinaryOp::Eq] = "seq";
    mipsName[BinaryOp::Less] = "slt";
    mipsName[BinaryOp::And] = "and";
    mipsName[BinaryOp::Or] = "or";

   regs[zero] = (RegContents){false, NULL, "$zero", false, false, false};
   regs[at] = (RegContents){false, NULL, "$at", false, false, false};
   regs[v0] = (RegContents){false, NULL, "$v0", false, false, false};
   regs[v1] = (RegContents){false, NULL, "$v1", false, false, false};
   regs[a0] = (RegContents){false, NULL, "$a0", false, false, false};
   regs[a1] = (RegContents){false, NULL, "$a1", false, false, false};
   regs[a2] = (RegContents){false, NULL, "$a2", false, false, false};
   regs[a3] = (RegContents){false, NULL, "$a3", false, false, false};
   regs[k0] = (RegContents){false, NULL, "$k0", false, false, false};
   regs[k1] = (RegContents){false, NULL, "$k1", false, false, false};
   regs[gp] = (RegContents){false, NULL, "$gp", false, false, false};
   regs[sp] = (RegContents){false, NULL, "$sp", false, false, false};
   regs[fp] = (RegContents){false, NULL, "$fp", false, false, false};
   regs[ra] = (RegContents){false, NULL, "$ra", false, false, false};

   regs[t0] = (RegContents){false, NULL, "$t0", true, false, false};
   regs[t1] = (RegContents){false, NULL, "$t1", true, false, false};
   regs[t2] = (RegContents){false, NULL, "$t2", true, false, false};
   regs[t3] = (RegContents){false, NULL, "$t3", true, false, false};
   regs[t4] = (RegContents){false, NULL, "$t4", true, false, false};
   regs[t5] = (RegContents){false, NULL, "$t5", true, false, false};
   regs[t6] = (RegContents){false, NULL, "$t6", true, false, false};
   regs[t7] = (RegContents){false, NULL, "$t7", true, false, false};
   regs[t8] = (RegContents){false, NULL, "$t8", true, false, false};
   regs[t9] = (RegContents){false, NULL, "$t9", true, false, false};
   regs[s0] = (RegContents){false, NULL, "$s0", true, false, false};
   regs[s1] = (RegContents){false, NULL, "$s1", true, false, false};
   regs[s2] = (RegContents){false, NULL, "$s2", true, false, false};
   regs[s3] = (RegContents){false, NULL, "$s3", true, false, false};
   regs[s4] = (RegContents){false, NULL, "$s4", true, false, false};
   regs[s5] = (RegContents){false, NULL, "$s5", true, false, false};
   regs[s6] = (RegContents){false, NULL, "$s6", true, false, false};
   regs[s7] = (RegContents){false, NULL, "$s7", true, false, false};
   //  rs = t, false0; rt = t1; rd = t2;
   
   // floating point registers (for fun)
   regs[f0] = (RegContents){false, NULL, "$f0", true, false, false};
   regs[f1] = (RegContents){false, NULL, "$f1", true, false, false};
   regs[f2] = (RegContents){false, NULL, "$f2", true, false, false};
   regs[f3] = (RegContents){false, NULL, "$f3", true, false, false};
   regs[f4] = (RegContents){false, NULL, "$f4", true, false, false};
   regs[f5] = (RegContents){false, NULL, "$f5", true, false, false};
   regs[f6] = (RegContents){false, NULL, "$f6", true, false, false};
   regs[f7] = (RegContents){false, NULL, "$f7", true, false, false};
   regs[f8] = (RegContents){false, NULL, "$f8", true, false, false};
   regs[f9] = (RegContents){false, NULL, "$f9", true, false, false};
   regs[f10] = (RegContents){false, NULL, "$f10", true, false, false};
   regs[f11] = (RegContents){false, NULL, "$f11", true, false, false};
   regs[f12] = (RegContents){false, NULL, "$f12", true, false, false};
   regs[f13] = (RegContents){false, NULL, "$f13", true, false, false};
   regs[f14] = (RegContents){false, NULL, "$f14", true, false, false};
   regs[f15] = (RegContents){false, NULL, "$f15", true, false, false};
   regs[f16] = (RegContents){false, NULL, "$f16", true, false, false};
   regs[f17] = (RegContents){false, NULL, "$f17", true, false, false};
   regs[f18] = (RegContents){false, NULL, "$f18", true, false, false};
   regs[f19] = (RegContents){false, NULL, "$f19", true, false, false};     
   regs[f20] = (RegContents){false, NULL, "$f20", true, false, false};
   regs[f21] = (RegContents){false, NULL, "$f21", true, false, false};
   regs[f22] = (RegContents){false, NULL, "$f22", true, false, false};
   regs[f23] = (RegContents){false, NULL, "$f23", true, false, false};
   regs[f24] = (RegContents){false, NULL, "$f24", true, false, false};
   regs[f25] = (RegContents){false, NULL, "$f25", true, false, false};
   regs[f26] = (RegContents){false, NULL, "$f26", true, false, false};
   regs[f27] = (RegContents){false, NULL, "$f27", true, false, false};
   regs[f28] = (RegContents){false, NULL, "$f28", true, false, false};
   regs[f29] = (RegContents){false, NULL, "$f29", true, false, false};
   regs[f30] = (RegContents){false, NULL, "$f30", true, false, false};
   regs[f31] = (RegContents){false, NULL, "$f31", true, false, false};
}
const char *Mips::mipsName[BinaryOp::NumOps];


