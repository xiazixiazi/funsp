#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include <vector>
#include <map>
#include <set>
using namespace llvm;

// origion_func
Function *funcO;

// func_a exit_blocks vector
std::vector<BasicBlock *> exit_rb, entry_ra;

// cross edge set
std::set<BasicBlock *> cross_out;
std::set<BasicBlock *> cross_in;
std::map<BasicBlock *, int> cross_in_map;
std::map<BasicBlock *, int> cross_out_map;

struct BlockRegionInfo_meta
{
    char region_id;
    char is_cross_edge; // 0:not cross edge;1:cross edge tail ;2:cross edge head
};
// Analyze the variables that need to be passed in and out in the basic block
struct BlockData
{
    std::vector<Value *> in_values;                  // Variables that need to be passed in from function a
    std::vector<Value *> out_values;                 // Variables that need to be passed back to function a
    std::vector<BasicBlock *> original_successors;   // Original successor blocks (targets in function A)
    std::vector<BasicBlock *> original_predecessors; // Original predecessor blocks (targets in function A)
    BlockRegionInfo_meta region_info;
};

struct RegionAnalysisResult
{
    std::vector<Value *> in_values;           // Input variables
    std::vector<Value *> out_values;          // Output variables
    std::vector<BasicBlock *> entries;        // Entry basic blocks (locations in function A that jump to region b)
    std::map<BasicBlock *, int> entry_id_map; // Mapping of entry blocks to IDs
    std::vector<BasicBlock *> exits;          // Exit blocks sorted by number
    std::map<BasicBlock *, int> exit_map;     // Mapping of exit blocks to numbered IDs

    std::map<BasicBlock *, std::vector<BasicBlock *>> entries_orig_pred; // Predecessors of entry blocks
    std::map<BasicBlock *, std::vector<BasicBlock *>> exits_orig_succ;   // Successors of exit blocks
};

// Define the strategy of region split
enum Stratery
{
    MEAN,    // Default value is 0
    DOMTREE, // Default value is 1
    LOOP     // Default value is 2
};

// Define a set of basic blocks (multiple basic blocks to be migrated)
using BasicBlockSet = std::set<BasicBlock *>;

BasicBlockSet region_global;

RegionAnalysisResult analyzeRegion(const BasicBlockSet region)
{
    RegionAnalysisResult result;
    std::set<Value *> defined_in_region;
    std::set<Value *> var_already_in;
    std::vector<BasicBlock *> temp_orig_pred_succ;

    // Collect entry blocks (locations in function A that jump to the region)
    for (BasicBlock *BB : region)
    {
        BB->print(outs());
        temp_orig_pred_succ.clear();
        for (auto pred : predecessors(BB))
        {
            if (!region.count(pred))
            { // Predecessor is outside the region, so it's an entry point
                temp_orig_pred_succ.push_back(pred);
                if (result.entry_id_map.find(BB) == result.entry_id_map.end())
                {
                    int id = result.entries.size();
                    result.entry_id_map[BB] = id;
                    result.entries.push_back(BB);
                }
            }
        }
        result.entries_orig_pred[BB] = temp_orig_pred_succ;
    }

    // Collect input variables (external dependencies)
    for (BasicBlock *BB : region)
    {
        BB->print(outs());
        for (Instruction &I : *BB)
        {
            I.print(outs());
            for (Use &U : I.operands())
            {
                Value *V = U.get();
                V->print(outs());
                outs() << "\n";
                // if (isa<Constant>(V) || isa<Argument>(V))
                if (isa<Constant>(V) || isa<BasicBlock>(V) || isa<GlobalValue>(V))
                    continue;
                if (auto MD = llvm::dyn_cast<llvm::MetadataAsValue>(V))
                {
                    // llvm::Metadata *metadata = MD->getMetadata();
                    continue;
                    // Handle logic for llvm::Metadata type
                }

                if (!defined_in_region.count(V) && !var_already_in.count(V))
                {
                    result.in_values.push_back(V);
                    var_already_in.insert(V);
                    V->print(outs());
                    outs() << "\n";
                }
            }
            // Collect defined variables
            if (!I.getType()->isVoidTy())
            {
                defined_in_region.insert(&I);
                // result.out_values.push_back(&I);
                I.print(outs());
            }
        }
    }

    // Analyze exit blocks (blocks that jump to function A)
    for (BasicBlock *BB : region)
    {
        temp_orig_pred_succ.clear();
        Instruction *term = BB->getTerminator();
        for (unsigned i = 0; i < term->getNumSuccessors(); i++)
        {
            BasicBlock *succ = term->getSuccessor(i);
            if (!region.count(succ))
            { // Successor is not in the region, so it's an exit
                temp_orig_pred_succ.push_back(succ);
                if (result.exit_map.find(succ) == result.exit_map.end())
                {
                    int exit_id = result.exits.size();
                    result.exit_map[succ] = exit_id;
                    result.exits.push_back(succ);
                }
            }
        }
        result.exits_orig_succ[BB] = temp_orig_pred_succ;
    }

    return result;
}

