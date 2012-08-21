#ifndef _UTHASH_UDI_
#define _UTHASH_UDI_

extern void UTHashUdiInit (control_t control);

/*this is called in each asserted term that was declared to udi_init*/
extern void UTHashUdiInsert (Term term, /*asserted term*/
                             control_t control,
                             size_t clausule); /*to store in hash and return
                                                 in search*/

extern int UTHashUdiSearch (control_t control,
                            Yap_UdiCallback callback, void *data);

extern void UTHashUdiDestroy(control_t control);

#endif /* _UTHASH_UDI_ */
