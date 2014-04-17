/* printq.c -- source file for printq.dll */
/* Version 1.31, (C) Peter Wansch, 1994   */
/* created: 93-5-28                       */

#include <stddef.h>
#include <string.h> 
#include "printq.h"

/* constants */
#define LEN_WORKSTRING 256
#define LEN_BUFFER 1024

/* global variables */
HAB habApp;
HWND hwndList;
HDC hdcScreen;
HDC  hdcPrn;
HPS hpsPrn;
PVOID  pBuf;
DEVOPENSTRUC dopData;
ULONG  ulDefIndex;
ULONG ulNumOfQs;
HMTX hmtxExec;
BOOL fAssoc;
BOOL fInitialized = FALSE;
BOOL fJobStarted = FALSE;
BOOL fListDisplayed = FALSE;
BOOL fQueryInfo = FALSE;
BOOL fRequestMutex = TRUE;
BOOL fListBox = FALSE;

/* exported global variables */
PRQINFO3 prqInfoDef;
HCINFO hcInfoDef; 
LONG alCaps[CAPS_DEVICE_POLYSET_POINTS + 1]; 

/* DLL initialisation/termination function */
/* we create a mutex semaphore to serialize the execution of library function in a process */
unsigned long _DLL_InitTerm(unsigned long modhandle, unsigned long flag)
{
  switch (flag)
  {
    case 0:
      /* initialization */
      if (NO_ERROR != DosCreateMutexSem((PSZ)NULL, &hmtxExec, 0UL, FALSE))
        return 0UL;
      break;

    case 1:
      /* termination */
      DosCloseMutexSem(hmtxExec);
      break;

    default:
      return 0UL;
  }
  modhandle;
  /* return 1 to indicate success */
  return 1UL;
}