std::map<BasicBlock *, BlockData> bb_info;
int analyzeBlock(BasicBlock *b_t)
{
    BlockData data;
    std::set<Value *> defined;

    // Collect defined variables and external dependencies
    for (Instruction &I : *b_t)
    {
        // Collect used variables (excluding constants and arguments)
        for (Use &U : I.operands())
        {
            Value *V = U.get();
            if (isa<Constant>(V) || isa<Argument>(V))
                continue;
            if (!defined.count(V))
            {
                data.in_values.push_back(V);
            }
        }
        // Collect defined variables
        if (!I.getType()->isVoidTy())
        {
            defined.insert(&I);
            // data.out_values.push_back(&I);
        }
    }

    // Record original successor blocks (targets in function A)
    BranchInst *term = dyn_cast<BranchInst>(b_t->getTerminator());
    if (term)
    {
        for (unsigned i = 0; i < term->getNumSuccessors(); i++)
        {
            data.original_successors.push_back(term->getSuccessor(i));
        }
    }

    // Note the original predecessors
    for (pred_iterator PI = pred_begin(b_t), PE = pred_end(b_t); PI != PE; ++PI)
    {
        data.original_predecessors.push_back(*PI);
    }

    bb_info[b_t] = data;
    return 0;
}

int analyzeBlock(BasicBlock *bb, BlockRegionInfo_meta *bb_region_info)
{
    analyzeBlock(bb);
    bb_info[bb].region_info = *bb_region_info;
    return 0;
}

// Create the struct parameter type for function b
StructType *createStructType(LLVMContext &Context, const BlockData &data)
{
    std::vector<Type *> types;
    for (Value *V : data.in_values)
        types.push_back(V->getType());
    for (Value *V : data.out_values)
        types.push_back(V->getType());
    return StructType::create(Context, types, "block_data_t");
}

// Sort the migrated blocks according to their order in the original function
std::vector<BasicBlock *> getOrderedBlocks(Function *func_o, const BasicBlockSet region)
{
    std::vector<BasicBlock *> ordered;
    for (BasicBlock &BB : *func_o)
    { // Assuming funcA is the original function
        if (region.count(&BB))
        {
            ordered.push_back(&BB);
        }
    }
    return ordered;
}

