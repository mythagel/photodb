/*
 * photodb.cc
 *
 *  Created on: 24/03/2013
 *      Author: nicholas
 */
#include <cassert>
#include <iostream>
#include <dirent.h>
#include <memory>
#include <vector>

#include <exiv2/exiv2.hpp>

#include <sys/stat.h>
#include "sha1.h"
#include "db.h"
#include "photo.h"
#include "timestamp.h"
#include "mmap.h"
#include "util.h"

#include <unistd.h>

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

bool rebuild_db(db_t& db, const std::string& src)
{
	timestamp_t rebuilt(time(nullptr));
	
	db_t::statement_t<std::string, std::string, uint64_t, std::string, std::string, std::string, std::string, std::string, std::string> insert_photo{db, "INSERT INTO photos VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"};
	db_t::statement_t<std::string, std::string, uint64_t, std::string> photo_exists{db, "SELECT ROWID, timestamp, checksum, pixel_size, exif_size FROM photos WHERE file_name = ? AND path = ? AND size = ? and mtime = ?"};
	db_t::statement_t<std::string, int64_t> update_timestamp{db, "UPDATE photos set rebuilt = ? WHERE ROWID = ?"};

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
		return false;

	std::cerr << photo_list.size() << " Files.\n";

	// update db.
	db.execute("PRAGMA synchronous = OFF");

	size_t stat_new(0);
	size_t stat_old(0);
	for(auto& photo : photo_list)
	{
		bool new_photo(true);
		auto x = [&photo, &new_photo](const std::tuple<int64_t, std::string, std::string, std::string, std::string>& t)
		{
			photo->id = std::get<0>(t);
			photo->timestamp = timestamp_t{std::get<1>(t)};
			photo->checksum = std::get<2>(t);
			photo->pixel_size = {std::get<3>(t)};
			photo->exif_size = {std::get<4>(t)};
			new_photo = false;
		};
		photo_exists.query<decltype(x), int64_t, std::string, std::string, std::string, std::string>(x, photo->file_name, photo->path, photo->size, photo->mtime.str());

		if(new_photo)
		{
			exif(*photo);
			checksum(*photo);

			insert_photo.execute(photo->file_name, photo->path, photo->size, photo->mtime.str(), photo->timestamp.str(), photo->checksum, photo->pixel_size.str(), photo->exif_size.str(), rebuilt.str());
		}
		else
		{
			update_timestamp.execute(rebuilt.str(), photo->id);
		}

		++(new_photo ? stat_new : stat_old);
		
		if(stat_new && stat_new % 100 == 0)
		{
			std::cout << "new: " << stat_new << "; old: " << stat_old << "\n";
		}
	}
	std::cout << "new: " << stat_new << "; old: " << stat_old << "\n";
	return true;
}

std::vector<std::string> identify_checksum_dups(db_t& db)
{
	std::vector<std::tuple<std::string, std::string> > dups;
	
	db_t::statement_t<> checksum_dups{db, "select file_name, checksum as id from photos group by file_name, checksum having count(path) > 1;"};
	db_t::statement_t<std::string, std::string> dup_list{db, "select path from photos where file_name = ? and checksum = ?"};
	
	auto x = [&dups](const std::tuple<std::string, std::string>& t)
	{
		auto filename = std::get<0>(t);
		auto checksum = std::get<1>(t);
		dups.emplace_back(filename, checksum);
	};
	checksum_dups.query<decltype(x), std::string, std::string>(x);

	for(auto& dup : dups)
	{
		auto filename = std::get<0>(dup);
		auto checksum = std::get<1>(dup);
		
		std::vector<std::string> file_dups;
		
		std::cout << "File: " << filename << " (" << checksum << ")\n";
		auto x = [&file_dups, &filename](const std::tuple<std::string>& t)
		{
			// TODO
			auto path = std::get<0>(t);
			file_dups.push_back(path + "/" + filename);
		};
		dup_list.query<decltype(x), std::string>(x, filename, checksum);
		
		for(auto& file_dup : file_dups)
		{
			std::cout << "   " << file_dup << "\n";
		}
		for(size_t i = 1; i < file_dups.size(); ++i)
		{
			std::cout << "rm " << file_dups[i] << "\n";
			unlink(file_dups[i].c_str());
		}
		// TODO
	}
	return {};
}

int main(int argc, char* argv[])
{
	std::vector<std::string> args(argv, argv+argc);
	assert(!args.empty());

	if(args.size() < 2)
	{
		std::cerr << args[0] << " src_folder\n";
		return 1;
	}

	auto src = args[1];
	if(src.empty())
	{
		std::cerr << args[0] << " src_folder\n";
		return 1;
	}

	if(src.back() == '/')
		src.pop_back();

	db_t db{src + "/photo.db"};
	db.execute("CREATE TABLE IF NOT EXISTS photos (file_name TEXT, path TEXT, size INTEGER, mtime TEXT, timestamp TEXT, checksum TEXT, pixel_size TEXT, exif_size TEXT, rebuilt TEXT)");
	db.execute("CREATE INDEX IF NOT EXISTS photos_idx ON photos (file_name, path, size, mtime)");

	if(!rebuild_db(db, src))
		return 1;

	

/*
TODO list
need to do something with the db
identify duplicates.
	checksum + filename duplicates are indistinct
	
reorganise into time date structure
*/

	//identify_checksum_dups(db);

	return 0;
}
