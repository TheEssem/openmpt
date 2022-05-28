/*
 * mptTime.cpp
 * -----------
 * Purpose: Various time utility functions.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "mptTime.h"

#include "mptStringBuffer.h"

#if MPT_CXX_AT_LEAST(20) && !defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE)
#include <chrono>
#endif

#if MPT_OS_WINDOWS
#include <windows.h>
#if defined(MODPLUG_TRACKER)
#include <mmsystem.h>
#endif
#endif


OPENMPT_NAMESPACE_BEGIN


namespace mpt
{
namespace Date
{

#if defined(MODPLUG_TRACKER)

#if MPT_OS_WINDOWS

namespace ANSI
{

uint64 Now()
{
	FILETIME filetime;
	GetSystemTimeAsFileTime(&filetime);
	return ((uint64)filetime.dwHighDateTime << 32 | filetime.dwLowDateTime);
}

mpt::ustring ToUString(uint64 time100ns)
{
	constexpr std::size_t bufsize = 256;

	mpt::ustring result;

	FILETIME filetime;
	SYSTEMTIME systime;
	filetime.dwHighDateTime = (DWORD)(((uint64)time100ns) >> 32);
	filetime.dwLowDateTime = (DWORD)((uint64)time100ns);
	FileTimeToSystemTime(&filetime, &systime);

	TCHAR buf[bufsize];

	GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, &systime, TEXT("yyyy-MM-dd"), buf, bufsize);
	result.append(mpt::ToUnicode(mpt::String::ReadWinBuf(buf)));

	result.append(U_(" "));

	GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT, &systime, TEXT("HH:mm:ss"), buf, bufsize);
	result.append(mpt::ToUnicode(mpt::String::ReadWinBuf(buf)));

	result.append(U_("."));

	result.append(mpt::ufmt::dec0<3>((unsigned)systime.wMilliseconds));

	return result;

}

} // namespace ANSI

#endif // MPT_OS_WINDOWS

#endif // MODPLUG_TRACKER

#if MPT_CXX_BEFORE(20) || defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE) || defined(MPT_TIME_CTIME)

static int32 ToDaynum(int32 year, int32 month, int32 day)
{
	month = (month + 9) % 12;
	year = year - (month / 10);
	int32 daynum = year*365 + year/4 - year/100 + year/400 + (month*306 + 5)/10 + (day - 1);
	return daynum;
}

static void FromDaynum(int32 d, int32 & year, int32 & month, int32 & day)
{
	int64 g = d;
	int64 y,ddd,mi,mm,dd;

	y = (10000*g + 14780)/3652425;
	ddd = g - (365*y + y/4 - y/100 + y/400);
	if(ddd < 0)
	{
		y = y - 1;
		ddd = g - (365*y + y/4 - y/100 + y/400);
	}
	mi = (100*ddd + 52)/3060;
	mm = (mi + 2)%12 + 1;
	y = y + (mi + 2)/12;
	dd = ddd - (mi*306 + 5)/10 + 1;

	year = static_cast<int32>(y);
	month = static_cast<int32>(mm);
	day = static_cast<int32>(dd);
}

#endif

#if MPT_CXX_BEFORE(20) || defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE)

mpt::Date::Unix UnixFromUTC(UTC timeUtc)
{
	int32 daynum = ToDaynum(timeUtc.year, timeUtc.month, timeUtc.day);
	int64 seconds = static_cast<int64>(daynum - ToDaynum(1970, 1, 1)) * 24 * 60 * 60 + timeUtc.hours * 60 * 60 + timeUtc.minutes * 60 + timeUtc.seconds;
	return Unix{seconds};
}

mpt::Date::UTC UnixAsUTC(Unix tp)
{
	int64 tmp = tp.value;
	int64 seconds = tmp % 60; tmp /= 60;
	int64 minutes = tmp % 60; tmp /= 60;
	int64 hours   = tmp % 24; tmp /= 24;
	int32 year = 0, month = 0, day = 0;
	FromDaynum(static_cast<int32>(tmp) + ToDaynum(1970,1,1), year, month, day);
	mpt::Date::UTC result = {};
	result.year = year;
	result.month = month;
	result.day = day;
	result.hours = static_cast<int32>(hours);
	result.minutes = static_cast<int32>(minutes);
	result.seconds = static_cast<int64>(seconds);
	return result;
}

#endif

template <LogicalTimezone TZ>
static mpt::ustring ToShortenedISO8601Impl(mpt::Date::Gregorian<TZ> date)
{
	mpt::ustring result;
	mpt::ustring tz;
	if constexpr(TZ == LogicalTimezone::Unspecified)
	{
		tz = U_("");
	} else if constexpr(TZ == LogicalTimezone::UTC)
	{
		tz = U_("Z");
	} else
	{
		tz = U_("");
	}
	if(date.year == 0)
	{
		return result;
	}
	result += mpt::ufmt::dec0<4>(date.year);
	result += U_("-") + mpt::ufmt::dec0<2>(date.month);
	result += U_("-") + mpt::ufmt::dec0<2>(date.day);
	if(date.hours == 0 && date.minutes == 0 && date.seconds)
	{
		return result;
	}
	result += U_("T");
	result += mpt::ufmt::dec0<2>(date.hours) + U_(":") + mpt::ufmt::dec0<2>(date.minutes);
	if(date.seconds == 0)
	{
		return result + tz;
	}
	result += U_(":") + mpt::ufmt::dec0<2>(date.seconds);
	result += tz;
	return result;
}

mpt::ustring ToShortenedISO8601(mpt::Date::AnyGregorian date)
{
	return ToShortenedISO8601Impl(date);
}

mpt::ustring ToShortenedISO8601(mpt::Date::UTC date)
{
	return ToShortenedISO8601Impl(date);
}

#if defined(MPT_TIME_CTIME)

mpt::Date::Unix UnixFromUTCtm(tm timeUtc)
{
	int32 daynum = ToDaynum(timeUtc.tm_year+1900, timeUtc.tm_mon+1, timeUtc.tm_mday);
	int64 seconds = static_cast<int64>(daynum - ToDaynum(1970,1,1))*24*60*60 + timeUtc.tm_hour*60*60 + timeUtc.tm_min*60 + timeUtc.tm_sec;
	return mpt::Date::UnixFromSeconds(seconds);
}

tm UnixAsUTCtm(mpt::Date::Unix unixtime)
{
	int64 tmp = mpt::Date::UnixAsSeconds(unixtime);
	int64 seconds = tmp % 60; tmp /= 60;
	int64 minutes = tmp % 60; tmp /= 60;
	int64 hours   = tmp % 24; tmp /= 24;
	int32 year = 0, month = 0, day = 0;
	FromDaynum(static_cast<int32>(tmp) + ToDaynum(1970,1,1), year, month, day);
	tm result = {};
	result.tm_year = year - 1900;
	result.tm_mon = month - 1;
	result.tm_mday = day;
	result.tm_hour = static_cast<int32>(hours);
	result.tm_min = static_cast<int32>(minutes);
	result.tm_sec = static_cast<int32>(seconds);
	return result;
}

mpt::ustring ToShortenedISO8601(tm date)
{
	// We assume date in UTC here.
	// There are too many differences in supported format specifiers in strftime()
	// and strftime does not support reduced precision ISO8601 at all.
	// Just do the formatting ourselves.
	mpt::ustring result;
	mpt::ustring tz = U_("Z");
	if(date.tm_year == 0)
	{
		return result;
	}
	result += mpt::ufmt::dec0<4>(date.tm_year + 1900);
	if(date.tm_mon < 0 || date.tm_mon > 11)
	{
		return result;
	}
	result += U_("-") + mpt::ufmt::dec0<2>(date.tm_mon + 1);
	if(date.tm_mday < 1 || date.tm_mday > 31)
	{
		return result;
	}
	result += U_("-") + mpt::ufmt::dec0<2>(date.tm_mday);
	if(date.tm_hour == 0 && date.tm_min == 0 && date.tm_sec == 0)
	{
		return result;
	}
	if(date.tm_hour < 0 || date.tm_hour > 23)
	{
		return result;
	}
	if(date.tm_min < 0 || date.tm_min > 59)
	{
		return result;
	}
	result += U_("T");
	if(date.tm_isdst > 0)
	{
		tz = U_("+01:00");
	}
	result += mpt::ufmt::dec0<2>(date.tm_hour) + U_(":") + mpt::ufmt::dec0<2>(date.tm_min);
	if(date.tm_sec < 0 || date.tm_sec > 61)
	{
		return result + tz;
	}
	result += U_(":") + mpt::ufmt::dec0<2>(date.tm_sec);
	result += tz;
	return result;
}

#endif

} // namespace Date
} // namespace mpt



#ifdef MODPLUG_TRACKER

namespace Util
{

#if MPT_OS_WINDOWS

void MultimediaClock::Init()
{
	m_CurrentPeriod = 0;
}

void MultimediaClock::SetPeriod(uint32 ms)
{
	TIMECAPS caps = {};
	if(timeGetDevCaps(&caps, sizeof(caps)) != MMSYSERR_NOERROR)
	{
		return;
	}
	if((caps.wPeriodMax == 0) || (caps.wPeriodMin > caps.wPeriodMax))
	{
		return;
	}
	ms = std::clamp(mpt::saturate_cast<UINT>(ms), caps.wPeriodMin, caps.wPeriodMax);
	if(timeBeginPeriod(ms) != MMSYSERR_NOERROR)
	{
		return;
	}
	m_CurrentPeriod = ms;
}

void MultimediaClock::Cleanup()
{
	if(m_CurrentPeriod > 0)
	{
		if(timeEndPeriod(m_CurrentPeriod) != MMSYSERR_NOERROR)
		{
			// should not happen
			MPT_ASSERT_NOTREACHED();
		}
		m_CurrentPeriod = 0;
	}
}

MultimediaClock::MultimediaClock()
{
	Init();
}

MultimediaClock::MultimediaClock(uint32 ms)
{
	Init();
	SetResolution(ms);
}

MultimediaClock::~MultimediaClock()
{
	Cleanup();
}

uint32 MultimediaClock::SetResolution(uint32 ms)
{
	if(m_CurrentPeriod == ms)
	{
		return m_CurrentPeriod;
	}
	Cleanup();
	if(ms != 0)
	{
		SetPeriod(ms);
	}
	return GetResolution();
}

uint32 MultimediaClock::GetResolution() const
{
	return m_CurrentPeriod;
}

uint32 MultimediaClock::Now() const
{
	return timeGetTime();
}

uint64 MultimediaClock::NowNanoseconds() const
{
	return (uint64)timeGetTime() * (uint64)1000000;
}

#endif // MPT_OS_WINDOWS

} // namespace Util

#endif // MODPLUG_TRACKER


OPENMPT_NAMESPACE_END
