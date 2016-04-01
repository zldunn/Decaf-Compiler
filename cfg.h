#ifndef _H_CFG
#define _H_CFG

#include <list>
#include <map>
#include <string>
#include <iostream>
#include <vector>

class Instruction;
typedef std::vector<Instruction*> EdgeList;
typedef std::list<Instruction*> Block;

/*==========================================================
 * ControlFlowGraph
 * ----------------
 * Represents control flow graph for a single function.
 * Assumes we're creating a CFG based on list<Instruction*>
 * found in CodeGen class.
 * Creates a map of edges between individual instructions.
 * Accessed using a flow class.
 */
class ControlFlowGraph
{
public:
  // based on CodeGen implementation of list-of-instruction-pointers
  typedef std::list<Instruction*>::iterator iterator;

  // Constructor - provide iterator to BeginFunc and EndFunc
  ControlFlowGraph(iterator first, iterator last);
  void add_block(Block);

  // Flow classes (below)
  class ForwardFlow;
  class ReverseFlow;

private:
  void map_labels();
  void map_edges();
  void add_edge(Instruction* from, Instruction* to);
  void map_edges_for_jump(iterator cur, std::string jumpType);

  iterator first;
  iterator last;
  std::map<std::string, Instruction*> instr_for_label;
  std::map<Instruction*, EdgeList> in_edges;
  std::map<Instruction*, EdgeList> out_edges;


};

/*==========================================================
 * ControlFlowGraph::ForwardFlow
 * -----------------------------
 * Used to walk over a CFG in a forward direction.
 */
class ControlFlowGraph::ForwardFlow
{
public:
  // iterator type
  typedef std::list<Instruction*>::const_iterator iterator;

  // Constructor - specify the CFG to walk over
  ForwardFlow( ControlFlowGraph& cfg ) : cfg( cfg )
  { }

  // First and last instruction (inclusive)
  iterator first() { return iterator( cfg.first ); }
  iterator last()  { return iterator( cfg.last );  }

  // Edges in forward direction
  std::map<Instruction*, EdgeList>& in() { return cfg.in_edges; }
  std::map<Instruction*, EdgeList>& out() { return cfg.out_edges; }

private:
  ControlFlowGraph& cfg;
};

/*==========================================================
 * ControlFlowGraph::ReverseFlow
 * -----------------------------
 * Used to walk over a CFG in a forward direction.
 */
class ControlFlowGraph::ReverseFlow
{
public:
  // iterator type
  typedef std::list<Instruction*>::const_reverse_iterator iterator;

  // Constructor - specify the CFG to walk over
  ReverseFlow( ControlFlowGraph& cfg ) : cfg( cfg )
  { }

  // First and last instruction (inclusive)
  iterator first() { return iterator( cfg.last );  }
  iterator last()  { return iterator( cfg.first ); }

  // Edges in reverse direction
  std::map<Instruction*, EdgeList>& in() { return cfg.out_edges; }
  std::map<Instruction*, EdgeList>& out() { return cfg.in_edges; }

private:
  ControlFlowGraph& cfg;
};

#endif
