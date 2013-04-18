/*
 * photo.cpp
 *
 *  Created on: 17/04/2013
 *      Author: nicholas
 */

#include "photo.h"
#include <sstream>
#include <tuple>

dim::dim()
 : width(), height()
{
}

dim::dim(long width, long height)
 : width(width), height(height)
{
}

dim::dim(const std::string& s)
 : width(), height()
{
	std::istringstream ss(s);
	ss >> width;
	ss.ignore();
	ss >> height;
}

std::string dim::str() const
{
	std::ostringstream s;
	s << width << "," << height;
	return s.str();
}

bool dim::operator<(const dim& o) const
{
	return std::tie(width, height) < std::tie(o.width, o.height);
}
bool dim::operator==(const dim& o) const
{
	return std::tie(width, height) == std::tie(o.width, o.height);
}

photo_t::photo_t(const std::string& name, const std::string& path)
 : file_name(name), path(path), size(0)
{
}

std::string photo_t::full_filename() const
{
	return path + '/' + file_name;
}

std::ostream& operator<<(std::ostream& os, const photo_t& photo)
{
	os << "{\n";
	os << "   \"file_name\":\"" << photo.file_name << "\",\n";
	os << "   \"path\":\"" << photo.path << "\",\n";
	os << "   \"size\":\"" << photo.size << "\",\n";
	os << "   \"mtime\":\"" << photo.mtime << "\",\n";
	os << "   \"timestamp\":\"" << photo.timestamp << "\",\n";
	os << "   \"checksum\":\"" << photo.checksum << "\",\n";
	os << "   \"pixel_size\":\"" << photo.pixel_size.width << "," << photo.pixel_size.height << "\",\n";
	os << "   \"exif_size\":\"" << photo.exif_size.width << "," << photo.exif_size.height << "\"\n";
	os << "}";
	return os;
}
