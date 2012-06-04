struct Control
{
  int arg; /*argument index*/
  void *pred; /*predicate*/
  void *tree; /*indexing struture*/
  UdiControlBlock controlblock; /*acess to indexing struture functions*/
  struct Control *next;
};
typedef struct Control * control_t;
