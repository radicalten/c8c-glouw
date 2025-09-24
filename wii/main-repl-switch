#define HW_RVL 1

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

#define NEXT switch(*ip++) {case 0: goto inst0; case 1: goto inst1; \
                            case 2: goto inst2; case 3: goto inst3; \
                            case 4: goto inst4; case 5: goto inst5; \
                           }
#define guard(n) asm("#" #n)

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth-20,rmode->xfbHeight-20,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	//SYS_STDIO_Report(true);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Clear the framebuffer
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();


	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );
	printf("\x1b[2;0H");

  u64 start_time_slow = gettick(); // Get start time in ticks
  
  static int prog[] = {0,1,0,2,0,3,0,4,0,5};
  int *ip=prog;
  int    count = 100000000;

  NEXT;

 inst0:
  guard(0);
  NEXT;
 inst1:
  guard(1);
  NEXT;
 inst2:
  guard(2);
  NEXT;
 inst3:
  guard(3);
  NEXT;
 inst4:
  guard(4);
  NEXT;
 inst5:
  if (count>0) {
    count--;
    ip=prog;
if (count % 10000000 == 0){	  // checks every 10M loop iterations using moduluo op
	u64 end_time_slow = gettick();   // Get end time in ticks
	u64 diff_ticks_slow = diff_ticks(start_time_slow, end_time_slow);
	u64	num1_ticks = ticks_to_cycles(diff_ticks_slow);
	u64 milliseconds_slow = ticks_to_millisecs(diff_ticks_slow);
	printf("Execution took: %llu milliseconds (ms)\n\n", milliseconds_slow);
	printf("cycles: %llu \n\n", num1_ticks);	
 	u64 start_time_slow = gettick();
 }
    
    NEXT;
  }
  else
    exit(0);
  

	while(1) {

		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = WPAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if ( pressed & WPAD_BUTTON_HOME ) exit(0);

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}

// 15.72 fps 0.0001s or .0000729 cycles
//all tests done with JIT ARM64, HLE audio, no audio backend 

// Update main-repl-switch.c #146
// Execution took: 7637 miliseconds (ms)
// cycles: 5371188024000
// Execution took: 14736 miliseconds (ms)
// cycles: 10742787048000
// Execution took: 16580 miliseconds (ms)
// cycles: 12087060204000

//Update main-call.c #141 
// Execution took: 15578 miliseconds (ms)
// cycles: 11356538676000
// Execution took: 21906 miliseconds (ms)
// cycles: 15969778800000
// Execution took: 23489 miliseconds (ms)
// cycles: 17123676660000

//Update main-switch.c #121
// Execution took: 13419 miliseconds (ms)
// cycles: 9782515716000
// Execution took: 15098 miliseconds (ms)
// cycles: 11006551188000
// Execution took: 16777 miliseconds (ms)
// cycles: 12230582664000


//if (count % 10000000 == 0){ main-repl-switch.c
//(5371188024000/10000000)/7637 = 70.33 cycles per dispatch (lower is better)
//(12087060204000/10000000)/16580 = 72.90 cycles per dispatch

//if (count % 10000000 == 0){ main-switch.c
//(11006551188000/10000000)/15098 = 72.90 cycles per dispatch (lower is better)
//(12230582664000/10000000)/16777 = 72.90 cycles per dispatch

//if (count % 10000000 == 0){ main-call.c	  
//(11356538676000/10000000)/15578 = 72.90 cycles per dispatch (lower is better)
//(17123676660000/10000000)/23489 = 72.90 cycles per dispatch

//either these results are truly identical... or there is something seriously wrong with my testing
//main-repl-switch had one result that was different from 72.90 (70.33)--which just appears to be an artifact of the 729Mhz cpu 


/*
minimal setup:
#include <ogc/lwp_watchdog.h>

u64 start_time_slow = gettick(); // Get start time in ticks
loop()
if (count % 10000000 == 0){	  // checks every 10M loop iterations using moduluo op
	u64 end_time_slow = gettick();   // Get end time in ticks
	u64 diff_ticks_slow = diff_ticks(start_time_slow, end_time_slow);
	u64	num1_ticks = ticks_to_cycles(diff_ticks_slow);
	u64 milliseconds_slow = ticks_to_millisecs(diff_ticks_slow);
	printf("Execution took: %llu milliseconds (ms)\n\n", milliseconds_slow);
	printf("cycles: %llu \n\n", num1_ticks);	
 	u64 start_time_slow = gettick();
 }
*/


// https://www.complang.tuwien.ac.at/forth/threading/
/*
You can compute the cycles per dispatch as
cycles = measured user time * clock frequency in MHz / 1000
*/


