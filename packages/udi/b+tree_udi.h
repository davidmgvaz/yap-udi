#ifndef _BTREE_UDI_
#define _BTREE_UDI_

extern void BtreeUdiInit (control_t control);

/*this is called in each asserted term that was declared to udi_init*/
extern void BtreeUdiInsert (Term term, /*asserted term*/
                            control_t control,
                            size_t clausule); /*to store in tree and
                                                      return in search*/

extern int BtreeUdiSearch (control_t control, Yap_UdiCallback callback, void *data);
extern void BtreeUdiDestroy(control_t control);

#endif /* _BTREE_UDI_ */
