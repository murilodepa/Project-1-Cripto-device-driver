/*
 * @file   ebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief   An introductory character driver to support the second article of my series on
 * Linux loadable kernel module (LKM) development. This module maps to /dev/ebbchar and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this the LKM.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
 */

/* Bibliotecas necessárias para o desenvolvimento do projeto */
#include <linux/mutex.h>
#include <linux/init.h>       // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>     // Core header for loading LKMs into the kernel
#include <linux/device.h>     // Header to support the kernel Driver Model
#include <linux/kernel.h>     // Contains types, macros, functions for the kernel
#include <linux/fs.h>         // Header for the Linux file system support
#include <linux/uaccess.h>    // Required for the copy to user function
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/string.h>

#define DEVICE_NAME "ebbchar" ///< The device will appear at /dev/ebbchar using this value
#define CLASS_NAME "ebb"      ///< The device class -- this is a character device driver

/* Skcipher kernel crypto API */
#include <crypto/skcipher.h>
/* Scatterlist manipulation */
#include <linux/scatterlist.h>
/* Error macros */
#include <linux/err.h>

/* Biblioteca necessária para realização do hash */
#include <crypto/hash.h>

struct tcrypt_result
{
    struct completion completion;
    int err;
};

/* tie all data structures together */
struct skcipher_def
{
    struct scatterlist sg;
    struct crypto_skcipher *tfm;
    struct skcipher_request *req;
    struct tcrypt_result result;
};

static char *keyp = "";
module_param(keyp, charp, 0000);
MODULE_PARM_DESC(keyp, "A character string");

static char *iv = "";
module_param(iv, charp, 0000);
MODULE_PARM_DESC(iv, "A character string");

MODULE_LICENSE("GPL");                                        ///< The license type -- this affects available functionality
MODULE_AUTHOR("Derek Molloy");                                ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver for the BBB"); ///< The description -- see modinfo
MODULE_VERSION("0.1");                                        ///< A version number to inform users

static int majorNumber; ///< Stores the device number -- determined automatically
static char message[1024] = {0};
char result[32];
char hashResult[40];
char message_conv[34];                      ///< Memory for the string that is passed from userspace
static short size_of_message;               ///< Used to remember the size of the string stored
static int numberOpens = 0;                 ///< Counts the number of times the device is opened
static struct class *ebbcharClass = NULL;   ///< The device-driver class struct pointer
static struct device *ebbcharDevice = NULL; ///< The device-driver device struct pointer

// The prototype functions for the character driver -- must come before the struct definition
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
    {
        .open = dev_open,
        .read = dev_read,
        .write = dev_write,
        .release = dev_release,
};

static DEFINE_MUTEX(ebbchar_mutex);
/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static void test_skcipher_cb(struct crypto_async_request *req, int error)
{
    struct tcrypt_result *result = req->data;

    if (error == -EINPROGRESS)
        return;
    result->err = error;
    complete(&result->completion);
    pr_info("Encryption finished successfully\n");
}

/* Realiza a conversão de Hexadecimal para String */
void conv(char *vet, char *resul, int tamanho)
{
    int calc = 0;
    int j = 0;
    int i = 0;
    while (i < tamanho)
    {
        if (vet[i] > 96)
        {
            calc += (vet[i] - 87) * 16;
        }
        else
        {
            calc += (vet[i] - 48) * 16;
        }

        if (vet[i + 1] > 96)
        {
            calc += (vet[i + 1] - 87);
        }
        else
        {
            calc += (vet[i + 1] - 48);
        }
        resul[j] = calc;
        calc = 0;
        j++;
        i += 2;
    }
    resul[j] = '\0';
}

/* Conversão para string */
int toString(unsigned char n)
{
    if (n > 9)
    {
        n += 87;
    }
    else
    {
        n += 48;
    }
    return n;
}

static unsigned int test_skcipher_encdec(struct skcipher_def *sk,
                                         int enc)
{
    int rc = 0;

    if (enc)
        rc = crypto_skcipher_encrypt(sk->req);
    else
        rc = crypto_skcipher_decrypt(sk->req);

    if (rc)
        pr_info("skcipher encrypt returned with result %d\n", rc);

    return rc;
}

/* Initialize and trigger cipher operation */
static int test_skcipher(char *scratchpad1, int tam, int tipo)
{
    struct skcipher_def sk;
    struct crypto_skcipher *skcipher = NULL;
    struct skcipher_request *req = NULL;
    char *scratchpad = NULL;
    char *ivdata = NULL;
    unsigned char key[32];
    int ret = -EFAULT;
    char *resultdata = NULL;

    skcipher = crypto_alloc_skcipher("cbc(aes)", 0, 0);
    if (IS_ERR(skcipher))
    {
        pr_info("could not allocate skcipher handle\n");
        return PTR_ERR(skcipher);
    }

    req = skcipher_request_alloc(skcipher, GFP_KERNEL);
    if (!req)
    {
        pr_info("could not allocate skcipher request\n");
        ret = -ENOMEM;
        goto out;
    }

    skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
                                  test_skcipher_cb,
                                  &sk.result);

    /* AES 256 with random key */
    //key=keyp;
    strcpy(key, keyp);
    if (crypto_skcipher_setkey(skcipher, key, 16))
    {
        pr_info("key could not be set\n");
        ret = -EAGAIN;
        goto out;
    }

    /* IV will be random */
    ivdata = kmalloc(16, GFP_KERNEL);
    if (!ivdata)
    {
        pr_info("could not allocate ivdata\n");
        goto out;
    }
    strcpy(ivdata, iv);
    //ivdata=iv;

    /* Input data will be random */
    scratchpad = kmalloc(16, GFP_KERNEL);
    if (!scratchpad)
    {
        pr_info("could not allocate scratchpad\n");
        goto out;
    }
    strcpy(scratchpad, scratchpad1);

    sk.tfm = skcipher;
    sk.req = req;

    /* We encrypt one block */
    sg_init_one(&sk.sg, scratchpad, 16);
    skcipher_request_set_crypt(req, &sk.sg, &sk.sg, 16, ivdata);
    init_completion(&sk.result.completion);
    //crypto_init_wait(&sk.wait);

    /* encrypt data */
    ret = test_skcipher_encdec(&sk, tipo);
    if (ret)
        goto out;

    pr_info("Encryption triggered successfully\n");
    resultdata = sg_virt(&sk.sg);
    strcpy(result, resultdata);

    print_hex_dump(KERN_DEBUG, "encr text: ", DUMP_PREFIX_NONE, 16, 1,
                   resultdata, 64, true);

