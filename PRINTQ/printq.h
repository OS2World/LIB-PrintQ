/* printq.h -- include file for printq.dll */
/* Version 1.3, (C) Peter Wansch, 1993     */

#define INCL_DOSSEMAPHORES
#define INCL_WINSHELLDATA
#define INCL_WINLISTBOXES
#define INCL_BASE
#define INCL_SPL
#define INCL_SPLDOSPRINT
#define INCL_DOSMEMMGR
#define INCL_DEV
#define INCL_GPICONTROL

#include <os2.h>

/* variables */
extern PRQINFO3 prqInfoDef; /* current queue description */
extern HCINFO hcInfoDef; /* current form description */
extern LONG alCaps[CAPS_DEVICE_POLYSET_POINTS + 1]; /* current device caps */
extern HDC hdcPrn; /* handle to printer device context */

/* function prototypes */
BOOL SpoolInitialize(HAB hab, PDRIVDATA pDriverData, PBOOL pfIni);
BOOL SpoolTerminate(void);
BOOL SpoolBeginSetup(HWND hwndListBox);
BOOL SpoolEndSetup(void);
BOOL SpoolBeginSpool(PSZ pszComment, PSZ pszDocName, PSZ pszQueueProcParams, ULONG ulOptions, PHPS phpsPrinter, BOOL fAssocPS);
BOOL SpoolEndSpool(PUSHORT pusJobId);
BOOL SpoolAbortSpool(void);
BOOL SpoolBeginInfo(PHPS phpsPrinterInfo, ULONG ulOptions, BOOL fAssocPS);
BOOL SpoolEndInfo(void);
BOOL SpoolJobProp(void);
BOOL SpoolNewFrame(void);
BOOL SpoolSetDef(void);
BOOL SpoolIsJobStarted(void);
