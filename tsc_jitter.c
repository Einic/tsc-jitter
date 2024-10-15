#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/math64.h>

#define SLEEP_MS 100 // sleep interval in milliseconds
#define MAX_BUFFER 256
#define CPUINFO_PATH "/proc/cpuinfo"


static unsigned long tsc_nominal_hz = 0;
static unsigned long tsc_jitter = 0;
static struct timer_list jitter_timer;
static unsigned long expected_inc;
static unsigned long low, high;

static unsigned long rdtscp(int *chip, int *core) {
    unsigned a, d, c;
    asm volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));

    *chip = (c & 0xFFF000)>>12;
    *core = c & 0xFFF;
    return ((unsigned long)a) | (((unsigned long)d) << 32);;
}

static unsigned long get_tsc_frequency(void) {
    struct file *file;
    char *buffer;
    mm_segment_t old_fs;
    unsigned long frequency = 0;
    ssize_t bytes_read;
    char *line, *value_start = NULL;
    unsigned long int_part = 0, frac_part = 0;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    file = filp_open(CPUINFO_PATH, O_RDONLY, 0);
    if (IS_ERR(file)) {
        printk(KERN_ERR "Failed to open %s\n", CPUINFO_PATH);
        set_fs(old_fs);
        return 0;
    }

    buffer = kmalloc(MAX_BUFFER, GFP_KERNEL);
    if (!buffer) {
        printk(KERN_ERR "Failed to allocate memory for buffer\n");
        filp_close(file, NULL);
        set_fs(old_fs);
        return 0;
    }

    bytes_read = vfs_read(file, buffer, MAX_BUFFER - 1, &file->f_pos);
    buffer[bytes_read] = '\0'; // Null-terminate the buffer

    // Directly find and parse the cpu MHz line
    line = strstr(buffer, "cpu MHz");
    if (line) {
        value_start = strchr(line, ':');
        if (value_start) {
            value_start++;
            value_start += strspn(value_start, " \t"); // Skip spaces

            // Parsing integer and decimal parts
            int_part = simple_strtoul(value_start, &value_start, 10);
            if (*value_start == '.') {
                frac_part = simple_strtoul(value_start + 1, NULL, 10);
            }

            frequency = int_part * 1000000 + (frac_part * 1000); // In Hz
            printk(KERN_INFO "Parsed frequency: %lu Hz\n", frequency);
        } else {
            printk(KERN_ERR "Failed to find ':' in cpu MHz line\n");
        }
    } else {
        printk(KERN_ERR "cpu MHz not found in /proc/cpuinfo\n");
    }

    kfree(buffer);
    filp_close(file, NULL);
    set_fs(old_fs);

    if (frequency == 0) {
        printk(KERN_ERR "Failed to read TSC frequency\n");
    }

    return frequency;
}


static void jitter_timer_callback(struct timer_list *timer) {
    unsigned long start, delta;
    int start_chip=0, start_core=0, end_chip=0, end_core=0;

    start = rdtscp(&start_chip, &start_core);
    mdelay(SLEEP_MS);
    delta = rdtscp(&end_chip, &end_core) - start;
			   
    if (delta > high || delta < low) {
        printk(KERN_WARNING "TSC jitter detected: %lu (chip: %d core: %d to chip: %d core: %d)\n",
               delta, start_chip, start_core, end_chip, end_core);
    } 
	
	tsc_jitter = delta;
	
    mod_timer(timer, jiffies + msecs_to_jiffies(SLEEP_MS));
}

static int jitter_proc_show(struct seq_file *m, void *v) {
	seq_printf(m, "tsc_jitter: %lu\n", tsc_jitter);	
    return 0;
}

static int jitter_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, jitter_proc_show, NULL);
}

static const struct file_operations jitter_proc_fops = {
    .owner = THIS_MODULE,
    .open = jitter_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init tsc_jitter_init(void) {
    printk(KERN_INFO "Initializing TSC jitter module\n");

    tsc_nominal_hz = get_tsc_frequency();
    if (!tsc_nominal_hz) {
        printk(KERN_ERR "Failed to read TSC frequency\n");
        return -EINVAL;
    }

    printk(KERN_INFO "TSC Frequency: %lu Hz\n", tsc_nominal_hz);

    expected_inc = (unsigned long)(1.0 * SLEEP_MS / 1000 * tsc_nominal_hz);
    low = (unsigned long)(expected_inc * 0.95);
    high = (unsigned long)(expected_inc * 1.05);

    printk(KERN_INFO "Expected Increment: %lu, Low: %lu, High: %lu\n", expected_inc, low, high);

    proc_create("tsc_jitter", 0, NULL, &jitter_proc_fops);

    timer_setup(&jitter_timer, jitter_timer_callback, 0);
    mod_timer(&jitter_timer, jiffies + msecs_to_jiffies(SLEEP_MS));

    return 0;
}

static void __exit tsc_jitter_exit(void) {
    printk(KERN_INFO "Exiting TSC jitter module\n");

    del_timer(&jitter_timer);
    remove_proc_entry("tsc_jitter", NULL);
}

module_init(tsc_jitter_init);
module_exit(tsc_jitter_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Einic <einicyeo AT gmail.com>");
MODULE_DESCRIPTION("Utilizing the TSC Jitter Kernel Module to Improve Clock Source Accuracy");
