/* porttime.h -- portable interface to millisecond timer */

/* CHANGE LOG FOR PORTTIME
  10-Jun-03 Mark Nelson & RBD
    boost priority of timer thread in ptlinux.c implementation
 */

/* Should there be a way to choose the source of time here? */

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    ptNoError = 0,
    ptHostError = -10000,
    ptAlreadyStarted,
    ptAlreadyStopped,
    ptInsufficientMemory
} PtError;


typedef long PtTimestamp;

typedef void (PtCallback)( PtTimestamp timestamp, void *userData );


PtError Pt_Start(int resolution, PtCallback *callback, void *userData);
PtError Pt_Stop();
int Pt_Started();
PtTimestamp Pt_Time();

#ifdef __cplusplus
}
#endif
