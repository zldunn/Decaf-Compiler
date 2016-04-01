/* File: codegen.cc
 * ----------------
 * Implementation for the CodeGenerator class. The methods don't do anything
 * too fancy, mostly just create objects of the various Tac instruction
 * classes and append them to the list.
 */

#include "codegen.h"
#include <string.h>
#include "tac.h"
#include "mips.h"
#include "ast_decl.h"
#include "errors.h"

Location* CodeGenerator::ThisPtr= new Location(fpRelative, 4, "this");

CodeGenerator::CodeGenerator()
{
    curGlobalOffset = 0;
}

char *CodeGenerator::NewLabel()
{
    static int nextLabelNum = 0;
    char temp[10];
    sprintf(temp, "_L%d", nextLabelNum++);
    return strdup(temp);
}


Location *CodeGenerator::GenTempVar()
{
    static int nextTempNum;
    char temp[10];
    Location *result = NULL;
    sprintf(temp, "_tmp%d", nextTempNum++);
    return GenLocalVariable(temp);
}


Location *CodeGenerator::GenLocalVariable(const char *varName)
{
    curStackOffset -= VarSize;
    return new Location(fpRelative, curStackOffset+4,  varName);
}

Location *CodeGenerator::GenGlobalVariable(const char *varName)
{
    curGlobalOffset += VarSize;
    return new Location(gpRelative, curGlobalOffset -4, varName);
}


Location *CodeGenerator::GenLoadConstant(int value)
{
    Location *result = GenTempVar();
    code.push_back(new LoadConstant(result, value));
    return result;
}

Location *CodeGenerator::GenLoadConstant(const char *s)
{
    Location *result = GenTempVar();
    code.push_back(new LoadStringConstant(result, s));
    return result;
}

Location *CodeGenerator::GenLoadLabel(const char *label)
{
    Location *result = GenTempVar();
    code.push_back(new LoadLabel(result, label));
    return result;
}


void CodeGenerator::GenAssign(Location *dst, Location *src)
{
    code.push_back(new Assign(dst, src));
}


Location *CodeGenerator::GenLoad(Location *ref, int offset)
{
    Location *result = GenTempVar();
    code.push_back(new Load(result, ref, offset));
    return result;
}

void CodeGenerator::GenStore(Location *dst,Location *src, int offset)
{
    code.push_back(new Store(dst, src, offset));
}


Location *CodeGenerator::GenBinaryOp(const char *opName, Location *op1,
                                     Location *op2)
{
    Location *result = GenTempVar();
    code.push_back(new BinaryOp(BinaryOp::OpCodeForName(opName), result, op1, op2));
    return result;
}


void CodeGenerator::GenLabel(const char *label)
{
    code.push_back(new Label(label));
}

void CodeGenerator::GenIfZ(Location *test, const char *label)
{
    code.push_back(new IfZ(test, label));
}

void CodeGenerator::GenGoto(const char *label)
{
    code.push_back(new Goto(label));
}

void CodeGenerator::GenReturn(Location *val)
{
    code.push_back(new Return(val));
}


BeginFunc *CodeGenerator::GenBeginFunc(FnDecl *fn)
{
    BeginFunc *result = new BeginFunc;
    code.push_back(insideFn = result);
    List<VarDecl*> *formals = fn->GetFormals();
    int start = OffsetToFirstParam;
    if (fn->IsMethodDecl()) start += VarSize;
    for (int i = 0; i < formals->NumElements(); i++)
        formals->Nth(i)->rtLoc = new Location(fpRelative, i*VarSize + start, formals->Nth(i)->GetName());
    curStackOffset = OffsetToFirstLocal;
    return result;
}

void CodeGenerator::GenEndFunc()
{
    code.push_back(new EndFunc());
    insideFn->SetFrameSize(OffsetToFirstLocal-curStackOffset);
    insideFn = NULL;
}

void CodeGenerator::GenPushParam(Location *param)
{
    code.push_back(new PushParam(param));
}

void CodeGenerator::GenPopParams(int numBytesOfParams)
{
    Assert(numBytesOfParams >= 0 && numBytesOfParams % VarSize == 0); // sanity check
    if (numBytesOfParams > 0)
        code.push_back(new PopParams(numBytesOfParams));
}

