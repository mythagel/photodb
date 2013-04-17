/*
 * photodb.cc
 *
 *  Created on: 24/03/2013
 *      Author: nicholas
 */
#include "timestamp.h"
#include <iostream>
#include <dirent.h>
#include <memory>
#include <vector>
#include <sstream>

#include <exiv2/exiv2.hpp>

#include "sha1.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <chrono>
#include <tuple>
#include <thread>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <ctime>

#include "db.h"

//template<typename T, typename... Args>
//std::unique_ptr<T> make_unique(Args&&... args)
//{
//    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
//}

struct dim
{
	long width;
	long height;

	dim()
	 : width(), height()
	{
	}

	dim(long width, long height)
	 : width(width), height(height)
	{
	}

	dim(const std::string& s)
	 : width(), height()
	{
		std::istringstream ss(s);
		ss >> width;
		ss.ignore();
		ss >> height;
	}

	std::string str() const
	{
		std::ostringstream s;
		s << width << "," << height;
		return s.str();
	}

	bool operator<(const dim& o) const
	{
		return std::tie(width, height) < std::tie(o.width, o.height);
	}
	bool operator==(const dim& o) const
	{
		return std::tie(width, height) == std::tie(o.width, o.height);
	}
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

	photo_t(const std::string& name, const std::string& path)
	 : file_name(name), path(path), size(0)
	{
	}

	std::string full_filename() const
	{
		return path + '/' + file_name;
	}

//	std::tuple<std::string, std::string, uint64_t, std::string, std::string, std::string, std::string> to_tuple() const
//	{
//		std::ostringstream smtime;
//		smtime << mtime;
//		std::ostringstream pixel;
//		pixel << pixel_size.width << "," << pixel_size.height;
//		std::ostringstream exif;
//		exif << exif_size.width << "," << exif_size.height;
//
//		return std::make_tuple(file_name, path, size, smtime.str(), checksum, pixel.str(), exif.str());
//	}
};

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

template <typename Fn>
bool enumerate_directory(const std::string& path, Fn func)
{
	auto delete_dir = [](DIR* d){ closedir(d); };
	std::unique_ptr<DIR, decltype(delete_dir)> dir(opendir(path.c_str()), delete_dir);
	if (!dir)
	{
		std::cerr << "Unable to open directory '" << path << "'\n";
		return false;
	}

	dirent entry;
	dirent *result;
	for (int res = readdir_r(dir.get(), &entry, &result); result != nullptr && res == 0; res = readdir_r(dir.get(), &entry, &result))
	{
		std::string name = entry.d_name;
		if(entry.d_type == DT_DIR)
		{
			if(name != "." && name != "..")
				if(!enumerate_directory(path + '/' + name, func))
					return false;
		}
		else if(entry.d_type == DT_REG)
		{
			func({name, path});
		}
	}

	return true;
}

void exif(photo_t& photo)
{
	try
	{
		auto image = Exiv2::ImageFactory::open(photo.full_filename());

		if (image.get())
		{
			image->readMetadata();
			photo.pixel_size = {image->pixelWidth(), image->pixelHeight()};

			Exiv2::ExifData &exifData = image->exifData();
			if (!exifData.empty())
			{
				{
					auto x = exifData.findKey(Exiv2::ExifKey("Exif.Photo.PixelXDimension"));
					if(x == exifData.end())
						x = exifData.findKey(Exiv2::ExifKey("Exif.Image.ImageWidth"));

					auto y = exifData.findKey(Exiv2::ExifKey("Exif.Photo.PixelYDimension"));
					if(y == exifData.end())
						y = exifData.findKey(Exiv2::ExifKey("Exif.Image.ImageLength"));

					if(x != exifData.end() && y != exifData.end())
					{
						photo.exif_size = {x->value().toLong(), y->value().toLong()};
					}
				}

				auto datetime = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
				if(datetime == exifData.end())
					datetime = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTime"));
				if(datetime == exifData.end())
					datetime = exifData.findKey(Exiv2::ExifKey("Exif.Image.DateTime"));

				if(datetime != exifData.end())
				{
					std::string timestamp = datetime->value().toString();
					for(auto x = begin(timestamp); x != end(timestamp); )
						if(*x == ' ')
							x = timestamp.erase(x);
						else
							++x;
					photo.timestamp = timestamp_t{timestamp};
				}
			}
		}
	}
	catch(const Exiv2::BasicError<char>& ex)
	{
		std::cerr << ex << "\n";
	}

}