// Create new function b and migrate basic blocks
Function *createFunctionB(Function *func_o, const BasicBlockSet region, StructType *structTy_ptr, int flag_in, RegionAnalysisResult &result)
{
    if (region.empty())
    {
        return nullptr;
    }

    LLVMContext &Context = func_o->getContext();
    Module *M = func_o->getParent();

    // Get the function name
    std::string func_b_name = func_o->getName().str().append("_splitFlag");
#if 1
    outs() << func_o->getName().str() << "\n";
    errs() << func_b_name << "\n";

    // Function type: parameters are struct pointer + entry_id
    FunctionType *funcTy = nullptr;
    if (structTy_ptr)
    {
        if (flag_in)
        {
            funcTy = FunctionType::get(
                Type::getInt32Ty(Context),
                {structTy_ptr->getPointerTo(), Type::getInt32Ty(Context)}, // Add entry_id parameter
                false);
        }
        else
        {
            funcTy = FunctionType::get(
                Type::getInt32Ty(Context),
                {structTy_ptr->getPointerTo()}, // Add entry_id parameter
                false);
        }
    }
    else
    {
        if (flag_in)
        {
            funcTy = FunctionType::get(
                Type::getInt32Ty(Context),
                {Type::getInt32Ty(Context)}, // Add entry_id parameter
                false);
        }
        else
        {
            funcTy = FunctionType::get(
                Type::getInt32Ty(Context),
                {}, // Add entry_id parameter
                false);
        }
    }

    Function *funcB = Function::Create(funcTy, Function::InternalLinkage, func_b_name, M);

    // Create entry and exit blocks
    BasicBlock *entry = BasicBlock::Create(Context, "entry", funcB);
    BasicBlock *exit_block = BasicBlock::Create(Context, "exit", funcB);
    IRBuilder<> Builder(entry);

    // Add flag local var, which type is i32
    // Get 32-bit integer type
    Type *Int32Ty = Type::getInt32Ty(Context);
    // Create alloca instruction, allocate memory space for a 32-bit integer type, named myInt
    Value *flagPtr = Builder.CreateAlloca(Int32Ty, nullptr, "flag_out");

    // Create the entry switch function
    std::map<Value *, Value *> oldToNew;
    if (structTy_ptr)
    {
        // Load input variables from the struct
        Value *structPtr = &*(funcB->arg_begin());
        for (unsigned i = 0; i < result.in_values.size(); i++)
        {
            Value *GEP = Builder.CreateStructGEP(structTy_ptr, structPtr, i);
            result.in_values[i]->print(outs());
            outs() << "\n";
            Value *loaded = Builder.CreateLoad(result.in_values[i]->getType(), GEP);
            oldToNew[result.in_values[i]] = loaded;
        }
        if (flag_in)
        {
            Value *entry_id = funcB->arg_begin() + 1; // The second parameter is entry_id

            // Create a case for each entry point
            SwitchInst *entrySwitch = Builder.CreateSwitch(entry_id, result.entries[0], result.entries.size());
            for (int i = 0; i < result.entries.size(); i++)
            {
                BasicBlock *target = result.entries[i];
                entrySwitch->addCase(ConstantInt::get(Type::getInt32Ty(Context), i), target);
            }
        }
        else
        {
            Builder.CreateBr(result.entries[0]);
        }
    }
    else
    {

        if (flag_in)
        {
            Value *entry_id = funcB->arg_begin(); // The second parameter is entry_id

            // Create a case for each entry point
            SwitchInst *entrySwitch = Builder.CreateSwitch(entry_id, result.entries[0], result.entries.size());
            for (int i = 0; i < result.entries.size(); i++)
            {
                BasicBlock *target = result.entries[i];
                entrySwitch->addCase(ConstantInt::get(Type::getInt32Ty(Context), i), target);
            }
        }
        else
        {
            Builder.CreateBr(result.entries[0]);
        }
    }

    // Insert migrated blocks in the order of the original function
    std::vector<BasicBlock *> ordered_region = getOrderedBlocks(func_o, region); // Assuming they are sorted
    for (BasicBlock *BB : ordered_region)
    {
        BB->removeFromParent();
        BB->insertInto(funcB);
        if (!oldToNew.empty())
        {
            for (Instruction &I : *BB)
            {
                outs() << I << "\n";
                for (unsigned i = 0; i < I.getNumOperands(); i++)
                {
                    Value *V = I.getOperand(i);
                    if (oldToNew.count(V))
                    {
                        I.setOperand(i, oldToNew[V]);
                    }
                }
            }
        }
    }

    // Handle branch logic of migrated blocks (redirect to exit)
    for (BasicBlock *BB : ordered_region)
    {
        Instruction *term = BB->getTerminator();
        // If the terminator instruction of the basic block is unreachable, the entire basic block is unreachable
        if (isa<UnreachableInst>(term))
        {
            IRBuilder<> tempBuilder(term);
            tempBuilder.SetInsertPoint(term);
            tempBuilder.CreateBr(exit_block);
            term->removeFromParent();
        }
        else
        {
            for (unsigned i = 0; i < term->getNumSuccessors(); i++)
            {
                BasicBlock *succ = term->getSuccessor(i);
                if (!region.count(succ))
                { // Jump outside the region, redirect to exit
                    // Create a temporary block to set the return value
                    BasicBlock *temp = BasicBlock::Create(Context, "temp" + BB->getName().str(), funcB);
                    term->setSuccessor(i, temp);

                    IRBuilder<> tempBuilder(temp);
                    tempBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), result.exit_map[succ]), flagPtr);
                    tempBuilder.CreateBr(exit_block);
                }
            }
        }
    }

    // Exit block returns
    Builder.SetInsertPoint(exit_block);
    Value *retVal = Builder.CreateLoad(Type::getInt32Ty(Context), flagPtr);
    Builder.CreateRet(retVal);

    funcB->print(outs());

    verifyFunction(*funcB);
    return funcB;

