/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

# include <linux/cache.h>
# include <linux/seqlock.h>
# include <linux/math64.h>
# include <linux/time64.h>

extern struct timezone sys_tz;

int get_timespec64(struct timespec64 *ts,
		const struct __kernel_timespec __user *uts);
int put_timespec64(const struct timespec64 *ts,
		struct __kernel_timespec __user *uts);
int get_itimerspec64(struct itimerspec64 *it,
			const struct __kernel_itimerspec __user *uit);
int put_itimerspec64(const struct itimerspec64 *it,
			struct __kernel_itimerspec __user *uit);

/*
 * mktime64 - Converts date to seconds.
 * Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * A leap second can be indicated by calling this function with sec as
 * 60 (allowable under ISO 8601).  The leap second is treated the same
 * as the following second since they don't exist in UNIX time.
 *
 * An encoding of midnight at the end of the day as 24:00:00 - ie. midnight
 * tomorrow - (allowable under ISO 8601) is supported.
 */
static inline time64_t mktime64(const unsigned int year0, const unsigned int mon0,
				const unsigned int day, const unsigned int hour,
				const unsigned int min, const unsigned int sec)
{
	unsigned int mon = mon0, year = year0;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (int) (mon -= 2)) {
		mon += 12;	/* Puts Feb last since it has leap day */
		year -= 1;
	}

	return ((((time64_t)
		  (year/4 - year/100 + year/400 + 367*mon/12 + day) +
		  year*365 - 719499
	    )*24 + hour /* now have hours - midnight tomorrow handled here */
	  )*60 + min /* now have minutes */
	)*60 + sec; /* finally seconds */
}

/* Some architectures do not supply their own clocksource.
 * This is mainly the case in architectures that get their
 * inter-tick times by reading the counter on their interval
 * timer. Since these timers wrap every tick, they're not really
 * useful as clocksources. Wrapping them to act like one is possible
 * but not very efficient. So we provide a callout these arches
 * can implement for use with the jiffies clocksource to provide
 * finer then tick granular time.
 */
#ifdef CONFIG_ARCH_USES_GETTIMEOFFSET
extern u32 (*arch_gettimeoffset)(void);
#endif

struct itimerval;
extern int do_setitimer(int which, struct itimerval *value,
			struct itimerval *ovalue);
extern int do_getitimer(int which, struct itimerval *value);

extern long do_utimes(int dfd, const char __user *filename, struct timespec64 *times, int flags);

/*
 * Similar to the struct tm in userspace <time.h>, but it needs to be here so
 * that the kernel source is self contained.
 */
struct tm {
	/*
	 * the number of seconds after the minute, normally in the range
	 * 0 to 59, but can be up to 60 to allow for leap seconds
	 */
	int tm_sec;
	/* the number of minutes after the hour, in the range 0 to 59*/
	int tm_min;
	/* the number of hours past midnight, in the range 0 to 23 */
	int tm_hour;
	/* the day of the month, in the range 1 to 31 */
	int tm_mday;
	/* the number of months since January, in the range 0 to 11 */
	int tm_mon;
	/* the number of years since 1900 */
	long tm_year;
	/* the number of days since Sunday, in the range 0 to 6 */
	int tm_wday;
	/* the number of days since January 1, in the range 0 to 365 */
	int tm_yday;
};

void time64_to_tm(time64_t totalsecs, int offset, struct tm *result);

# include <linux/time32.h>

static inline bool itimerspec64_valid(const struct itimerspec64 *its)
{
	if (!timespec64_valid(&(its->it_interval)) ||
		!timespec64_valid(&(its->it_value)))
		return false;

	return true;
}

/**
 * time_after32 - compare two 32-bit relative times
 * @a:	the time which may be after @b
 * @b:	the time which may be before @a
 *
 * time_after32(a, b) returns true if the time @a is after time @b.
 * time_before32(b, a) returns true if the time @b is before time @a.
 *
 * Similar to time_after(), compare two 32-bit timestamps for relative
 * times.  This is useful for comparing 32-bit seconds values that can't
 * be converted to 64-bit values (e.g. due to disk format or wire protocol
 * issues) when it is known that the times are less than 68 years apart.
 */
#define time_after32(a, b)	((s32)((u32)(b) - (u32)(a)) < 0)
#define time_before32(b, a)	time_after32(a, b)

/**
 * time_between32 - check if a 32-bit timestamp is within a given time range
 * @t:	the time which may be within [l,h]
 * @l:	the lower bound of the range
 * @h:	the higher bound of the range
 *
 * time_before32(t, l, h) returns true if @l <= @t <= @h. All operands are
 * treated as 32-bit integers.
 *
 * Equivalent to !(time_before32(@t, @l) || time_after32(@t, @h)).
 */
#define time_between32(t, l, h) ((u32)(h) - (u32)(l) >= (u32)(t) - (u32)(l))

# include <vdso/time.h>

#endif
