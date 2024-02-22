#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include "message_slot.h"

/* code from recitation */
#include <linux/slab.h>
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/fcntl.h>      /* open */
#include <linux/unistd.h>     /* exit */
#include <linux/errno.h>

MODULE_LICENSE("GPL");

/* data to keep in file->private_data */
struct file_descriptor {
    struct device_channels* channel;
    int minor;
};

struct device_channels {
    ssize_t message_length;
    char message[BUFFER_SIZE];
    unsigned long channel;
    struct device_channels* next;
};

/* keep all channels */
static struct device_channels* device_files[257];

struct device_channels* last_channel(struct device_channels* channel);
struct device_channels* get_channel(struct device_channels* head, unsigned long channel_id);
static int device_open(struct inode* inode, struct file* file);
static int device_release(struct inode* inode, struct file* file);
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param);
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
static int __init simple_init(void);
void free_file(struct file* file);
void free_devices_list(void);
static void __exit simple_cleanup(void);


struct device_channels* last_channel(struct device_channels* channel) {
    struct device_channels* head;
    head = channel;
    while (head != NULL && head->next != NULL){
        head = head->next;
    }
    return head;
}

/* get channel by channel_id */
struct device_channels* get_channel(struct device_channels* head, unsigned long channel_id){
    struct device_channels* channel_node;
    channel_node = head;
    while (channel_node != NULL){
        if (channel_node->channel == channel_id){
            return channel_node;
        }
        channel_node = channel_node->next;
    }
    return NULL;
}


static int device_open(struct inode* inode, struct file* file) {
    int minor;
    struct device_channels* device_channel;
    struct file_descriptor* file_des;

    /* update file -> private data*/
    minor = iminor(inode);
    if (file->private_data == NULL) {
        file_des = (struct file_descriptor *) kmalloc(sizeof(struct file_descriptor), GFP_KERNEL);
        if (file_des == NULL) {
            return -ENOMEM;
        }
        file_des->minor = minor;
        file->private_data = file_des;
    }

    /* crete dummy channel */
    if (device_files[minor] == NULL) {
        device_channel = (struct device_channels*) kmalloc(sizeof(struct device_channels), GFP_KERNEL);
        if (device_channel == NULL) {
            return -ENOMEM;
        }
        device_channel->channel = 0;
        device_channel->message_length = 0;
        device_files[minor] = device_channel;
    }
    return 0;
}


static int device_release(struct inode* inode, struct file* file) {
    free_file(file);
    return 0;
}


static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param){
    int minor;
    struct device_channels* last_device_channel;
    struct device_channels* new_device_channel;
    struct device_channels* exist_channel;

    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0) {
        return -EINVAL;
    }

    minor = ((struct file_descriptor*)(file->private_data))->minor;

    /* should never be NULL */
    if (device_files[minor] == NULL){
        return -1;
    }

    exist_channel = get_channel(device_files[minor], ioctl_param);

    if (exist_channel == NULL){
        last_device_channel = last_channel(device_files[minor]);
        new_device_channel = (struct device_channels*) kmalloc(sizeof(struct device_channels), GFP_KERNEL);
        if (new_device_channel == NULL){
            return -ENOMEM;
        }
        new_device_channel->channel = ioctl_param;
        new_device_channel->message_length = 0;
        last_device_channel->next = new_device_channel;
        exist_channel = new_device_channel;
    }

    ((struct file_descriptor*)(file->private_data))->channel = exist_channel;

    return 0;
}

static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset){
ssize_t i;
ssize_t message_length;
char* message;
struct device_channels* channel_struct;
struct file_descriptor* file_des;

file_des = (struct file_descriptor*)(file->private_data);
if (file_des == NULL){
return -EINVAL;
}

channel_struct = (struct device_channels*)(file_des->channel);
if (channel_struct == NULL){
return -EINVAL;
}

message = channel_struct->message;
message_length = channel_struct->message_length;

if (message_length == 0){
return -EWOULDBLOCK;
}

if (length < strlen(message)){
return -ENOSPC;
}

for (i = 0; i < message_length; i++) {
if (put_user(message[i], &buffer[i]) < 0){
return -EFAULT;
}
}
return i;
}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
ssize_t i;
char* message;
struct file_descriptor* file_des;
struct device_channels* channel_struct;

file_des = (struct file_descriptor*)(file->private_data);

if (file_des == NULL){
return -EINVAL;
}

if (length > BUFFER_SIZE || length <= 0){
return -EMSGSIZE;
}

channel_struct = (struct device_channels*)(file_des->channel);
if (channel_struct == NULL){
return -EINVAL;
}

message = channel_struct->message;
for (i = 0; i < length; i++) {
if (get_user(message[i], &buffer[i]) < 0){
return -EFAULT;
}
}
channel_struct->message_length = length;

return i;
}



/* rec06 */
struct file_operations Fops = {
        .owner	  = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl = device_ioctl,
        .release        = device_release,
};

static int __init simple_init(void) {
    int rc;
    rc = register_chrdev(MAJOR_NUMBER, "Message_slot", &Fops);
    if( rc < 0 ) {
        return rc;
    }
    return 0;
}

void free_file(struct file* file){
    kfree(file->private_data);
}


void free_devices_list(void){
    int i;
    struct device_channels* head;
    struct device_channels* prev;
    /* iterate over device_files and free all channels */
    for (i = 0; i < 257; i++){
        head = device_files[i];
        prev = head;
        while (head != NULL){
            head = head->next;
            kfree(prev);
            prev = head;
        }
    }
}


static void __exit simple_cleanup(void) {
    unregister_chrdev(MAJOR_NUMBER, "Message_slot");
    free_devices_list();
}


module_init(simple_init);
module_exit(simple_cleanup);



