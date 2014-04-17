/* hello.c: source code file for Hello queue/2 */
/* Version 1.0, (C) Peter Wansch, 1993         */
/* created: 93-5-7                             */
/* modified: 93-8-16                           */

#define INCL_PM    

#include <printq.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 

#include "hello.h"

/* global variables */
HAB hab;                                     /* anchor-block handle */
HWND hwndFrame;                              /* frame window handle */
CHAR szOut[NUM_OF_LINES][LENGTH_STRING+1];   /* message strings     */
BOOL fIni = TRUE;                            /* logo display flag   */
LONG lMaxAscender;
LONG lMaxBaselineExt;          
HPOINTER hptrWait, hptrSystem;
POINTL ptlStart = {0, 0};
    
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

  /* query required pointers */
  hptrWait = WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE);
  hptrSystem = WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE);        

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
  HPS hpsPrn;
  POINTL pt;
  FONTMETRICS fm;
  LONG lprnMaxAscender, lprnMaxBaselineExt;
  SHORT i;
  USHORT usJobId;
  BOOL fExceed;
  RECTL rclUpdate, rclClient;
  POINTL  aptl[LENGTH_STRING+1];

  switch( msg )
  {
    case WM_CREATE:
      InitOut();
      return (MRESULT)FALSE;

    case WM_ERASEBACKGROUND:
      return (MRESULT)TRUE;

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

        case MI_PRINT:
          /* print text displayed in the application window */
          WinSetPointer(HWND_DESKTOP, hptrWait);
          fExceed = FALSE;

          /* obtain an information printer presentation space to query */
          /* necessary font metrics of the selected font */
          SpoolBeginInfo(&hpsPrn, 0UL, FALSE);
          GpiQueryFontMetrics (hpsPrn, sizeof(FONTMETRICS), &fm);
          lprnMaxAscender=fm.lMaxAscender;
          lprnMaxBaselineExt=fm.lMaxBaselineExt;
          SpoolEndInfo();

          /* change the application title bar text to inform user of spooling action */
          WinSetWindowText(hwndFrame, (PSZ)"Spooling ...");

          /* begin a print job and obtain a handle to the printer presentation space */
          SpoolBeginSpool("Printer Capabilities", "Printer Caps", "COP=1", 0UL, &hpsPrn, FALSE);

          /* check if the lines are within page boundaries */
          for (i=0; i < NUM_OF_LINES && !fExceed; i++)
          {
            GpiQueryCharStringPosAt(hpsPrn, &ptlStart, 0L, (LONG)strlen(szOut[i]), (PSZ)szOut[i], NULL, aptl);
            if (aptl[strlen(szOut[i])].x > alCaps[CAPS_WIDTH])
              fExceed = TRUE;
          }

          /* inform user if line(s) exceed page boundaries */
          if (fExceed)
            if (WinMessageBox(HWND_DESKTOP, hwnd, "Line(s) will exceed right margin of selected form. Print anyway?",
                              (PCH) "Hello/2", ID_MESSAGEBOX, MB_YESNO | MB_APPLMODAL | MB_MOVEABLE | MB_ICONQUESTION) == MBID_NO)
            {
              /* the user wants to abort the job */
              SpoolAbortSpool();
              WinSetPointer(HWND_DESKTOP, hptrSystem);
              WinSetWindowText(hwndFrame, "Hello queue/2");
              break;
            }

          /* print lines */
          pt.x = 0; pt.y = hcInfoDef.yPels-lprnMaxAscender;
          for (i=0; i < NUM_OF_LINES; i++)
          {
            /* form feed */
            if (i%alCaps[CAPS_HEIGHT_IN_CHARS]==0 && i>0)
            {
              SpoolNewFrame();
              pt.y = hcInfoDef.yPels-lprnMaxAscender;
            }
            GpiCharStringAt(hpsPrn, &pt, (LONG)strlen(szOut[i]), (PSZ)szOut[i]);
            pt.y-=lprnMaxBaselineExt;
          }

          /* end print job */
          SpoolEndSpool(&usJobId);

          /* restore arrow pointer an set old application title bar text */
          WinSetPointer(HWND_DESKTOP, hptrSystem);
          WinSetWindowText(hwndFrame, (PSZ)"Hello queue/2");
          break;
 
        case MI_PRINT_BITMAP:
          {
            HDC hdcPrnMem, hdcMem;
            HPS hpsPrnMem, hpsMem, hpsToPrint;
            SIZEL sizl = {0, 0};
            BITMAPINFOHEADER2 bmih;
            LONG alBitmapStuff[2];
            SWP swp;
            HBITMAP hbmToPrint;
            POINTL apointl[3];

            /* print desktop window as bitmap */
            WinSetPointer(HWND_DESKTOP, hptrWait);
            fExceed = FALSE;

            /* change the application title bar text to inform user of spooling action */
            WinSetWindowText(hwndFrame, (PSZ)"Spooling ...");

            /* begin a print job and obtain a handle to the printer presentation space */
            SpoolBeginSpool("Bitmap", "Bitmap", "COP=1", 0UL, &hpsPrn, FALSE);

            /* get device context and presentation space compatible with printer */
            /* the imported PrintQ variable hdcPrn contains a handle to the printer dc */
            hdcPrnMem = DevOpenDC(hab, OD_MEMORY, "*", 0, NULL, hdcPrn);
            hpsPrnMem = GpiCreatePS(hab, hdcPrnMem, &sizl, PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);

            /* get device context and presentation space compatible with display */
            hdcMem = DevOpenDC(hab, OD_MEMORY, "*", 0, NULL, NULLHANDLE);
            hpsMem = GpiCreatePS(hab, hdcMem, &sizl, PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);

            /* get PS for the frame window */
            hpsToPrint = WinGetPS(HWND_DESKTOP);

            WinQueryWindowPos(HWND_DESKTOP, &swp);
            bmih.cbFix = sizeof(BITMAPINFOHEADER2);
            bmih.cx = swp.cx;
            bmih.cy = swp.cy;
            GpiQueryDeviceBitmapFormats(hpsToPrint, 2L, alBitmapStuff);
            bmih.cPlanes = (USHORT) alBitmapStuff[0];
            bmih.cBitCount = (USHORT) alBitmapStuff[1];

            hbmToPrint = GpiCreateBitmap(hpsMem, &bmih, 0, NULL, NULL);
            GpiSetBitmap(hpsMem, hbmToPrint);
            apointl[0].x = 0;
            apointl[0].y = 0;
            apointl[1].x = swp.cx;
            apointl[1].y = swp.cy;
            apointl[2].x = 0;
            apointl[2].y = 0;
            GpiBitBlt(hpsMem, hpsToPrint, 3L, apointl, ROP_SRCCOPY, BBO_IGNORE);
            GpiSetBitmap(hpsMem, NULLHANDLE);
            GpiSetBitmap(hpsPrnMem, hbmToPrint);
            GpiBitBlt(hpsPrn, hpsPrnMem, 3L, apointl, ROP_SRCCOPY, BBO_IGNORE);
            GpiSetBitmap(hpsMem, NULLHANDLE);
            GpiDeleteBitmap(hbmToPrint);
            GpiDestroyPS(hpsMem);
            GpiDestroyPS(hpsPrnMem);
            DevCloseDC(hdcMem);
            DevCloseDC(hdcPrnMem);
           
            /* end print job */
            SpoolEndSpool(&usJobId);

            /* restore arrow pointer an set old application title bar text */
            WinSetPointer(HWND_DESKTOP, hptrSystem);
            WinSetWindowText(hwndFrame, (PSZ)"Hello queue/2");
          }
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
