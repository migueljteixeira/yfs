// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iterator>
#include <list>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::ilookup(inum di, std::string name, inum &inum)
{
	// get directory
	std::list<yfs_client::dirent> dir_entries;
	yfs_client::status ret = this->getDirectoryContent(di, dir_entries);
	if(ret != yfs_client::OK)
		return ret;

	// search for file in directory
	std::list<yfs_client::dirent>::iterator it;
	for(it = dir_entries.begin(); it != dir_entries.end(); it++) {
		if((*it).name.compare(name) == 0) {
			inum = (*it).inum;
			return yfs_client::OK;
		}
	}

	return yfs_client::NOENT;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;


  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;


  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int
yfs_client::getDirectoryContent(inum inum, std::list<dirent> &entries)
{
	std::string buf;
	if (ec->get(inum, buf) != extent_protocol::OK)
		return IOERR;

	std::istringstream is(buf);
	std::string line;

	// directory format: "dircontent" ; "inum" : "filename"
	while( getline(is, line) ) {
		printf("line: %s", line.c_str());

		// separate content from inum and filename
		std::string f = line.substr(line.find(";"));

		dirent e;
		e.name = f.substr(f.find(":"));
		e.inum = n2i( f.substr(1, f.find(":")) );

		// add new entry
		entries.push_back(e);
	}

	return OK;
}

int
yfs_client::createfile(inum parent, inum inum, std::string file_name)
{
	printf("createfile %016llx in parent %016llx\n", inum, parent);

	// check if parent exists
	std::string dir;
	if(ec->get(parent, dir) != extent_protocol::OK)
		return NOENT;

	// create file
	if(ec->put(inum, "") != extent_protocol::OK)
		return IOERR;

	// update parent content
	if(!dir.empty())
		dir.append(";");
	dir.append(filename(inum) + ":" + file_name);

	// update parent
	if(ec->put(parent, dir) != extent_protocol::OK)
		return IOERR;

	return OK;
}