// https://github.com/devkitPro/libogc/blob/master/gc/ogc/lwp_watchdog.h
/*
#ifndef __LWP_WATCHDOG_H__
#define __LWP_WATCHDOG_H__

#include <gctypes.h>
#include <gcbool.h>

#include "lwp_queue.h"
#include <time.h>

#if defined(HW_RVL)
	#define TB_BUS_CLOCK				243000000u
	#define TB_CORE_CLOCK				729000000u
#elif defined(HW_DOL)
	#define TB_BUS_CLOCK				162000000u
	#define TB_CORE_CLOCK				486000000u
#endif
#define TB_TIMER_CLOCK				(TB_BUS_CLOCK/4000)			//4th of the bus frequency

#define TB_SECSPERMIN				60
#define TB_MINSPERHR				60
#define TB_MONSPERYR				12
#define TB_DAYSPERYR				365
#define TB_HRSPERDAY				24
#define TB_SECSPERDAY				(TB_SECSPERMIN*TB_MINSPERHR*TB_HRSPERDAY)
#define TB_SECSPERNYR				(365*TB_SECSPERDAY)
								
#define TB_MSPERSEC					1000
#define TB_USPERSEC					1000000
#define TB_NSPERSEC					1000000000
#define TB_NSPERMS					1000000
#define TB_NSPERUS					1000
#define TB_USPERTICK				10000

#define ticks_to_cycles(ticks)		((((u64)(ticks)*(u64)((TB_CORE_CLOCK*2)/TB_TIMER_CLOCK))/2))
#define ticks_to_secs(ticks)		(((u64)(ticks)/(u64)(TB_TIMER_CLOCK*1000)))
#define ticks_to_millisecs(ticks)	(((u64)(ticks)/(u64)(TB_TIMER_CLOCK)))
#define ticks_to_microsecs(ticks)	((((u64)(ticks)*8)/(u64)(TB_TIMER_CLOCK/125)))
#define ticks_to_nanosecs(ticks)	((((u64)(ticks)*8000)/(u64)(TB_TIMER_CLOCK/125)))

#define tick_microsecs(ticks)		((((u64)(ticks)*8)/(u64)(TB_TIMER_CLOCK/125))%TB_USPERSEC)
#define tick_nanosecs(ticks)		((((u64)(ticks)*8000)/(u64)(TB_TIMER_CLOCK/125))%TB_NSPERSEC)

#define secs_to_ticks(sec)			((u64)(sec)*(TB_TIMER_CLOCK*1000))
#define millisecs_to_ticks(msec)	((u64)(msec)*(TB_TIMER_CLOCK))
#define microsecs_to_ticks(usec)	(((u64)(usec)*(TB_TIMER_CLOCK/125))/8)
#define nanosecs_to_ticks(nsec)		(((u64)(nsec)*(TB_TIMER_CLOCK/125))/8000)

#define diff_ticks(tick0,tick1)		(((u64)(tick1)<(u64)(tick0))?((u64)-1-(u64)(tick0)+(u64)(tick1)):((u64)(tick1)-(u64)(tick0)))

#define LWP_WD_INACTIVE				0
#define LWP_WD_INSERTED				1
#define LWP_WD_ACTIVE				2
#define LWP_WD_REMOVE				3
								
#define LWP_WD_FORWARD				0
#define LWP_WD_BACKWARD				1
								
#define LWP_WD_NOTIMEOUT			0

#define LWP_WD_ABS(x)				((s64)(x)>0?(s64)(x):-((s64)(x)))

#ifdef __cplusplus
extern "C" {
#endif

extern vu32 _wd_sync_level;
extern vu32 _wd_sync_count;
extern u32 _wd_ticks_since_boot;

extern lwp_queue _wd_ticks_queue;

extern u32 gettick(void);
extern u64 gettime(void);
extern void settime(u64);

u32 diff_sec(u64 start,u64 end);
u32 diff_msec(u64 start,u64 end);
u32 diff_usec(u64 start,u64 end);
u32 diff_nsec(u64 start,u64 end);

typedef void (*wd_service_routine)(void *);

typedef struct _wdcntrl {
	lwp_node node;
	u64 start;
	u32 id;
	u32 state;
	u64 fire;
	wd_service_routine routine;
	void *usr_data;
} wd_cntrl;

void __lwp_watchdog_init();
void __lwp_watchdog_settimer(wd_cntrl *wd);
void __lwp_wd_insert(lwp_queue *header,wd_cntrl *wd);
u32 __lwp_wd_remove(lwp_queue *header,wd_cntrl *wd);
void __lwp_wd_tickle(lwp_queue *queue);
void __lwp_wd_adjust(lwp_queue *queue,u32 dir,s64 interval);

#ifdef __cplusplus
	}
#endif

#endif

*/
