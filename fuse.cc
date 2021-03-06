/*
 * receive request from fuse and call methods of yfs_client
 *
 * started life as low-level example in the fuse distribution.  we
 * have to use low-level interface in order to get i-numbers.  the
 * high-level interface only gives us complete paths.
 */

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include "yfs_client.h"

int myid;
yfs_client *yfs;

int id() { 
	return myid;
}

yfs_client::status
getattr(yfs_client::inum inum, struct stat &st)
{
	yfs_client::status ret;

	bzero(&st, sizeof(st));

	yfs->acquire_lock(inum);

	st.st_ino = inum;
	printf("getattr %016llx %d\n", inum, yfs->isfile(inum));
	if(yfs->isfile(inum)){
		yfs_client::fileinfo info;
		ret = yfs->getfile(inum, info);
		if(ret != yfs_client::OK) {
			yfs->release_lock(inum);
			return ret;
		}
		st.st_mode = S_IFREG | 0666;
		st.st_nlink = 1;
		st.st_atime = info.atime;
		st.st_mtime = info.mtime;
		st.st_ctime = info.ctime;
		st.st_size = info.size;
		printf("   getattr -> %llu\n", info.size);
	} else {
		yfs_client::dirinfo info;
		ret = yfs->getdir(inum, info);
		if(ret != yfs_client::OK) {
			yfs->release_lock(inum);
			return ret;
		}
		st.st_mode = S_IFDIR | 0777;
		st.st_nlink = 2;
		st.st_atime = info.atime;
		st.st_mtime = info.mtime;
		st.st_ctime = info.ctime;
		printf("   getattr -> %lu %lu %lu\n", info.atime, info.mtime, info.ctime);
	}
	yfs->release_lock(inum);
	return yfs_client::OK;
}


void
fuseserver_getattr(fuse_req_t req, fuse_ino_t ino,
          struct fuse_file_info *fi)
{
	struct stat st;
	yfs_client::inum inum = ino; // req->in.h.nodeid;
	yfs_client::status ret;

	ret = getattr(inum, st);
	if(ret != yfs_client::OK){
		fuse_reply_err(req, ENOENT);
		return;
	}
	fuse_reply_attr(req, &st, 0);
}

void
fuseserver_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	if (FUSE_SET_ATTR_SIZE & to_set) {
		// check if its a directory, we can't change its size
		if(yfs->isdir(ino)) {
			fuse_reply_err(req, EISDIR);
			return;
		}

		yfs->acquire_lock(ino);

		// sets file size
		if(yfs->setfilesize(ino, attr->st_size) != yfs_client::OK) {
			yfs->release_lock(ino);
			fuse_reply_err(req, ENOENT);
			return;
		}

		yfs->release_lock(ino);

		struct stat st;
		getattr(ino, st);
		fuse_reply_attr(req, &st, 0);
	} else {
		fuse_reply_err(req, ENOSYS);
	}
}

void
fuseserver_read(fuse_req_t req, fuse_ino_t ino, size_t size,
	off_t off, struct fuse_file_info *fi)
{
	yfs->acquire_lock(ino);

	std::string file;
	if(yfs->read(ino, off, size, file) != yfs_client::OK) {
		yfs->release_lock(ino);
		fuse_reply_err(req, EIO);
		return;
	}

	yfs->release_lock(ino);
	fuse_reply_buf(req, file.c_str(), file.size());
}

void
fuseserver_write(fuse_req_t req, fuse_ino_t ino,
	const char *buf, size_t size, off_t off,
	struct fuse_file_info *fi)
{
	yfs->acquire_lock(ino);

	if(yfs->write(ino, off, std::string(buf, size)) != yfs_client::OK) {
		yfs->release_lock(ino);
		fuse_reply_err(req, EIO);
		return;
	}
	
	yfs->release_lock(ino);
	fuse_reply_write(req, size);
}

