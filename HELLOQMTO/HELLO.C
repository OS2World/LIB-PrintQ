/* hello.c: source code file for Hello queue/2 */
/* Version 1.2 MT, (C) Peter Wansch, 1993      */
/* created: 93-5-7                             */
/* modified: 93-8-16                           */

#define INCL_PM    

#include <printq.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 

#include "hello.h"

/* global variables */
HAB hab;                                     /* anchor-block handle            */
HWND hwndFrame;                              /* frame window handle            */
CHAR szOut[NUM_OF_LINES][LENGTH_STRING+1];   /* message strings                */
BOOL fIni = TRUE;                            /* logo display flag              */
LONG lMaxAscender;                           /* screen font metric information */
LONG lMaxBaselineExt;                        /* screen font metric information */
BOOL fPrintThreadStarted = FALSE;            /* flag to indicate if a print    */
                                             /* job has been started in a      */
                                             /* secondary thread               */
unsigned long tid;                           /* print thread id                */
    
int main(VOID)
{
  QMSG  qmsg;                                 /* message queue handle               */
  FONTMETRICS fm;                             /* default screen font metrics        */
  HPS hps;                                    /* screen ps handle for retrieving fm */
  ULONG flCreate = FCF_STANDARD;              /* frame creation flag                */
  HWND hwndClient;                            /* client window handle               */
  HMQ hmq;                                    /* queue handle (1st thread)          */
  BOOL fIniQ = FALSE;

  /* register application to Presentation Manager */
  hab = WinInitialize(0);
  if(!hab)
  {
    WinAlarm(HWND_DESKTOP, WA_ERROR); 
    exit(RETURN_ERROR);
  }

  /* initialize PRINTQ */
  if (!SpoolInitialize(hab, NULL, &fIniQ))
  {
    WinAlarm(HWND_DESKTOP, WA_ERROR); 
    WinTerminate(hab);
    exit(RETURN_ERROR);
  }

  /* create message queue */
  hmq = WinCreateMsgQueue( hab, 0 );
  if(!hmq)
  {
    WinAlarm(HWND_DESKTOP, WA_ERROR); 
    SpoolTerminate();
    WinTerminate(hab);
    exit(RETURN_ERROR);
  }

  /* display timed logo */
  if (!WinDlgBox(HWND_DESKTOP, HWND_DESKTOP, (PFNWP)dpProdInfo, NULLHANDLE, DB_PRODINFO, (PBOOL)&fIni))
  {
    WinDestroyMsgQueue(hmq);
    SpoolTerminate();
    WinTerminate(hab);
    exit(RETURN_ERROR);
  }
  fIni = FALSE;

  /* register private window class for application window */
  if (!WinRegisterClass(hab,(PSZ)"Hello", wpMain, CS_SIZEREDRAW, 0))
  {
    WinDestroyMsgQueue(hmq);
    SpoolTerminate();
    WinTerminate(hab);
    exit(RETURN_ERROR);
  }

  /* create application window */
  hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE, &flCreate, "Hello", "Hello queue/2", 
                                 WS_VISIBLE, NULLHANDLE, WD_MAIN, &hwndClient);
  if (!hwndFrame)
  {
    WinDestroyMsgQueue(hmq);
    SpoolTerminate();
    WinTerminate(hab);
    exit(RETURN_ERROR);
  }

  /* query default screen font metrics to position strings in the application window */
  hps = WinGetPS (HWND_DESKTOP);
  GpiQueryFontMetrics (hps, sizeof(FONTMETRICS), &fm);
  WinReleasePS(hps); 
  lMaxAscender=fm.lMaxAscender;
  lMaxBaselineExt=fm.lMaxBaselineExt;

  /* main message loop */
  while(WinGetMsg(hab, &qmsg, (HWND)NULL, 0, 0))
    WinDispatchMsg(hab, &qmsg);

  /* terminate application */
  WinDestroyWindow(hwndFrame);        
  WinDestroyMsgQueue(hmq);   
         
  /* de-register from PRINTQ and Presentation Manager */
  SpoolTerminate();
  WinTerminate(hab);                  
  return(0);
}

