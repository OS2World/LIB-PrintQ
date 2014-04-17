/* hello.h: include file for Hello queue/2 */
/* Version 1.2 MT, (C) Peter Wansch, 1993  */
/* created: 93-5-7                         */
/* modified: 93-8-16                       */

/* windows, dialog boxes and message boxes */
#define WD_MAIN                 300
#define DB_PRODINFO             301
#define DB_QUERYPRINT           302
#define ID_MESSAGEBOX           303

/* menus and menu items */
#define SM_OPTIONS              304
#define MI_EXIT                 305
#define MI_INFO                 306
#define MI_SETUP                307
#define MI_PRINT                308
#define MI_ABORT                309

/* controls */
#define LB_QUEUES               310
#define PB_JOBPROP              311

/* timers */
#define TI_LOGO                 0

/* miscellaneous constants */
#define ID_NULL                 -1
#define LENGTH_STRING           256
#define RETURN_ERROR            1       /* return value for exit */
#define NUM_OF_LINES            24
#define STACK_SIZE              8192
#define WMP_JOB_DONE            WM_USER+1 /* print thread end message */
#define BACKOFF_TIME            10UL

/* function prototypes */
int main(void);
MRESULT EXPENTRY wpMain(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY dpProdInfo(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY dpQueryQInfo(HWND, ULONG, MPARAM, MPARAM);
void InitOut(void);
void PrintInfo(ULONG ulParam);
