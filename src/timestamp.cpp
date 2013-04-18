/*
 * timestamp.cpp
 *
 *  Created on: 10/04/2013
 *      Author: nicholas
 */

#include "timestamp.h"
#include <sstream>
#include <tuple>
#include <iomanip>
#include <cstring>
#include <stdexcept>

timestamp_t::timestamp_t()
 : year(), month(), day(), hour(), minute(), second()
{
}

timestamp_t::timestamp_t(const time_t& time)
 : year(), month(), day(), hour(), minute(), second()
{
	struct tm res;
	localtime_r(&time, &res);
	year = res.tm_year + 1900;
	month = res.tm_mon;
	day = res.tm_mday;
	hour = res.tm_hour;
	minute = res.tm_min;
	second = res.tm_sec;
}

timestamp_t::timestamp_t(const std::string& time)
 : year(), month(), day(), hour(), minute(), second()
{
	if(!time.empty())
	{
		bool res = sscanf(time.c_str(), "%04u-%02u-%02u %02u:%02u:%02u.000", &year, &month, &day, &hour, &minute, &second) == 6;
		if(!res)
			res = sscanf(time.c_str(), "%04u:%02u:%02u %02u:%02u:%02u", &year, &month, &day, &hour, &minute, &second) == 6;
		if(!res)
			throw std::runtime_error("unknown timestamp format: " + time);
	}
}

std::string timestamp_t::str() const
{
	std::ostringstream s;
	s << *this;
	return s.str();
}

bool timestamp_t::operator<(const timestamp_t& o) const
{
	return std::tie(year, month, day, hour, minute, second) < std::tie(o.year, o.month, o.day, o.hour, o.minute, o.second);
}

std::ostream& operator<<(std::ostream& os, const timestamp_t& ts)
{
	return os << std::setw(4) << std::setfill('0') << ts.year << '-'
			  << std::setw(2) << std::setfill('0') << ts.month << '-'
			  << std::setw(2) << std::setfill('0') << ts.day << ' '
			  << std::setw(2) << std::setfill('0') << ts.hour << ':'
			  << std::setw(2) << std::setfill('0') << ts.minute << ':'
			  << std::setw(2) << std::setfill('0') << ts.second
			  << ".000";
}
