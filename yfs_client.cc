// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
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
	lc = new lock_client(lock_dst);
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
		return yfs_client::NOENT;

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
	std::cout << "aqui1" << std::endl;
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
	if (ec->get(inum, 0, 0, buf) != extent_protocol::OK)
		return IOERR;

	// directory format: "dircontent" ; "inum" : "filename"
	std::vector<std::string> tokens = split(buf, ';');
	
	std::vector<std::string>::iterator it;
	for(it = tokens.begin(); it != tokens.end(); it++) {
		std::vector<std::string> dir_info = split(*it, ':');

		dirent e;
		e.name = dir_info[1];
		e.inum = n2i(dir_info[0]);

		// add new entry
		entries.push_back(e);
	}

	return OK;
}

int
yfs_client::createfile(inum parent, inum inum, std::string file_name)
{
	// check if parent exists
	std::string dir;
	if(ec->get(parent, 0, 0, dir) != extent_protocol::OK)
		return NOENT;

	// check if file exists
	if(ilookup(parent, file_name, inum) != yfs_client::NOENT)
		return OK;

	// create file
	if(ec->put(inum, 0, "", PUT_CREATE) != extent_protocol::OK)
		return IOERR;

	// update parent content
	if(!dir.empty())
		dir.append(";");
	dir.append(filename(inum) + ":" + file_name);

	// update parent
	if(ec->put(parent, 0, dir, PUT_UPDATE) != extent_protocol::OK)
		return IOERR;

	// update parent times
	extent_protocol::attr a;
	if(ec->getattr(parent, a) != extent_protocol::OK)
		return NOENT;

	a.mtime = time(NULL);
	a.ctime = time(NULL);

	if(ec->setattr(parent, a) != extent_protocol::OK)
		return NOENT;

	return OK;
}

int
yfs_client::removefile(inum parent, inum inum, std::string name)
{
	// get directory content
	std::list<yfs_client::dirent> dir_entries;
	yfs_client::status ret = this->getDirectoryContent(parent, dir_entries);
	if(ret != yfs_client::OK)
		return ret;

	// update directory content
	std::string new_content;
	std::list<yfs_client::dirent>::iterator it;
	for(it = dir_entries.begin(); it != dir_entries.end(); it++) {
		if((*it).name.compare(name) != 0) {
			if(!new_content.empty())
				new_content.append(";");
			new_content.append(filename((*it).inum) + ":" + (*it).name);
		}
	}

	// remove file
	if (ec->remove(inum) != extent_protocol::OK)
		return IOERR;

	// update parent content
	if(ec->put(parent, -1, new_content, PUT_UPDATE) != extent_protocol::OK)
		return IOERR;

	return OK;
}

int
yfs_client::write(inum inum, off_t offset, std::string file) {
	if(ec->put(inum, offset, file, PUT_UPDATE) != extent_protocol::OK)
		return IOERR;

	return OK;
}

int
yfs_client::read(inum inum, off_t offset, size_t len, std::string &file) {
	if(ec->get(inum, offset, len, file) != extent_protocol::OK)
		return IOERR;

	return OK;
}

int
yfs_client::setfilesize(inum inum, int size)
{
	// get current file size
	extent_protocol::attr attr;
	
	// read file attributes
    if(ec->getattr(inum, attr) != extent_protocol::OK)
        return NOENT;

	attr.size = size;

	// rewrite file attributes
	if(ec->setattr(inum, attr) != extent_protocol::OK)
		return NOENT;

	return OK;
}

lock_protocol::status yfs_client::acquire_lock(lock_protocol::lockid_t lockid)
{
    return lc->acquire(lockid);
}

lock_protocol::status yfs_client::release_lock(lock_protocol::lockid_t lockid)
{
    return lc->release(lockid);
}

