/*RTree Indexer*/
extern control_t RtreeUdiInit (Term spec, void *pred, int arity);
extern control_t RtreeUdiInsert (Term term, control_t control, void *clausule);
extern void *RtreeUdiSearch (control_t control);
extern int RtreeUdiDestroy(control_t control);

/*B+Tree Indexer*/
extern control_t BtreeUdiInit (Term spec, void *pred, int arity);
extern control_t BtreeUdiInsert (Term term, control_t control, void *clausule);
extern void *BtreeUdiSearch (control_t control);
extern int BtreeUdiDestroy(control_t control);

/* To store UDI indexers access functions */
struct UDIIndexers
{
  char *decl;    /* declaration in udi init to use this indexer */
  Atom atomdecl; /* used in run time to store Yap_LookupAtom 
                    pointer for fast compare tests */
  struct udi_control_block control; /* */
};

/* New Indexers can be added here */
struct UDIIndexers udi_indexers[] = {
  {
    "rtree",
    NULL,
    {&RtreeUdiInit, &RtreeUdiInsert, &RtreeUdiSearch, &RtreeUdiDestroy}
  },
  {
    "btree",
    NULL,
    {&BtreeUdiInit, &BtreeUdiInsert, &BtreeUdiSearch, &BtreeUdiDestroy}
  }
};