yfs_client::status
fuseserver_createhelper(fuse_ino_t parent, const char *name,
     mode_t mode, struct fuse_entry_param *e)
{
	// generate file random inum
	yfs_client::inum file_inum = random();

	// because it's a file we have to put the 32th bit at 1
	file_inum = file_inum | 0x80000000;

	yfs->acquire_lock(file_inum);

	// if file exists we get the existing inum
	yfs_client::inum s_inum;
	if(yfs->ilookup(parent, name, s_inum) == yfs_client::OK) {
		yfs->release_lock(file_inum);
		file_inum = s_inum;
		yfs->acquire_lock(file_inum);
	}

	// try to create file
	yfs_client::status ret = yfs->createfile(parent, file_inum, name);
	if(ret != yfs_client::OK) {
		yfs->release_lock(file_inum);
		return ret;
	}

	// update fuse entry
	yfs_client::fileinfo info;
	ret = yfs->getfile(file_inum, info);
	if(ret != yfs_client::OK) {
		yfs->release_lock(file_inum);
		return ret;
	}

	e->ino = file_inum;
	e->generation = 1;
	e->attr.st_mode = S_IFREG | 0666;
	e->attr.st_nlink = 1;
	e->attr.st_size = info.size;
	e->attr.st_atime = info.atime;
	e->attr.st_mtime = info.mtime;
	e->attr.st_ctime = info.ctime;
	e->entry_timeout = 0.0;
	e->attr_timeout = 0.0;

	yfs->release_lock(file_inum);

	return yfs_client::OK;
}

void
fuseserver_create(fuse_req_t req, fuse_ino_t parent, const char *name,
   mode_t mode, struct fuse_file_info *fi)
{
	yfs->acquire_lock(parent);
	
	struct fuse_entry_param e;
	if( fuseserver_createhelper( parent, name, mode, &e ) == yfs_client::OK ) {
		fuse_reply_create(req, &e, fi);
	} else {
		fuse_reply_err(req, ENOENT);
	}

	yfs->release_lock(parent);
}

void fuseserver_mknod( fuse_req_t req, fuse_ino_t parent, 
    const char *name, mode_t mode, dev_t rdev )
{
	yfs->acquire_lock(parent);

	struct fuse_entry_param e;
	if( fuseserver_createhelper( parent, name, mode, &e ) == yfs_client::OK ) {
		fuse_reply_entry(req, &e);
	} else {
		fuse_reply_err(req, ENOENT);
	}

	yfs->release_lock(parent);
}

void
fuseserver_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e;

	e.attr_timeout = 0.0;
	e.entry_timeout = 0.0;

	yfs->acquire_lock(parent);

	// check if parent is a directory
	if(!yfs->isdir(parent)) {
		yfs->release_lock(parent);
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	// lookup for inum of 'name'
	yfs_client::inum l_inum;
	yfs_client::status ret = yfs->ilookup(parent, name, l_inum);
	if(ret != yfs_client::OK) {
		yfs->release_lock(parent);
		fuse_reply_err(req, ENOENT);
		return;
	}
	yfs->release_lock(parent);

	yfs->acquire_lock(l_inum);

	e.ino = l_inum;
	e.generation = 1;

	// get attributes
	if(yfs->isfile(l_inum)) {
		yfs_client::fileinfo info;
		ret = yfs->getfile(l_inum, info);
		if(ret != yfs_client::OK) {
			yfs->release_lock(l_inum);
			fuse_reply_err(req, EIO);
			return;
		}

		e.attr.st_mode = S_IFREG | 0666;
		e.attr.st_nlink = 1;
		e.attr.st_size = info.size;
		e.attr.st_atime = info.atime;
		e.attr.st_mtime = info.mtime;
		e.attr.st_ctime = info.ctime;
	}
	else {
		yfs_client::dirinfo info;
		ret = yfs->getdir(l_inum, info);
		if(ret != yfs_client::OK) {
			yfs->release_lock(l_inum);
			fuse_reply_err(req, EIO);
			return;
		}

		e.attr.st_mode = S_IFDIR | 0777;
		e.attr.st_nlink = 2;
		e.attr.st_atime = info.atime;
		e.attr.st_mtime = info.mtime;
		e.attr.st_ctime = info.ctime;
	}

	yfs->release_lock(l_inum);

	fuse_reply_entry(req, &e);
}


