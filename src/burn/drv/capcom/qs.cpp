#include "cps.h"
/* QSound */
#include <3ds.h>

Thread threadHandle;
Handle threadEventStart;
Handle threadEventEnd;
int threadInited = 0;

void (*entry)(void);
void entryEnd(void) {}

// void (*entry)(void *);
// void *entryArgs;
//void entryEnd(void *args) {}

void threadMain(void *arg)
{
  while (1) {
      svcSignalEvent(threadEventEnd);
      svcWaitSynchronization(threadEventStart, U64_MAX);
      svcClearEvent(threadEventStart);
      if (entry == entryEnd) break;
      entry();
  }
}

#define STACKSIZE (32 * 1024)
void initThread() {
  svcCreateEvent(&threadEventStart, RESET_ONESHOT);
  svcCreateEvent(&threadEventEnd, RESET_ONESHOT);
  APT_SetAppCpuTimeLimit(80);
  threadHandle = threadCreate(threadMain, 0, STACKSIZE, 0x18, 1, true);
}

void waitThread() {
  svcWaitSynchronization(threadEventEnd, U64_MAX);
  svcClearEvent(threadEventEnd);
}

void startThread() {
  svcSignalEvent(threadEventStart);
}

void releaseThread() {
  svcSignalEvent(threadEventEnd);
}



static INT32 nQsndCyclesExtra;

static INT32 qsndTimerOver(INT32, INT32)
{
	ZetSetIRQLine(0xFF, ZET_IRQSTATUS_AUTO);

	return 0;
}

INT32 QsndInit(void)
{
	INT32 nRate = 11025;

	if (!threadInited) {
    threadInited = 1;
    initThread();
  }

	/* Init QSound z80 */
	if (QsndZInit())
		return 1;
	BurnTimerInit(qsndTimerOver, NULL);

	if (Cps1Qs == 1)
   {
		nCpsZ80Cycles = 6000000 * 100 / nBurnFPS;
		BurnTimerAttachZet(6000000);
	}
   else
   {
		nCpsZ80Cycles = 8000000 * 100 / nBurnFPS;
		BurnTimerAttachZet(8000000);
	}

	if (nBurnSoundRate >= 0)
		nRate = nBurnSoundRate;

	QscInit(nRate);		/* Init QSound chip */

	return 0;
}

void QsndSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
	QscSetRoute(nIndex, nVolume, nRouteDir);
}

void QsndReset(void)
{
	ZetOpen(0);
	BurnTimerReset();
	BurnTimerSetRetrig(0, 1.0 / 252.0);
	ZetClose();

	nQsndCyclesExtra = 0;
}

void QsndExit(void)
{
	QscExit();							/* Exit QSound chip */
	QsndZExit();
}

INT32 QsndScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA)
   {
		QsndZScan(nAction);				/* Scan Z80 */
		QscScan(nAction);				/* Scan QSound Chip */
	}

	return 0;
}

void QsndNewFrame_IMPL(void)
{
   ZetNewFrame();

   ZetOpen(0);
   ZetIdle(nQsndCyclesExtra);

   QscNewFrame();
}

void QsndNewFrame(void) {
	waitThread();
	entry = QsndNewFrame_IMPL;
	startThread();
}


void QsndEndFrame_IMPL(void)
{
   BurnTimerEndFrame(nCpsZ80Cycles);
   if (pBurnSoundOut)
      QscUpdate(nBurnSoundLen);

   nQsndCyclesExtra = ZetTotalCycles() - nCpsZ80Cycles;
   ZetClose();
}

void QsndEndFrame(void) {
	waitThread();
	entry = QsndEndFrame_IMPL;
	startThread();
}


void QsndSyncZ80_IMPL(void)
{
   int nCycles = (INT64)SekTotalCycles() * nCpsZ80Cycles / nCpsCycles;

   if (nCycles <= ZetTotalCycles())
      return;

   BurnTimerUpdate(nCycles);
}

void QsndSyncZ80(void) {
	waitThread();
	entry = QsndSyncZ80_IMPL;
	startThread();
}
