/*
 * timestamp.h
 *
 *  Created on: 10/04/2013
 *      Author: nicholas
 */

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_
#include <ctime>
#include <ostream>
#include <string>

struct timestamp_t
{
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;

	timestamp_t();
	explicit timestamp_t(const time_t& time);
	explicit timestamp_t(const std::string& time);

	std::string str() const;
	bool operator<(const timestamp_t&) const;
};

std::ostream& operator<<(std::ostream& os, const timestamp_t& ts);

#endif /* TIMESTAMP_H_ */
