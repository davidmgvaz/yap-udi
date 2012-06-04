#ifndef _BTREE_UDI_I_
#define _BTREE_UDI_I_

#define NARGS 5
struct Control
{
  int arg;
  void *pred;
  btree_t tree;
};
typedef struct Control control_t[NARGS];

typedef void (*BTreeSearchAtt) (struct Control, Term, clause_list_t);

struct Att
{
  const char *att;
  BTreeSearchAtt proc_att;
};

void BTreeMinAtt (struct Control, Term, clause_list_t);
void BTreeMaxAtt (struct Control, Term, clause_list_t);
void BTreeEqAtt (struct Control, Term, clause_list_t);
void BTreeLtAtt (struct Control, Term, clause_list_t);
void BTreeLeAtt (struct Control, Term, clause_list_t);
void BTreeGtAtt (struct Control, Term, clause_list_t);
void BTreeGeAtt (struct Control, Term, clause_list_t);
void BTreeRangeAtt (struct Control, Term, clause_list_t);

static struct Att att_func[] = 
  {
    {"min",BTreeMinAtt},
    {"max",BTreeMaxAtt},
    {"eq",BTreeEqAtt},
    {"lt",BTreeLtAtt},
    {"le",BTreeLeAtt},
    {"gt",BTreeGtAtt},
    {"ge",BTreeGeAtt},
    {"range",BTreeRangeAtt}
  };

#endif /* _BTREE_UDI_I_ */
