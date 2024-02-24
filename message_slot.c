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


/* Function to find the last channel in the chain */
struct device_channels* last_channel(struct device_channels* channel) {
    struct device_channels* traverse = channel;
    // Iterate until the last channel is reached
    while (traverse != NULL && traverse->next != NULL){
        traverse = traverse->next;
    }
    return traverse; // Return the last or terminal channel
}

/* Function to locate a channel by its unique channel_id */
struct device_channels* get_channel(struct device_channels* head, unsigned long channel_id){
    struct device_channels* search = head;
    // Loop through the channels to find a match by ID
    while (search != NULL){
        if (search->channel == channel_id){
            return search; // Channel found
        }
        search = search->next;
    }
    return NULL; // If no matching channel, return NULL
}



static int device_open(struct inode* inode, struct file* file) {
    int minorNumber;
    struct device_channels* newDeviceChannel;
    struct file_descriptor* newFileDescriptor;

    /* Initialize file's private data */
    minorNumber = iminor(inode);
    if (file->private_data == NULL) {
        newFileDescriptor = (struct file_descriptor *) kmalloc(sizeof(struct file_descriptor), GFP_KERNEL);
        if (newFileDescriptor == NULL) {
            return -ENOMEM; // Memory allocation failed
        }
        newFileDescriptor->minor = minorNumber;
        file->private_data = newFileDescriptor;
    }

    /* Create a placeholder channel if none exists */
    if (device_files[minorNumber] == NULL) {
        newDeviceChannel = (struct device_channels*) kmalloc(sizeof(struct device_channels), GFP_KERNEL);
        if (newDeviceChannel == NULL) {
            return -ENOMEM; // Memory allocation failed
        }
        newDeviceChannel->channel = 0; // Initialize dummy channel
        newDeviceChannel->message_length = 0; // Set initial message length to 0
        device_files[minorNumber] = newDeviceChannel; // Assign to device files
    }
    return 0; // Successful operation
}


static int device_release(struct inode* inode, struct file* file) {
    free_file(file);
    return 0;
}


static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param){
    int deviceMinor;
    struct device_channels* tailChannel;
    struct device_channels* newChannel;
    struct device_channels* existingChannel;

    // Validate the command ID and parameter
    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0) {
        return -EINVAL; // Invalid argument
    }

    deviceMinor = ((struct file_descriptor*)(file->private_data))->minor;

    // Device files should already be initialized
    if (device_files[deviceMinor] == NULL){
        return -1; // Error condition, should not happen
    }

    // Attempt to find an existing channel with the provided ID
    existingChannel = get_channel(device_files[deviceMinor], ioctl_param);

    // If the channel does not exist, create a new one
    if (existingChannel == NULL){
        tailChannel = last_channel(device_files[deviceMinor]);
        newChannel = (struct device_channels*) kmalloc(sizeof(struct device_channels), GFP_KERNEL);
        if (newChannel == NULL){
            return -ENOMEM; // Failed to allocate memory
        }
        newChannel->channel = ioctl_param; // Set channel ID
        newChannel->message_length = 0; // Initialize message length
        tailChannel->next = newChannel; // Link the new channel
        existingChannel = newChannel; // Update pointer to the newly created channel
    }

    // Update the file descriptor with the (newly) selected channel
    ((struct file_descriptor*)(file->private_data))->channel = existingChannel;

    return 0; // Success
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



