#include "cfg.h"
#include "tac.h"

/*----------------------------------------------------------
 * Create and initialize new CFG
 */
ControlFlowGraph::ControlFlowGraph(iterator first, iterator last)
   : first( first ), last( last )
{
  //printf("-------------------- BEGIN BLOCK --------------------\n");
  map_labels();
  map_edges();
  //printf("--------------------- END BLOCK ---------------------\n");
}

/*----------------------------------------------------------
 * Build map from labels -> instructions
 */
void ControlFlowGraph::map_labels()
{
  for (iterator p= first; p != last; ++p)
  {
    Label* label= dynamic_cast<Label*>(*p);
    if (label) {
      instr_for_label[label->text()]= *p;
    }
  }
}

/*----------------------------------------------------------
 * Build map from instruction -> input edges
 * Build map from instruction -> output edges
 */
void ControlFlowGraph::map_edges()
{
  std::string jumpType;
  for (iterator cur= first; cur != last; ++cur)
  {
    iterator next= cur;
    ++next;

    //add_edge(*cur, *next); 
    if (dynamic_cast<Return*>(*cur)) {
        next = first;
        add_edge(*cur, *next); 
    }
    else if (dynamic_cast<LCall*>(*cur)) {
        jumpType = "LCall";
        map_edges_for_jump(cur, jumpType);
    }
    else if (dynamic_cast<ACall*>(*cur)) {
        jumpType = "ACall";
        map_edges_for_jump(cur, jumpType);
    }
    else if (dynamic_cast<IfZ*>(*cur)) {
        jumpType = "IfZ";
        map_edges_for_jump(cur, jumpType);
    }
    else if (dynamic_cast<Goto*>(*cur)) {
        jumpType = "Goto";
        map_edges_for_jump(cur, jumpType);
    }
  }
}

void ControlFlowGraph::map_edges_for_jump(iterator cur, std::string jumpType)
{
    int jumpInstrLength = jumpType.length() + 1;
    std::string label_string_to_find = (*cur)->getPrinted();
    size_t pos_to_remove_from = label_string_to_find.find(jumpType);
    label_string_to_find.erase(0, pos_to_remove_from + jumpInstrLength);
    Instruction* label_instr;

    // if label is in this block
    if (instr_for_label.find(label_string_to_find) != instr_for_label.end()) {
        label_instr = instr_for_label.at(label_string_to_find);
        add_edge(*cur, label_instr);
    }
    else {
        //label is in another block, come back to it. 
        if (strcmp(label_string_to_find.c_str(), "_PrintString") == 0 || 
                strcmp(label_string_to_find.c_str(), "_PrintInt") == 0) {
            // PrintString, PrintInt todo.
            // (jump to their offset relative to the end of the file?
            //    that code is always the same...)
            return;
        }
        //printf("%s is in another block.\n", label_string_to_find.c_str());
        add_edge(*cur, *cur);
    }
}

/*----------------------------------------------------------
 * Add edge going between two instructions
 */
void ControlFlowGraph::add_edge(Instruction* from, Instruction* to)
{
    /*
  printf("BEGIN EDGE\n");
  printf("    from: ");
  from->Print();
  printf("    to:    ");
  to->Print();
  printf("END EDGE\n\n");
  */
  in_edges[to].push_back(from);
  out_edges[from].push_back(to);
}
