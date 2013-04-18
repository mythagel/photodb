/*
 * photo.h
 *
 *  Created on: 17/04/2013
 *      Author: nicholas
 */

#ifndef PHOTO_H_
#define PHOTO_H_
#include <string>
#include <ostream>
#include "timestamp.h"

struct dim
{
	long width;
	long height;

	dim();
	dim(long width, long height);
	dim(const std::string& s);

	std::string str() const;

	bool operator<(const dim& o) const;
	bool operator==(const dim& o) const;
};

struct photo_t
{
	std::string file_name;
	std::string path;

	uint64_t size;
	timestamp_t mtime;

	timestamp_t timestamp;
	std::string checksum;

	dim pixel_size;
	dim exif_size;

	photo_t(const std::string& name, const std::string& path);

	std::string full_filename() const;
};

std::ostream& operator<<(std::ostream& os, const photo_t& photo);

#endif /* PHOTO_H_ */