out:
    if (skcipher)
        crypto_free_skcipher(skcipher);
    if (req)
        skcipher_request_free(req);
    if (ivdata)
        kfree(ivdata);
    if (scratchpad)
        kfree(scratchpad);
    return ret;
}

static int __init ebbchar_init(void)
{

    printk(KERN_INFO "EBBChar: Initializing the EBBChar LKM\n");

    // Try to dynamically allocate a major number for the device -- more difficult but worth it
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0)
    {
        printk(KERN_ALERT "EBBChar failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "EBBChar: registered correctly with major number %d\n", majorNumber);

    // Register the device class
    ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(ebbcharClass))
    { // Check for error and clean up if there is
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(ebbcharClass); // Correct way to return an error on a pointer
    }
    printk(KERN_INFO "EBBChar: device class registered correctly\n");

    // Register the device driver
    ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(ebbcharDevice))
    {                                // Clean up if there is an error
        class_destroy(ebbcharClass); // Repeated code but the alternative is goto statements
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(ebbcharDevice);
    }
    printk(KERN_INFO "EBBChar: device class created correctly\n"); // Made it! device was initialized

    mutex_init(&ebbchar_mutex);
    return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit ebbchar_exit(void)
{
    mutex_destroy(&ebbchar_mutex);
    device_destroy(ebbcharClass, MKDEV(majorNumber, 0)); // remove the device
    class_unregister(ebbcharClass);                      // unregister the device class
    class_destroy(ebbcharClass);                         // remove the device class
    unregister_chrdev(majorNumber, DEVICE_NAME);         // unregister the major number
    printk(KERN_INFO "EBBChar: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
    if (!mutex_trylock(&ebbchar_mutex))
    { // Try to acquire the mutex (returns 0 on fail)
        printk(KERN_ALERT "EBBChar: Device in use by another process");
        return -EBUSY;
    }
    numberOpens++;
    printk(KERN_INFO "EBBChar: Device has been opened %d time(s)\n", numberOpens);
    return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    int error_count = 0;
    // copy_to_user has the format ( * to, *from, size) and returns 0 on success

    error_count = copy_to_user(buffer, message, size_of_message);

    if (error_count == 0)
    { // if true then have success
        printk(KERN_INFO "EBBChar: Sent %d characters to the user\n", size_of_message);
        return (size_of_message = 0); // clear the position to the start and return 0
    }
    else
    {
        printk(KERN_INFO "EBBChar: Failed to send %d characters to the user\n", error_count);
        return -EFAULT; // Failed -- return a bad address message (i.e. -14)
    }
}

/* Realiza os cálculos necessários do hash */
void func_hash(char *buffer, int tam)
{
    struct shash_desc *desc;
    struct crypto_shash *tfm;
    int rc = 0;
    char hashText[21] = {0};

    desc = vmalloc(sizeof(struct shash_desc));
    desc->tfm = crypto_alloc_shash("sha1", 0, 0);

    rc = crypto_shash_init(desc);
    
	if (rc)
    {
        printk("Erro na execução da crypto hash init");
    }
    
	rc = crypto_shash_update(desc, buffer, tam);
    
	if (rc)
    {
        printk("Erro na execução da crypto hash update");
    }

    rc = crypto_shash_final(desc, hashText);
    
	if (rc)
    {
        printk("Erro na execução da crypto hash final");
    }

    print_hex_dump(KERN_DEBUG, "SHAZAOO - ", DUMP_PREFIX_NONE, 20, 1, hashText, 20, true);
    strcpy(hashResult, hashText);

    crypto_free_shash(desc->tfm);
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    conv((buffer + 2), message_conv, len - 2);
    int l = 0;

    switch (buffer[0])
    {
    case 'c':
        test_skcipher((char *)message_conv, 16, 1);
        break;
    case 'd':
        test_skcipher((char *)message_conv, 256, 0);
        break;
    case 'h':

        func_hash((char *)message_conv, strlen(message_conv));
        break;
    }

    char vet[64];
    int j = 0, i = 0;

    if (buffer[0] != 'h')
    {
        while (i < 16)
        {
            vet[j] = toString((unsigned char)result[i] / 16);
            j++;
            vet[j] = toString((unsigned char)result[i] % 16);
            j++;
            i++;
        }
        vet[j] = '\0';
    }
    else
    {
        while (i < 20)
        {
            vet[j] = toString((unsigned char)hashResult[i] / 16);
            j++;
            vet[j] = toString((unsigned char)hashResult[i] % 16);
            j++;
            i++;
        }
        vet[j] = '\0';
    }

    sprintf(message, "%s", vet);       // appending received string with its length
    size_of_message = strlen(message); // store the length of the stored message
    printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", size_of_message);

    return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
    mutex_unlock(&ebbchar_mutex);
    printk(KERN_INFO "EBBChar: Device successfully closed\n");
    return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(ebbchar_init);
module_exit(ebbchar_exit);