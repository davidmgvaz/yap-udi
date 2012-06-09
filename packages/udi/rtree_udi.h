#ifndef _RTREE_UDI_
#define _RTREE_UDI_

extern void RtreeUdiInit (control_t control);

/*this is called in each asserted term that was declared to udi_init*/
extern void RtreeUdiInsert (Term term, /*asserted term*/
                            control_t control,
                            size_t clausule); /*to store in tree and return
                                                     in search*/

extern int RtreeUdiSearch (control_t control, Yap_UdiCallback callback, void *data);
extern void RtreeUdiDestroy(control_t control);

#endif /* _RTREE_UDI_ */
