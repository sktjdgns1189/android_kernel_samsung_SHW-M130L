/* drivers/media/tdmb/tdmb.c
 *
 *  TDMB Driver for Linux
 *
 *  klaatu, Copyright (c) 2009 Samsung Electronics
 *  		http://www.samsung.com/
 *
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/fcntl.h>

// for delay(sleep)
#include <linux/delay.h>

// for mutex
#include <linux/mutex.h>

//using copy to user
#include <asm/uaccess.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <linux/workqueue.h>
#include <linux/irq.h>
#include <asm/mach/irq.h>
#include <linux/interrupt.h>

#include <asm/io.h>
//#include <asm/arch/regs-gpio.h>

//#include <plat/regs-gpio.h>
//#include <plat/gpio-cfg.h>
//#include <plat/gpio-bank-j4.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-aries.h>    //#include <plat/regs-gpio.h>

#include "tdmb.h"

#ifdef CONFIG_TDMB_GDM7024
#include "gdm_headers.h"
#include "tdmb_drv_gct.h"
#endif

#ifdef CONFIG_TDMB_T3700
#include "INC_INCLUDES.h"
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
struct early_suspend    early_suspend;
int    OnSleepFlag = 0;
unsigned long savArg = 0;
#endif

#define TDMB_DEBUG

#ifdef TDMB_DEBUG
//#define DPRINTK(x...) printk(KERN_DEBUG "TDMB " x)
#define DPRINTK(x...) printk("TDMB " x)
#else /* TDMB_DEBUG */
#define DPRINTK(x...)  /* null */
#endif /* TDMB_DEBUG */

#define TDMB_DEV_NAME "tdmb"
#define TDMB_DEV_MAJOR 225
#define TDMB_DEV_MINOR 0

extern unsigned int HWREV;

// for Factory
#define TDMB_FA_NAME "tdmbFA"
int FactoryMode = 0;


//
#define TDMB_PRE_MALLOC 1

// global variables --------------------------------

static struct class *tdmb_class;
static int tdmb_major;

// ring buffer
char * TS_RING = NULL;
unsigned int *ts_head = NULL;
unsigned int *ts_tail = NULL ;
char *ts_buffer = NULL ;
unsigned int ts_size = NULL;

unsigned int *cmd_head = NULL;
unsigned int *cmd_tail = NULL ;
char *cmd_buffer = NULL ;
unsigned int cmd_size = NULL;

#ifdef CONFIG_TDMB_T3700 
enum {
    FALSE = 0,
    TRUE  = 1
};
#endif
// extern -----------------------------------------
extern int tdmbspi_init(void);
extern tdmb_type g_TDMBGlobal;

// ------------------------------------------------

//
int tdmb_open(struct inode *inode, struct file *filp)
{
	DPRINTK("tdmb_open! \r\n");

	return 0;
}

int tdmb_read(struct inode *inode, struct file *filp)
{
	DPRINTK("tdmb_read! \r\n");

	return 0;
}

//#ifdef CONFIG_PROC_FS
static int tdmb_proc_read(char *page, char **start, off_t off, int count, 
	int *eof, void *data)
{

    return 1;
}

static int tdmb_proc_write(struct file *file, const char *buf, 
	unsigned long count, void *data)
{
    char tfa[30];
    DPRINTK("TDMBFA write %d\n",count);
    if(count > 10)
        return -EFAULT;
    if(copy_from_user(tfa,buf, count)) return -EFAULT;

    DPRINTK("TDMB [%s]\n", tfa);
    if(!strncmp(tfa,"15FAON",6))
        FactoryMode = 1;
    else if(!strncmp(tfa,"15FAOFF",7))
        FactoryMode = 0;
    
	return count;
}

static int __init tdmb_create_procfs(void)
{
	struct proc_dir_entry *proc_pdc_root = NULL;
	struct proc_dir_entry *ent;

	ent = create_proc_entry(TDMB_FA_NAME, S_IFREG|S_IRUGO|S_IWUGO, 0);
	if (!ent) 
    {
        DPRINTK("tdmbFA create error\n");
        return -1;
    }
	ent->read_proc = tdmb_proc_read;
	ent->write_proc = tdmb_proc_write;
//	ent->owner = THIS_MODULE;
	return 0;
}

void tdmb_remove_procfs()
{
    remove_proc_entry(TDMB_FA_NAME, 0);
}

