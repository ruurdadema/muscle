/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTimeUtilityFunctions_h
#define MuscleTimeUtilityFunctions_h

#include <time.h>

#include "support/MuscleSupport.h"
#include "syslog/SysLog.h"  // for MUSCLE_TIMEZONE_*
#include "util/TimeUnitConversionFunctions.h"

#ifndef WIN32
# include <sys/time.h>
#endif

#if defined(TARGET_PLATFORM_XENOMAI) && !defined(MUSCLE_AVOID_XENOMAI)
# include "native/timer.h"
#endif

namespace muscle {

/** @defgroup timeutilityfunctions The TimeUtilityFunctions function API
 *  These functions are all defined in TimeUtilityFunctions.h, and are stand-alone
 *  inline functions that do various time-related calculations
 *  @{
 */

/** A value that GetPulseTime() can return to indicate that Pulse() should never be called. */
#define MUSCLE_TIME_NEVER ((uint64)-1) // (9223372036854775807LL)

/** Returns the current real-time clock time as a uint64.  The returned value is expressed
 *  as microseconds since the beginning of the year 1970.
 *  @param timeType if left as MUSCLE_TIMEZONE_UTC (the default), the value returned will be the current UTC time.
 *                  If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, the returned value will include the proper
 *                  offset to make the time be the current time as expressed in this machine's local time zone.
 *                  For any other value, the behaviour of this function is undefined.
 *  @note that the values returned by this function are NOT guaranteed to be monotonically increasing!
 *        Events like leap seconds, the user changing the system time, or even the OS tweaking the system
 *        time automatically to eliminate clock drift may cause this value to decrease occasionally!
 *        If you need a time value that is guaranteed to never decrease, you may want to call GetRunTime64() instead.
 */
uint64 GetCurrentTime64(uint32 timeType=MUSCLE_TIMEZONE_UTC);

/** Returns a current time value, in microseconds, that is guaranteed never to decrease.  The absolute
 *  values returned by this call are undefined, so you should only use it for measuring relative time
 *  (i.e. how much time has passed between two events).  For a "wall clock" type of result with
 *  a well-defined time-base, you can call GetCurrentTime64() instead.
 */
uint64 GetRunTime64();

/** Set an offset-value that you want to be added to the values returned by GetRunTime64() for this process.
  * Not usually necessary, but this can be useful if you want to simulate different clocks while testing on a single host.
  * @param offset The offset (in microseconds) that GetRunTime64() should add to all values it returns in the future.
  */
void SetPerProcessRunTime64Offset(int64 offset);

/** Returns the offset currently being added to all values returned by GetRunTime64().  Default value is zero. */
int64 GetPerProcessRunTime64Offset();

/** Given a run-time value, returns the equivalent current-time value.
  * @param runTime64 A run-time value, e.g. as returned by GetRunTime64().
  * @param timeType if left as MUSCLE_TIMEZONE_UTC (the default), the value returned will be the equivalent UTC time.
  *                 If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, the returned value will include the proper
  *                 offset to make the time be the equivalent time as expressed in this machine's local time zone.
  *                 For any other value, the behaviour of this function is undefined.
  * @returns a current-time value, as might have been returned by GetCurrentTime64(timeType) at the specified run-time.
  * @note The values returned by this function are only approximate, and may differ slightly from one call to the next.
  *       Of course they will differ significantly if the system's real-time clock value is changed by the user.
  */
inline uint64 GetCurrentTime64ForRunTime64(uint64 runTime64, uint32 timeType=MUSCLE_TIMEZONE_UTC) {return GetCurrentTime64(timeType)+(runTime64-GetRunTime64());}

/** Given a current-time value, returns the equivalent run-time value.
  * @param currentTime64 A current-time value, e.g. as returned by GetCurrentTime64(timeType).
  * @param timeType if left as MUSCLE_TIMEZONE_UTC (the default), the (currentTime64) argument will be interpreted as a UTC time.
  *                 If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, the (currentTime64) argument will be interpreted as
  *                 a current-time in this machine's local time zone.
  *                 For any other value, the behaviour of this function is undefined.
  * @returns a run-time value, as might have been returned by GetRunTime64() at the specified current-time.
  * @note The values returned by this function are only approximate, and may differ slightly from one call to the next.
  *       Of course they will differ significantly if the system's real-time clock value is changed by the user.
  */
inline uint64 GetRunTime64ForCurrentTime64(uint64 currentTime64, uint32 timeType=MUSCLE_TIMEZONE_UTC) {return GetRunTime64()+(currentTime64-GetCurrentTime64(timeType));}

/** Convenience function:  Won't return for a given number of microsends.
 *  @param micros The number of microseconds to wait for.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t Snooze64(uint64 micros);

/** Convenience function:  Returns true no more often than once every (interval).
 *  Useful if you are in a tight loop, but don't want e.g. more than one debug output line per second, or something like that.
 *  @param interval The minimum time that must elapse between two calls to this function returning true.
 *  @param lastTime A state variable.  Pass in the same reference every time you call this function.
 *  @return true iff it has been at least (interval) since the last time this function returned true, else false.
 */
inline bool OnceEvery(const struct timeval & interval, struct timeval & lastTime)
{
   uint64 now64 = GetRunTime64();
   struct timeval now;
   Convert64ToTimeVal(now64, now);
   if (!IsLessThan(now, lastTime))
   {
      lastTime = now;
      AddTimeVal(lastTime, interval);
      return true;
   }
   return false;
}

/** Convenience function:  Returns true no more than once every (interval).
 *  Useful if you are in a tight loop, but don't want e.g. more than one debug output line per second, or something like that.
 *  @param interval The minimum time that must elapse between two calls to this function returning true (in microseconds).
 *  @param lastTime A state variable.  Pass in the same reference every time you call this function.  (set to zero the first time)
 *  @return true iff it has been at least (interval) since the last time this function returned true, else false.
 */
inline bool OnceEvery(uint64 interval, uint64 & lastTime)
{
   const uint64 now = GetRunTime64();
   if (now >= lastTime+interval)
   {
      lastTime = now;
      return true;
   }
   return false;
}

/** This handy macro will print out, twice a second,
 *  the average number of times per second it is being called.
 */
#define PRINT_CALLS_PER_SECOND(x) \
{ \
   static uint32 count     = 0; \
   static uint64 startTime = 0; \
   static uint64 lastTime  = 0; \
   const uint64 now = GetRunTime64(); \
   if (startTime == 0) startTime = now; \
   count++; \
   if ((OnceEvery(500000, lastTime))&&(now>startTime)) printf("%s: " UINT64_FORMAT_SPEC "/s\n", x, (MICROS_PER_SECOND*((uint64)count))/(now-startTime)); \
}

/** @} */ // end of timeutilityfunctions doxygen group

} // end namespace muscle

#endif
