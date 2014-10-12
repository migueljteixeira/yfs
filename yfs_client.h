#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include <list>

#define PUT_CREATE 0
#define PUT_UPDATE 1

class yfs_client {
	extent_client *ec;
	lock_client *lc;

	public:
		typedef unsigned long long inum;
		enum xxstatus { OK, RPCERR, NOENT, IOERR, FBIG };
		typedef int status;

		struct fileinfo {
			unsigned long long size;
			unsigned long atime;
			unsigned long mtime;
			unsigned long ctime;
		};
		struct dirinfo {
			unsigned long atime;
			unsigned long mtime;
			unsigned long ctime;
		};
		struct dirent {
			std::string name;
			unsigned long long inum;
		};

	private:
		static std::string filename(inum);
		static inum n2i(std::string);

		static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
			std::stringstream ss(s);
			std::string item;
			while (std::getline(ss, item, delim)) {
				elems.push_back(item);
			}
			return elems;
		}

		static std::vector<std::string> split(const std::string &s, char delim) {
			std::vector<std::string> elems;
			split(s, delim, elems);
			return elems;
		}

	public:
		yfs_client(std::string, std::string);

		bool isfile(inum);
		bool isdir(inum);
		int ilookup(inum di, std::string name, inum &inum);

		int getfile(inum, fileinfo &);
		int getdir(inum, dirinfo &);
		int getDirectoryContent(inum, std::list<dirent> &);
		
		int createfile(inum, inum, std::string);
		int removefile(inum, std::string);

		int write(inum, off_t offset, std::string file);
		int read(inum, off_t offset, size_t len, std::string &file);

		int setfilesize(inum, int);
};

#endif 
