#ifndef _H_DF_BASE
#define _H_DF_BASE

#include <map>
#include <algorithm>
#include "cfg.h"

typedef std::map<const Instruction*, EdgeList> (ControlFlowGraph::*EdgeMap)();

/*==========================================================
 * DataFlow
 * --------
 * Abstract base class for data flow analysis classes.
 * Type parameters:
 *   ValueType - the information type associated with each instruction
 *   FlowType - either ForwardFlow or ReverseFlow
 */
template <typename ValueType, typename FlowType>
class DataFlow
{
public:
  // Constructor - specify the CFG to walk over
  DataFlow( ControlFlowGraph& cfg ) : flow( cfg )
  { }

  // Safe deletion for derived classes
  virtual ~DataFlow() { }

  // Perform analysis
  void analyze();

  // Retrieve information associated with a particular instruction
  const ValueType& data_in( const Instruction* i ) { return df_in[i]; }
  const ValueType& data_out( const Instruction* i ) { return df_out[i]; }

protected:
  //// NOTE: These four functions need to be implemented in derived class

  // Value for beginning of function (or end in ReverseFlow)
  virtual ValueType init()= 0;

  // "Empty" value used to accumulate effects
  virtual ValueType top()= 0;

  // Calculate effect a particular instruction has on value
  virtual ValueType effect(const Instruction* instr, const ValueType& in)= 0;

  // Calculate the meet of two values
  virtual ValueType meet(const ValueType& a, const ValueType& b)= 0;

private:
  FlowType flow;
  std::map<const Instruction*, ValueType> df_in;
  std::map<const Instruction*, ValueType> df_out;
};

/*----------------------------------------------------------
 * Perform data flow analysis
 */
template <typename ValueType, typename FlowType>
void DataFlow<ValueType, FlowType>::analyze()
{
  // Initialize worklist with all instructions (except first)
  // Initialize value for all instructions
  std::list<Instruction*> worklist;
  {
    typename FlowType::iterator p= flow.first();
    df_in[*p]= init();
    df_out[*p]= effect(*p, init());

    while (p != flow.last())
    {
      ++p;
      df_in[*p]= top();
      df_out[*p]= effect(*p, top());
      worklist.push_back( *p );
    }
  }

  while (!worklist.empty())
  {
    Instruction* i= worklist.front();
    worklist.pop_front();

    // Calculate meet of df_out for all incoming edges
    ValueType total_in= top();
    EdgeList& prev_edges= flow.in()[i];
    for (EdgeList::iterator p= prev_edges.begin(); p != prev_edges.end(); ++p)
      total_in= meet(total_in, df_out[*p]);

    // If we changed something, reseed worklist
    if (total_in != df_in[i])
    {
      df_in[i]= total_in;
      df_out[i]= effect(i, total_in);
      EdgeList& next_edges= flow.out()[i];
      for (EdgeList::iterator n= next_edges.begin(); n != next_edges.end(); ++n)
      {
        if (find(worklist.begin(), worklist.end(), *n) == worklist.end())
          worklist.push_back(*n);
      }
    }
  }
}

#endif
