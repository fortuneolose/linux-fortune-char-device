#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEVICE_NAME "fortune_device"
#define CLASS_NAME "fortune"
#define FORTUNE_BUFFER_SIZE 256
#define DEFAULT_FORTUNE "Fortune favors the prepared kernel hacker.\n"

static dev_t fortune_dev_number;
static struct cdev fortune_cdev;
static struct class *fortune_class;
static DEFINE_MUTEX(fortune_lock);

static char fortune_buffer[FORTUNE_BUFFER_SIZE] = DEFAULT_FORTUNE;
static size_t fortune_size = sizeof(DEFAULT_FORTUNE) - 1;

static int fortune_open(struct inode *inode, struct file *file)
{
	pr_info("%s: opened\n", DEVICE_NAME);
	return 0;
}

static int fortune_release(struct inode *inode, struct file *file)
{
	pr_info("%s: closed\n", DEVICE_NAME);
	return 0;
}

static ssize_t fortune_read(struct file *file, char __user *user_buffer,
			    size_t count, loff_t *offset)
{
	size_t bytes_available;
	size_t bytes_to_copy;
	size_t position;
	ssize_t result;

	if (*offset < 0)
		return -EINVAL;

	mutex_lock(&fortune_lock);

	position = (size_t)*offset;

	if (position >= fortune_size) {
		result = 0;
		goto out;
	}

	bytes_available = fortune_size - position;
	bytes_to_copy = min(count, bytes_available);

	if (copy_to_user(user_buffer, fortune_buffer + position, bytes_to_copy)) {
		result = -EFAULT;
		goto out;
	}

	*offset += bytes_to_copy;
	result = bytes_to_copy;

out:
	mutex_unlock(&fortune_lock);
	return result;
}

static ssize_t fortune_write(struct file *file, const char __user *user_buffer,
			     size_t count, loff_t *offset)
{
	char new_fortune[FORTUNE_BUFFER_SIZE];
	size_t bytes_to_copy;

	if (count == 0)
		return 0;

	if (count > FORTUNE_BUFFER_SIZE - 1)
		return -EMSGSIZE;

	bytes_to_copy = count;

	if (copy_from_user(new_fortune, user_buffer, bytes_to_copy))
		return -EFAULT;

	new_fortune[bytes_to_copy] = '\0';

	mutex_lock(&fortune_lock);
	memcpy(fortune_buffer, new_fortune, bytes_to_copy + 1);
	fortune_size = bytes_to_copy;
	mutex_unlock(&fortune_lock);

	pr_info("%s: stored %zu byte fortune\n", DEVICE_NAME, bytes_to_copy);
	return bytes_to_copy;
}

static const struct file_operations fortune_fops = {
	.owner = THIS_MODULE,
	.open = fortune_open,
	.release = fortune_release,
	.read = fortune_read,
	.write = fortune_write,
	.llseek = no_llseek,
};

static int __init fortune_init(void)
{
	struct device *fortune_device;
	int result;

	result = alloc_chrdev_region(&fortune_dev_number, 0, 1, DEVICE_NAME);
	if (result < 0) {
		pr_err("%s: failed to allocate device number: %d\n",
		       DEVICE_NAME, result);
		return result;
	}

	cdev_init(&fortune_cdev, &fortune_fops);
	fortune_cdev.owner = THIS_MODULE;

	result = cdev_add(&fortune_cdev, fortune_dev_number, 1);
	if (result < 0) {
		pr_err("%s: failed to add cdev: %d\n", DEVICE_NAME, result);
		goto unregister_device_number;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	fortune_class = class_create(CLASS_NAME);
#else
	fortune_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
	if (IS_ERR(fortune_class)) {
		result = PTR_ERR(fortune_class);
		pr_err("%s: failed to create class: %d\n", DEVICE_NAME, result);
		goto delete_cdev;
	}

	fortune_device = device_create(fortune_class, NULL, fortune_dev_number,
				       NULL, DEVICE_NAME);
	if (IS_ERR(fortune_device)) {
		result = PTR_ERR(fortune_device);
		pr_err("%s: failed to create /dev/%s\n", DEVICE_NAME,
		       DEVICE_NAME);
		goto destroy_class;
	}

	pr_info("%s: loaded with major=%d minor=%d\n",
		DEVICE_NAME, MAJOR(fortune_dev_number), MINOR(fortune_dev_number));
	return 0;

destroy_class:
	class_destroy(fortune_class);
delete_cdev:
	cdev_del(&fortune_cdev);
unregister_device_number:
	unregister_chrdev_region(fortune_dev_number, 1);
	return result;
}

static void __exit fortune_exit(void)
{
	device_destroy(fortune_class, fortune_dev_number);
	class_destroy(fortune_class);
	cdev_del(&fortune_cdev);
	unregister_chrdev_region(fortune_dev_number, 1);
	pr_info("%s: unloaded\n", DEVICE_NAME);
}

module_init(fortune_init);
module_exit(fortune_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Avent");
MODULE_DESCRIPTION("Simple /dev/fortune_device character device driver");
MODULE_VERSION("1.0");