Location *CodeGenerator::GenLCall(const char *label, bool fnHasReturnValue)
{
    Location *result = fnHasReturnValue ? GenTempVar() : NULL;
    code.push_back(new LCall(label, result));
    return result;
}

Location *CodeGenerator::GenFunctionCall(const char *fnLabel, List<Location*> *args, bool hasReturnValue)
{
    for (int i = args->NumElements()-1; i >= 0; i--) // push params right to left
        GenPushParam(args->Nth(i));
    Location *result = GenLCall(fnLabel, hasReturnValue);
    GenPopParams(args->NumElements()*VarSize);
    return result;
}

Location *CodeGenerator::GenACall(Location *fnAddr, bool fnHasReturnValue)
{
    Location *result = fnHasReturnValue ? GenTempVar() : NULL;
    code.push_back(new ACall(fnAddr, result));
    return result;
}

Location *CodeGenerator::GenMethodCall(Location *rcvr,
                                       Location *meth, List<Location*> *args, bool fnHasReturnValue)
{
    for (int i = args->NumElements()-1; i >= 0; i--)
        GenPushParam(args->Nth(i));
    GenPushParam(rcvr);	// hidden "this" parameter
    Location *result= GenACall(meth, fnHasReturnValue);
    GenPopParams((args->NumElements()+1)*VarSize);
    return result;
}


static struct _builtin {
    const char *label;
    int numArgs;
    bool hasReturn;
} builtins[] =
{   {"_Alloc", 1, true},
    {"_ReadLine", 0, true},
    {"_ReadInteger", 0, true},
    {"_StringEqual", 2, true},
    {"_PrintInt", 1, false},
    {"_PrintString", 1, false},
    {"_PrintBool", 1, false},
    {"_Halt", 0, false}
};

Location *CodeGenerator::GenBuiltInCall(BuiltIn bn,Location *arg1, Location *arg2)
{
    Assert(bn >= 0 && bn < NumBuiltIns);
    struct _builtin *b = &builtins[bn];
    Location *result = NULL;

    if (b->hasReturn) result = GenTempVar();
    // verify appropriate number of non-NULL arguments given
    Assert((b->numArgs == 0 && !arg1 && !arg2)
           || (b->numArgs == 1 && arg1 && !arg2)
           || (b->numArgs == 2 && arg1 && arg2));
    if (arg2) code.push_back(new PushParam(arg2));
    if (arg1) code.push_back(new PushParam(arg1));
    code.push_back(new LCall(b->label, result));
    GenPopParams(VarSize*b->numArgs);
    return result;
}


void CodeGenerator::GenVTable(const char *className, List<const char *> *methodLabels)
{
    code.push_back(new VTable(className, methodLabels));
}