//#endif

#ifdef TDMB_FROM_FILE
extern void tfftimer_exit(void);
#endif

int tdmb_release(struct inode *inode, struct file *filp)
{
	DPRINTK("tdmb_release! \r\n");

#ifdef TDMB_FROM_FILE
	tfftimer_exit();
#endif
    // For tdmb_release() without TDMB POWER OFF (App abnormal -> kernal panic)
    if(IsTDMBPowerOn())
    {
        TDMB_PowerOff();
    }

#if TDMB_PRE_MALLOC
    ts_size = 0;
    cmd_size = 0;
#else
	if(TS_RING != 0)
	{
		kfree(TS_RING);
		TS_RING = 0;
        ts_size = 0;
        cmd_size = 0;
	}
#endif    
	return 0;
}

#if TDMB_PRE_MALLOC
int tdmb_makeRingBuffer()
{
	size_t size = TDMB_RING_BUFFER_MAPPING_SIZE;

	// size should aligned in PAGE_SIZE
	if(size % PAGE_SIZE) // klaatu hard coding
		size = size + size % PAGE_SIZE;
	
	TS_RING = kmalloc(size, GFP_KERNEL);
    DPRINTK("RING Buff Create OK\n");
}

#endif

int tdmb_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size;
	unsigned long pfn;
	
	DPRINTK(" %s \n", __FUNCTION__);

	vma->vm_flags |= VM_RESERVED;
	size = vma->vm_end - vma->vm_start;
	DPRINTK("size given : %x\n",size);

#if TDMB_PRE_MALLOC
    size = TDMB_RING_BUFFER_MAPPING_SIZE;
    if(!TS_RING)
    {
        DPRINTK("RING Buff ReAlloc !!\n");
#endif
	// size should aligned in PAGE_SIZE
	if(size % PAGE_SIZE) // klaatu hard coding
		size = size + size % PAGE_SIZE;
	
	TS_RING = kmalloc(size, GFP_KERNEL);
#if TDMB_PRE_MALLOC
    }
#endif

	pfn = virt_to_phys(TS_RING) >> PAGE_SHIFT;

	DPRINTK("vm_start:%x,TS_RING:%x,size:%x,prot:%x,pfn:%x\n", 
			vma->vm_start, TS_RING, size, vma->vm_page_prot,pfn);

	if(remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
		return -EAGAIN;

	DPRINTK(" succeeded \n");

	ts_head   = TS_RING;
	ts_tail   = TS_RING + 4;
	ts_buffer = TS_RING + 8;

	*ts_head = 0;
	*ts_tail = 0;

	ts_size = size-8; // klaatu hard coding
	ts_size = (ts_size/DMB_TS_SIZE)*DMB_TS_SIZE - 30*DMB_TS_SIZE;

	DPRINTK("ts_head : %x, ts_tail : %x, ts_buffer : %x,ts_size : %x \n", 
			ts_head, ts_tail, ts_buffer, ts_size);
	 
	cmd_buffer = ts_buffer + ts_size + 8;
	cmd_head   = cmd_buffer - 8;
	cmd_tail   = cmd_buffer - 4;

	*cmd_head = 0;
	*cmd_tail = 0;

	cmd_size = 30*DMB_TS_SIZE - 8; // klaatu hard coding

	DPRINTK("cmd_head : %x, cmd_tail : %x, cmd_buffer : %x,cmd_size : %x \n", 
			cmd_head, cmd_tail, cmd_buffer, cmd_size);

	 
	return 0;
}


