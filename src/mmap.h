/*
 * mmap.h
 *
 *  Created on: 17/04/2013
 *      Author: nicholas
 */

#ifndef MMAP_H_
#define MMAP_H_
#include <cstddef>

class fd_t
{
private:
	int fd;
public:
	fd_t(const char* name, int flags);
	operator int() const;
	~fd_t();
};
class mmap_t
{
private:
	fd_t fd;
	void* addr;
	std::size_t size;
public:
	mmap_t(const char* filename, std::size_t size);
	operator void*() const;
	~mmap_t();
};

#endif /* MMAP_H_ */