void CodeGenerator::DoFinalCodeGen()
{
    if (IsDebugOn("tac")) { // if debug don't translate to mips, just print Tac
        std::list<Instruction*>::iterator p;
        for (p= code.begin(); p != code.end(); ++p) {
            (*p)->Print();
        }
        return;
    }  
    Mips mips;
    mips.EmitPreamble();

    std::list<Instruction*>::iterator p= code.begin(),
                                      begin_block= code.end(),
                                      end_block= code.end();

    int block_count = 0;
    Block current_block;
    std::list<ControlFlowGraph> cfg_list;
    std::list<ControlFlowGraph>::iterator cfg_list_iter;
    while (p != code.end())
    {
        // Watch for beginning of function
        if (dynamic_cast<BeginFunc*>(*p))
        {
            //begin_block = --p;
            begin_block = p;
            current_block.push_back((*begin_block));
            //current_block.push_back((*p));

            /*
            do ++p;
            while (p != code.end() && !dynamic_cast<EndFunc*>(*p));
            */
            while (p != code.end()) 
            {
                if (dynamic_cast<EndFunc*>(*p)) {
                    break;
                }
                ++p;
            }
            if (p == code.end())
                p= begin_block;
            else
            {
                end_block= p;
                current_block.push_back((*end_block)); 
                Block::iterator b_iter = current_block.begin();
                /*
                for (p= begin_block; p != end_block; ++p) {
                    (*p)->Print();
                }
                */

                ControlFlowGraph cfg(begin_block, end_block);
                cfg_list.push_back(cfg);
                current_block.clear();
                block_count++;

                //ControlFlowGraph cfg(begin_block, end_block);
                // TODO (phase 2): use CFG for liveness analysis

                // Now go back and print out the instructions
                for (p= begin_block; p != end_block; ++p) {
                    //(*p)->Emit(&mips);
                    //(*p)->Print();
                }
            }
        }

        //(*p)->Emit(&mips);
        //(*p)->Print();
        current_block.push_back((*p));
        ++p;
    }

    //printf("Size of cfg_list: %d\n", cfg_list.size());

    std::list<std::map<Location*, Instruction*> > liveness_list;
    std::list<std::map<Location*, Instruction*> >::iterator liveness_list_iter;

    std::map<Location*, Instruction*> variable_list;
    std::map<Location*, Instruction*>::iterator variable_list_iter;
    for (cfg_list_iter = cfg_list.begin(); cfg_list_iter != cfg_list.end(); cfg_list_iter++) {
        //printf("NEW FUNCTION\n");
        variable_list.clear(); // clear for each cfg.
        ControlFlowGraph::ForwardFlow flow((*cfg_list_iter));
        ControlFlowGraph::ForwardFlow::iterator cfg_iter = flow.first();
        /*
        ControlFlowGraph::ReverseFlow flow((*cfg_list_iter));
        ControlFlowGraph::ReverseFlow::iterator cfg_iter = flow.first();
        */
        for (cfg_iter; cfg_iter != flow.last(); cfg_iter++) {
            //(*cfg_iter)->Print();
            //(*cfg_iter)->Emit(&mips);
            int numVars = (*cfg_iter)->numVars;
            if(numVars == 1 || numVars == 2 || numVars == 3) {
                variable_list_iter = variable_list.find((*cfg_iter)->varA);
                if (variable_list_iter == variable_list.end()) {
                }
                else {
                    variable_list.erase(variable_list_iter);
                }
                if (strstr((*cfg_iter)->varA->GetName(), "_tmp") != NULL) {
                    variable_list.insert(std::pair<Location*, Instruction*>((*cfg_iter)->varA,(*cfg_iter)));
                }
            }
            if(numVars == 2 || numVars == 3) {
                variable_list_iter = variable_list.find((*cfg_iter)->varB);
                if (variable_list_iter == variable_list.end()) {
                }
                else {
                    variable_list.erase(variable_list_iter);
                }
                if (strstr((*cfg_iter)->varB->GetName(), "_tmp") != NULL) {
                    variable_list.insert(std::pair<Location*, Instruction*>((*cfg_iter)->varB,(*cfg_iter)));
                }
            }
            if(numVars == 3) {
                variable_list_iter = variable_list.find((*cfg_iter)->varC);
                if (variable_list_iter == variable_list.end()) {
                }
                else {
                    variable_list.erase(variable_list_iter);
                }
                if (strstr((*cfg_iter)->varC->GetName(), "_tmp") != NULL) {
                    variable_list.insert(std::pair<Location*, Instruction*>((*cfg_iter)->varC,(*cfg_iter)));
                }
            }
            /*
            if (flow.in().find((*cfg_iter)) != flow.in().end()) {
                printf("  To: ");
                ((flow.in().find((*cfg_iter)))->first)->Print();
            }
            if (flow.out().find((*cfg_iter)) != flow.out().end()) {
                printf("From: ");
                ((flow.out().find((*cfg_iter)))->first)->Print();
            }
            */
        }
        liveness_list.push_back(variable_list);
        for (variable_list_iter = variable_list.begin(); variable_list_iter != variable_list.end(); variable_list_iter++) {
            /*
            Instruction* discard_instr = new DiscardValue(variable_list_iter->first);
            discard_instr->EmitSpecific(&mips);
            */
            /*
            // print last use of variable *in this function*
            printf("Last use of variable %s: ", variable_list_iter->first->GetName());
            (variable_list_iter->second)->Print();
            */
        }
    }

    /*
    if (block_count != liveness_list.size()) {
        printf("Number of blocks: %d, Number of liveness data sets: %d\n", block_count, liveness_list.size());
        return;
    }
    */

    block_count = 0;
    p = code.begin();
    while (p != code.end())
    {
        // Watch for beginning of function
        if (dynamic_cast<BeginFunc*>(*p))
        {
            begin_block = p;
            while (p != code.end()) 
            {
                if (dynamic_cast<EndFunc*>(*p)) {
                    break;
                }
                ++p;
            }
            if (p == code.end())
                p= begin_block;
            else
            {
                end_block= p;
                block_count++;
                if (block_count == 1) {
                    liveness_list_iter = liveness_list.begin();
                }
                else {
                    liveness_list_iter++;
                }
                for (p= begin_block; p != end_block; ++p) {
                    (*p)->Emit(&mips);
                    for (variable_list_iter = (*liveness_list_iter).begin(); variable_list_iter != (*liveness_list_iter).end(); variable_list_iter++) {
                        if ((*p) == (*variable_list_iter).second) {
                            Instruction* discard_instr = new DiscardValue(variable_list_iter->first);
                            discard_instr->EmitSpecific(&mips);
                            /*
                            (*p)->Print();
                            (*variable_list_iter).second->Print();
                            */
                        }
                        /*
                        if (&(*variable_list_iter) == (*p)) {
                            (*p)->Emit(&mips);
                        }
                        */
                    }
                }
            }
        }
        (*p)->Emit(&mips);
        ++p;
    }
}



