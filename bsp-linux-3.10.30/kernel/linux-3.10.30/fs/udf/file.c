/*
 * file.c
 *
 * PURPOSE
 *  File handling routines for the OSTA-UDF(tm) filesystem.
 *
 * COPYRIGHT
 *  This file is distributed under the terms of the GNU General Public
 *  License (GPL). Copies of the GPL can be obtained from:
 *    ftp://prep.ai.mit.edu/pub/gnu/GPL
 *  Each contributing author retains all rights to their own work.
 *
 *  (C) 1998-1999 Dave Boynton
 *  (C) 1998-2004 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  10/02/98 dgb  Attempt to integrate into udf.o
 *  10/07/98      Switched to using generic_readpage, etc., like isofs
 *                And it works!
 *  12/06/98 blf  Added udf_file_read. uses generic_file_read for all cases but
 *                ICBTAG_FLAG_AD_IN_ICB.
 *  04/06/99      64 bit file handling on 32 bit systems taken from ext2 file.c
 *  05/12/99      Preliminary file write support
 */

#include "udfdecl.h"
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h> /* memset */
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/pagemap.h>
#include <linux/buffer_head.h>
#include <linux/aio.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "udf_i.h"
#include "udf_sb.h"

static void __udf_adinicb_readpage(struct page *page)
{
	struct inode *inode = page->mapping->host;
	char *kaddr;
	struct udf_inode_info *iinfo = UDF_I(inode);

	kaddr = kmap(page);
	memcpy(kaddr, iinfo->i_ext.i_data + iinfo->i_lenEAttr, inode->i_size);
	memset(kaddr + inode->i_size, 0, PAGE_CACHE_SIZE - inode->i_size);
	flush_dcache_page(page);
	SetPageUptodate(page);
	kunmap(page);
}

static int udf_adinicb_readpage(struct file *file, struct page *page)
{
	BUG_ON(!PageLocked(page));
	__udf_adinicb_readpage(page);
	unlock_page(page);

	return 0;
}

static int udf_adinicb_writepage(struct page *page,
				 struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	char *kaddr;
	struct udf_inode_info *iinfo = UDF_I(inode);

	BUG_ON(!PageLocked(page));

	kaddr = kmap(page);
	memcpy(iinfo->i_ext.i_data + iinfo->i_lenEAttr, kaddr, inode->i_size);
	mark_inode_dirty(inode);
	SetPageUptodate(page);
	kunmap(page);
	unlock_page(page);

	return 0;
}

static int udf_adinicb_write_begin(struct file *file,
			struct address_space *mapping, loff_t pos,
			unsigned len, unsigned flags, struct page **pagep,
			void **fsdata)
{
	struct page *page;

	if (WARN_ON_ONCE(pos >= PAGE_CACHE_SIZE))
		return -EIO;
	page = grab_cache_page_write_begin(mapping, 0, flags);
	if (!page)
		return -ENOMEM;
	*pagep = page;

	if (!PageUptodate(page) && len != PAGE_CACHE_SIZE)
		__udf_adinicb_readpage(page);
	return 0;
}

static int udf_adinicb_write_end(struct file *file,
			struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	unsigned offset = pos & (PAGE_CACHE_SIZE - 1);
	char *kaddr;
	struct udf_inode_info *iinfo = UDF_I(inode);

	kaddr = kmap_atomic(page);
	memcpy(iinfo->i_ext.i_data + iinfo->i_lenEAttr + offset,
		kaddr + offset, copied);
	kunmap_atomic(kaddr);

	return simple_write_end(file, mapping, pos, len, copied, page, fsdata);
}

static ssize_t udf_adinicb_direct_IO(int rw, struct kiocb *iocb,
				     const struct iovec *iov,
				     loff_t offset, unsigned long nr_segs)
{
	/* Fallback to buffered I/O. */
	return 0;
}

const struct address_space_operations udf_adinicb_aops = {
	.readpage	= udf_adinicb_readpage,
	.writepage	= udf_adinicb_writepage,
	.write_begin	= udf_adinicb_write_begin,
	.write_end	= udf_adinicb_write_end,
	.direct_IO	= udf_adinicb_direct_IO,
};

static ssize_t udf_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
				  unsigned long nr_segs, loff_t ppos)
{
	ssize_t retval;
	struct file *file = iocb->ki_filp;
	struct inode *inode = file_inode(file);
	int err, pos;
	size_t count = iocb->ki_left;
	struct udf_inode_info *iinfo = UDF_I(inode);

	down_write(&iinfo->i_data_sem);
	if (iinfo->i_alloc_type == ICBTAG_FLAG_AD_IN_ICB) {
		if (file->f_flags & O_APPEND)
			pos = inode->i_size;
		else
			pos = ppos;

		if (inode->i_sb->s_blocksize <
				(udf_file_entry_alloc_offset(inode) +
						pos + count)) {
			err = udf_expand_file_adinicb(inode);
			if (err) {
				udf_debug("udf_expand_adinicb: err=%d\n", err);
				return err;
			}
		} else {
			if (pos + count > inode->i_size)
				iinfo->i_lenAlloc = pos + count;
			else
				iinfo->i_lenAlloc = inode->i_size;
			up_write(&iinfo->i_data_sem);
		}
	} else
		up_write(&iinfo->i_data_sem);

	retval = generic_file_aio_write(iocb, iov, nr_segs, ppos);
	if (retval > 0)
		mark_inode_dirty(inode);

	return retval;
}

