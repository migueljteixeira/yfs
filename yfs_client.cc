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

	// check for root directory, if it doesn't exist we create it
	std::string buf;
	if(ec->get(0x00000001, 0, 0, buf) == extent_protocol::NOENT) {
		printf("Creating root directory\n");
		extent_protocol::status ret = ec->put(0x00000001, 0, "");

		if(ret != extent_protocol::OK) {
			printf("Couldn't create root directory\n");
			exit(0);
		}
	}
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
	if(ret != yfs_client::OK) {
		return ret;
	}

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

	// create file
	if(ec->put(inum, 0, "") != extent_protocol::OK)
		return IOERR;

	// update parent content
	if(!dir.empty())
		dir.append(";");
	dir.append(filename(inum) + ":" + file_name);

	// update parent
	if(ec->put(parent, 0, dir) != extent_protocol::OK)
		return IOERR;

	return OK;
}

int
yfs_client::write(inum inum, off_t offset, std::string file) {
	
	if(ec->put(inum, offset, file) != extent_protocol::OK)
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

int
yfs_client::remove(inum parent, std::string name){


	// check if parent exists
	//std::string dir;
	//if(ec->get(parent, 0, 0, dir) != extent_protocol::OK)
	//	return NOENT;

	//searches for file
	yfs_client::inum l_inum;
	yfs_client::status ret = this->ilookup(parent, name, l_inum);
	if(ret != yfs_client::OK)
		std::cout << "ERRO 1" << std::endl;
		return NOENT;

	//update parent content
	std::list<yfs_client::dirent> dir_entries;
	ret = this->getDirectoryContent(parent, dir_entries);
	if(ret != yfs_client::OK) {
		std::cout << "ERRO 3" << std::endl;
		return ret;
	}

	std::string dir;
	std::list<yfs_client::dirent>::iterator it;
	for(it = dir_entries.begin(); it != dir_entries.end(); it++) {
		if((*it).name.compare(name) == 0) {

	
		}
		else{
			dir.append(filename((*it).inum) + ":" + (*it).name);
		}
	}

	//delete file
	if (ec->remove(l_inum) != extent_protocol::OK) 
		std::cout << "ERRO 2" << std::endl;
		return IOERR;

	//update parent content
	if(ec->put(parent, 0, dir) != extent_protocol::OK)
		std::cout << "ERRO 4" << std::endl;
		return IOERR;

	return OK;
}

