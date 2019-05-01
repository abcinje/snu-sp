#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

struct packet {
	pid_t pid;
	unsigned long vaddr;
	unsigned long paddr;
};

static struct dentry *dir, *output;

static ssize_t read_output(struct file *fp,
			char __user *user_buf,
			size_t count,
			loff_t *pos)
{
	struct packet pckt;
	struct task_struct *task;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	memcpy(&pckt, user_buf, sizeof(pckt));

	if (!(task = pid_task(find_get_pid(pckt.pid), PIDTYPE_PID)))
		return -ESRCH;

	pgd = pgd_offset(task->mm, pckt.vaddr);
	if (pgd_none(*pgd))
		return -EFAULT;

	pud = pud_offset(pgd, pckt.vaddr);
	if (pud_none(*pud))
		return -EFAULT;

	pmd = pmd_offset(pud, pckt.vaddr);
	if (pmd_none(*pmd))
		return -EFAULT;

	pte = pte_offset_kernel(pmd, pckt.vaddr);
	if (pte_none(*pte))
		return -EFAULT;

	pckt.paddr = (pte_val(*pte) & PTE_PFN_MASK) | (pckt.vaddr & ~PAGE_MASK);

	return simple_read_from_buffer(user_buf, count, pos, &pckt, sizeof(pckt));
}

static const struct file_operations output_fops = {
	.owner = THIS_MODULE,
	.read = read_output
};

static int __init dbfs_module_init(void)
{
	if (!(dir = debugfs_create_dir("paddr", NULL))) {
		printk("Cannot create paddr dir\n");
		return -1;
	}

	if (!(output = debugfs_create_file("output", 0444, dir, NULL, &output_fops))) {
		printk("Cannot create output file\n");
		debugfs_remove(dir);
		return -1;
	}

	printk("dbfs_paddr module initialize done\n");
	return 0;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir);

	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);