int _tdmb_cmd_update(unsigned char* byCmdsHeader, unsigned char byCmdsHeaderSize, unsigned char* byCmds, unsigned short bySize)
{
	unsigned int size;
	unsigned int head;
	unsigned int tail;
	unsigned int dist;
	unsigned int temp_size;
	unsigned int dataSize;	

	if(bySize > cmd_size )
	{
		DPRINTK(" Error - cmd size too large \n");
		return FALSE;
	}

	head = *cmd_head;
	tail = *cmd_tail;
    size = cmd_size;
    dataSize = bySize + byCmdsHeaderSize;

	if( head >= tail )
		dist = head-tail;
	else
		dist = size + head-tail;
	
	if( size - dist <= dataSize )
	{
		DPRINTK(" too small space is left in Command Ring Buffer!!\n");
		return FALSE;
	}

    DPRINTK(" Error - %x head %d tail %d\n", cmd_buffer, head, tail);

    if( head+dataSize <= size )
    {
        memcpy( (cmd_buffer+head), (char*)byCmdsHeader, byCmdsHeaderSize); 
        memcpy( (cmd_buffer+head+byCmdsHeaderSize), (char*)byCmds, size);   
        head += dataSize;
        if(head == size)
            head = 0;
    }
    else
    {
        temp_size = size-head;
        if ( temp_size < byCmdsHeaderSize )
        {
            memcpy( (cmd_buffer+head), (char*)byCmdsHeader, temp_size);
            memcpy( (cmd_buffer), (char*)byCmdsHeader+temp_size, byCmdsHeaderSize-temp_size);
            head = byCmdsHeaderSize-temp_size;
        }
        else 
        {
            memcpy( (cmd_buffer+head), (char*)byCmdsHeader, byCmdsHeaderSize);
            head += byCmdsHeaderSize;
            if(head == size)
                head = 0;
        }
        temp_size = size-head;
        memcpy( (cmd_buffer+head), (char*)byCmds, temp_size);
        head = dataSize-temp_size;        
        memcpy( cmd_buffer, (char*)(byCmds+temp_size), head);
    }
    *cmd_head = head;
	return TRUE ;
}

unsigned char _tdmb_make_result(unsigned char byCmd, unsigned short byDataLength, unsigned char* pbyData)
{
	unsigned char byCmds[256] = { 0, } ;
	
	byCmds[0] = TDMB_CMD_START_FLAG ;
	byCmds[1] = byCmd ;
	byCmds[2] = (byDataLength>>8)&0xff ;
	byCmds[3] = byDataLength&0xff ;

#if 0	
	if (byDataLength > 0)
	{
		if (pbyData == NULL)
		{
		    // to error 
			return FALSE;
	    }
        memcpy(byCmds + 8, pbyData, byDataLength) ;
	}
#endif	
	_tdmb_cmd_update( byCmds, 4 , pbyData,  byDataLength ) ;
	
	return TRUE ;
}

extern int UpdateEnsembleInfo(EnsembleInfoType* ensembleInfo, unsigned long freq);

static int tdmb_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret =0;
	unsigned long ulFreq;
	
