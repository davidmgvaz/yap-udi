#ifndef _BTREE_UDI_
#define _BTREE_UDI_

/* #ifndef _BTREE_ */
/* typedef void control_t; */
/* #endif */

/*Prolog term from :- udi(a(-,+,+)).
  User defined index announce
*/
extern control_t *BtreeUdiInit (Term spec,
                                void *pred,
                                int arity);

/*this is called in each asserted term that was declared to udi_init*/
extern control_t *BtreeUdiInsert (Term term, /*asserted term*/
                                  control_t *control,
                                  void *clausule); /*to store in tree and return
                                                     in search*/

extern void *BtreeUdiSearch (control_t *control);
extern int BtreeUdiDestroy(control_t *control);

#endif /* _BTREE_UDI_ */