bool checksum(photo_t& photo)
{
	int fd = open(photo.full_filename().c_str(), O_RDONLY);
	if (fd == -1)
		return false;

	void* addr = mmap(nullptr, photo.size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
	{
		close(fd);
		return false;
	}

	unsigned char hash[20];
	sha1::calc(addr, photo.size, hash);

	char hexstring[41];
	sha1::toHexString(hash, hexstring);
	photo.checksum = hexstring;

	munmap(addr, photo.size);
	close(fd);
	return true;
}

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		std::cerr << argv[0] << " src_folder dest_folder\n";
		return false;
	}

	std::string src = argv[1];
	std::string dest = argv[2];

	db_t db{src + "/photo.db"};
	db.execute("CREATE TABLE IF NOT EXISTS photos (file_name TEXT, path TEXT, size INTEGER, mtime TEXT, timestamp TEXT, checksum TEXT, pixel_size TEXT, exif_size TEXT)");

	db_t::statement_t<std::string, std::string, uint64_t, std::string, std::string, std::string, std::string, std::string> insert_photo{db, "INSERT INTO photos VALUES (?, ?, ?, ?, ?, ?, ?, ?)"};
	db_t::statement_t<std::string, std::string, uint64_t, std::string> photo_exists{db, "SELECT timestamp, checksum, pixel_size, exif_size FROM photos WHERE file_name = ? AND path = ? AND size = ? and mtime = ?"};

	std::vector<std::shared_ptr<photo_t> > photo_list;
	if(!enumerate_directory(src, [&photo_list](photo_t photo)
	{
		struct stat sb;
		if(stat(photo.full_filename().c_str(), &sb) != 0)
		{
			std::cerr << photo.full_filename() << ": Unable to stat()\n";
			return;
		}

		photo.size = sb.st_size;
		photo.mtime = timestamp_t{sb.st_mtime};
		photo_list.emplace_back(new photo_t(photo));
	}))
		return 1;

	std::cerr << photo_list.size() << " Files.\n";

	// update db.
	db.execute("PRAGMA synchronous = OFF");

	std::cout << std::string(80, '-') << std::endl;
	size_t index(0);
	size_t count = photo_list.size();
	for(auto& photo : photo_list)
	{
		bool new_photo(true);
		auto x = [&photo, &new_photo](const std::tuple<std::string, std::string, std::string, std::string>& t){
			photo->timestamp = timestamp_t{std::get<0>(t)};
			photo->checksum = std::get<1>(t);
			photo->pixel_size = {std::get<2>(t)};
			photo->exif_size = {std::get<3>(t)};
			new_photo = false;
		};
		photo_exists.query<decltype(x), std::string, std::string, std::string, std::string>(x, photo->file_name, photo->path, photo->size, photo->mtime.str());

		if(new_photo)
		{
			exif(*photo);
			checksum(*photo);

			insert_photo.execute(photo->file_name, photo->path, photo->size, photo->mtime.str(), photo->timestamp.str(), photo->checksum, photo->pixel_size.str(), photo->exif_size.str());
		}

//		std::cout /*<< (new_photo ? "NEW " : "DB  ")*/ << *photo << "\n";
		++index;
		std::cout << "\r" << std::string((static_cast<double>(index) / count) * 80, '=');
	}
	std::cout << "\n";

//	std::map<std::string, std::vector<std::shared_ptr<photo_t> > > name_dups;
//	std::map<std::string, std::vector<std::shared_ptr<photo_t> > > checksum_dups;
//	for(auto& photo : photo_list)
//	{
//		name_dups[photo->file_name].push_back(photo);
//		checksum_dups[photo->checksum].push_back(photo);
//	}
//
//	std::cout << "Name dups:\n";
//	for(const auto& x : name_dups)
//	{
//		if(x.second.size() <= 1)
//			continue;
//
//		std::cout << x.first << " {\n";
//		for(auto& l : x.second)
//			std::cout << *l << "\n";
//		std::cout << "}\n";
//	}
//
//	std::cout << "Checksum dups:\n";
//	for(auto& x : checksum_dups)
//	{
//		if(x.second.size() <= 1)
//			continue;
//		std::cout << x.first << " {\n";
//		for(auto& l : x.second)
//			std::cout << *l << "\n";
//		std::cout << "}\n";
//	}

	return 0;
}