#endif
}

Function *createFunctionB_v1(Module *M, BasicBlock *moved_bb, StructType *structTy)
{
    LLVMContext &Context = M->getContext();
    FunctionType *funcTy = FunctionType::get(
        Type::getInt32Ty(Context), {structTy->getPointerTo()}, false);
    Function *funcB = Function::Create(funcTy, Function::InternalLinkage, "funcB", M);

    // Create entry and exit blocks
    BasicBlock *entry = BasicBlock::Create(Context, "entry", funcB);
    BasicBlock *exit = BasicBlock::Create(Context, "exit", funcB);

    // Move b_t to function B
    moved_bb->removeFromParent();
    moved_bb->insertInto(funcB);

    IRBuilder<> Builder(entry);
    Value *structPtr = funcB->arg_begin();

    // Add flag local var, which type is i32
    // Get 32-bit integer type
    Type *Int32Ty = Type::getInt32Ty(Context);
    // Create alloca instruction, allocate memory space for a 32-bit integer type, named myInt
    Value *flagPtr = Builder.CreateAlloca(Int32Ty, nullptr, "myInt");

    // Load input variables from the struct and replace references in b_t
    BlockData data = bb_info[moved_bb];
    std::map<Value *, Value *> oldToNew;
    for (unsigned i = 0; i < data.in_values.size(); i++)
    {
        Value *GEP = Builder.CreateStructGEP(structTy, structPtr, i);
        Value *Loaded = Builder.CreateLoad(data.in_values[i]->getType(), GEP);
        oldToNew[data.in_values[i]] = Loaded;
    }

    // Replace variable references in b_t
    for (Instruction &I : *moved_bb)
    {
        for (unsigned i = 0; i < I.getNumOperands(); i++)
        {
            Value *V = I.getOperand(i);
            if (oldToNew.count(V))
            {
                I.setOperand(i, oldToNew[V]);
            }
        }
    }

    // Handle branches in b_t: insert empty blocks before each successor to set the return value
    BranchInst *br = dyn_cast<BranchInst>(moved_bb->getTerminator());
    if (br && br->isConditional())
    {
        // Create two empty blocks to set return values 0 and 1
        BasicBlock *succ0 = BasicBlock::Create(Context, "succ0", funcB);
        BasicBlock *succ1 = BasicBlock::Create(Context, "succ1", funcB);

        // Redirect original branches
        br->setSuccessor(0, succ0);
        br->setSuccessor(1, succ1);

        // Set return values in empty blocks and jump to exit
        IRBuilder<> Builder0(succ0);
        Builder0.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 0), flagPtr);
        Builder0.CreateBr(exit);

        IRBuilder<> Builder1(succ1);
        Builder1.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 1), flagPtr);
        Builder1.CreateBr(exit);
    }

    // Exit block loads the return value and returns
    Builder.SetInsertPoint(exit);
    Value *retVal = Builder.CreateLoad(Type::getInt32Ty(Context), flagPtr);
    Builder.CreateRet(retVal);

    return funcB;
}