MRESULT EXPENTRY wpMain (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  RECTL rclUpdate, rclClient;
  SHORT i;
  POINTL pt;
  CHAR szJobInfo[128];

  switch( msg )
  {
    case WM_CREATE:
      InitOut();
      return (MRESULT)FALSE;

    case WM_ERASEBACKGROUND:
      return (MRESULT)TRUE;

    case WM_INITMENU:
      /* disable Print and Printer setup menu items while there is a print job */
      WinEnableMenuItem (HWNDFROMMP(mp2), MI_SETUP, !fPrintThreadStarted);
      WinEnableMenuItem (HWNDFROMMP(mp2), MI_PRINT, !fPrintThreadStarted);
      /* enable Abort print job after the job has been started */
      /* a job is started when SpoolBeginSpool has been called */
      WinEnableMenuItem (HWNDFROMMP(mp2), MI_ABORT, SpoolIsJobStarted());
      return (MRESULT)(0);

    case WMP_JOB_DONE:
      /* print job has ended */
      fPrintThreadStarted = FALSE;

      /* give the user information about the print job */
      if (LONGFROMMP(mp2))
        sprintf(szJobInfo, "Print job with id %ld has been spooled.", LONGFROMMP(mp1));
      else
        sprintf(szJobInfo, "Print job has been aborted.");
      WinMessageBox(HWND_DESKTOP, hwnd, szJobInfo,
                    (PCH) "Hello/2", ID_MESSAGEBOX, MB_OK | MB_APPLMODAL | MB_MOVEABLE | MB_INFORMATION);

      /* restore old window title */
      WinSetWindowText(hwndFrame, (PSZ)"Hello queue/2");
      break;

    case WM_COMMAND:
      switch (SHORT1FROMMP(mp1))
      {
        case MI_SETUP:
          /* display Printer setup dialog box */
          WinDlgBox(HWND_DESKTOP, hwnd, dpQueryQInfo, (HMODULE)0, DB_QUERYPRINT, NULL);
          InitOut();
          WinInvalidateRegion(hwnd, NULLHANDLE, FALSE);
          SpoolEndSetup();
          break;

        case MI_ABORT:
          /* abort current print job */
          if (!SpoolAbortSpool())
            WinMessageBox(HWND_DESKTOP, hwnd, "Unable to abort print job.",
                              (PCH) "Hello/2", ID_MESSAGEBOX, MB_OK | MB_APPLMODAL | MB_MOVEABLE | MB_ERROR);
          break;

        case MI_PRINT:
          /* print text displayed in the application window */
          /* create print thread */
          fPrintThreadStarted = TRUE;
          tid = _beginthread(PrintInfo, NULL, STACK_SIZE, NULL);
          if (tid == -1)
          {
            WinMessageBox(HWND_DESKTOP, hwnd, "Unable to create print thread.",
                              (PCH) "Hello/2", ID_MESSAGEBOX, MB_OK | MB_APPLMODAL | MB_MOVEABLE | MB_ERROR);
            fPrintThreadStarted = FALSE;
          }
          else
            /* print thread was successfully created */
            WinSetWindowText(hwndFrame, (PSZ)"Printing in secondary thread is in progress...");
          break;

        case MI_INFO:
          /* display product information dialog box */
          WinDlgBox( HWND_DESKTOP, hwndFrame, (PFNWP)dpProdInfo, (HMODULE)0, DB_PRODINFO, (PBOOL)&fIni);           /* initialization data          */
          break;

        default:
          return WinDefWindowProc( hwnd, msg, mp1, mp2 );
      }
      break;

      case WM_PAINT:
      {
        HPS hps;
        /* paint client window */
        /* begin drawing */
        hps = WinBeginPaint( hwnd, (HPS)NULL, &rclUpdate);

        /* paint invalid rectangle first */
        WinFillRect (hps, &rclUpdate, SYSCLR_WINDOW);

        /* draw lines */
        WinQueryWindowRect(hwnd, &rclClient);
        pt.x = 0; pt.y = rclClient.yTop-lMaxAscender;
        for (i=0; i < NUM_OF_LINES; i++)
        {
          GpiCharStringAt(hps, &pt, (LONG)strlen(szOut[i]), (PSZ)szOut[i]);
          pt.y-=lMaxBaselineExt;
        }

      WinEndPaint(hps);               /* drawing is complete */
      }
      break;
      
    case WM_CLOSE:
      WinPostMsg( hwnd, WM_QUIT, NULL, NULL);     /* cause termination */
      break;
    
    case WM_DESTROY:
      /* if a job has been started it is aborted */
      if (SpoolIsJobStarted())
        SpoolAbortSpool();
      /* we have to wait until the print thread terminates (if there is one) */
      DosWaitThread(&tid, DCWW_WAIT);
      break;

    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return (MRESULT) FALSE;
}

MRESULT EXPENTRY dpQueryQInfo( HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static BOOL fVisitedJobProp;
  HWND hwndListBox;

  switch (msg)
  {
    case WM_INITDLG:
      fVisitedJobProp = FALSE;
      hwndListBox = WinWindowFromID(hwndDlg, LB_QUEUES);
      /* fill list box and select default printer */
      SpoolBeginSetup(hwndListBox);
      break;

    case WM_CONTROL:
      /* list box item is double-clicked */
      if (SHORT2FROMMP(mp1) == LN_ENTER && SHORT1FROMMP(mp1) == LB_QUEUES)
        WinPostMsg(hwndDlg, WM_COMMAND, MPFROM2SHORT(DID_OK, 0), NULL);
      break;
     
    case WM_COMMAND:                    /* posted by push button or key */
      switch(SHORT1FROMMP(mp1))       /* extract the command value */
      {
        case PB_JOBPROP:
          /* Job properties button has been chosen, display Jop properties dialog box */
          if (!SpoolJobProp())
            WinAlarm(HWND_DESKTOP, WA_ERROR);
          fVisitedJobProp = TRUE;
          break;
        case DID_OK:            /* the OK push button or key */
          /* set the currently hilited printer the default */
          SpoolSetDef();
          /* close dialog */
          WinDismissDlg(hwndDlg, TRUE);
          break;
        case DID_CANCEL:        /* the Cancel pushbutton or Escape key */
          WinDismissDlg(hwndDlg, fVisitedJobProp ? TRUE : FALSE);   /* removes the dialog window */
          break;
        default:
          break;
      }
      break;
    default:
      return WinDefDlgProc( hwndDlg, msg, mp1, mp2 );
  }
  return (MRESULT) FALSE;
}

MRESULT EXPENTRY dpProdInfo(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  LONG lLogoDisplayTime;
  ULONG ulTimerId;

  switch(msg)
  {
    case WM_INITDLG:
      if (*((PBOOL)mp2))  
      {
        lLogoDisplayTime = PrfQueryProfileInt(HINI_USERPROFILE, "PM_ControlPanel", "LogoDisplayTime", -1L);
        switch(lLogoDisplayTime)
        {  
          case 0: WinDismissDlg(hwnd, TRUE); break; /* logo is not displayed */
          case -1: break; /* indefinite logo display */
          default: ulTimerId = WinStartTimer(hab, hwnd, TI_LOGO, lLogoDisplayTime); break;
        }
      }
      break;

    case WM_TIMER:
      if (SHORT1FROMMP(mp1) == TI_LOGO)
        WinPostMsg(hwnd, WM_COMMAND, NULL, NULL);
      break;

    case WM_COMMAND:
      /* OK has been pressed or timer messages has arrived */
      WinStopTimer(hab, hwnd, ulTimerId);
      WinDismissDlg(hwnd, TRUE);
      break;

    default:
      return(WinDefDlgProc(hwnd, msg, mp1, mp2));
  }
  return (MRESULT)0;
}

void InitOut(void)
{
  sprintf(szOut[0], "Queue information:");
  sprintf(szOut[1], "  Queue name: %s", prqInfoDef.pszName);
  sprintf(szOut[2], "  Default queue processor: %s", prqInfoDef.pszPrProc);
  sprintf(szOut[3], "  Queue description: %s", prqInfoDef.pszComment);
  sprintf(szOut[4], "  Default device driver: %s", prqInfoDef.pszDriverName);
  sprintf(szOut[5], "  Print devices connected to queue: %s", prqInfoDef.pszPrinters);
  sprintf(szOut[6], "");
  sprintf(szOut[7], "Default form description: ");
  sprintf(szOut[8], "  Form name: %s", hcInfoDef.szFormname);
  sprintf(szOut[9], "  Width (left-to-right): %ld milimeters", hcInfoDef.cx);
  sprintf(szOut[10], "  Height (top-to-bottom): %ld milimeters", hcInfoDef.cy);
  sprintf(szOut[11], "  Number of pels between left and right clip limits: %ld", hcInfoDef.xPels);
  sprintf(szOut[12], "  Number of pels between bottom and top clip limits: %ld", hcInfoDef.yPels);
  sprintf(szOut[13], "");  
  sprintf(szOut[14], "Device capabilities: ");
  sprintf(szOut[15], "  Media width: %ld pels", alCaps[CAPS_WIDTH]);
  sprintf(szOut[16], "  Media height: %ld pels", alCaps[CAPS_HEIGHT]);
  sprintf(szOut[17], "  Media width: %ld chars", alCaps[CAPS_WIDTH_IN_CHARS]);
  sprintf(szOut[18], "  Media height: %ld chars", alCaps[CAPS_HEIGHT_IN_CHARS]);
  sprintf(szOut[19], "  Char height: %ld pels", alCaps[CAPS_CHAR_HEIGHT]);
  sprintf(szOut[20], "  Number of device specific fonts: %ld", alCaps[CAPS_DEVICE_FONTS]);
  sprintf(szOut[21], "  Number of distinct colors available on the device: %ld", alCaps[CAPS_PHYS_COLORS]);
  sprintf(szOut[22], "  Horizontal resolution: %ld dpi", alCaps[CAPS_HORIZONTAL_FONT_RES]);
  sprintf(szOut[23], "  Vertical resolution: %ld dpi", alCaps[CAPS_VERTICAL_FONT_RES]);
}

void PrintInfo(void *arg)
{
  HPS hpsPrn;
  FONTMETRICS fm;
  LONG lprnMaxAscender, lprnMaxBaselineExt;
  SHORT i;
  POINTL pt;
  USHORT usJobId = 0;

  /* begin a print job and obtain a handle to the printer presentation space */
  SpoolBeginSpool("Printer Capabilities", "Printer Caps", "COP=1", 0UL, &hpsPrn, FALSE);

  /* obtain necessary font metrics of the selected font */
  GpiQueryFontMetrics (hpsPrn, sizeof(FONTMETRICS), &fm);
  lprnMaxAscender=fm.lMaxAscender;
  lprnMaxBaselineExt=fm.lMaxBaselineExt;

  /* print lines */
  pt.x = 0; pt.y = hcInfoDef.yPels-lprnMaxAscender;
  for (i=0; i < NUM_OF_LINES; i++)
  {
    /* has the job been aborted from the primary thread? */
    if (!SpoolIsJobStarted())
    {
      /* job has been aborted, so we quit as well */
      while (!WinPostMsg(hwndFrame, WMP_JOB_DONE, MPFROMLONG(usJobId), MPFROMLONG(FALSE)))
        DosSleep(BACKOFF_TIME);
      _endthread();
    }
    /* form feed */
    if (i%alCaps[CAPS_HEIGHT_IN_CHARS]==0 && i>0)
    {
      SpoolNewFrame();
      pt.y = hcInfoDef.yPels-lprnMaxAscender;
    }
    GpiCharStringAt(hpsPrn, &pt, (LONG)strlen(szOut[i]), (PSZ)szOut[i]);
    pt.y-=lprnMaxBaselineExt;
    /* nap attack after spooling one line */
    DosSleep (1000UL);
  }
  /* end print job */
  SpoolEndSpool(&usJobId);
  /* make sure that the Job done message is successfully posted */
  /* mp1 contains the job id */
  /* mp2 contains an indicator if the job was successful */
  while (!WinPostMsg(hwndFrame, WMP_JOB_DONE, MPFROMLONG(usJobId), MPFROMLONG(TRUE)))
    DosSleep(BACKOFF_TIME);
  _endthread();
}