long udf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(filp);
	long old_block, new_block;
	int result = -EINVAL;

	if (inode_permission(inode, MAY_READ) != 0) {
		udf_debug("no permission to access inode %lu\n", inode->i_ino);
		result = -EPERM;
		goto out;
	}

	if (!arg) {
		udf_debug("invalid argument to udf_ioctl\n");
		result = -EINVAL;
		goto out;
	}

	switch (cmd) {
	case UDF_GETVOLIDENT:
		if (copy_to_user((char __user *)arg,
				 UDF_SB(inode->i_sb)->s_volume_ident, 32))
			result = -EFAULT;
		else
			result = 0;
		goto out;
	case UDF_RELOCATE_BLOCKS:
		if (!capable(CAP_SYS_ADMIN)) {
			result = -EPERM;
			goto out;
		}
		if (get_user(old_block, (long __user *)arg)) {
			result = -EFAULT;
			goto out;
		}
		result = udf_relocate_blocks(inode->i_sb,
						old_block, &new_block);
		if (result == 0)
			result = put_user(new_block, (long __user *)arg);
		goto out;
	case UDF_GETEASIZE:
		result = put_user(UDF_I(inode)->i_lenEAttr, (int __user *)arg);
		goto out;
	case UDF_GETEABLOCK:
		result = copy_to_user((char __user *)arg,
				      UDF_I(inode)->i_ext.i_data,
				      UDF_I(inode)->i_lenEAttr) ? -EFAULT : 0;
		goto out;
	}

out:
	return result;
}

int udf_extent_descriptor_cache_release(udf_extent_descriptor_cache *p_this);
static int udf_release_file(struct inode *inode, struct file *filp)
{
#ifdef CONFIG_SSIF_EXT_CACHE
	struct udf_inode_info *iinfo = UDF_I(inode);
	atomic_dec(&(iinfo->extent_desc_cache.ref_count));
	if (atomic_read(&(iinfo->extent_desc_cache.ref_count)) == 0)
	udf_extent_descriptor_cache_release(&(iinfo->extent_desc_cache));
#endif
	if (filp->f_mode & FMODE_WRITE) {
		down_write(&UDF_I(inode)->i_data_sem);
		udf_discard_prealloc(inode);
		udf_truncate_tail_extent(inode);
		up_write(&UDF_I(inode)->i_data_sem);
	}
#ifdef CONFIG_SSIF_EXT_CACHE
	iinfo->recent_access.sanity = ~(UDF3D_MAGIC_NUM);
#endif
	return 0;
}

#ifdef CONFIG_SSIF_EXT_CACHE
void udf_extent_descriptor_cache_clear(udf_extent_descriptor_cache *p_this)
{
	int n;
	p_this->sanity = ~UDF3D_MAGIC_NUM;
	p_this->n_descriptors = 0;
	for(n = 0; n < UDF3D_MAXN_PRELOAD_BLOCKS; n++){
		p_this->descriptor_blocks[n] = (void*)0;
	}
	return;
}

void udf_release_data(struct buffer_head *bh);
int udf_extent_descriptor_cache_release(udf_extent_descriptor_cache *p_this)
{
	int n=0;
	if(p_this->sanity != UDF3D_MAGIC_NUM){
		printk("udf_extent_descriptor_cache_release::Error-1\n");
		return -1;
	}
	if(p_this->descriptor_blocks != NULL){
		for(n = 0; n < p_this->n_descriptors; n++){
			if(p_this->descriptor_blocks[n] != NULL){
				udf_release_data((struct buffer_head*)p_this->descriptor_blocks[n]);
				p_this->descriptor_blocks[n] = NULL;
			}
		}
		vfree(p_this->descriptor_blocks);
		p_this->descriptor_blocks=NULL;
	}
	p_this->sanity = ~(UDF3D_MAGIC_NUM);
	p_this->n_descriptors = 0;
	return n;
}

int8_t inode_bmap_preload_extent_blocks(struct inode *inode,
				 struct kernel_lb_addr *bloc,
				 uint32_t *extoffset,
				 uint32_t *elen,
				 struct buffer_head **bh,
				 udf_extent_descriptor_cache *p_ext_desc_cache);

int udf_extent_descriptor_cache_open_inode(
       udf_extent_descriptor_cache* p_this, struct inode *inode)
{
	int n_blocks;
	struct kernel_lb_addr bloc;
	uint32_t extoffset;
	uint32_t elen;
	struct buffer_head *bh = NULL;
	atomic_inc(&p_this->ref_count);
	if (atomic_read(&p_this->ref_count) == 1) {
		n_blocks = inode_bmap_preload_extent_blocks(inode, &bloc, &extoffset, &elen, &bh, p_this);
		p_this->sanity = UDF3D_MAGIC_NUM;
		return n_blocks;
	} else
		return 0;
}

static int udf_file_open(struct inode *inode, struct file *filp)
{
	struct udf_inode_info *iinfo = UDF_I(inode);
	
	udf_extent_descriptor_cache_open_inode(&(iinfo->extent_desc_cache), inode);
	return generic_file_open(inode, filp);
}
#endif

const struct file_operations udf_file_operations = {
	.read			= do_sync_read,
	.aio_read		= generic_file_aio_read,
	.unlocked_ioctl		= udf_ioctl,
	.open                   = udf_file_open,
	.mmap			= generic_file_mmap,
	.write			= do_sync_write,
	.aio_write		= udf_file_aio_write,
	.release		= udf_release_file,
	.fsync			= generic_file_fsync,
	.splice_read		= generic_file_splice_read,
	.llseek			= generic_file_llseek,
};

static int udf_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int error;

	error = inode_change_ok(inode, attr);
	if (error)
		return error;

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size != i_size_read(inode)) {
		error = udf_setsize(inode, attr->ia_size);
		if (error)
			return error;
	}

	setattr_copy(inode, attr);
	mark_inode_dirty(inode);
	return 0;
}

const struct inode_operations udf_file_inode_operations = {
	.setattr		= udf_setattr,
};