// Modify the original function a to create proxy logic
void modifyFunctionA(Function *funcA, const BasicBlockSet region, Function *funcB, StructType *structTy, RegionAnalysisResult &result)
{
    if (region.empty())
    {
        return;
    }
    LLVMContext &Context = funcA->getContext();
    IRBuilder<> Builder(funcA->getEntryBlock().getTerminator());
    Builder.SetInsertPoint(funcA->getEntryBlock().getTerminator());

    // Add flag local var, which type is i32
    // Get 32-bit integer type
    Type *Int32Ty_funcA = Type::getInt32Ty(Context);
    // Create alloca instruction, allocate memory space for a 32-bit integer type, named myInt
    Value *flagPtr = Builder.CreateAlloca(Int32Ty_funcA, nullptr, "flag_in");

    // Create proxy block
    BasicBlock *proxy = BasicBlock::Create(Context, "proxy", funcA);

    // Create the switch default label
    // BasicBlock *switch_default_label = BasicBlock::Create(Context, "switch_default", funcA);
    // Builder.SetInsertPoint(switch_default_label);

    // Create proxy blocks for each entry point
    for (BasicBlock *entryBB : result.entries)
    {
        BasicBlock *proxy_flag = BasicBlock::Create(Context, "proxy_" + entryBB->getName(), funcA);

        // Redirect predecessors to proxy block
        // for (auto pred : predecessors(entryBB))
        for (auto pred : result.entries_orig_pred[entryBB])
        {
            if (!region.count(pred))
            {
                Instruction *term = pred->getTerminator();
                term->print(outs());
                outs() << "\n";
                for (unsigned i = 0; i < term->getNumSuccessors(); i++)
                {
                    if (term->getSuccessor(i) == entryBB)
                    {
                        term->setSuccessor(i, proxy_flag);
                    }
                }
            }
        }

        // Set the flag for calling function b
        IRBuilder<> Builder(proxy_flag);

        Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), result.entry_id_map[entryBB]), flagPtr);
        Builder.CreateBr(proxy);
    }

    Value *retCode = nullptr;
    if (structTy)
    {
        // Allocate struct in proxy block and fill in inputs
        Builder.SetInsertPoint(&funcA->getEntryBlock(), funcA->getEntryBlock().begin());
        Value *structAlloca = Builder.CreateAlloca(structTy);

        Builder.SetInsertPoint(proxy);
        for (unsigned i = 0; i < result.in_values.size(); i++)
        {
            Value *GEP = Builder.CreateStructGEP(structTy, structAlloca, i);
            Builder.CreateStore(result.in_values[i], GEP);
        }

        // Call function B and get the return value
        if (result.entries.size() > 1)
        {
            Value *flag = Builder.CreateLoad(flagPtr);
            retCode = Builder.CreateCall(funcB, {structAlloca, flag});
        }
        else
        {
            retCode = Builder.CreateCall(funcB, {structAlloca});
        }
    }
    else
    {
        Builder.SetInsertPoint(proxy);
        if (result.entries.size() > 1)
        {
            Value *flag = Builder.CreateLoad(flagPtr);
            retCode = Builder.CreateCall(funcB, {flag});
        }
        else
        {
            retCode = Builder.CreateCall(funcB, {});
            retCode->print(outs());
            outs() << "\n";
        }
    }

    Builder.SetInsertPoint(proxy);
    if (result.exits.size() != 0)
    {
        // Create Switch to jump to the corresponding exit
        SwitchInst *switchInst = Builder.CreateSwitch(
            retCode,
            result.exits[0], // Default to the first exit
            result.exits.size());
        for (unsigned i = 0; i < result.exits.size(); i++)
        {
            switchInst->addCase(ConstantInt::get(Type::getInt32Ty(Context), i), result.exits[i]);
        }
    }
    else
    {
        for (BasicBlock &bb : *funcA)
        {
            if (isa<ReturnInst>(bb.getTerminator()))
            {
                Builder.CreateBr(&bb);
                break;
            }
        }
    }

    funcA->print(outs());
    funcB->print(outs());
}

