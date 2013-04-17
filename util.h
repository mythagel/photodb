/*
 * util.h
 *
 *  Created on: 17/04/2013
 *      Author: nicholas
 */

#ifndef UTIL_H_
#define UTIL_H_
#include <stdexcept>

template <typename ex = std::runtime_error>
void throw_if(bool cond, const std::string& what)
{
	if(cond)
		throw ex(what);
}

#endif /* UTIL_H_ */