//	DPRINTK("call tdmb_ioctl \r\n");

    if(!IsTDMBPowerOn())
    {
        if ( cmd == IOCTL_TDMB_POWER_OFF )
        {
            DPRINTK("0x%x cmd : current state poweroff \r\n", cmd);
            return TRUE;
        }
        else if ( cmd > IOCTL_TDMB_POWER_ON )
        {
            DPRINTK("error 0x%x cmd : current state poweroff \r\n", cmd);
            return FALSE;
        }
    }
    else
    {
        if ( cmd == IOCTL_TDMB_POWER_ON )
        {
            DPRINTK("%d cmd : current state poweron \r\n", cmd);
            return TRUE;
        }
#ifdef CONFIG_HAS_EARLYSUSPEND        
        else if((OnSleepFlag)&&( cmd == IOCTL_TDMB_ASSIGN_CH ))
        {
            DPRINTK("error 0x%x cmd : current state OnSleep \r\n", cmd);
            savArg = arg;
            return FALSE;
        }
#endif        
    }
    
	switch(cmd)
	{
		case IOCTL_TDMB_GET_DATA_BUFFSIZE :
			DPRINTK("IOCTL_TDMB_GET_DATA_BUFFSIZE %d \r\n", ts_size);
			ret = copy_to_user( (unsigned int*)arg, &ts_size, sizeof(unsigned int));
			break;
			
        case IOCTL_TDMB_GET_CMD_BUFFSIZE :
            DPRINTK("IOCTL_TDMB_GET_CMD_BUFFSIZE %d \r\n", cmd_size);
            ret = copy_to_user( (unsigned int*)arg, &cmd_size, sizeof(unsigned int));
            break;
            
		case IOCTL_TDMB_POWER_ON:
			DPRINTK("IOCTL_TDMB_POWER_ON \n");
			ret = TDMB_PowerOn();
			break;

		case IOCTL_TDMB_POWER_OFF:
			DPRINTK("IOCTL_TDMB_POWER_OFF \r\n");
			TDMB_PowerOff();
            FactoryMode = 0;
			ret = TRUE;
			break;


		case IOCTL_TDMB_SCAN_FREQ_ASYNC:
		    {
		        unsigned long FIG_Frequency;
                FIG_Frequency = arg;
    			DPRINTK("IOCTL_TDMB_SCAN_FREQ_ASYNC \r\n");
#if defined(CONFIG_TDMB_GDM7024)
    			ret = TDMB_drv_gct_scan( arg );
#elif defined(CONFIG_TDMB_T3700)
                ulFreq = arg / 1000;
                if(INTERFACE_SCAN(T3700_I2C_ID80, ulFreq) == INC_SUCCESS)
                {
                    //TODO Scan good code ....
                    ret = TRUE;
                }
#endif    			
    			if ( ret == TRUE )
    			{
    		        EnsembleInfoType ensembleInfo;
    		        memset((char*)&ensembleInfo, 0x00, sizeof(EnsembleInfoType));
                    UpdateEnsembleInfo(&ensembleInfo, FIG_Frequency);
                    _tdmb_make_result( DMB_FIC_RESULT_DONE, sizeof(EnsembleInfoType), &ensembleInfo);
    			}
    			else
    			{
                    _tdmb_make_result( DMB_FIC_RESULT_FAIL, sizeof(unsigned long), &FIG_Frequency);    			    
    			}
    		}
			break;

		case IOCTL_TDMB_SCAN_FREQ_SYNC:
		    {
		        EnsembleInfoType* pEnsembleInfo = (EnsembleInfoType*)arg;
   		        unsigned long FIG_Frequency = pEnsembleInfo->EnsembleFrequency;

    			DPRINTK("IOCTL_TDMB_SCAN_FREQ_SYNC \r\n");
#if defined(CONFIG_TDMB_GDM7024)
    			ret = TDMB_drv_gct_scan( pEnsembleInfo->EnsembleFrequency );
#elif defined(CONFIG_TDMB_T3700)
                ulFreq = pEnsembleInfo->EnsembleFrequency / 1000;
                if(INTERFACE_SCAN(T3700_I2C_ID80, ulFreq) == INC_SUCCESS)
                {
                    //TODO Scan good code ....
                    ret = TRUE;
                }               
#endif    			
    			if ( ret == TRUE )
    			{
    		        EnsembleInfoType ensembleInfo;
    		        memset((char*)&ensembleInfo, 0x00, sizeof(EnsembleInfoType));
                    UpdateEnsembleInfo(&ensembleInfo, FIG_Frequency);    			
    			    copy_to_user( (EnsembleInfoType*)arg, &ensembleInfo, sizeof(EnsembleInfoType));
    			}
    			else
    			{
                    //
    			}
    		}
			break;

        case IOCTL_TDMB_SCANSTOP:
            DPRINTK("IOCTL_TDMB_SCANSTOP \r\n");
#if defined(CONFIG_TDMB_GDM7024)
			ret = TDMB_drv_gct_scan_stop( arg );
#elif defined(CONFIG_TDMB_T3700)
            ret = FALSE; //temp
#endif          
            break;			

		case IOCTL_TDMB_ASSIGN_CH :
			DPRINTK("IOCTL_TDMB_ASSIGN_CH %d\r\n", arg);
#if defined(CONFIG_TDMB_GDM7024)
			TDMB_drv_gct_set_freq_channel( arg );
			ret = TRUE;
#elif defined(CONFIG_TDMB_T3700)
            INC_CHANNEL_INFO stChInfo;
            INC_UINT8 reErr;
            
            stChInfo.ulRFFreq       = arg/1000 ;
            stChInfo.ucSubChID      = arg%1000;
            stChInfo.ucServiceType  = 0x0;
            if (stChInfo.ucSubChID >= 64 )
            {
                stChInfo.ucSubChID -= 64;
                stChInfo.ucServiceType  = 0x18;
            }           
/*            
            // Test 208736064
            stChInfo.ulRFFreq       = 205280 ;
            stChInfo.ucSubChID      = 0x04;
            stChInfo.ucServiceType  = 0x18;
            // Test end
*/ 

            if(FactoryMode == 1)
            {
                if((reErr = INTERFACE_START_TEST(T3700_I2C_ID80, &stChInfo)) == INC_SUCCESS)
                {
                     //TODO Ensemble  good code ....
                    ret = TRUE;
                }
/*                
                else if (reErr == INC_RETRY )
                {
                    int temp_ts_size = ts_size;
                    DPRINTK("IOCTL_TDMB_ASSIGN_CH retry\r\n");
                	TDMB_PowerOff();
                	TDMB_PowerOn();            	
                	ts_size = temp_ts_size;
                    if(INTERFACE_START_TEST(T3700_I2C_ID80, &stChInfo) == INC_SUCCESS)
                    {
                        ret = TRUE;
                    }
                }
*/                
            }
            else
            {
                if((reErr = INTERFACE_START(T3700_I2C_ID80, &stChInfo)) == INC_SUCCESS)
                {
                     //TODO Ensemble  good code ....
                    ret = TRUE;
                }
                else if (reErr == INC_RETRY )
                {
                    int temp_ts_size = ts_size;
                    DPRINTK("IOCTL_TDMB_ASSIGN_CH retry\r\n");
                	TDMB_PowerOff();
                	TDMB_PowerOn();            	
                	ts_size = temp_ts_size;
                if(INTERFACE_START(T3700_I2C_ID80, &stChInfo) == INC_SUCCESS)
                {
                    ret = TRUE;
                }
                }
            
            }
#endif			
			break;			

		case IOCTL_TDMB_GET_DM :
			{
#if defined(CONFIG_TDMB_GDM7024)
			    extern unsigned long TDMB_drv_gct_get_per(void);
			    tdmb_dm dmBuff;
			    dmBuff.rssi = (unsigned int)TDMB_drv_gct_get_rssi();
			    dmBuff.BER = (unsigned int)TDMB_drv_gct_get_ber();
			    dmBuff.PER = (unsigned int)TDMB_drv_gct_get_per();
			    dmBuff.antenna = TDMB_drv_gct_get_antenna();
                DPRINTK("TDMB " "rssi %d, ber %d, per %d", dmBuff.rssi, dmBuff.BER, dmBuff.PER);
				ret = copy_to_user( (tdmb_dm*)arg, &dmBuff, sizeof(tdmb_dm));
#elif defined(CONFIG_TDMB_T3700)
                extern INC_UINT8 INC_GET_SAMSUNG_ANT_LEVEL(INC_UINT8 ucI2CID);

			    tdmb_dm dmBuff;
			    dmBuff.rssi = INC_GET_RSSI(T3700_I2C_ID80/*T3700_I2C_ID80*/);
			    dmBuff.BER = INC_GET_SAMSUNG_BER(T3700_I2C_ID80/*T3700_I2C_ID80*/);
			    dmBuff.PER = 0;
			    dmBuff.antenna = INC_GET_SAMSUNG_ANT_LEVEL(T3700_I2C_ID80/*T3700_I2C_ID80*/);
				ret = copy_to_user( (tdmb_dm*)arg, &dmBuff, sizeof(tdmb_dm));
                DPRINTK("rssi %d, ber %d, ANT %d\r\n", dmBuff.rssi, dmBuff.BER, dmBuff.antenna);
				// to do...
#endif
			}
			break;
			
	}
	return ret;
}