void modifyFunctionA_v1(Function *funcA, BasicBlock *moved_bb, Function *funcB, StructType *structTy, BlockData &data)
{
    LLVMContext &Context = funcA->getContext();
    IRBuilder<> Builder(funcA->getEntryBlock().getTerminator());

    // Create proxy basic block
    BasicBlock *proxy_bb = BasicBlock::Create(Context, "proxy", funcA);

    // If the bb is a cross tail
    if (data.region_info.is_cross_edge == 2)
    {
        // Redirect all predecessors jumping to b_t to proxy
        for (auto pred : data.original_predecessors)
        {
            Instruction *term = pred->getTerminator();
            for (unsigned i = 0; i < term->getNumSuccessors(); i++)
            {
                if (term->getSuccessor(i) == moved_bb)
                {
                    term->setSuccessor(i, proxy_bb);
                }
            }
        }
    }

    // Allocate struct in proxy block and fill in data
    Builder.SetInsertPoint(proxy_bb);
    Value *structAlloca = Builder.CreateAlloca(structTy);

    // Fill in input variables
    for (unsigned i = 0; i < data.in_values.size(); i++)
    {
        Value *GEP = Builder.CreateStructGEP(structTy, structAlloca, i);
        Builder.CreateStore(data.in_values[i], GEP);
    }

    // Call function B
    Value *retCode = Builder.CreateCall(funcB, {structAlloca});

    // Load output variables and replace original uses
    for (unsigned i = 0; i < data.out_values.size(); i++)
    {
        Value *GEP = Builder.CreateStructGEP(structTy, structAlloca, data.in_values.size() + i);
        Value *loaded = Builder.CreateLoad(data.out_values[i]->getType(), GEP);
        data.out_values[i]->replaceAllUsesWith(loaded);
    }

    // Get the branch instruction of b_t (assuming it's BranchInst)
    BranchInst *b_tTerminator = dyn_cast<BranchInst>(moved_bb->getTerminator());
    if (!b_tTerminator)
    {
        // Handle error: b_t does not have a branch instruction
        return;
    }

    // Use terminator method to get successors
    BasicBlock *defaultSuccessor = b_tTerminator->getSuccessor(0); // Default jump to the first successor
    unsigned numSuccessors = b_tTerminator->getNumSuccessors();    // Number of branches

    // Jump to original successor based on return value
    BranchInst *br = dyn_cast<BranchInst>(moved_bb->getTerminator());
    SwitchInst *switchInst = Builder.CreateSwitch(retCode, /* default */ br->getSuccessor(0), br->getNumSuccessors());
    for (unsigned i = 0; i < br->getNumSuccessors(); i++)
    {
        switchInst->addCase(ConstantInt::get(Type::getInt32Ty(Context), i), br->getSuccessor(i));
    }

    // Delete the original b_t block
    moved_bb->eraseFromParent();
}

