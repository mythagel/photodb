/*
 * photodb.cc
 *
 *  Created on: 24/03/2013
 *      Author: nicholas
 */
#include <iostream>
#include <dirent.h>
#include <memory>
#include <vector>
#include <sstream>

#include <exiv2/exiv2.hpp>

#include <sys/stat.h>
#include "sha1.h"
#include "db.h"
#include "photo.h"
#include "timestamp.h"
#include "mmap.h"
#include "util.h"

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
	try
	{
		mmap_t addr(photo.full_filename().c_str(), photo.size);

		unsigned char hash[20];
		sha1::calc(addr, photo.size, hash);

		char hexstring[41];
		sha1::toHexString(hash, hexstring);
		photo.checksum = hexstring;

		return true;
	}
	catch(const std::runtime_error& ex)
	{
		return false;
	}
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