static struct file_operations tdmb_ctl_fops = {
	owner:		THIS_MODULE,
	open:		tdmb_open,
	read:		tdmb_read,
	ioctl:		tdmb_ioctl,
	mmap:		tdmb_mmap,
	release:	tdmb_release,
	llseek:		no_llseek,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
void tdmb_setchannel()
{
    INC_CHANNEL_INFO stChInfo;
    INC_UINT8 reErr;

    DPRINTK("tdmb_setchannel\n");
    stChInfo.ulRFFreq       = savArg/1000 ;
    stChInfo.ucSubChID      = savArg%1000;
    stChInfo.ucServiceType  = 0x0;
    if (stChInfo.ucSubChID >= 64 )
    {
        stChInfo.ucSubChID -= 64;
        stChInfo.ucServiceType  = 0x18;
    }
    if((reErr = INTERFACE_START(T3700_I2C_ID80, &stChInfo)) == INC_SUCCESS)
    {
            //TODO Ensemble  good code ....
    //        ret = TRUE;
    }
}

void tdmb_early_suspend(struct early_suspend *h)
{
    DPRINTK("Call tdmb_early_suspend! \r\n");
    OnSleepFlag = 1;
    savArg = 0;
	return ;
}
void tdmb_late_resume(struct early_suspend *h)
{
    DPRINTK("Call tdmb_late_resume! \r\n");
    
    if(OnSleepFlag&&savArg)
    {
    int temp_ts_size = ts_size;    
        TDMB_PowerOff();
        mdelay(10);
       	TDMB_PowerOn();
   	ts_size = temp_ts_size;        
//        tdmb_setchannel();
    }
    
    OnSleepFlag = 0;
	return ;
}
#endif

int tdmb_probe(struct platform_device *pdev)
{
	int ret;
	struct device *tdmb_dev_t;

	DPRINTK("call tdmb_probe\r\n");

	ret=register_chrdev(TDMB_DEV_MAJOR, TDMB_DEV_NAME, &tdmb_ctl_fops);
	if(ret < 0)
	{
		DPRINTK("register_chrdev(TDMB_DEV) failed !\r\n");
	}
	
	tdmb_class = class_create(THIS_MODULE, TDMB_DEV_NAME);
	if(IS_ERR(tdmb_class))
	{
		unregister_chrdev(TDMB_DEV_MAJOR, TDMB_DEV_NAME);
		class_destroy(tdmb_class);
		DPRINTK("class_create failed !\r\n");
		return -EFAULT;
	}

	tdmb_major = TDMB_DEV_MAJOR;
	tdmb_dev_t = device_create(tdmb_class, NULL, MKDEV(tdmb_major,0), NULL, TDMB_DEV_NAME);
	if(IS_ERR(tdmb_dev_t))
	{
		DPRINTK("device_create failed !\r\n");
		return -EFAULT;
	}

	tdmbspi_init(); 
    tdmb_create_procfs();
    
#if TDMB_PRE_MALLOC
    tdmb_makeRingBuffer();
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = tdmb_early_suspend;
	early_suspend.resume = tdmb_late_resume;
	early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 10;
	register_early_suspend(&early_suspend);
#endif

	return 0;
}

int tdmb_remove(struct platform_device *pdev)
{
	//DPRINTK("Call tdmb_remove! \r\n");

	return 0;
}

int tdmb_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	//DPRINTK("Call tdmb_suspend! \r\n");

