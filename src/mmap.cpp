/*
 * mmap.cpp
 *
 *  Created on: 17/04/2013
 *      Author: nicholas
 */

#include "mmap.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "util.h"
#include <cstring>
#include <unistd.h>

fd_t::fd_t(const char* name, int flags)
 : fd(open(name, flags))
{
	throw_if(fd == -1, strerror(errno));
}

fd_t::operator int() const
{
	return fd;
}

fd_t::~fd_t()
{
	close(fd);
}

mmap_t::mmap_t(const char* filename, std::size_t size)
 : fd(filename, O_RDONLY), addr(nullptr), size(size)
{
	addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
	throw_if(addr == MAP_FAILED, strerror(errno));
}

mmap_t::operator void*() const
{
	return addr;
}

mmap_t::~mmap_t()
{
	munmap(addr, size);
}

