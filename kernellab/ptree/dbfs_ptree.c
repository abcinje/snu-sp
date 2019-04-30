#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define BUF_SIZE	1024
#define INFO_SIZE	256

MODULE_LICENSE("GPL");

static struct dentry *dir, *input, *ptree;
static u32 pid;

static ssize_t read_ptree(struct file *fp, char __user *user_buf,
				size_t count, loff_t *pos)
{
	struct task_struct *task;
	char *buf, *proc_info;
	int cursor, len;
	ssize_t ret;

	buf = kzalloc(BUF_SIZE, GFP_KERNEL);
	proc_info = kzalloc(INFO_SIZE, GFP_KERNEL);

	if (!(task = pid_task(find_get_pid(pid), PIDTYPE_PID))) {
		strncpy(buf, "No such process\n", BUF_SIZE);
		ret = simple_read_from_buffer(user_buf, count, pos, buf, strlen(buf));
	}

	else {
		cursor = BUF_SIZE - 1;

		while (task->pid) {
			snprintf(proc_info, INFO_SIZE, "%s (%d)\n", task->comm, task->pid);
			len = strlen(proc_info);
			cursor -= len;
			strncpy(buf + cursor, proc_info, len);
			task = task->parent;
		}

		ret = simple_read_from_buffer(user_buf, count, pos, buf + cursor, strlen(buf + cursor));
	}

	kfree(buf);
	kfree(proc_info);

	return ret;
}

static const struct file_operations ptree_fops = {
	.owner = THIS_MODULE,
	.read = read_ptree
};

static int __init dbfs_module_init(void)
{
	if (!(dir = debugfs_create_dir("ptree", NULL))) {
		printk("Cannot create ptree dir\n");
		goto fail;
	}

	if (!(input = debugfs_create_u32("input", 0644, dir, &pid))) {
		printk("Cannot create input file\n");
		goto fail;
	}

	if (!(ptree = debugfs_create_file("ptree", 0444, dir, NULL, &ptree_fops))) {
		printk("Cannot create ptree file\n");
		goto fail;
	}
	
	printk("dbfs_ptree module initialize done\n");
        return 0;

fail:
	if (dir)
		debugfs_remove_recursive(dir);

	return -1;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir);
	
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);