struct dirbuf {
	char *p;
	size_t size;
};

void dirbuf_add(struct dirbuf *b, const char *name, fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize = b->size;
	b->size += fuse_dirent_size(strlen(name));
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	fuse_add_dirent(b->p + oldsize, name, &stbuf, b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
	off_t off, size_t maxsize)
{
	if (off < bufsize)
		return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

void
fuseserver_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
          off_t off, struct fuse_file_info *fi)
{
	yfs_client::inum inum = ino; // req->in.h.nodeid;
	struct dirbuf b;
	yfs_client::dirent e;

	yfs->acquire_lock(inum);

	if(!yfs->isdir(inum)){
		yfs->release_lock(inum);
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	memset(&b, 0, sizeof(b));

	std::list<yfs_client::dirent> entries;

	// get listing for the dir
	yfs->getDirectoryContent(inum, entries);

	// add information about the files to the buffer
	for (std::list<yfs_client::dirent>::const_iterator it =
		entries.begin(); it != entries.end(); it++) {

		dirbuf_add(&b, it->name.c_str(), static_cast<fuse_ino_t>(it->inum));
	}

	yfs->release_lock(inum);

	reply_buf_limited(req, b.p, b.size, off, size);
	free(b.p);
 }


void
fuseserver_open(fuse_req_t req, fuse_ino_t ino,
     struct fuse_file_info *fi)
{
	yfs->acquire_lock(ino);

	// check if is a directory
	if(yfs->isdir(ino)) {
		fuse_reply_err(req, EISDIR);
	}

	yfs->release_lock(ino);

  	fuse_reply_open(req, fi);
}



yfs_client::status
fuseserver_mkdir_helper(fuse_ino_t parent, const char *name,
     mode_t mode, struct fuse_entry_param *e)
{
	// generate dir random inum
	yfs_client::inum dir_inum = random();

	// because it's a dir we have to put the 32th bit at 0
	dir_inum = dir_inum & 0x7fffffff;

	yfs->acquire_lock(dir_inum);

	// try to create directory
	yfs_client::status ret = yfs->createfile(parent, dir_inum, name);
	if(ret != yfs_client::OK) {
		yfs->release_lock(dir_inum);
		return ret;
	}

	// update fuse entry
	yfs_client::dirinfo info;
	ret = yfs->getdir(dir_inum, info);
	if(ret != yfs_client::OK) {
		yfs->release_lock(dir_inum);
		return ret;
	}

	e->ino = dir_inum;
	e->generation = 1;
	e->attr.st_mode = S_IFDIR | 0777;
	e->attr.st_nlink = 2;
	e->attr.st_atime = info.atime;
	e->attr.st_mtime = info.mtime;
	e->attr.st_ctime = info.ctime;
	e->entry_timeout = 0.0;
	e->attr_timeout = 0.0;

	yfs->release_lock(dir_inum);

	return yfs_client::OK;

}

void
fuseserver_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
     mode_t mode)
{
	yfs->acquire_lock(parent);

	struct fuse_entry_param e;
	if( fuseserver_mkdir_helper(parent, name, mode, &e) == yfs_client::OK ) {
		fuse_reply_entry(req, &e);
	} else {
		fuse_reply_err(req, ENOENT);
	}

	yfs->release_lock(parent);
}

void
fuseserver_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	yfs->acquire_lock(parent);

	// check if parent exists
	yfs_client::dirinfo info;
	yfs_client::status ret = yfs->getdir(parent, info);
	if(ret != yfs_client::OK) {
		yfs->release_lock(parent);
		fuse_reply_err(req, ENOENT);
		return;
	}

	// get file inum
	yfs_client::inum l_inum;
	ret = yfs->ilookup(parent, name, l_inum);
	if(ret != yfs_client::OK) {
		yfs->release_lock(parent);
		fuse_reply_err(req, ENOENT);
		return;
	}

	yfs->acquire_lock(l_inum);

	ret = yfs->removefile(parent, l_inum, name);

	yfs->release_lock(l_inum);
	yfs->release_lock(parent);

	if(ret == yfs_client::OK)
		fuse_reply_err(req, 0);
	else
		fuse_reply_err(req, ENOENT);
}