	return 0;
}

int tdmb_resume(struct platform_device *pdev, pm_message_t mesg)
{
	//DPRINTK("Call tdmb_resume! \r\n");

	return 0;
}

static struct platform_device *tdmbdrv_device;

static struct platform_driver tdmb_driver = {
	.probe	= tdmb_probe,
	.remove	= tdmb_remove,
	.suspend	= tdmb_suspend,
	.resume	=	tdmb_resume,
	.driver	= {
			.owner	= THIS_MODULE,
			.name	= "tdmb"
	},
};


static int __init tdmb_init(void)
{
	int ret;

	DPRINTK("<klaatu TDMB> module init\n");
    if(HWREV > 1)
    {
        gpio_request(GPIO_TDMB_BUFF_EN_N,"tdmb" );
        gpio_direction_output(GPIO_TDMB_BUFF_EN_N, GPIO_LEVEL_HIGH);
    }
	ret = platform_driver_register(&tdmb_driver);
	if(ret)
	{
		return ret;
	}

	DPRINTK("platform_driver_register! \r\n");
	tdmbdrv_device = platform_device_register_simple("tdmb",-1,NULL,0);
	if(IS_ERR(tdmbdrv_device))
	{
			DPRINTK("platform_device_register! \r\n");
			return PTR_ERR(tdmbdrv_device);
	}

	g_TDMBGlobal.b_isTDMB_Enable = 0;

	return 0;
}

static void __exit tdmb_exit(void)
{
	DPRINTK("<klaatu TDMB> module exit\n");
#if TDMB_PRE_MALLOC
	if(TS_RING != 0)
	{
		kfree(TS_RING);
		TS_RING = 0;
	}
#endif   
    tdmb_remove_procfs();
	unregister_chrdev(TDMB_DEV_MAJOR,"tdmb");
	device_destroy(tdmb_class, MKDEV(tdmb_major,0));
	class_destroy(tdmb_class);

	platform_device_unregister(tdmbdrv_device);
	platform_driver_unregister(&tdmb_driver);
}

module_init(tdmb_init);
module_exit(tdmb_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("TDMB Driver(GDM7024)");
MODULE_LICENSE("GPL v2");