BOOL SpoolInitialize(HAB hab, PDRIVDATA pDriverData, PBOOL pfIni)
{
  PPRQINFO3 prq;
  PPRQINFO3 prqIni;
  PPRQINFO3 prqDef;
  PHCINFO phcinfo;
  CHAR szBuffer[LEN_BUFFER+1];
  SPLERR splerr;
  CHAR   szDefQName[LEN_WORKSTRING]; 
  ULONG  cbBuf ;
  ULONG  cTotal;
  ULONG  cbNeeded;
  ULONG  i, iDef, iIni;
  PCHAR  pch;
  PVOID  pBuffer;
  LONG clForms;
  BOOL fIniFound;
  BOOL fDefFound;

  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;

  if (fInitialized || fJobStarted || fListDisplayed || fQueryInfo)
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  habApp = hab;
  if (PrfQueryProfileString(HINI_PROFILE, "PM_SPOOLER", "QUEUE", NULL, szDefQName, LEN_WORKSTRING))
  {  
    pch = strchr(szDefQName, ';');
    if (pch)
      *pch = 0;
  }
  else
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  
  splerr = SplEnumQueue((PSZ)NULL, 3L, pBuf, 0L, &ulNumOfQs, &cTotal, &cbNeeded, NULL);
  if (splerr != ERROR_MORE_DATA)
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  if (DosAllocMem(&pBuf, cbNeeded, PAG_READ | PAG_WRITE | PAG_COMMIT))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  cbBuf = cbNeeded;
  splerr = SplEnumQueue((PSZ)NULL, 3L, pBuf, cbBuf, &ulNumOfQs, &cTotal, &cbNeeded, NULL);
  if (splerr != NO_ERROR)
  {
    DosFreeMem(pBuf);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  prq = (PPRQINFO3)pBuf;        

  /* ulNumOfQs has the count of the number of PRQINFO3 structures */

  fIniFound = FALSE;
  fDefFound = FALSE;

  for (i = 0; i < ulNumOfQs; i++) 
  {

    if (strcmp(prq->pszName, szDefQName) == 0) 
    {
      prqDef = prq;
      fDefFound = TRUE;
      iDef = i;
    }
    if (*pfIni && pDriverData->cb == prq->pDriverData->cb && pDriverData->lVersion == prq->pDriverData->lVersion
        && strcmp(pDriverData->szDeviceName, prq->pDriverData->szDeviceName)==0)
    {
      prqIni = prq;
      fIniFound = TRUE;
      iIni = i;
    }
    prq++;
  }

  if (*pfIni && fIniFound)
  {
    prq = prqIni;
    memcpy(prq->pDriverData, pDriverData, pDriverData->cb);
    ulDefIndex = iIni;
  }
  else
    if (fDefFound)
    {
      prq = prqDef;
      ulDefIndex = iDef;
      *pfIni = FALSE;
    }
    else
      {
        DosFreeMem(pBuf);
        DosReleaseMutexSem(hmtxExec);
        return FALSE;
      }

  /* prq points to default PRQINFO3 structure */
  prqInfoDef = *prq;
  /* fill in device open structure */
  dopData.pszLogAddress = prq->pszName;
  dopData.pdriv = prq->pDriverData;
  strcpy(szBuffer, prq->pszDriverName);
  pch = strchr(szBuffer, '.');
  if (pch)
    *pch = 0;
  dopData.pszDriverName = szBuffer;
  hdcPrn = DevOpenDC(habApp, OD_INFO, "*", 3L, (PDEVOPENDATA)&dopData, (HDC)0);
  if (hdcPrn == DEV_ERROR)
  {
    DosFreeMem(pBuf);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* now we obtain some device information */
  if (!DevQueryCaps(hdcPrn, CAPS_FAMILY, CAPS_DEVICE_POLYSET_POINTS + 1, alCaps))
  {
    DevCloseDC(hdcPrn);
    DosFreeMem(pBuf);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* now we obtain information on the default form */
  clForms = DevQueryHardcopyCaps(hdcPrn, 0L, 0L, NULL);
  if (clForms == DQHC_ERROR)
  {
    DevCloseDC(hdcPrn);
    DosFreeMem(pBuf);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  if (DosAllocMem(&pBuffer, clForms*sizeof(HCINFO), PAG_READ|PAG_WRITE|PAG_COMMIT)!=NO_ERROR)
  {
    DevCloseDC(hdcPrn);
    DosFreeMem(pBuf);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  phcinfo = (PHCINFO)pBuffer;
  if (DevQueryHardcopyCaps(hdcPrn, 0L, clForms, phcinfo) == DQHC_ERROR)
  {
    DosFreeMem(pBuffer); 
    DevCloseDC(hdcPrn);
    DosFreeMem(pBuf); 
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }          
  while (!(phcinfo->flAttributes & HCAPS_CURRENT)) 
    phcinfo++;
  /* phcinfo now points to the HCINFO for the current form */
  hcInfoDef = *phcinfo;
  DosFreeMem(pBuffer);
  DevCloseDC(hdcPrn);
  fInitialized = TRUE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolBeginInfo(PHPS phpsPrinterInfo, ULONG ulOptions, BOOL fAssocPS)
{
  SIZEL szl;
  PPRQINFO3 prq;
  CHAR szBuffer[LEN_BUFFER+1];
  PCHAR  pch;

  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && !fListDisplayed && !fJobStarted && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  prq = (PPRQINFO3) pBuf;
  prq += ulDefIndex;
  /* fill in device open structure */
  strcpy(szBuffer, prq->pszDriverName);
  pch = strchr(szBuffer, '.');
  if (pch)
    *pch = 0;
  dopData.pszDriverName = szBuffer;
  hdcPrn = DevOpenDC(habApp, OD_INFO, "*", 3L, (PDEVOPENDATA)&dopData, (HDC)0);
  if (hdcPrn == DEV_ERROR)
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* if the presentation space is currently not associated with a screen device context,
     create a new presentation space */
  fAssoc = fAssocPS;
  if (fAssoc)
  {
    /* associate device context with presentation space */
    /* initialisation */
    hpsPrn = *phpsPrinterInfo;
    hdcScreen = GpiQueryDevice(hpsPrn);
    if (hdcScreen == HDC_ERROR || hdcScreen == NULLHANDLE)
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* hdcScreen is now a handle to a screen device context */
    /* disassociate the presentation space from the screen device context */
    if (!GpiAssociate(hpsPrn, NULLHANDLE))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* now the presentation space is associated with the printer device context */
    if (!GpiAssociate(hpsPrn, hdcPrn))
    {
      /* try to reassociate presentation space with screen device context */
      GpiAssociate(hpsPrn, hdcScreen);
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
  }
  else
  {
    /* create a presentation space and associate it with the printer device context */
    szl.cx=0L;
    szl.cy=0L;
    if (ulOptions == 0UL)
      ulOptions = PU_PELS;
    hpsPrn = GpiCreatePS(habApp, hdcPrn, &szl, ulOptions | GPIA_ASSOC);
    if (hpsPrn == GPI_ERROR)
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    *phpsPrinterInfo = hpsPrn;
  }
  fQueryInfo = TRUE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolEndInfo(void)
{
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && !fListDisplayed && !fJobStarted && fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  if (!fAssoc)
    /* the presentation space was created */
    GpiDestroyPS(hpsPrn);
  else
  {
    /* the presentation space was only associated */
    /* disassociate presentation space from printer device context */
    if (!GpiAssociate(hpsPrn, NULLHANDLE))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* reassociate presentation space with display device context */
    if (!GpiAssociate(hpsPrn, hdcScreen))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }    
  }
  DevCloseDC(hdcPrn);
  fQueryInfo = FALSE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolSetDef(void)
{
  SHORT index;  
  PPRQINFO3 prq;
  PHCINFO phcinfo;
  CHAR szBuffer[LEN_BUFFER+1];
  LONG clForms;
  PVOID  pBuffer;
  PCHAR  pch;
  PRQINFO3 prqInfoDefTemp;
  ULONG ulDefIndexTemp;
  LONG alCapsTemp[CAPS_DEVICE_POLYSET_POINTS + 1]; 

  if (fRequestMutex)
    if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
      return FALSE;

  if (!(fInitialized && fListDisplayed && !fJobStarted && !fQueryInfo))
  {
    if (fRequestMutex)
      DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }

  if (fListBox)
  {
    index = WinQueryLboxSelectedItem(hwndList);
    if (index == LIT_NONE)
    {
      if (fRequestMutex)
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
  }
  else
    index = (SHORT)ulDefIndex;

  prq = (PPRQINFO3) pBuf;
  prq += index;
  prqInfoDefTemp = *prq;
  ulDefIndexTemp = (ULONG)index;
  /* fill in device open structure */
  dopData.pszLogAddress = prq->pszName;
  dopData.pdriv = prq->pDriverData;
  strcpy(szBuffer, prq->pszDriverName);
  pch = strchr(szBuffer, '.');
  if (pch)
    *pch = 0;
  dopData.pszDriverName = szBuffer;
  hdcPrn = DevOpenDC(habApp, OD_INFO, "*", 3L, (PDEVOPENDATA)&dopData, (HDC)0);
  if (hdcPrn == DEV_ERROR)
  {
    if (fRequestMutex)
      DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* now we obtain some device information */
  if (!DevQueryCaps(hdcPrn, CAPS_FAMILY, CAPS_DEVICE_POLYSET_POINTS + 1, alCapsTemp))
  {
    DevCloseDC(hdcPrn);
    if (fRequestMutex)
      DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* now we obtain information on the default form */
  clForms = DevQueryHardcopyCaps(hdcPrn, 0L, 0L, NULL);
  if (clForms == DQHC_ERROR)
  {
    DevCloseDC(hdcPrn);
    if (fRequestMutex)
      DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  if (DosAllocMem(&pBuffer, clForms*sizeof(HCINFO), PAG_READ|PAG_WRITE|PAG_COMMIT)!=NO_ERROR)
  {
    DevCloseDC(hdcPrn);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  phcinfo = (PHCINFO)pBuffer;
  if (DevQueryHardcopyCaps(hdcPrn, 0L, clForms, phcinfo) == DQHC_ERROR)
  {
    DosFreeMem(pBuffer); 
    DevCloseDC(hdcPrn);
    if (fRequestMutex)
      DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }          
  while (!(phcinfo->flAttributes & HCAPS_CURRENT))
    phcinfo++;
  /* phcinfo now points to the HCINFO for the current form */

  hcInfoDef = *phcinfo;
  prqInfoDef = prqInfoDefTemp;
  ulDefIndex = ulDefIndexTemp;
  for (index = 0; index <= CAPS_DEVICE_POLYSET_POINTS; index++)
    alCaps[index] = alCapsTemp[index]; 
  
  DosFreeMem(pBuffer);
  DevCloseDC(hdcPrn);
  if (fRequestMutex)
    DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolBeginSetup(HWND hwndListBox)
{
  PPRQINFO3 prq;
  PSZ psz;
  SHORT i,index;

  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;

  hwndList = hwndListBox;
  if (!(fInitialized && !fJobStarted && !fListDisplayed && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }

  if (hwndListBox == NULLHANDLE)
    fListBox = FALSE;
  else
    fListBox = TRUE;

  if (fListBox)
  {
    if (!WinSendMsg(hwndList, LM_DELETEALL, (MPARAM)NULL, (MPARAM)NULL))
    {
      fListBox = FALSE;
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    prq = (PPRQINFO3) pBuf;
    /* prq points to the first PRQINFO3 structure */
    for (i = 0; i < ulNumOfQs; i++)
    {
      psz = *prq->pszComment ? prq->pszComment : prq->pszName;
      index = WinInsertLboxItem(hwndList, LIT_END, psz);
      if (index == LIT_MEMERROR || index == LIT_ERROR || index != i)
      {
        fListBox = FALSE;
        DosReleaseMutexSem(hmtxExec);
        return FALSE;
      }
      prq++;
    }
    if (!WinSendMsg(hwndList, LM_SELECTITEM, (MPARAM)ulDefIndex, (MPARAM)TRUE))
    {
      fListBox = FALSE;
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
  }
  fListDisplayed = TRUE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolJobProp(void)
{
  PPRQINFO3 prq;
  LONG cbBuf;
  PSZ pszDeviceName, psz;
  SHORT index;
  CHAR szBuffer[LEN_BUFFER+1];
  
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && fListDisplayed && !fJobStarted && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }

  if (fListBox)
  {
    index = WinQueryLboxSelectedItem(hwndList);
    if (index == LIT_NONE)
    {
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
  }
  else
    index = (SHORT)ulDefIndex;

  prq = (PPRQINFO3) pBuf;
  prq += index;

  psz = strchr(prq->pszPrinters, ',');
  if (psz)
    *psz=0;
  strcpy(szBuffer, prq->pszDriverName);
  pszDeviceName = strchr(szBuffer, '.');
  if (pszDeviceName)
  {
    *pszDeviceName=0;
    pszDeviceName++;
  }

  cbBuf = DevPostDeviceModes(habApp, (PDRIVDATA)NULL, szBuffer, pszDeviceName, prq->pszPrinters, DPDM_POSTJOBPROP);
  if (cbBuf<=0 || cbBuf != prq->pDriverData->cb)
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE; 
  }
  cbBuf = DevPostDeviceModes(habApp, prq->pDriverData, szBuffer, pszDeviceName, prq->pszPrinters, DPDM_POSTJOBPROP);
  if (cbBuf == DPDM_ERROR)
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* the Job properties of the default printer might have been edited */
  fRequestMutex = FALSE;
  if ((ULONG)index == ulDefIndex)
    if (!SpoolSetDef())
    {
      fRequestMutex = TRUE;
      DosReleaseMutexSem(hmtxExec);
      return FALSE; 
    }
  fRequestMutex = TRUE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolBeginSpool(PSZ pszComment, PSZ pszDocName, PSZ pszQueueProcParams, ULONG ulOptions, PHPS phpsPrinter, BOOL fAssocPS)
{
  SIZEL szl;
  PPRQINFO3 prq;
  CHAR szBuffer[LEN_BUFFER+1];
  PCHAR  pch;
  
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && !fJobStarted && !fListDisplayed && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  fAssoc = fAssocPS;
  /* SpoolBeginSpool starts a print job on the default queue and returns hpsPrn */
  prq = (PPRQINFO3) pBuf;
  prq += ulDefIndex;
  strcpy(szBuffer, prq->pszDriverName);
  pch = strchr(szBuffer, '.');
  if (pch)
    *pch = 0;
  dopData.pszDriverName = szBuffer;
  if (fAssoc)
    dopData.pszDataType="PM_Q_RAW";
  else
    dopData.pszDataType="PM_Q_STD";
  dopData.pszComment=pszComment;
  dopData.pszQueueProcName=NULL;
  dopData.pszQueueProcParams=pszQueueProcParams; 
  dopData.pszSpoolerParams=NULL;
  dopData.pszNetworkParams=NULL;
  hdcPrn = DevOpenDC (habApp, OD_QUEUED, "*", 9L, (PDEVOPENDATA)&dopData, (HDC)0);
  if (hdcPrn == DEV_ERROR)
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  /* if the presentation space is currently not associated with a screen device context,
     create a new presentation space */
  if (fAssoc)
  {
    /* associate device context with presentation space */
    /* initialisation */
    hpsPrn = *phpsPrinter;

    hdcScreen = GpiQueryDevice(hpsPrn);
    if (hdcScreen == HDC_ERROR || hdcScreen == NULLHANDLE)
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* hdcScreen is now a handle to a screen device context */
    /* disassociate the presentation space from the screen device context */
    if (!GpiAssociate(hpsPrn, NULLHANDLE))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* now the presentation space is associated with the printer device context */
    if (!GpiAssociate(hpsPrn, hdcPrn))
    {
      /* try to reassociate presentation space with screen device context */
      GpiAssociate(hpsPrn, hdcScreen);
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
  }
  else
  {
    /* create a presentation space and associate it with the printer device context */
    szl.cx=0L;
    szl.cy=0L;
    if (ulOptions == 0UL)
      ulOptions = PU_PELS;
    hpsPrn = GpiCreatePS(habApp, hdcPrn, &szl, ulOptions | GPIA_ASSOC);
    if (hpsPrn == GPI_ERROR)
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    *phpsPrinter = hpsPrn;
  }

  if (DevEscape(hdcPrn, DEVESC_STARTDOC, (LONG)sizeof(pszDocName), (PBYTE)pszDocName, (LONG)0, (PBYTE)NULL)!=DEV_OK)
  {
    GpiDestroyPS(hpsPrn);
    DevCloseDC(hdcPrn);
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  fJobStarted = TRUE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolTerminate(void)
{
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && !fJobStarted && !fListDisplayed && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  DosFreeMem(pBuf);
  fInitialized = FALSE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolNewFrame(void)
{
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && !fListDisplayed && fJobStarted && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  if (DevEscape (hdcPrn, DEVESC_NEWFRAME, 0L, NULL, 0L, NULL) == DEV_OK) 
  {
    DosReleaseMutexSem(hmtxExec);
    return TRUE;
  }
  else
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
}

BOOL SpoolEndSpool(PUSHORT pusJobId)
{
  LONG loutData=2L;

  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && fJobStarted && !fListDisplayed && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  DevEscape (hdcPrn, DEVESC_ENDDOC, (LONG)0, (PBYTE)NULL, &loutData, (PBYTE)pusJobId);

  if (!fAssoc)
    /* the presentation space was created */
    GpiDestroyPS(hpsPrn);
  else
  {
    /* the presentation space was only associated */
    /* disassociate presentation space from printer device context */
    if (!GpiAssociate(hpsPrn, NULLHANDLE))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* reassociate presentation space with display device context */
    if (!GpiAssociate(hpsPrn, hdcScreen))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }    
  }
  DevCloseDC(hdcPrn);
  fJobStarted = FALSE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolAbortSpool(void)
{
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && fJobStarted && !fListDisplayed && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  DevEscape (hdcPrn, DEVESC_ABORTDOC, (LONG)0, (PBYTE)NULL, (PLONG)0L, (PBYTE)NULL);
  if (!fAssoc)
    /* the presentation space was created */
    GpiDestroyPS(hpsPrn);
  else
  {
    /* the presentation space was only associated */
    /* disassociate presentation space from printer device context */
    if (!GpiAssociate(hpsPrn, NULLHANDLE))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }
    /* reassociate presentation space with display device context */
    if (!GpiAssociate(hpsPrn, hdcScreen))
    {
      DevCloseDC(hdcPrn);
      DosReleaseMutexSem(hmtxExec);
      return FALSE;
    }    
  }
  DevCloseDC(hdcPrn);
  fJobStarted = FALSE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}

BOOL SpoolIsJobStarted(void)
{
  return fJobStarted;
}

BOOL SpoolEndSetup(void)
{
  if (NO_ERROR != DosRequestMutexSem(hmtxExec, SEM_INDEFINITE_WAIT))
    return FALSE;
  if (!(fInitialized && !fJobStarted && fListDisplayed && !fQueryInfo))
  {
    DosReleaseMutexSem(hmtxExec);
    return FALSE;
  }
  fListBox = FALSE;
  fListDisplayed = FALSE;
  DosReleaseMutexSem(hmtxExec);
  return TRUE;
}
