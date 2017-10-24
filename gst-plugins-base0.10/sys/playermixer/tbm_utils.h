#ifndef _TBM_UTILS_H_
#define _TBM_UTILS_H_

#include <tbm_bufmgr.h>

int init_tbm_bufmgr(int *pFD, tbm_bufmgr *pMgr, void* pDisp);
void deinit_tbm_bufmgr(int *pFD, tbm_bufmgr *pMgr);

#endif // _TBM_UTILS_H_