Location *CodeGenerator::GenArrayLen(Location *array)
{
    return GenLoad(array, -4);
}

Location *CodeGenerator::GenNew(const char *vTableLabel, int instanceSize)
{
    Location *size = GenLoadConstant(instanceSize);
    Location *result = GenBuiltInCall(Alloc, size);
    Location *vt = GenLoadLabel(vTableLabel);
    GenStore(result, vt);
    return result;
}


Location *CodeGenerator::GenDynamicDispatch(Location *rcvr, int vtableOffset, List<Location*> *args, bool hasReturnValue)
{
    Location *vptr = GenLoad(rcvr); // load vptr
    Assert(vtableOffset >= 0);
    Location *m = GenLoad(vptr, vtableOffset*4);
    return GenMethodCall(rcvr, m, args, hasReturnValue);
}

// all variables (ints, bools, ptrs, arrays) are 4 bytes in for code generation
// so this simplifies the math for offsets
Location *CodeGenerator::GenSubscript(Location *array, Location *index)
{
    Location *zero = GenLoadConstant(0);
    Location *isNegative = GenBinaryOp("<", index, zero);
    Location *count = GenLoad(array, -4);
    Location *isWithinRange = GenBinaryOp("<", index, count);
    Location *pastEnd = GenBinaryOp("==", isWithinRange, zero);
    Location *outOfRange = GenBinaryOp("||", isNegative, pastEnd);
    const char *pastError = NewLabel();
    GenIfZ(outOfRange, pastError);
    GenHaltWithMessage(err_arr_out_of_bounds);
    GenLabel(pastError);
    Location *four = GenLoadConstant(VarSize);
    Location *offset = GenBinaryOp("*", four, index);
    Location *elem = GenBinaryOp("+", array, offset);
    return new Location(elem, 0);
}



Location *CodeGenerator::GenNewArray(Location *numElems)
{
    Location *zero = GenLoadConstant(0);
    Location *isNegative = GenBinaryOp("<", numElems, zero);
    const char *pastError = NewLabel();
    GenIfZ(isNegative, pastError);
    GenHaltWithMessage(err_arr_bad_size);
    GenLabel(pastError);

    // need (numElems+1)*VarSize total bytes (extra 1 is for length)
    Location *arraySize = GenLoadConstant(1);
    Location *num = GenBinaryOp("+", arraySize, numElems);
    Location *four = GenLoadConstant(VarSize);
    Location *bytes = GenBinaryOp("*", num, four);
    Location *result = GenBuiltInCall(Alloc, bytes);
    GenStore(result, numElems);
    return GenBinaryOp("+", result, four);
}


void CodeGenerator::GenHaltWithMessage(const char *message)
{
    Location *msg = GenLoadConstant(message);
    GenBuiltInCall(PrintString, msg);
    GenBuiltInCall(Halt, NULL);
}



