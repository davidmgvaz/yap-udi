#ifndef _BTREE_UDI_PRIVATE_
#define _BTREE_UDI_PRIVATE_

typedef int (*BTreeSearchAtt) (btree_t tree, Term constraint,
                                Yap_UdiCallback callback, void * arg);

struct Att
{
  const char *att;
  BTreeSearchAtt proc_att;
};

int BTreeMinAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeMaxAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeEqAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeLtAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeLeAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeGtAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeGeAtt (btree_t, Term, Yap_UdiCallback, void *);
int BTreeRangeAtt (btree_t, Term, Yap_UdiCallback, void *);

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

#endif /* _BTREE_UDI_PRIVATE_ */