// Check if the function contains inline assembly instructions
bool hasInlineAssembly(llvm::Function &F)
{
    for (auto &BB : F)
    {
        for (auto &I : BB)
        {
            if (llvm::isa<llvm::CallInst>(I))
            {
                llvm::CallInst *CI = llvm::cast<llvm::CallInst>(&I);
                // Use getCalledOperand instead of getCalledValue
                llvm::Value *CalledOperand = CI->getCalledOperand();
                if (llvm::isa<llvm::InlineAsm>(CalledOperand))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

namespace llvm
{
    void fixStack(Function &F);
}
void llvm::fixStack(Function &F)
{
    std::vector<PHINode *> origPHI;
    std::vector<Instruction *> origReg;
    BasicBlock &entryBB = F.getEntryBlock();
    for (BasicBlock &BB : F)
    {
        for (Instruction &I : BB)
        {
            if (PHINode *PN = dyn_cast<PHINode>(&I))
            {
                origPHI.push_back(PN);
            }
            else if (!(isa<AllocaInst>(&I) && I.getParent() == &entryBB) && I.isUsedOutsideOfBlock(&BB))
            {
                origReg.push_back(&I);
            }
        }
    }
    for (PHINode *PN : origPHI)
    {
        DemotePHIToStack(PN, entryBB.getTerminator());
    }
    for (Instruction *I : origReg)
    {
        DemoteRegToStack(*I, entryBB.getTerminator());
    }
}

std::string removeFileExtension(const std::string &filename)
{
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos)
    {
        return filename.substr(0, lastDot);
    }
    return filename;
}

// Create a region by strategy
int create_region(Function *func_ptr, Stratery stratery, BasicBlockSet *region_ptr)
{
    std::vector<BasicBlock *> vec_bb_temp;
    region_global.clear();
    switch (stratery)
    {

    case MEAN:
        for (BasicBlock &bb : *func_ptr)
        {
            vec_bb_temp.push_back(&bb);
        }
        for (int i = vec_bb_temp.size() / 2; i < vec_bb_temp.size() - 1; i++)
        {
            if (!isa<ReturnInst>(vec_bb_temp[i]->getTerminator()))
            {
                region_ptr->insert(vec_bb_temp[i]);
            }
        }

        break;

    case DOMTREE:
        break;
    case LOOP:
        break;
    }

    return 0;
}

int func_split_by_region(Function *func_ptr, BasicBlockSet region)
{
    if (region.empty())
    {
        return 1;
    }
    LLVMContext Context;
    // Analyze the region
    RegionAnalysisResult result = analyzeRegion(region);

    StructType *structTy = nullptr;
    if (result.in_values.empty())
    {
    }
    else
    {

        // Create struct type
        std::vector<Type *> structTypes;
        for (Value *V : result.in_values)
            structTypes.push_back(V->getType());
        for (Value *V : result.out_values)
            structTypes.push_back(V->getType());
        std::string str_struct_pre("struct_pass_");
        StringRef str_struct = str_struct_pre.append(func_ptr->getName().str());
        outs() << str_struct.str() << "\n";
        structTy = StructType::create(func_ptr->getContext(), structTypes, str_struct);
    }
    int flag_in = 0;
    if (result.entries.size() > 1)
    {
        flag_in = result.entries.size();
    }

    // Create function B and migrate basic blocks
    Function *funcB = createFunctionB(func_ptr, region, structTy, flag_in, result);

    // Modify function A
    modifyFunctionA(func_ptr, region, funcB, structTy, result);
    func_ptr->print(outs());
    funcB->print(outs());

    // Verify and output
    verifyFunction(*func_ptr);
    verifyFunction(*funcB);

    return 0;
}

#if 1
// Define command-line parameters
static cl::opt<std::string> MyParameter(
    "my-parameter",                            // Parameter name
    cl::desc("A custom parameter for MyPass"), // Parameter description
    cl::value_desc("string"),                  // Parameter value type
    cl::init("default-value")                  // Default value
);

namespace
{
    // Define Pass
    struct MyPass : public FunctionPass
    {
        static char ID; // Pass identifier
        MyPass() : FunctionPass(ID) {}

        // Override runOnFunction method to define Pass logic
        bool runOnFunction(Function &F) override
        {
            outs() << F.getName().str() << "\n";
            StringRef str_func_name = F.getName();

            // bool a = str_func_name.contains("_splitabc");
            // Check if the function has already been processed
            if (F.getName().contains("_splitFlag"))
            {
                return false; // If already processed, return false directly
            }

            // Check if it has inline assembly
            if (hasInlineAssembly(F))
            {
                return false; // If it has inline assembly, return false directly
            }

            // 1. Create Pass Manager
            legacy::FunctionPassManager FPM(F.getParent());

            // 2. Add mergereturn Pass
            FPM.add(createUnifyFunctionExitNodesPass());

            // 3. Run mergereturn Pass
            FPM.run(F);

            // Repair evasion variable and phi node
            fixStack(F);
            // Note: repair the phi first and the phi result is used in other block
            fixStack(F);

            errs() << "MyPass is running on function: " << F.getName() << "\n";
            errs() << "MyParameter value: " << MyParameter << "\n";

            region_global.clear();
            create_region(&F, MEAN, &region_global);
            int hold = func_split_by_region(&F, region_global);

            if (hold)
            {
                return false;
            }
            else
            {
                return true; // Return false to indicate no modification to the function
            }
        }
    };
}

char MyPass::ID = 0; // Initialize Pass identifier

// Register Pass
static RegisterPass<MyPass> X("func_split", "func_split pass");

#endif
