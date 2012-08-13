#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define FIRMWARE_STRING "lifeline"

static int firmware_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", FIRMWARE_STRING);
	return 0;
}

static int firmware_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, firmware_proc_show, NULL);
}

static const struct file_operations firmware_proc_fops = {
	.open		= firmware_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_firmware_init(void)
{
	proc_create("firmware", 0, NULL, &firmware_proc_fops);
	return 0;
}
module_init(proc_firmware_init);
