
#include "tbm_utils.h"

#include <fcntl.h>
#include <X11/Xutil.h>

extern Bool DRI2Connect(Display * display, XID window, char **driverName, char **deviceName);
extern Bool DRI2Authenticate(Display * display, XID window, unsigned int magic);

int
init_tbm_bufmgr(int *pFD, tbm_bufmgr *pMgr, void* pDisp)
{
	if (pFD && pMgr && pDisp)
	{
		char *driverName = NULL;
		char *deviceName = NULL;
		if (*pFD != -1 || *pMgr)
		{
			g_print("Alread opened\n");
			return 0;
		}

		if (!DRI2Connect(pDisp, DefaultRootWindow(pDisp), &driverName, &deviceName))
		{
			g_print("DRI2Connect fail!!\n");
			return 0;
		}

		if (!deviceName)
		{
			g_print("deviceName is NULL\n");
			return 0;
		}
		*pFD = open(deviceName, O_RDWR | O_CLOEXEC);
		if (*pFD)
		{
			/* authentication */                                                         
			unsigned int magic = 0;
			if(drmGetMagic(*pFD, &magic))
			{
				g_print("Can't get magic key from drm\n");
				goto FAIL_TO_INIT;
			}

			if(False == DRI2Authenticate(pDisp, DefaultRootWindow(pDisp), magic))
			{                                                                            
				g_print("Can't get the permission\n");
				goto FAIL_TO_INIT;
			}

			*pMgr = tbm_bufmgr_init(*pFD);
			if (*pMgr == NULL)
			{
				g_print("tbm_bufmgr_init failed\n");
				goto FAIL_TO_INIT;
			}
			return 1;
		}
	}

FAIL_TO_INIT:
	if (pFD && *pFD != -1)
	{
		close(*pFD);
		*pFD = -1;
	}
	return 0;
}

void 
deinit_tbm_bufmgr(int *pFD, tbm_bufmgr *pMgr)
{
	if (pFD && pMgr)
	{
		if (*pMgr)
		{
			tbm_bufmgr_deinit(*pMgr);
			*pMgr = NULL;
		}
		if (*pFD != -1)
		{
			close(*pFD);
			*pFD = -1;
		}
	}
}