void
fuseserver_statfs(fuse_req_t req)
{
	struct statvfs buf;

	printf("statfs\n");

	memset(&buf, 0, sizeof(buf));

	buf.f_namemax = 255;
	buf.f_bsize = 512;

	fuse_reply_statfs(req, &buf);
}

struct fuse_lowlevel_ops fuseserver_oper;

int
main(int argc, char *argv[])
{
	char *mountpoint = 0;
	int err = -1;
	int fd;

	setvbuf(stdout, NULL, _IONBF, 0);

	if(argc != 4){
		fprintf(stderr, "Usage: yfs_client <mountpoint> <port-extent-server> <port-lock-server>\n");
		exit(1);
	}
	mountpoint = argv[1];

	srandom(getpid());

	myid = random();

	yfs = new yfs_client(argv[2], argv[3]);

	fuseserver_oper.getattr    = fuseserver_getattr;
	fuseserver_oper.statfs     = fuseserver_statfs;
	fuseserver_oper.readdir    = fuseserver_readdir;
	fuseserver_oper.lookup     = fuseserver_lookup;
	fuseserver_oper.create     = fuseserver_create;
	fuseserver_oper.mknod      = fuseserver_mknod;
	fuseserver_oper.open       = fuseserver_open;
	fuseserver_oper.read       = fuseserver_read;
	fuseserver_oper.write      = fuseserver_write;
	fuseserver_oper.setattr    = fuseserver_setattr;
	fuseserver_oper.unlink     = fuseserver_unlink;
	fuseserver_oper.mkdir      = fuseserver_mkdir;

	const char *fuse_argv[20];
	int fuse_argc = 0;
	fuse_argv[fuse_argc++] = argv[0];
	#ifdef __APPLE__
	fuse_argv[fuse_argc++] = "-o";
	fuse_argv[fuse_argc++] = "nolocalcaches"; // no dir entry caching
	fuse_argv[fuse_argc++] = "-o";
	fuse_argv[fuse_argc++] = "daemon_timeout=86400";
	#endif

	// everyone can play, why not?
	//fuse_argv[fuse_argc++] = "-o";
	//fuse_argv[fuse_argc++] = "allow_other";

	fuse_argv[fuse_argc++] = mountpoint;
	fuse_argv[fuse_argc++] = "-d";

	fuse_args args = FUSE_ARGS_INIT( fuse_argc, (char **) fuse_argv );
	int foreground;
	int res = fuse_parse_cmdline( &args, &mountpoint, 0 /*multithreaded*/, 
	&foreground );
	if( res == -1 ) {
		fprintf(stderr, "fuse_parse_cmdline failed\n");
		return 0;
	}

	args.allocated = 0;

	fd = fuse_mount(mountpoint, &args);
	if(fd == -1){
		fprintf(stderr, "fuse_mount failed\n");
		exit(1);
	}

	struct fuse_session *se;

	se = fuse_lowlevel_new(&args, &fuseserver_oper, sizeof(fuseserver_oper),
	NULL);
	if(se == 0){
		fprintf(stderr, "fuse_lowlevel_new failed\n");
		exit(1);
	}

	struct fuse_chan *ch = fuse_kern_chan_new(fd);
	if (ch == NULL) {
		fprintf(stderr, "fuse_kern_chan_new failed\n");
		exit(1);
	}

	fuse_session_add_chan(se, ch);
	// err = fuse_session_loop_mt(se);   // FK: wheelfs does this; why?
	err = fuse_session_loop(se);

	fuse_session_destroy(se);
	close(fd);
	fuse_unmount(mountpoint);

	return err ? 1 : 0;
}
