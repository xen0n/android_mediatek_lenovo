#include <linux/module.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <ddp_reg.h>
#include <ddp_drv.h>
#include <ddp_debug.h>
#include <ddp_color.h>
#include <ddp_tdshp.h>

#include "ddp_cmdq.h"
#include "ddp_cmdq_debug.h"

#include <mach/mt_irq.h>

#define CMDQ_DEBUG_WROT_PORT_CHECK (1)
#if CMDQ_DEBUG_WROT_PORT_CHECK
#include <mach/m4u.h>
#endif

static DEFINE_SPINLOCK(gCmdqTaskLock);      //ensure atomic access task info
static DEFINE_SPINLOCK(gCmdqThreadLock);    //ensure atomic access thread info
static DEFINE_SPINLOCK(gCmdqExecLock);      //ensure atomic access GCE HW 
static DEFINE_SPINLOCK(gCmdqRecordLock);    //ensure atomic access profile info
static ContextStruct           gCmdqContext;
static wait_queue_head_t       gCmdWaitQueue[CMDQ_MAX_THREAD_COUNT];
static struct list_head        gCmdqFreeTask;
static struct completion       gCmdqComplete;
static struct proc_dir_entry   *gCmdqProcEntry;

static atomic_t gCmdqDebugLevel = ATOMIC_INIT(0);
static atomic_t gCmdqDebugProgressiveTimeout = ATOMIC_INIT(0);

extern void smi_dumpDebugMsg(void);
extern void mt_irq_dump_status(int irq);

typedef struct {
    int              moduleType[cbMAX];
    CMDQ_TIMEOUT_PTR cmdqTimeout_cb[cbMAX];
    CMDQ_RESET_PTR   cmdqReset_cb[cbMAX];
} CMDQ_CONFIG_CB_ARRAY;

CMDQ_CONFIG_CB_ARRAY g_CMDQ_CB_Array = { {cbMDP, cbISP}, { NULL, NULL }, { NULL, NULL } };  //if cbMAX == 2

// Hardware Mutex Variables
#if 1
#define ENGINE_MUTEX_NUM 8
static DEFINE_SPINLOCK(gMutexLock);
static int mutex_used[ENGINE_MUTEX_NUM] = {1, 1, 1, 1, 0, 0, 0, 0};    // 0 for FB, 1 for Bitblt, 2 for HDMI, 3 for BLS
static DECLARE_WAIT_QUEUE_HEAD(gMutexWaitQueue);

typedef struct
{
    pid_t open_pid;
    pid_t open_tgid;
    unsigned int u4LockedMutex;
    spinlock_t node_lock;
} cmdq_proc_node_struct;

static int disp_lock_mutex(void)
{
    int id = -1;
    int i;
    spin_lock(&gMutexLock);

    for(i = 0 ; i < ENGINE_MUTEX_NUM ; i++)
        if(mutex_used[i] == 0)
        {
            id = i;
            mutex_used[i] = 1;
            //DISP_REG_SET_FIELD((1 << i) , DISP_REG_CONFIG_MUTEX_INTEN , 1);
            break;
        }
    spin_unlock(&gMutexLock);

    return id;
}

static int disp_unlock_mutex(int id)
{
    if(id < 0 && id >= ENGINE_MUTEX_NUM) 
        return -1;

    spin_lock(&gMutexLock);
    
    mutex_used[id] = 0;
    //DISP_REG_SET_FIELD((1 << id) , DISP_REG_CONFIG_MUTEX_INTEN , 0);
    
    spin_unlock(&gMutexLock);
    return 0;
}

static long cmdq_proc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    cmdqCommandStruct command = {0};
    DISP_EXEC_COMMAND cParams = {0};
    int mutex_id = 0;
    DISP_PQ_PARAM * pq_param = NULL;
    DISPLAY_TDSHP_T * tdshp_index = NULL;
    
    cmdq_proc_node_struct *pNode = (cmdq_proc_node_struct*)file->private_data;

    switch(cmd)
    {
        case DISP_IOCTL_EXEC_COMMAND:
            if(copy_from_user(&cParams, (void*)arg, sizeof(DISP_EXEC_COMMAND)))
            {
                CMDQ_ERR("DISP_IOCTL_EXEC_COMMAND Copy from user error\n");
                return -EFAULT;
            }

            command.scenario   = cParams.scenario; 
            command.priority   = cParams.priority; 
            command.engineFlag = cParams.engineFlag; 
            command.pVABase  = cParams.pFrameBaseSW; 
            command.blockSize  = cParams.blockSize; 

            #if CMDQ_DEBUG_WROT_PORT_CHECK
            command.debugPortData.MDP_ROTO = cParams.debugPortData.MDP_ROTO;
            command.debugPortData.MDP_ROTCO= cParams.debugPortData.MDP_ROTCO;
            command.debugPortData.MDP_ROTVO= cParams.debugPortData.MDP_ROTVO;
            #endif
            
            if (cmdqSubmitTask(&command))
            {
                CMDQ_ERR("DISP_IOCTL_EXEC_COMMAND: Execute commands failed\n");
                return -EFAULT;
            }

            break;
        case DISP_IOCTL_LOCK_MUTEX:
        {
            wait_event_interruptible_timeout(
            gMutexWaitQueue, 
            (mutex_id = disp_lock_mutex()) != -1, 
            msecs_to_jiffies(200) );             

            if((-1) != mutex_id)
            {
                spin_lock(&pNode->node_lock);
                pNode->u4LockedMutex |= (1 << mutex_id);
                spin_unlock(&pNode->node_lock);
            }
            
            if(copy_to_user((void *)arg, &mutex_id, sizeof(int)))
            {
                CMDQ_ERR("disp driver : Copy to user error (mutex)\n");
                return -EFAULT;            
            }
            break;
        }
        case DISP_IOCTL_UNLOCK_MUTEX:
            if(copy_from_user(&mutex_id, (void*)arg , sizeof(int)))
            {
                CMDQ_ERR("DISP_IOCTL_UNLOCK_MUTEX, copy_from_user failed\n");
                return -EFAULT;
            }
            disp_unlock_mutex(mutex_id);

            if((-1) != mutex_id)
            {
                spin_lock(&pNode->node_lock);
                pNode->u4LockedMutex &= ~(1 << mutex_id);
                spin_unlock(&pNode->node_lock);
            }

            wake_up_interruptible(&gMutexWaitQueue);             

            break;
            
        case DISP_IOCTL_GET_TDSHPINDEX:
            // this is duplicated to disp_unlocked_ioctl
            // be careful when modify the definition
            tdshp_index = get_TDSHP_index();
            if(copy_to_user((void *)arg, tdshp_index, sizeof(DISPLAY_TDSHP_T)))
            {
                printk("disp driver : DISP_IOCTL_GET_TDSHPINDEX Copy to user failed\n");
                return -EFAULT;            
            }
    
            break; 
        case DISP_IOCTL_GET_PQPARAM:
            // this is duplicated to disp_unlocked_ioctl
            // be careful when modify the definition
            pq_param = get_Color_config();
            if(copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
                return -EFAULT;            
            }

            break;
            
        case DISP_IOCTL_SET_PQPARAM:
            {
                int ret;
                ret = disp_color_set_pq_param((void*)arg);

                if (ret < 0)
                {
                    return ret;
                }
            }
            break;
    }

    return 0;
}

static int cmdq_proc_open(struct inode *inode, struct file *file)
{
    cmdq_proc_node_struct *pNode = NULL;

    DISP_DBG("enter cmdq_proc_open() process:%s\n", current->comm);

    //Allocate and initialize private data
    file->private_data = kmalloc(sizeof(cmdq_proc_node_struct), GFP_ATOMIC);
    if(NULL == file->private_data)
    {
        DISP_MSG("Not enough entry for DDP open operation\n");
        return -ENOMEM;
    }
   
    pNode = (cmdq_proc_node_struct *)file->private_data;
    pNode->open_pid = current->pid;
    pNode->open_tgid = current->tgid;
    pNode->u4LockedMutex = 0;
    spin_lock_init(&pNode->node_lock);
    return 0;
}

static int cmdq_proc_release(struct inode *inode, struct file *file)
{
    cmdq_proc_node_struct *pNode = NULL;
    unsigned int index = 0;
    DISP_DBG("enter cmdq_proc_release() process:%s\n",current->comm);
    
    pNode = (cmdq_proc_node_struct *)file->private_data;

    spin_lock(&pNode->node_lock);

    if(pNode->u4LockedMutex)
    {
        CMDQ_ERR("Proccess terminated[Mutex] ! :%s , mutex:%u\n" 
            , current->comm , pNode->u4LockedMutex);

        for(index = 0 ; index < ENGINE_MUTEX_NUM ; index += 1)
        {
            if((1 << index) & pNode->u4LockedMutex)
            {
                disp_unlock_mutex(index);
                DISP_DBG("unlock index = %d ,mutex_used[ %d %d %d %d ]\n",index,mutex_used[0],mutex_used[1] ,mutex_used[2],mutex_used[3]);
            }
        }
        
    } 

    spin_unlock(&pNode->node_lock);

    if(NULL != file->private_data)
    {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    
    return 0;
}

static ssize_t cmdq_proc_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
    return 0;
}

static int cmdq_proc_flush(struct file * file , fl_owner_t a_id)
{
    return 0;
}

static struct file_operations cmdq_proc_fops = {
    .owner      = THIS_MODULE,
    .open		= cmdq_proc_open,
    .read       = cmdq_proc_read,
    .flush      = cmdq_proc_flush,
	.release	= cmdq_proc_release,
    .unlocked_ioctl = cmdq_proc_unlocked_ioctl,
};

#endif //1


int32_t cmdqRegisterCallback(int32_t          index,
                             CMDQ_TIMEOUT_PTR pTimeoutFunc,
                             CMDQ_RESET_PTR   pResetFunc)
{
    if((index >= 2) || (NULL == pTimeoutFunc) || (NULL == pResetFunc))
    {
        CMDQ_LOG("Warning! [Func]%s register NULL function : %p,%p\n", __func__ , pTimeoutFunc , pResetFunc);
        return -1;
    }

    g_CMDQ_CB_Array.cmdqTimeout_cb[index] = pTimeoutFunc;
    g_CMDQ_CB_Array.cmdqReset_cb[index]   = pResetFunc;

    return 0;
}


static int32_t cmdq_read_status_proc(char    *pPage,
                                     char    **ppStart,
                                     off_t   offset,
                                     int32_t count,
                                     int32_t *pEOF,
                                     void    *pData)
{
    u_long       flags;
    char         *pBuffer;
    EngineStruct *pEngine;
    TaskStruct   *pTask;
    ThreadStruct *pThread;
    int32_t      index;
    int32_t      inner;
    int32_t      begin;
    int32_t      curPos;
    int32_t      length;
    int32_t      isFull;

    pEngine = &(gCmdqContext.engine[0]);
    pBuffer = pPage;
    begin   = 0;
    curPos  = 0;
    length  = 0;
    *pEOF   = 0x1;
    isFull  = 0;

    spin_lock_irqsave(&gCmdqThreadLock, flags); 
    smp_mb();

    length += sprintf(&pBuffer[length], "====== Clock Status =======\n");
    length += sprintf(&pBuffer[length], "MT_CG_DISP0_MM_CMDQ: %d, MT_CG_DISP0_MUTEX: %d\n",
        clock_is_on(MT_CG_DISP0_MM_CMDQ), clock_is_on(MT_CG_DISP0_MUTEX));

    length += sprintf(&pBuffer[length], "====== Engine Usage =======\n");
    length += sprintf(&pBuffer[length], "IMGI: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tIMGI].userCount, pEngine[tIMGI].currOwner, pEngine[tIMGI].failCount, pEngine[tIMGI].resetCount);
    length += sprintf(&pBuffer[length], "RDMA0: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tRDMA0].userCount, pEngine[tRDMA0].currOwner, pEngine[tRDMA0].failCount, pEngine[tRDMA0].resetCount);
    length += sprintf(&pBuffer[length], "RSZ0: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tSCL0].userCount, pEngine[tSCL0].currOwner, pEngine[tSCL0].failCount, pEngine[tSCL0].resetCount);
    length += sprintf(&pBuffer[length], "RSZ1: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tSCL1].userCount, pEngine[tSCL1].currOwner, pEngine[tSCL1].failCount, pEngine[tSCL1].resetCount);   
    length += sprintf(&pBuffer[length], "TDSHP: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tTDSHP].userCount, pEngine[tTDSHP].currOwner, pEngine[tTDSHP].failCount, pEngine[tTDSHP].resetCount);
    length += sprintf(&pBuffer[length], "tWDMA1: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tWDMA1].userCount, pEngine[tWDMA1].currOwner, pEngine[tWDMA1].failCount, pEngine[tWDMA1].resetCount);  
    length += sprintf(&pBuffer[length], "tWROT: count %d, owner %d, fail: %d, reset: %d\n", 
        pEngine[tWROT].userCount, pEngine[tWROT].currOwner, pEngine[tWROT].failCount, pEngine[tWROT].resetCount); 

    curPos = begin + length;
    if (curPos < offset)
    {
        length = 0;
        begin  = curPos;
    }

    if (curPos > (offset + count))
    {
        *pEOF  = 0x0;
        isFull = 1;
    }

    for (index = 0; ((index < CMDQ_MAX_FIXED_TASK) && (0 == isFull)); index++)
    {
        pTask = &(gCmdqContext.taskInfo[index]);
        
        length += sprintf(&pBuffer[length], "====== Task %d Usage =======\n", index);

        length += sprintf(&pBuffer[length], "State %d, VABase: 0x%08x, MVABase: 0x%08x, Size: %d\n",
            pTask->taskState, (uint32_t)pTask->pVABase, pTask->MVABase, pTask->blockSize);
        length += sprintf(&pBuffer[length], "Scenario %d, Priority: %d, Flag: 0x%08x, VAEnd: 0x%08x\n",
            pTask->scenario, pTask->priority, pTask->engineFlag, (uint32_t)pTask->pCMDEnd);
        length += sprintf(&pBuffer[length], "Reorder: %d, Trigger %d:%d, IRQ: %d:%d, Wake Up: %d:%d\n",
            pTask->reorder,
            (uint32_t)pTask->trigger.tv_sec, (uint32_t)pTask->trigger.tv_usec,
            (uint32_t)pTask->gotIRQ.tv_sec, (uint32_t)pTask->gotIRQ.tv_usec, 
            (uint32_t)pTask->wakedUp.tv_sec, (uint32_t)pTask->wakedUp.tv_usec);

        curPos = begin + length;
        if (curPos < offset)
        {
            length = 0;
            begin  = curPos;
        }

        if (curPos > (offset + count))
        {
            *pEOF  = 0x0;
            isFull = 1;
            break;
        }
    }

    for (index = 0; ((index < CMDQ_MAX_THREAD_COUNT) && (0 == isFull)); index++)
    {
        pThread = &(gCmdqContext.thread[index]);
        
        if (pThread->taskCount > 0)
        {
            length += sprintf(&pBuffer[length], "====== Thread %d Usage =======\n", index);
            length += sprintf(&pBuffer[length], "taskCount: %d, Wait Cookie %d, Next Cookie %d\n", 
                pThread->taskCount, pThread->waitCookie, pThread->nextCookie);

            curPos = begin + length;
            if (curPos < offset)
            {
                length = 0;
                begin  = curPos;
            }

            if (curPos > (offset + count))
            {
                *pEOF  = 0x0;
                isFull = 1;
            }

            for (inner = 0; ((inner < CMDQ_MAX_TASK_COUNT) && (0 == isFull)); inner++)
            {
                pTask = pThread->pCurTask[inner];
                if (NULL != pTask)
                {
                    length += sprintf(&pBuffer[length], "Slot: %d, Task: 0x%08x, MVABase: 0x%08x, Size: %d, Last Command 0x%08x, 0x%08x\n", 
                        inner, (uint32_t)pTask, pTask->MVABase, pTask->blockSize, pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
                    
                    curPos = begin + length;
                    if (curPos < offset)
                    {
                        length = 0;
                        begin  = curPos;
                    }

                    if (curPos > (offset + count))
                    {
                        *pEOF  = 0x0;
                        isFull = 1;
                        break;
                    }
                }
            }
        }
    }
    spin_unlock_irqrestore(&gCmdqThreadLock, flags); 

    *ppStart  = pPage + (offset - begin);
    length   -= (offset - begin);

    return (length > count)? count: length;
}


static int32_t cmdq_read_error_proc(char    *pPage,
                                    char    **ppStart,
                                    off_t   offset,
                                    int32_t count,
                                    int32_t *pEOF,
                                    void    *pData)
{
    u_long      flags;
    int32_t     begin;
    int32_t     curPos;
    int32_t     length;
    int32_t     index;
#if 0
    int32_t     inner;
#endif // 0
    ErrorStruct *pError;
    TaskStruct  *pTask;

#if 0
    uint32_t    *pCMD;
    int32_t     size;
    uint32_t    MVABase;
    uint32_t    mask;
    uint32_t    value;
    uint32_t    addr;
#endif // 0

    uint32_t    isFull;

    begin   = 0;
    curPos  = 0;
    length  = 0;
    *pEOF   = 0x1;
    isFull  = 0;

    *ppStart  = pPage;
    length    = 0;
    return 0;

    spin_lock_irqsave(&gCmdqExecLock, flags);
    smp_mb();

    length += sprintf(&pPage[length], "====== Error Task count: %d =======\n", gCmdqContext.errNum);
    pError = &(gCmdqContext.error[0]);

    count = gCmdqContext.errNum;
    for (index = 0; ((index < count) && (0 == isFull)); index++)
    {
        pTask = pError[index].pTask;

        if(pTask)
        {
            length += sprintf(&pPage[length], "====== Error Task 0x%08x =======\n", (uint32_t)pTask);
            length += sprintf(&pPage[length], "State %d, VABase: 0x%08x, MVABase: 0x%08x, Size: %d\n",
                pTask->taskState, (uint32_t)pTask->pVABase, pTask->MVABase, pTask->blockSize);
            length += sprintf(&pPage[length], "Scenario %d, Priority: %d, Flag: 0x%08x, VAEnd: 0x%08x\n",
                pTask->scenario, pTask->priority, pTask->engineFlag, (uint32_t)pTask->pCMDEnd);
            length += sprintf(&pPage[length], "Reorder: %d, Trigger %d:%d, IRQ: %d:%d, Wake Up: %d:%d\n",
                pTask->reorder,
                (uint32_t)pTask->trigger.tv_sec, (uint32_t)pTask->trigger.tv_usec,
                (uint32_t)pTask->gotIRQ.tv_sec, (uint32_t)pTask->gotIRQ.tv_usec, 
                (uint32_t)pTask->wakedUp.tv_sec, (uint32_t)pTask->wakedUp.tv_usec);
            length += sprintf(&pPage[length], "================================\n");
        }

        curPos = begin + length;
        if (curPos < offset)
        {
            length = 0;
            begin  = curPos;
        }

        if (curPos > (offset + count))
        {
            *pEOF  = 0x0;
            isFull = 1;
            continue;
        }

    #if 0
        if (pTask->engineFlag & (0x01 << tRDMA0))
        {
            for (inner = 0; inner < 1024; inner++)
            {
                length += sprintf(&pPage[length], "0x%08x: 0x%08x\n", (0xF4001000 + (inner << 2)), pError->regVal[tRDMA0][inner]);
        
                curPos = begin + length;
                if (curPos < offset)
                {
                    length = 0;
                    begin  = curPos;
                }

                if (curPos > (offset + count))
                {
                    *pEOF  = 0x0;
                    isFull = 1;
                    continue;
                }
            }
        }

        if (pTask->engineFlag & (0x01 << tWROT))
        {
            for (inner = 0; inner < 1024; inner++)
            {
                length += sprintf(&pPage[length], "0x%08x: 0x%08x\n", (0xF4005000 + (inner << 2)), pError->regVal[tWROT][inner]);
        
                curPos = begin + length;
                if (curPos < offset)
                {
                    length = 0;
                    begin  = curPos;
                }

                if (curPos > (offset + count))
                {
                    *pEOF  = 0x0;
                    isFull = 1;
                    continue;
                }
            }
        }

        if (pTask->engineFlag & (0x01 << tWDMA1))
        {
            for (inner = 0; inner < 1024; inner++)
            {
                length += sprintf(&pPage[length], "0x%08x: 0x%08x\n", (0xF4004000 + (inner << 2)), pError->regVal[tWDMA1][inner]);
        
                curPos = begin + length;
                if (curPos < offset)
                {
                    length = 0;
                    begin  = curPos;
                }

                if (curPos > (offset + count))
                {
                    *pEOF  = 0x0;
                    isFull = 1;
                    continue;
                }
            }
        }
    #endif // 0

    #if 0 // too huge
        pCMD    = pTask->pVABase;
        size    = pTask->blockSize;
        MVABase = pTask->MVABase;
        mask    = 0xFFFFFFFF;
        for (pCMD  = pTask->pVABase; size > 0; pCMD += 2, size -= 8, MVABase += 8)
        {
            switch(pCMD[1] >> 24)
            {
                case CMDQ_CODE_MOVE:
                    mask = ~pCMD[0];
                    length += sprintf(&pPage[length], "MVA: 0x%08x, MOVE 0x%08x\n", MVABase, pCMD[0]);
                    break;
                case CMDQ_CODE_WRITE:
                    value = (pCMD[1] >> 22) & 0x03;
                    addr  = (pCMD[1] & 0x3fffff);

                    if (0 != mask)
                    {
                        value &= mask;
                        addr  &= ~1;
                    }
                    else
                    {
                        mask = 0xFFFFFFFF;
                    }

                    if (0x00 == value)
                    {
                        addr |= (0x14 << 24);
                    }
                    else if (0x01 == value)
                    {
                        addr |= (0x15 << 24);
                    }

                    length += sprintf(&pPage[length], "MVA: 0x%08x, WRITE Addr: 0x%08x, Value: 0x%08x\n", MVABase, addr, pCMD[0]);
                    break;
                case CMDQ_CODE_POLL:
                    value = (pCMD[1] >> 22) & 0x03;
                    addr  = (pCMD[1] & 0x3fffff);
                    
                    if (0 != mask)
                    {
                        value &= mask;
                        addr  &= ~1;
                    }
                    else
                    {
                        mask = 0xFFFFFFFF;
                    }
                    
                    if (0x00 == value)
                    {
                        addr |= (0x14 << 24);
                    }
                    else if (0x01 == value)
                    {
                        addr |= (0x15 << 24);
                    }

                    length += sprintf(&pPage[length], "MVA: 0x%08x, POLL Addr: 0x%08x, Value: 0x%08x\n", MVABase, addr, pCMD[0]);
                    break;
                case CMDQ_CODE_JUMP:
                    length += sprintf(&pPage[length], "MVA: 0x%08x, JUMP 0x%08x\n", MVABase, pCMD[0]);
                    break;
                case CMDQ_CODE_WFE:
                    value = pCMD[1] & 0x00FFFFFF;
                    length += sprintf(&pPage[length], "MVA: 0x%08x, WFE 0x%08x\n", MVABase, value);
                    break;
                case CMDQ_CODE_EOC:
                    length += sprintf(&pPage[length], "MVA: 0x%08x, EOC 0x%08x\n", MVABase, pCMD[0]);
                    break;
                default:
                    length += sprintf(&pPage[length], "MVA: 0x%08x, 0x%08x:0x%08x\n", MVABase, pCMD[1], pCMD[0]);
                    break;
            }

            curPos = begin + length;
            if (curPos < offset)
            {
                length = 0;
                begin  = curPos;
            }

            if (curPos > (offset + count))
            {
                *pEOF  = 0x0;
                isFull = 1;
                break;
            }
        }
    #endif // 0
    }
    spin_unlock_irqrestore(&gCmdqExecLock, flags); 

    *ppStart  = pPage + (offset - begin);
    length   -= (offset - begin);

    return (length > count)? count: length;
}


static int32_t cmdq_read_record_proc(char    *pPage,
                                     char    **ppStart,
                                     off_t   offset,
                                     int32_t count,
                                     int32_t *pEOF,
                                     void    *pData)
{
    u_long       flags;
    int32_t      begin;
    int32_t      curPos;
    int32_t      length;
    int32_t      index;
    int32_t      numRec;
    RecordStruct *pRecord;
    int32_t      IRQTime;
    int32_t      execTime;
    int32_t      totalTime;

    begin   = 0;
    curPos  = 0;
    length  = 0;
    *pEOF   = 0x1;

    spin_lock_irqsave(&gCmdqRecordLock, flags);
    smp_mb();

    numRec  = gCmdqContext.recNum;
    if (numRec >= CMDQ_MAX_RECORD_COUNT)
    {
        index = gCmdqContext.lastID;
    }
    else
    {
        index = gCmdqContext.lastID - numRec;
        if (index < 0)
        {
            index = CMDQ_MAX_RECORD_COUNT - index;
        }
    }

    for (;numRec > 0; numRec--, index++)
    {
        if (index >= CMDQ_MAX_RECORD_COUNT)
        {
            index = 0;
        }
        
        pRecord = &(gCmdqContext.record[index]);

        CMDQ_GET_TIME_DURATION(pRecord->trigger, pRecord->gotIRQ, IRQTime)
        CMDQ_GET_TIME_DURATION(pRecord->trigger, pRecord->wakedUp, execTime);
        CMDQ_GET_TIME_DURATION(pRecord->start,   pRecord->done, totalTime);

        length += sprintf(&pPage[length], "Idx: %3d, SCE: %d, PRI: %2d, REO: %d, SEC: %d, trigger[%d:%06d], Time[IRQ, Exec, Total]: ms[%3d, %3d, %3d]\n",
            index, pRecord->scenario, pRecord->priority, pRecord->reorder, false, 
            (uint32_t)pRecord->trigger.tv_sec, (uint32_t)pRecord->trigger.tv_usec, 
            IRQTime, execTime, totalTime);

        curPos = begin + length;
        if (curPos < offset)
        {
            length = 0;
            begin  = curPos;
        }

        if (curPos > (offset + count))
        {
            *pEOF = 0x0;
            break;
        }
    }

    spin_unlock_irqrestore(&gCmdqRecordLock, flags); 

    *ppStart  = pPage + (offset - begin);
    length   -= (offset - begin);

    return (length > count)? count: length;
}

static int32_t cmdq_read_log_level_proc(char    *pPage,
                                         char    **ppStart,
                                         off_t   offset,
                                         int32_t count,
                                         int32_t *pEOF,
                                         void    *pData)
{
    u_long       flags = 0;
    int32_t      length = 0;
    int32_t      logLevelSafe = 0;
    
    logLevelSafe = gCmdqContext.logLevel;
    if (logLevelSafe < 0 || logLevelSafe > 3)
    {
        logLevelSafe = 0;
    }
    
    spin_lock_irqsave(&gCmdqExecLock, flags);

    length = sprintf(&pPage[offset], "%d\n", logLevelSafe);
    *pEOF = 1;
    
    spin_unlock_irqrestore(&gCmdqExecLock, flags); 
    
    return length;
}

static int cmdq_write_log_level_proc(struct file* file, 
                                     const char *buf, 
                                     unsigned long count, 
                                     void *data)
{
    u_long       flags = 0;
    int len = 0;
    int32_t value = 0;
    char textBuf[10] = {0};
    if (count >= 10)
    {
        return -EFAULT;
    }

    len = count;
    if (copy_from_user(textBuf, buf, len))
    {
        return -EFAULT;
    }

    textBuf[len] = '\0';

    sscanf(textBuf, "%d", &value);

    if (value < 0 || value > 3)
    {
        value = 0;
    }

    spin_lock_irqsave(&gCmdqExecLock, flags);
    gCmdqContext.logLevel = value;
    spin_unlock_irqrestore(&gCmdqExecLock, flags);
    
    return len;
}

static int32_t cmdq_read_debug_level_proc(char    *pPage,
                                         char    **ppStart,
                                         off_t   offset,
                                         int32_t count,
                                         int32_t *pEOF,
                                         void    *pData)
{
    int32_t      length = 0;

    length = sprintf(&pPage[offset], "debug level: %d\n", atomic_read(&gCmdqDebugLevel));
    *pEOF = 1;
        
    return length;
}

static int cmdq_write_debug_level_proc(struct file* file, 
                                     const char *buf, 
                                     unsigned long count, 
                                     void *data)
{
    int len = 0;
    int32_t value = 0;
    char textBuf[10] = {0};
    if (count >= 10)
    {
        return -EFAULT;
    }

    len = count;
    if (copy_from_user(textBuf, buf, len))
    {
        return -EFAULT;
    }

    textBuf[len] = '\0';


    sscanf(textBuf, "%d", &value);
    if (value < 0 || value > 3)
    {
        value = 0;
    }

    atomic_set(&gCmdqDebugLevel, value); 
    
    return len;
}

static int cmdq_write_sw_timeout_proc(struct file* file, 
                                     const char *buf, 
                                     unsigned long count, 
                                     void *data)
{
    int32_t      len = 0;
    uint32_t     swTimeout = 0;
    uint32_t     predumpStartTime = 0;
    uint32_t     predumpDuration = 0;
    char textBuf[30] = {0};

    if (count >= 30)
    {
        return -EFAULT;
    }

    len = count;
    if (copy_from_user(textBuf, buf, len))
    {
        return -EFAULT;
    }

    textBuf[len] = '\0';

    sscanf(textBuf, "%d %d %d", &swTimeout, &predumpStartTime, &predumpDuration);
    cmdq_debug_set_sw_timeout(swTimeout, predumpStartTime, predumpDuration); 

    return len;
}

static int32_t cmdq_read_sw_timeout_proc(char    *pPage,
                                         char    **ppStart,
                                         off_t   offset,
                                         int32_t count,
                                         int32_t *pEOF,
                                         void    *pData)
{
    u_long       flags = 0;
    int32_t      length = 0;
    uint32_t     swTimeout = 0;
    uint32_t     predumpStartTime = 0;
    uint32_t     predumpDuration = 0;
    uint32_t     predumpMaxCount = 0;
    
    spin_lock_irqsave(&gCmdqExecLock, flags);
    swTimeout        = gCmdqContext.swTimeoutDurationMS;
    predumpStartTime = gCmdqContext.predumpStartTimeMS;
    predumpDuration  = gCmdqContext.predumpDurationMS;
    predumpMaxCount  = gCmdqContext.predumpMaxRetryCount;
    spin_unlock_irqrestore(&gCmdqExecLock, flags); 

    length = sprintf(&pPage[offset], 
                "sw timeout: %d ms = start[%d] + duration[%d] * max_predump_count[%d]\n", 
                swTimeout, predumpStartTime, predumpDuration, predumpMaxCount);
    *pEOF = 1;
    
    return length;
}


void cmdq_init_procfs(void)
{
    struct proc_dir_entry *pEntry;

    // Mout proc entry for debug
    gCmdqProcEntry = proc_mkdir("cmdq", NULL);
    if (NULL != gCmdqProcEntry)
    {
        pEntry = create_proc_entry("status", 0660,gCmdqProcEntry);    
        if (NULL != pEntry)
        {
            pEntry->read_proc = cmdq_read_status_proc;
        }

        pEntry = create_proc_entry("error", 0660, gCmdqProcEntry);    
        if (NULL != pEntry)
        {
            pEntry->read_proc = cmdq_read_error_proc;
        }

        pEntry = create_proc_entry("record", 0660, gCmdqProcEntry);    
        if (NULL != pEntry)
        {
            pEntry->read_proc = cmdq_read_record_proc;
        }

        pEntry = create_proc_entry("log_level", 0660, gCmdqProcEntry);    
        if (NULL != pEntry)
        {
            pEntry->read_proc = cmdq_read_log_level_proc;
            pEntry->write_proc = cmdq_write_log_level_proc; 
        }
        
        pEntry = create_proc_entry("debug_level", 0660, gCmdqProcEntry);    
        if (NULL != pEntry)
        {
            pEntry->read_proc = cmdq_read_debug_level_proc;
            pEntry->write_proc = cmdq_write_debug_level_proc; 
        }

        pEntry = create_proc_entry("sw_timeout", 0660, gCmdqProcEntry);    
        if (NULL != pEntry)
        {
            pEntry->read_proc = cmdq_read_sw_timeout_proc;
            pEntry->write_proc = cmdq_write_sw_timeout_proc; 
        }        
    }
}

void cmdq_deinit_procfs(void)
{
    if (NULL != gCmdqProcEntry)
    {
        remove_proc_entry("record",  gCmdqProcEntry);
        remove_proc_entry("error", gCmdqProcEntry);
        remove_proc_entry("status", gCmdqProcEntry);
        remove_proc_entry("log_level", gCmdqProcEntry);        
        remove_proc_entry("debug_level", gCmdqProcEntry);        
        remove_proc_entry("sw_timeout", gCmdqProcEntry);
        remove_proc_entry("cmdq", NULL);
        gCmdqProcEntry = NULL;
    }
}

void cmdqInitialize()
{
    //u_long                flags;
    uint8_t               *pVABase;
    uint32_t              MVABase;
    EngineStruct          *pEngine;
    TaskStruct            *pTask;
    int32_t               index;
    struct proc_dir_entry *pEntry; 

    pVABase = dma_alloc_coherent(NULL, CMDQ_MAX_DMA_BUF_SIZE, &MVABase, GFP_ATOMIC);
    if(NULL == pVABase)
    {
        CMDQ_AEE("CMDQ", "Allocate command buffer failed\n");
        //spin_unlock_irqrestore(&gCmdqTaskLock, flags); 
        return;
    }

    memset(pVABase, 0, CMDQ_MAX_DMA_BUF_SIZE);
    CMDQ_MSG("Command buffer VA:%x PA:%x \n", (uint32_t)pVABase, MVABase);

    for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
    {
        init_waitqueue_head(&gCmdWaitQueue[index]);
    }

    init_completion(&gCmdqComplete);

    // Reset overall context
    memset(&gCmdqContext, 0x0, sizeof(ContextStruct));

    // Reset engine status
    pEngine = &(gCmdqContext.engine[0]);
    for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
    {
        pEngine[index].currOwner = CMDQ_INVALID_THREAD;
    }

    // Reset task status
    INIT_LIST_HEAD(&gCmdqFreeTask);
    pTask = &(gCmdqContext.taskInfo[0]);
    for (index = 0; index < CMDQ_MAX_FIXED_TASK; index++)
    {
        INIT_LIST_HEAD(&(pTask[index].listEntry));

        pTask[index].thread  = CMDQ_INVALID_THREAD; 
        pTask[index].taskType  = TASK_TYPE_FIXED;
        pTask[index].taskState = TASK_STATE_IDLE;
        pTask[index].pVABase   = (uint32_t*)pVABase;
        pTask[index].MVABase   = MVABase;
        pTask[index].bufSize   = CMDQ_MAX_BLOCK_SIZE;
        list_add_tail(&(pTask[index].listEntry), &gCmdqFreeTask);

        pVABase += CMDQ_MAX_BLOCK_SIZE;
        MVABase += CMDQ_MAX_BLOCK_SIZE;
    }

    // init predump default value
    gCmdqContext.swTimeoutDurationMS = CMDQ_DEFAULT_TIMEOUT_MS;
    gCmdqContext.predumpStartTimeMS  = CMDQ_DEFAULT_PREDUMP_START_TIME_MS;
    gCmdqContext.predumpDurationMS   = CMDQ_DEFAULT_PREDUMP_TIMEOUT_MS; 
    gCmdqContext.predumpMaxRetryCount= CMDQ_DEFAULT_PREDUMP_RETRY_COUNT;   

    // Clear CMDQ event for engines
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x14);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x15);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x16);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x17);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x18);

    // Mount proc entry for non-specific group ioctls
    pEntry = create_proc_entry("mtk_mdp_cmdq", 0444, NULL);
    if (NULL != pEntry)
    {
        pEntry->proc_fops = &cmdq_proc_fops;
    }

    // proc init for cmdq debug/internel usage
    cmdq_init_procfs(); 

    // performance statistics init, 
    // note that we don't need to uninit it.
    CMDQ_PROF_INIT();
}


int32_t cmdqSuspendTask()
{
    int32_t      status = 0;
    u_long       flags;
    EngineStruct *pEngine;

    pEngine = &(gCmdqContext.engine[0]);

    spin_lock_irqsave(&gCmdqThreadLock, flags); 
    smp_mb();

    if((0 != pEngine[tIMGI].userCount) ||
       (0 != pEngine[tRDMA0].userCount) ||
       (0 != pEngine[tSCL0].userCount) ||
       (0 != pEngine[tSCL1].userCount) ||
       (0 != pEngine[tTDSHP].userCount) ||
       (0 != pEngine[tWROT].userCount) ||
       (0 != pEngine[tWDMA1].userCount))
    {
        status =  -EFAULT;
    }

    spin_unlock_irqrestore(&gCmdqThreadLock, flags); 

    return status;
}


int32_t cmdqResumeTask()
{
    u_long flags;

    spin_lock_irqsave(&gCmdqThreadLock, flags); 

    // note the tokens' value become random number after diable and enable clock
    // so we should not expect HW register value is consistent after clock off
    // 
    // clrear CMDQ HW token
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x14);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x15);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x16);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x17);
    DISP_REG_SET(DISP_REG_CMDQ_SYNC_TOKEN_UPDATE, 0x18);

    spin_unlock_irqrestore(&gCmdqThreadLock, flags); 

    return 0;
}


static void cmdq_dump_task_usage(void)
{
    int32_t    index;
    TaskStruct *pTask;

    for (index = 0; index < CMDQ_MAX_FIXED_TASK; index++)
    {
        pTask = &(gCmdqContext.taskInfo[index]);
        if (TASK_STATE_IDLE != pTask->taskState)
        {
            CMDQ_ERR("====== Task %d Usage =======\n", index);

            CMDQ_ERR("State %d, VABase: 0x%08x, MVABase: 0x%08x, Size: %d\n",
                pTask->taskState, (uint32_t)pTask->pVABase, pTask->MVABase, pTask->blockSize);
            CMDQ_ERR("Scenario %d, Priority: %d, Flag: 0x%08x, VAEnd: 0x%08x\n",
                pTask->scenario, pTask->priority, pTask->engineFlag, (uint32_t)pTask->pCMDEnd);
            CMDQ_ERR("Reorder: %d, Trigger %d:%d, IRQ: %d:%d, Wake Up: %d:%d\n",
                pTask->reorder,
                (uint32_t)pTask->trigger.tv_sec, (uint32_t)pTask->trigger.tv_usec,
                (uint32_t)pTask->gotIRQ.tv_sec, (uint32_t)pTask->gotIRQ.tv_usec, 
                (uint32_t)pTask->wakedUp.tv_sec, (uint32_t)pTask->wakedUp.tv_usec);
        }
    }
}

void cmdq_release_task(TaskStruct *pTask)
{
    u_long flags;

    if(NULL == pTask)
    {
        CMDQ_ERR("release NULL task\n");
        return; 
    }

    CMDQ_PROF_START("CMDQ_TASK_REL");
    CMDQ_MSG("Release task structure begin, pTask[0x%p], state[%d]\n", pTask, pTask->taskState);
    
    {
        if (TASK_TYPE_FIXED == pTask->taskType)
        {
            spin_lock_irqsave(&gCmdqTaskLock, flags);            
            pTask->taskState = TASK_STATE_IDLE;
            list_add_tail(&(pTask->listEntry), &gCmdqFreeTask);
            spin_unlock_irqrestore(&gCmdqTaskLock, flags);
        }
        else
        {
            dma_free_coherent(NULL, pTask->bufSize, pTask->pVABase, pTask->MVABase);
            kfree(pTask);
        }
    }

    CMDQ_MSG("Release task structure end\n");
    CMDQ_PROF_END("CMDQ_TASK_REL");
}


static TaskStruct* cmdq_acquire_task(cmdqCommandStruct *pCommandDesc)
{
    const bool copyCmdFromUserSpace = (CMDQ_SCENARIO_DEBUG == pCommandDesc->scenario) ? (false): (true);
    u_long     flags;
    TaskStruct *pTask;
    uint32_t   *pVABase;
    uint32_t   MVABase;

    CMDQ_MSG("-->TASK: ENG: 0x%08x, CMD: 0x%08x, size: %d\n", pCommandDesc->engineFlag, (uint32_t)(pCommandDesc->pVABase), (pCommandDesc->blockSize));

    pTask = NULL;

    spin_lock_irqsave(&gCmdqTaskLock, flags); 
    smp_mb();

    if (list_empty(&gCmdqFreeTask) ||
        (pCommandDesc->blockSize >= CMDQ_MAX_BLOCK_SIZE))
    {
        spin_unlock_irqrestore(&gCmdqTaskLock, flags); 

        pTask = (TaskStruct*)kmalloc(sizeof(TaskStruct), GFP_KERNEL);
        if (NULL != pTask)
        {
            pVABase = dma_alloc_coherent(NULL, pCommandDesc->blockSize + CMDQ_EXTRA_MARGIN, &MVABase, GFP_KERNEL);
            if (NULL == pVABase)
            {
                kfree(pTask);
                pTask = NULL;
                CMDQ_ERR("Can't allocate DMA buffer\n");
            }
            else
            {
                pTask->pVABase   = pVABase;
                pTask->MVABase   = MVABase;
                pTask->bufSize   = pCommandDesc->blockSize + CMDQ_EXTRA_MARGIN;
                pTask->taskType  = TASK_TYPE_DYNAMIC;
            }
        }
    }
    else
    {
        pTask = list_first_entry(&(gCmdqFreeTask), TaskStruct, listEntry);
        list_del(&(pTask->listEntry));
        spin_unlock_irqrestore(&gCmdqTaskLock, flags);
    }
    
    if (NULL != pTask)
    {
        pTask->scenario   = pCommandDesc->scenario;
        pTask->priority   = pCommandDesc->priority;
        pTask->engineFlag = pCommandDesc->engineFlag;
        pTask->pCMDEnd    = pTask->pVABase + ((pCommandDesc->blockSize) >> 2) - 1;
        pTask->blockSize  = pCommandDesc->blockSize;
        pTask->firstEOC   = pTask->MVABase + (pCommandDesc->blockSize - 16);
        pTask->taskState  = TASK_STATE_WAITING ; //TASK_STATE_BUSY;
        pTask->reorder    = 0;
        pTask->thread     = CMDQ_INVALID_THREAD;
        pTask->irqFlag    = 0x0; 

        memset(&(pTask->trigger), 0x0, sizeof(struct timeval));
        memset(&(pTask->gotIRQ), 0x0, sizeof(struct timeval));
        memset(&(pTask->wakedUp), 0x0, sizeof(struct timeval));

        #if CMDQ_DEBUG_WROT_PORT_CHECK
        pTask->debugPortData.MDP_ROTO = pCommandDesc->debugPortData.MDP_ROTO; 
        pTask->debugPortData.MDP_ROTCO = pCommandDesc->debugPortData.MDP_ROTCO; 
        pTask->debugPortData.MDP_ROTVO = pCommandDesc->debugPortData.MDP_ROTVO;
        #endif

        CMDQ_MSG("Copy command buffer begin, copyCmdFromUserSpace[%d]\n", copyCmdFromUserSpace);

        if(false == copyCmdFromUserSpace)
        {
            memcpy(pTask->pVABase, pCommandDesc->pVABase, pCommandDesc->blockSize); 
        }
        else if (copy_from_user(pTask->pVABase, pCommandDesc->pVABase, pCommandDesc->blockSize))
        {
            if (TASK_TYPE_DYNAMIC != pTask->taskType)
            {
                spin_lock_irqsave(&gCmdqTaskLock, flags); 
                list_add_tail(&(pTask->listEntry), &gCmdqFreeTask);
                spin_unlock_irqrestore(&gCmdqTaskLock, flags); 
            }

            CMDQ_AEE("CMDQ", "Copy commands buffer failed\n");
            CMDQ_ERR("Source: 0x%08x, target: 0x%08x, size: %d\n", (uint32_t)pCommandDesc->pVABase, (uint32_t)pTask->pVABase, pCommandDesc->blockSize);

            return NULL;
        }

        CMDQ_MSG("Copy command buffer end\n");
    }
    else
    {
        CMDQ_AEE("CMDQ", "Can't acquire task info\n");
        cmdq_dump_task_usage();
    }

    CMDQ_MSG("Allocate task structure end\n");

    CMDQ_PROF_END("CMDQ_TASK_ACQ");
    return pTask;
}


void cmdq_enable_clock(uint32_t engineFlag, int32_t  thread)
{
    EngineStruct *pEngine;

    CMDQ_MSG("-->CLOCK: Enable hardware clock 0x%08x begin\n", engineFlag);

    pEngine = &(gCmdqContext.engine[0]);

    if((0 == pEngine[tIMGI].userCount) &&
       (0 == pEngine[tRDMA0].userCount) &&
       (0 == pEngine[tSCL0].userCount) &&
       (0 == pEngine[tSCL1].userCount) &&
       (0 == pEngine[tTDSHP].userCount) &&
       (0 == pEngine[tWROT].userCount) &&
       (0 == pEngine[tWDMA1].userCount))
    {
        enable_clock(MT_CG_DISP0_SMI_COMMON, "SMI_COMMON");
        enable_clock(MT_CG_DISP0_SMI_LARB0, "SMI_LARB0");
        enable_clock(MT_CG_DISP0_MM_CMDQ, "MM_CMDQ");
        enable_clock(MT_CG_DISP0_MUTEX, "MUTEX");
    }

    if (engineFlag & (0x1 << tIMGI))
    {
        if(!clock_is_on(MT_CG_DISP0_CAM_MDP))
        {
            enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
            enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
            enable_clock(MT_CG_IMAGE_SEN_TG,  "CAMERA");
            enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
            enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");

            enable_clock(MT_CG_DISP0_CAM_MDP, "CAM_MDP"); 
        
            pEngine[tIMGI].currOwner = thread;
        }

        pEngine[tIMGI].userCount++;
    }
    
    if (engineFlag & (0x1 << tRDMA0))
    {
        if(!clock_is_on(MT_CG_DISP0_MDP_RDMA))
        {
            enable_clock(MT_CG_DISP0_MDP_RDMA, "MDP_RDMA");
        
            pEngine[tRDMA0].currOwner = thread;
        }

        pEngine[tRDMA0].userCount++;
    }

    if (engineFlag & (0x1 << tSCL0))
    {
        if(!clock_is_on(MT_CG_DISP0_MDP_RSZ0))
        {
            enable_clock(MT_CG_DISP0_MDP_RSZ0, "MDP_RSZ0");
        
            pEngine[tSCL0].currOwner = thread;
        }

        pEngine[tSCL0].userCount++;
    }

    if (engineFlag & (0x1 << tSCL1))
    {
        if(!clock_is_on(MT_CG_DISP0_MDP_RSZ1))
        {
            enable_clock(MT_CG_DISP0_MDP_RSZ1, "MDP_RSZ1");
        
            pEngine[tSCL1].currOwner = thread;
        }

        pEngine[tSCL1].userCount++;
    }

    if (engineFlag & (0x1 << tTDSHP))
    {
        if(!clock_is_on(MT_CG_DISP0_MDP_TDSHP))
        {
            enable_clock(MT_CG_DISP0_MDP_TDSHP, "MDP_TDSHP");

            pEngine[tTDSHP].currOwner = thread;
        }

        pEngine[tTDSHP].userCount++;
    }

    if (engineFlag & (0x1 << tWROT))
    {
        if(!clock_is_on(MT_CG_DISP0_MDP_WROT))
        {
            enable_clock(MT_CG_DISP0_MDP_WROT, "MDP_WROT");

            pEngine[tWROT].currOwner = thread;
        }

        pEngine[tWROT].userCount++;
    }

    if (engineFlag & (0x1 << tWDMA1))
    {
        if(!clock_is_on(MT_CG_DISP0_MDP_WDMA))
        {
            enable_clock(MT_CG_DISP0_MDP_WDMA, "MDP_WDMA");
        
            pEngine[tWDMA1].currOwner = thread;
        }

        pEngine[tWDMA1].userCount++;
    }

    CMDQ_MSG("<--CLOCK: Enable hardware clock 0x%08x end\n", engineFlag);
}


static int32_t cmdq_acquire_thread(uint32_t engineFlag)
{
    EngineStruct *pEngine;
    ThreadStruct *pThread;
    u_long       flags;
    uint32_t     engine;
    uint32_t     free;
    int32_t      index;
    int32_t      thread;

    pEngine = &(gCmdqContext.engine[0]);
    pThread = &(gCmdqContext.thread[0]);
    do
    {
        spin_lock_irqsave(&gCmdqThreadLock, flags); 
        smp_mb();

        // Default values
        engine = engineFlag;
        free   = 0xFFFFFFFF;
        thread = CMDQ_INVALID_THREAD;

        for (index = 0; ((index < tTotal) && (engine != 0)); index++)
        {
            if (engine & (0x1 << index))
            {               
                if (CMDQ_INVALID_THREAD == pEngine[index].currOwner)
                {
                    continue;
                }
                else if (CMDQ_INVALID_THREAD == thread)
                {
                    thread = pEngine[index].currOwner;
                    free   &= ~(0x1 << thread);
                }
                else if (thread != pEngine[index].currOwner)
                {
                    thread = CMDQ_INVALID_THREAD;
                    spin_unlock_irqrestore(&gCmdqThreadLock, flags);
                                       
                    wait_for_completion(&gCmdqComplete);
                    break;
                }
                engine &= ~(0x1 << index);
            }
        }
        
        if ((0xFFFFFFFF == free) &&
            (CMDQ_INVALID_THREAD == thread))
        {
            const int startIndex = 0; 
            const int endIndex   = CMDQ_MAX_THREAD_COUNT;
            for (index = startIndex; index < endIndex; ++index)
            {
                if (0 == pThread[index].taskCount)
                {
                    thread = index;
                    break;
                }
                else
                {
                    CMDQ_LOG("THREAD: thread %d, taskCount:%d\n", index, pThread[index].taskCount);
                }
            }

            // no thread available now, wait for it
            if (CMDQ_INVALID_THREAD == thread)
            {
                spin_unlock_irqrestore(&gCmdqThreadLock, flags);                          
                wait_for_completion(&gCmdqComplete);
                break;
            }
        }

    } while(index == CMDQ_INVALID_THREAD);

    cmdq_enable_clock(engineFlag, thread);

    spin_unlock_irqrestore(&gCmdqThreadLock, flags);

    return thread;
}


int32_t cmdq_disable_clock(uint32_t engineFlag)
{
    EngineStruct *pEngine;
    int32_t      loopCount;

    CMDQ_MSG("-->CLOCK: Disable hardware clock 0x%08x begin\n", engineFlag);

    pEngine = &(gCmdqContext.engine[0]);
    
    if (engineFlag & (0x1 << tWDMA1))
    {
        pEngine[tWDMA1].userCount--;
        if((pEngine[tWDMA1].userCount <= 0) &&
           clock_is_on(MT_CG_DISP0_MDP_WDMA))
        {
            DISP_REG_SET(0xF400400C, 0x1);

            loopCount = 0;
            while(loopCount < CMDQ_MAX_LOOP_COUNT)
            {
                if (0x1 == (DISP_REG_GET(0xF40040A0) & 0x3FF))
                {
                    break;
                }

                loopCount++;
            }
        
            if (loopCount >= CMDQ_MAX_LOOP_COUNT)
            {
                CMDQ_ERR("Reset WDMA engine failed\n");
                return -EFAULT;
            }
        
            DISP_REG_SET(0xF400400C, 0x0);

            disable_clock(MT_CG_DISP0_MDP_WDMA, "MDP_WDMA");
            pEngine[tWDMA1].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if (engineFlag & (0x1 << tWROT))
    {
        pEngine[tWROT].userCount--;
        if((pEngine[tWROT].userCount <= 0) &&
           clock_is_on(MT_CG_DISP0_MDP_WROT))
        {
            DISP_REG_SET(0xF4005010, 0x1);

            loopCount = 0;
            while(loopCount < CMDQ_MAX_LOOP_COUNT)
            {
                if (0x0 == (DISP_REG_GET(0xF4005014) & 0x1))
                {
                    break;
                }

                loopCount++;
            }

            if (loopCount >= CMDQ_MAX_LOOP_COUNT)
            {
                CMDQ_ERR("Reset WROT engine failed\n");
                return -EFAULT;
            }

            DISP_REG_SET(0xF4005010, 0x0);
            
            disable_clock(MT_CG_DISP0_MDP_WROT, "MDP_WROT");
            pEngine[tWROT].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if (engineFlag & (0x1 << tTDSHP))
    {
        pEngine[tTDSHP].userCount--;
        if((pEngine[tTDSHP].userCount <= 0) &&
           clock_is_on(MT_CG_DISP0_MDP_TDSHP))
        {
            DISP_REG_SET(0xF4006100, 0x0);
            DISP_REG_SET(0xF4006100, 0x2);
            DISP_REG_SET(0xF4006100, 0x0);

            disable_clock(MT_CG_DISP0_MDP_TDSHP, "MDP_TDSHP");
            pEngine[tTDSHP].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if (engineFlag & (0x1 << tSCL1))
    {
        pEngine[tSCL1].userCount--;
        if((pEngine[tSCL1].userCount <= 0) &&
           clock_is_on(MT_CG_DISP0_MDP_RSZ1))
        {
            DISP_REG_SET(0xF4003000, 0x0);
            DISP_REG_SET(0xF4003000, 0x10000);
            DISP_REG_SET(0xF4003000, 0x0);

            disable_clock(MT_CG_DISP0_MDP_RSZ1, "MDP_RSZ1");
            pEngine[tSCL1].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if (engineFlag & (0x1 << tSCL0))
    {
        pEngine[tSCL0].userCount--;
        if((pEngine[tSCL0].userCount <= 0) &&
           clock_is_on(MT_CG_DISP0_MDP_RSZ0))
        {
            DISP_REG_SET(0xF4002000, 0x0);
            DISP_REG_SET(0xF4002000, 0x10000);
            DISP_REG_SET(0xF4002000, 0x0);

            disable_clock(MT_CG_DISP0_MDP_RSZ0, "MDP_RSZ0");
            pEngine[tSCL0].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if (engineFlag & (0x1 << tRDMA0))
    {
        pEngine[tRDMA0].userCount--;

        if((pEngine[tRDMA0].userCount <= 0) &&
           clock_is_on(MT_CG_DISP0_MDP_RDMA))
        {
            DISP_REG_SET(0xF4001008, 0x1);

            loopCount = 0;
            while(loopCount < CMDQ_MAX_LOOP_COUNT)
            {
                if (0x100 == (DISP_REG_GET(0xF4001408) & 0x7FF00))
                {
                    break;
                }
                loopCount++;
            }

            if (loopCount >= CMDQ_MAX_LOOP_COUNT)
            {
                CMDQ_ERR("Reset RDMA engine failed\n");
                return -EFAULT;
            }

            DISP_REG_SET(0xF4001008, 0x0);            
            
            disable_clock(MT_CG_DISP0_MDP_RDMA, "MDP_RDMA");
            pEngine[tRDMA0].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if (engineFlag & (0x1 << tIMGI))
    {
        pEngine[tIMGI].userCount--;
        if(pEngine[tIMGI].userCount <= 0)
        {
            disable_clock(MT_CG_DISP0_CAM_MDP, "CAM_MDP");

            disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
            disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
            disable_clock(MT_CG_IMAGE_SEN_TG,  "CAMERA");
            disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
            disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
            
            pEngine[tIMGI].currOwner = CMDQ_INVALID_THREAD;
        }
    }

    if((0 == pEngine[tIMGI].userCount) &&
       (0 == pEngine[tRDMA0].userCount) &&
       (0 == pEngine[tSCL0].userCount) &&
       (0 == pEngine[tSCL1].userCount) &&
       (0 == pEngine[tTDSHP].userCount) &&
       (0 == pEngine[tWROT].userCount) &&
       (0 == pEngine[tWDMA1].userCount))
    {
        disable_clock(MT_CG_DISP0_MUTEX, "MUTEX");
        disable_clock(MT_CG_DISP0_MM_CMDQ, "MM_CMDQ");
        disable_clock(MT_CG_DISP0_SMI_LARB0, "SMI_LARB0");
        disable_clock(MT_CG_DISP0_SMI_COMMON, "SMI_COMMON");
    }    

    CMDQ_MSG("<--CLOCK: Disable hardware clock 0x%08x end\n", engineFlag);

    return 0;
}


void cmdq_release_thread(int32_t   thread,
                                uint32_t  engineFlag)
{
    int32_t status;
    u_long  flags;

    spin_lock_irqsave(&gCmdqThreadLock, flags);
   
    status = cmdq_disable_clock(engineFlag);

    spin_unlock_irqrestore(&gCmdqThreadLock, flags);

    if (-EFAULT == status)
    {
        CMDQ_AEE("CMDQ", "Can't disable clock\n");
    }

    complete_all(&gCmdqComplete);

    // we must reinitialize completion structure after complete_all()
    INIT_COMPLETION(gCmdqComplete);
}


int32_t cmdq_suspend_HW_thread(int32_t thread)
{
    int32_t loop = 0;

    CMDQ_MSG("EXEC: suspend HW thread[%d]\n", thread);
    
    DISP_REG_SET(DISP_REG_CMDQ_THRx_SUSPEND(thread), 0x01);
    while(0x0 == (DISP_REG_GET(DISP_REG_CMDQ_THRx_STATUS(thread)) & 0x2))
    {
        if(loop > CMDQ_MAX_LOOP_COUNT)
        {
            CMDQ_ERR("suspend HW thread %d failed\n", thread);
            return -EFAULT;
        }
        loop++;
    }
    
    return 0;
}


void cmdq_resume_HW_thread(int32_t thread)
{
    CMDQ_MSG("EXEC: resume HW thread[%d]\n", thread);
    DISP_REG_SET(DISP_REG_CMDQ_THRx_SUSPEND(thread), 0x00);
}


int32_t cmdq_reset_HW_thread(int32_t thread)
{
    int32_t loop = 0;

    CMDQ_MSG("EXEC: reset HW thread[%d]\n", thread);

    DISP_REG_SET(DISP_REG_CMDQ_THRx_RESET(thread), 0x01);
    while(0x1 == (DISP_REG_GET(DISP_REG_CMDQ_THRx_RESET(thread))))
    {
        if(loop > CMDQ_MAX_LOOP_COUNT)
        {
            CMDQ_ERR("reset HW thread %d failed\n", thread);
            return -EFAULT;
        }
        loop++;
    }

    return 0;
}


int32_t cmdq_disable_HW_thread(int32_t thread)
{
    int32_t status = cmdq_reset_HW_thread(thread);
    
    CMDQ_MSG("EXEC: disable HW thread[%d]\n", thread);
    DISP_REG_SET(DISP_REG_CMDQ_THRx_EN(thread), 0x00);
    return status;
}


int32_t cmdq_enable_HW_thread(int32_t thread)
{
    CMDQ_MSG("EXEC: enable thread[%d]\n", thread);
    DISP_REG_SET(DISP_REG_CMDQ_THRx_EN(thread), 0x01);
    return 0;
}


static int32_t cmdq_reset_hw_engine(int32_t engineFlag)
{
    EngineStruct *pEngine;
    int32_t      loopCount;
    uint32_t     regValue;

    CMDQ_MSG("Reset hardware engine begin\n");

    pEngine = &(gCmdqContext.engine[0]);

    if (engineFlag & (0x01 << tIMGI))
    {
        CMDQ_LOG("Reset ISP pass2 start\n");
      
        if(NULL != g_CMDQ_CB_Array.cmdqReset_cb[cbISP])
        {
            g_CMDQ_CB_Array.cmdqReset_cb[cbISP](0);
        }

        CMDQ_LOG("Reset ISP pass2 end\n");
    }

    if (engineFlag & (0x1 << tRDMA0))
    {
        if(clock_is_on(MT_CG_DISP0_MDP_RDMA))
        {
            DISP_REG_SET(0xF4001008, 0x1);
        
            loopCount = 0;
            while(loopCount < CMDQ_MAX_LOOP_COUNT)
            {
                if (0x100 == (DISP_REG_GET(0xF4001408) & 0x7FF00))
                {
                    break;
                }

                loopCount++;
            }

            if (loopCount >= CMDQ_MAX_LOOP_COUNT)
            {
                CMDQ_ERR("Reset RDMA engine failed\n");
                return -EFAULT;
            }

            pEngine[tRDMA0].resetCount++;

            DISP_REG_SET(0xF4001008, 0x0);
        }
    }

    if (engineFlag & (0x1 << tSCL0))
    {
        if(clock_is_on(MT_CG_DISP0_MDP_RSZ0))
        {
            DISP_REG_SET(0xF4002000, 0x0);
            DISP_REG_SET(0xF4002000, 0x10000);
            DISP_REG_SET(0xF4002000, 0x0);
        
            pEngine[tSCL0].resetCount++;
        }
    }

    if (engineFlag & (0x1 << tSCL1))
    {
        if(clock_is_on(MT_CG_DISP0_MDP_RSZ1))
        {
            DISP_REG_SET(0xF4003000, 0x0);
            DISP_REG_SET(0xF4003000, 0x10000);
            DISP_REG_SET(0xF4003000, 0x0);
        
            pEngine[tSCL1].resetCount++;
        }
    }

    if (engineFlag & (0x1 << tTDSHP))
    {
        if(clock_is_on(MT_CG_DISP0_MDP_TDSHP))
        {
            DISP_REG_SET(0xF4006100, 0x0);
            DISP_REG_SET(0xF4006100, 0x2);
            DISP_REG_SET(0xF4006100, 0x0);
        
            pEngine[tTDSHP].resetCount++;
        }
    }

    if (engineFlag & (0x1 << tWROT))
    {
        if(clock_is_on(MT_CG_DISP0_MDP_WROT))
        {
            DISP_REG_SET(0xF4005010, 0x1);

            loopCount = 0;
            while(loopCount < CMDQ_MAX_LOOP_COUNT)
            {
                if (0x0 == (DISP_REG_GET(0xF4005014) & 0x1))
                {
                    break;
                }

                loopCount++;
            }

            DISP_REG_SET(0xF4005010, 0x0);

            if (loopCount >= CMDQ_MAX_LOOP_COUNT)
            {
                CMDQ_ERR("WROT 2nd pass reset start\n");
                
                regValue = DISP_REG_GET(0xF4000138);
                regValue &= 0xFFFFFBFF;
                DISP_REG_SET(0xF4000138, regValue);

                loopCount = 0;
                while(loopCount < CMDQ_MAX_LOOP_COUNT)
                {
                    loopCount++;
                }

                CMDQ_ERR("WROT 2nd pass reset count %d\n", loopCount);

                regValue |= 0x00000400;
                DISP_REG_SET(0xF4000138, regValue);
            
                if (0x0 != (DISP_REG_GET(0xF4005014) & 0x1))
                {
                    CMDQ_ERR("Reset WROT engine failed\n");
                    return -EFAULT;
                }
            
                CMDQ_ERR("WROT 2nd pass reset end\n");
            }

            pEngine[tWROT].resetCount++;
        }
    }

    if (engineFlag & (0x1 << tWDMA1))
    {
        if(clock_is_on(MT_CG_DISP0_MDP_WDMA))
        {
            DISP_REG_SET(0xF400400C, 0x1);

            loopCount = 0;
            while(loopCount < CMDQ_MAX_LOOP_COUNT)
            {
                if (0x1 == (DISP_REG_GET(0xF40040A0) & 0x3FF))
                {
                    break;
                }

                loopCount++;
            }
        
            if (loopCount >= CMDQ_MAX_LOOP_COUNT)
            {
                CMDQ_ERR("Reset WDMA engine failed\n");
                return -EFAULT;
            }

            pEngine[tWDMA1].resetCount++;

            DISP_REG_SET(0xF400400C, 0x0);
        }
    }

    CMDQ_MSG("Reset hardware engine end\n");

    return 0;
}


void cmdq_dump_mmsys_config(void)
{
    CMDQ_ERR("CAM_MDP_MOUT_EN: 0x%08x, MDP_RDMA_MOUT_EN: 0x%08x, MDP_RSZ0_MOUT_EN: 0x%08x\n",         
        DISP_REG_GET(0xF4000000 + 0x01c), DISP_REG_GET(0xF4000000 + 0x020), DISP_REG_GET(0xF4000000 + 0x024));    
    CMDQ_ERR("MDP_RSZ1_MOUT_EN: 0x%08x, MDP_TDSHP_MOUT_EN: 0x%08x, DISP_OVL_MOUT_EN: 0x%08x\n",
        DISP_REG_GET(0xF4000000 + 0x028), DISP_REG_GET(0xF4000000 + 0x02c), DISP_REG_GET(0xF4000000 + 0x030));    
    CMDQ_ERR("MDP_RSZ0_SEL: 0x%08x, MDP_RSZ1_SEL: 0x%08x, MDP_TDSHP_SEL: 0x%08x\n", 
        DISP_REG_GET(0xF4000000 + 0x038), DISP_REG_GET(0xF4000000 + 0x03c), DISP_REG_GET(0xF4000000 + 0x040));
    CMDQ_ERR("MDP_WROT_SEL: 0x%08x, MDP_WDMA_SEL: 0x%08x, DISP_OUT_SEL: 0x%08x\n", 
        DISP_REG_GET(0xF4000000 + 0x044), DISP_REG_GET(0xF4000000 + 0x048), DISP_REG_GET(0xF4000000 + 0x04c)); 
    CMDQ_ERR("MMSYS_DL_VALID_0: 0x%08x, MMSYS_DL_VALID_1: 0x%08x, MMSYS_DL_READY0: 0x%08x, MMSYS_DL_READY1: 0x%08x\n",
        DISP_REG_GET(0xF4000000 + 0x860), DISP_REG_GET(0xF4000000 + 0x864), DISP_REG_GET(0xF4000000 + 0x868), DISP_REG_GET(0xF4000000 + 0x86c)); 
}


void cmdq_core_dump_status(void)
{    
    uint32_t reg[16] = { 0x00, 0x10, 0x18, 0x30, 
                         0x34, 0x38, 0x40, 0x20, 
                         0x50, 0x60, 0x64, 0x68, 
                         0x70, 0x74, 0x78, 0x7c};

    uint32_t i = 0; 
    uint32_t col = 4; 
    uint32_t row = sizeof(reg)/ (sizeof(uint32_t) * col); 
    for(i = 0; i < row; i++)
    {        
        uint32_t index = i * 4; 
        CMDQ_ERR("[0x%03x]: 0x%08x, [0x%x]: 0x%08x, [0x%03x]: 0x%08x, [0x%03x]: 0x%08x\n", 
                reg[index + 0], DISP_REG_GET( DISPSYS_CMDQ_BASE + reg[index + 0]), 
                reg[index + 1], DISP_REG_GET( DISPSYS_CMDQ_BASE + reg[index + 1]),
                reg[index + 2], DISP_REG_GET( DISPSYS_CMDQ_BASE + reg[index + 2]),
                reg[index + 3], DISP_REG_GET( DISPSYS_CMDQ_BASE + reg[index + 3]));
    }    
}


static void cmdq_attach_error_task(const TaskStruct *pTask,
                                   int32_t    thread)
{
    ThreadStruct *pThread = NULL;
    int32_t      index;
    EngineStruct *pEngine = NULL;
    uint32_t     engineFlag = 0;
    ErrorStruct  *pError = NULL;
    uint32_t     value[10] = {0};
    uint32_t      *hwPC = NULL;

    //
    //  Update engine fail count
    //
    if (pTask)
    {
        pEngine    = &(gCmdqContext.engine[0]);
        engineFlag = pTask->engineFlag;
        for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
        {
            if (engineFlag & (1L << index))
            {   
                pEngine[index].failCount++;
            }
        }
    }
    
    //
    //  Then we just print out info
    //
    CMDQ_ERR("================= [CMDQ] Begin of Error %d================\n", gCmdqContext.errNum);
    
    CMDQ_ERR("=============== [CMDQ] SMI Status ===============\n");
    // dump 5 times to observe timing issue
    for (index = 0; index < 5; index++)
    {
        CMDQ_ERR("=============== [CMDQ] SMI Dump %d ===============\n", index);
    #ifndef CONFIG_MTK_FPGA
        smi_dumpDebugMsg();
    #endif
    }
    
    CMDQ_ERR("=============== [CMDQ] Error Thread Status ===============\n");
    
    value[0] = DISP_REG_GET(DISP_REG_CMDQ_THRx_EN(thread));
    value[1] = DISP_REG_GET(DISP_REG_CMDQ_THRx_PC(thread));
    value[2] = DISP_REG_GET(DISP_REG_CMDQ_THRx_END_ADDR(thread));
    value[3] = DISP_REG_GET(DISP_REG_CMDQ_THRx_WAIT_EVENTS0(thread));
    value[4] = DISP_REG_GET(DISP_REG_CMDQ_THRx_EXEC_CMDS_CNT(thread)) & 0x0FFFF;
    value[5] = DISP_REG_GET(DISP_REG_CMDQ_THRx_IRQ_FLAG(thread));
    value[6] = DISP_REG_GET(DISP_REG_CMDQ_THRx_IRQ_FLAG_EN(thread));
    value[7] = DISP_REG_GET(DISP_REG_CMDQ_THRx_INSTN_TIMEOUT_CYCLES(thread));
    value[8] = DISP_REG_GET(DISP_REG_CMDQ_THRx_STATUS(thread));

    pThread = &(gCmdqContext.thread[thread]);

    CMDQ_ERR("THR:%d, Enabled: %d, Thread PC: 0x%08x, End: 0x%08x, Wait Event: 0x%02x\n", 
        thread, value[0], value[1], value[2], value[3]);
    CMDQ_ERR("Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d, Task Count %d, IRQ: 0x%08x, IRQ_EN: 0x%08x\n",
        value[4], pThread->waitCookie, pThread->nextCookie, pThread->taskCount, value[5], value[6]);
    CMDQ_ERR("Timeout Cycle:%d, Status:0x%08x\n", value[7], value[8]);

    if (NULL != pTask)
    {
        CMDQ_ERR("=============== [CMDQ] Error Thread PC ===============\n");
        hwPC = cmdq_core_dump_pc(pTask, thread, "ERR");

        CMDQ_ERR("=============== [CMDQ] Error Task Status ===============\n");
        CMDQ_ERR("Task: 0x%08x, Scenario: %d, State: %d, Priority: %d, Reorder: %d, Engine: 0x%08x, irgFlag: 0x%08x\n",
            (uint32_t)pTask, pTask->scenario, pTask->taskState, pTask->priority, pTask->reorder, pTask->engineFlag, pTask->irqFlag);
        CMDQ_ERR("VABase: 0x%08x, CMDEnd: 0x%08x, MVABase: 0x%08x, Size: %d, Last Inst: 0x%08x:0x%08x, 0x%08x:0x%08x\n",
            (uint32_t)pTask->pVABase, (uint32_t)pTask->pCMDEnd, pTask->MVABase, pTask->blockSize, pTask->pCMDEnd[-3], pTask->pCMDEnd[-2], pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
        CMDQ_ERR("Trigger: %d:%d, Got IRQ: %d:%d, Finish: %d:%d\n", (uint32_t)pTask->trigger.tv_sec, (uint32_t)pTask->trigger.tv_usec,
            (uint32_t)pTask->gotIRQ.tv_sec, (uint32_t)pTask->gotIRQ.tv_usec, (uint32_t)pTask->wakedUp.tv_sec, (uint32_t)pTask->wakedUp.tv_usec);
    }

    CMDQ_ERR("=============== [CMDQ] Mutex Status ===============\n");
    value[0] = DISP_REG_GET(0xF400E004);
    value[1] = DISP_REG_GET(0xF400E00C);
    CMDQ_ERR("[CMDQ] DISP_MUTEX_INTSTA: 0x%08x, DISP_REG_COMMIT: 0x%08x\n", value[0], value[1]);

    for (index = 0; index < 8; index++)
    {        
        CMDQ_ERR("[CMDQ] DISP_MUTEX%d_RST: 0x%08x\n", index, DISP_REG_GET(DISP_REG_CONFIG_MUTEX_RST(index)));
        CMDQ_ERR("[CMDQ] DISP_MUTEX%d_MOD: 0x%08x\n", index, DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(index)));
    }

    CMDQ_ERR("=============== [CMDQ] CMDQ Status ===============\n");
    cmdq_core_dump_status(); 

    CMDQ_ERR("=============== [CMDQ] Clock Status ===============\n");
    value[0] = DISP_REG_GET(0xF4000100);
    value[1] = DISP_REG_GET(0xF4000110);
    CMDQ_ERR("[CMDQ] MMSYS_CG_CON0: 0x%08x, MMSYS_CG_CON1: 0x%08x\n", value[0], value[1]);
    CMDQ_ERR("ISPSys clock state %d\n", subsys_is_on(SYS_ISP));
    CMDQ_ERR("DisSys clock state %d\n", subsys_is_on(SYS_DIS));

    
    CMDQ_ERR("=============== [CMDQ] MMSYS_CONFIG ===============\n");
    cmdq_dump_mmsys_config();

    //
    //  ask each module to print their status
    //
    CMDQ_ERR("=============== [CMDQ] Engine Status ===============\n");


    if (NULL != pTask)
    {
        engineFlag = pTask->engineFlag;

        // Setup error info
        pError = &(gCmdqContext.error[0]);

        // CMDQ_MAX_ERROR_COUNT is 1 now, replace the old one instead of using gCmdqContext.errNum as the index init value
        // otherwise, the corruption may happen while there are more than one error tasks
        index  = (CMDQ_MAX_ERROR_COUNT <= gCmdqContext.errNum) ? (0) : (gCmdqContext.errNum);

        if (engineFlag & (0x01 << tIMGI))
        {
            value[0] = DISP_REG_GET(CLK_CFG_0);
            value[1] = DISP_REG_GET(CLK_CFG_3);
            value[2] = DISP_REG_GET(0xF5000000);
            CMDQ_ERR("[CMDQ] CLK_CFG_0: 0x%08x, CLK_CFG_3: 0x%08x, ISP_CLK_CG: 0x%08x\n", value[0], value[1], value[2]);

            CMDQ_ERR("=============== [CMDQ] ISP Status ====================================\n");

            if (NULL != g_CMDQ_CB_Array.cmdqTimeout_cb[cbISP])
            {
                g_CMDQ_CB_Array.cmdqTimeout_cb[cbISP](pTask->engineFlag & tIMGI, gCmdqContext.logLevel);
            }
        }

        CMDQ_ERR("=============== [CMDQ] MDP Status ====================================\n");
        cmdq_core_dump_mdp_status(engineFlag, gCmdqContext.logLevel);

        CMDQ_ERR("=============== [CMDQ] All Task in Error Thread Info ===============\n");    
        for (index = 0; index < CMDQ_MAX_TASK_COUNT; index++)
        {
            if (NULL != pThread->pCurTask[index])
            {
                CMDQ_ERR("Slot %d, Task: 0x%08x, VABase: 0x%08x, MVABase 0x%08x, Size: %d, Last Inst 0x%08x:0x%08x, 0x%08x:0x%08x\n", 
                    index, (uint32_t)(pThread->pCurTask[index]), (uint32_t)(pThread->pCurTask[index]->pVABase), pThread->pCurTask[index]->MVABase, pThread->pCurTask[index]->blockSize,
                    pThread->pCurTask[index]->pCMDEnd[-3], pThread->pCurTask[index]->pCMDEnd[-2], pThread->pCurTask[index]->pCMDEnd[-1], pThread->pCurTask[index]->pCMDEnd[0]);
            }
        }

        CMDQ_ERR("=============== [CMDQ] GIC dump ===============\n");
        mt_irq_dump_status(MT_DISP_CMDQ_IRQ_ID); 
        
        CMDQ_ERR("=============== [CMDQ] Error Command Buffer ===============\n");
        if (hwPC && pTask && hwPC >= pTask->pVABase)
        {
            // because hwPC points to "start" of the instruction
            // add offset 1
            print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
                           pTask->pVABase, (2 + hwPC - pTask->pVABase) * sizeof(uint32_t), true);
        }
        else
        {
            CMDQ_ERR("hwPC is not in region, dump all\n");            
            print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
                           pTask->pVABase, (pTask->blockSize), true);
        }

        #if 0        
        // reserve the first error task
        if(0 == gCmdqContext.errNum)
        {
            pError[index].pTask = pTask;
        }
        #endif
        CMDQ_ERR("================= [CMDQ] End of Error %d ================\n", gCmdqContext.errNum);
        gCmdqContext.errNum++;
    }

    CMDQ_ERR("=============== [CMDQ] warning task history, total:%d ===============\n", gCmdqContext.warningNum);
    index = 0;
    while(index < gCmdqContext.warningNum && index < CMDQ_MAX_WARNING_COUNT)
    {
        CMDQ_ERR("warning task[%d][0x%p], THR[%d], taskCount[%d], CurrCookie[%d], irqFlag[%d], gotIRQ[%d:%d]", 
                    index, 
                    gCmdqContext.warning[index].taskAddr, 
                    gCmdqContext.warning[index].thread, 
                    gCmdqContext.warning[index].taskCount, 
                    gCmdqContext.warning[index].cookie, 
                    gCmdqContext.warning[index].irqFlag, 
                    (uint32_t)(gCmdqContext.warning[index].gotIRQ.tv_sec), 
                    (uint32_t)(gCmdqContext.warning[index].gotIRQ.tv_usec)); 
        index ++; 
    }    
}


void cmdq_attach_warning_task(
            TaskStruct *pTask,
            uint32_t thread, 
            uint32_t taskCountInThread, 
            uint32_t cookie, 
            uint32_t irqFlag, 
            struct timeval gotIRQ)
{
    uint32_t index = gCmdqContext.warningNum; 
    do
    {
        CMDQ_ERR("warning task[%d][0x%p], THR[%d], taskCount[%d], CurrCookie[%d], irqFlag[%d], gotIRQ[%d:%d]", 
            index, pTask, thread, taskCountInThread, cookie, irqFlag, (uint32_t)(gotIRQ.tv_sec), (uint32_t)(gotIRQ.tv_usec)); 
        if(CMDQ_MAX_WARNING_COUNT <= index)
        {
            break; 
        }
        
        gCmdqContext.warning[index].taskAddr  = (void*)pTask; 
        gCmdqContext.warning[index].thread    = thread; 
        gCmdqContext.warning[index].taskCount = taskCountInThread; 
        gCmdqContext.warning[index].cookie    = cookie; 
        gCmdqContext.warning[index].irqFlag   = irqFlag; 
        gCmdqContext.warning[index].gotIRQ    = gotIRQ;
    }while(0); 
    
    gCmdqContext.warningNum ++; 
}


void cmdq_remove_task_from_thread_unlocked(ThreadStruct  *pThread, int32_t index, TASK_STATE_ENUM targetTaskStaus)
{    
    if((NULL == pThread) || (CMDQ_MAX_TASK_COUNT < index) || (0 > index))
    {
        CMDQ_ERR("remove task, invalid param. pThread[0x%p], task_slot[%d], targetTaskStaus[%d]\n", 
            pThread, index, targetTaskStaus); 
        return; 
    }

    // note timing to switch a task to done_status(_ERROR, _KILLED, _DONE) is aligned with thread's taskcount change
    // check task status to prevent double clean-up thread's taskcount
    if(TASK_STATE_BUSY != pThread->pCurTask[index]->taskState)
    {
        CMDQ_ERR("remove task, taskStatus err[%d]. pThread[0x%p], task_slot[%d], targetTaskStaus[%d]\n",
            pThread->pCurTask[index]->taskState, pThread, index, targetTaskStaus);
        return; 
    }
    
    CMDQ_MSG("remove task, slot[%d], targetStatus: %d\n", index, targetTaskStaus);    
    pThread->pCurTask[index]->taskState = targetTaskStaus;
    
    pThread->pCurTask[index] = NULL;
    pThread->taskCount--;
}


void cmdq_remove_all_task_in_thread_unlocked(int32_t thread)
{
    ThreadStruct *pThread = NULL; 
    TaskStruct   *pTask   = NULL;
    uint32_t      index   = 0;

    do
    {        
        pThread = &(gCmdqContext.thread[thread]);
        
        for (index = 0; index < CMDQ_MAX_TASK_COUNT; ++index)
        {
            pTask = pThread->pCurTask[index];
            if (NULL == pTask)
            {
                continue;
            }
    
            CMDQ_ERR("WAIT: force remove task 0x%p from THR %d\n", pTask, thread);
            pTask->gotIRQ.tv_sec = 0;
            pTask->gotIRQ.tv_usec = 0;
            pTask->irqFlag = 0xDEADDEAD; // unknown state
                
            cmdq_remove_task_from_thread_unlocked(pThread, index, TASK_STATE_ERROR);
        }

        // reset thread info
        pThread->taskCount = 0;
        pThread->waitCookie = pThread->nextCookie;
    }while(0);

     smp_mb();
     wake_up(&gCmdWaitQueue[thread]);
}


int32_t cmdq_core_wait_task_done_with_interruptible_timeout(
            TaskStruct *pTask,
            int32_t thread,
            uint32_t predump_start_time_ms,
            uint32_t predump_duration_ms,
            uint32_t predump_retry_count)
{
    int32_t   waitQ;
    uint32_t  timeout_ms    = 0;
    uint32_t  i             = 0; 
    uint32_t* hwPC          = NULL; 
    uint32_t  swTimeout     = predump_start_time_ms + predump_duration_ms * predump_retry_count;
    uint32_t  predump_max_retry_count = predump_retry_count; 
    bool      hasInterrupted = false; 
    char      predumpTag[60] = {0}; 

    CMDQ_MSG("swTimeout: %d, predumpStartTime: %d, predumpDurationMS: %d, predumpRetryCount: %d\n", 
            swTimeout, predump_start_time_ms, predump_duration_ms, predump_retry_count);

    timeout_ms = predump_start_time_ms;
    waitQ = wait_event_interruptible_timeout(gCmdWaitQueue[thread], (TASK_STATE_BUSY != pTask->taskState), msecs_to_jiffies(timeout_ms));

    // if SW timeout, predump HW PC
    timeout_ms = predump_duration_ms; 
    for(i = 0; i < predump_max_retry_count; i++)
    {
        if(0 < waitQ)
        {
            // condition gets true, task state has became DONE/ERR
            break;
        }
        else if(0 > waitQ && i > 2 * predump_retry_count)
        {
            // extend predump too many times
            CMDQ_ERR("not allow extend predump maxRetryCount[%d], pTask[0x%p], thr[%d], predump[%d : %d]\n", predump_max_retry_count, pTask, thread, i, timeout_ms);
            break;
        }
        else if(0 > waitQ)
        {
            if(!hasInterrupted)
            {                
                CMDQ_ERR("change to use wait_event_timeout, pTask[0x%p], thr[%d]\n", pTask, thread);
            }
            // extend predump count if process is interrupted by signal, and try again
            predump_max_retry_count = predump_max_retry_count + 1;
            hasInterrupted = true; 
            CMDQ_ERR("extend predump maxRetryCount to %d, pTask[0x%p], thr[%d],  predump[%d : %d]\n", predump_max_retry_count, pTask, thread, i, timeout_ms);
        }

        sprintf(predumpTag, "PREDUMP %3d|THR_%d_STATUS:0x%08x, ExecTHR:0x%x", i, thread, DISP_REG_GET(DISP_REG_CMDQ_THRx_STATUS(thread)), DISP_REG_GET(DISP_REG_CMDQ_LOADED_THR));
        hwPC = cmdq_core_dump_pc(pTask, thread, predumpTag);

        // continue to wait
        if(hasInterrupted)
        {
            waitQ = wait_event_timeout(gCmdWaitQueue[thread], (TASK_STATE_BUSY != pTask->taskState), msecs_to_jiffies(timeout_ms));
        }
        else
        {
            waitQ = wait_event_interruptible_timeout(gCmdWaitQueue[thread], (TASK_STATE_BUSY != pTask->taskState), msecs_to_jiffies(timeout_ms));
        }
    }

    if(0 > waitQ)
    {
        CMDQ_ERR("treats as SW timeout[waitQ: %d][predump_count: %d]\n", waitQ, i); 
        waitQ = 0; 
    }
    return waitQ; 
}


int32_t cmdq_core_wait_task_done(
            TaskStruct *pTask,
            int32_t thread,
            uint32_t predump_start_time_ms,
            uint32_t predump_duration_ms,
            uint32_t predump_retry_count)
{
    int32_t       status;
    ThreadStruct  *pThread;
    u_long        flags;
    int32_t       waitQ;
    int32_t       currPC;
    // error report
    bool         throwAEE = false;
    const char   *module = NULL;
    uint32_t     instA, instB;
    uint32_t     irqFlag;
    bool         alreadyResetHWThread;

    // init
    pThread = &(gCmdqContext.thread[thread]);
    status = 0;
    alreadyResetHWThread = false; 

    // get wait time when condition gets true
    waitQ = cmdq_core_wait_task_done_with_interruptible_timeout(pTask, thread, predump_start_time_ms, predump_duration_ms, predump_retry_count); 

    // recorde wake up time
    CMGQ_GET_CURRENT_TIME(pTask->wakedUp);

    // get SW lock
    spin_lock_irqsave(&gCmdqExecLock, flags); 
    smp_mb();


    CMDQ_MSG("WAIT: waitQ[%d], thread[%d], taskCount[%d], pTask[0x%p], taskState[%d]\n", 
        waitQ, thread, pThread->taskCount, pTask, pTask->taskState);

    if(TASK_STATE_DONE == pTask->taskState)
    {
        // success case, continue to execute next task
    }
    else if (TASK_STATE_ERROR == pTask->taskState)
    {
        // CMDQ err
        // except thread's taskCount, HW/SW cookie count, GCE/HW reset haved been fixed, so print err only
        status = -EFAULT;
        cmdq_attach_error_task(pTask, thread);
        CMDQ_ERR("execute error, thread[%d], pTask[0x%p], irqFlag[%x]\n", thread, pTask, pTask->irqFlag);
    }
    else if (0 > waitQ)  // Task be killed
    {
        // waitQ < 0 indicataes the process is interrupted by signal, including SIGKILL and othres
        // CMDQ HW executes quickly, 
        // instead of treating pTask is a ABORT task, let HW continues to executed it
        CMDQ_ERR("should not go here. waitQ[%d], thread[%d], taskCount[%d], pTask[0x%p], pTask->MVABase[0x%08x]\n",
            waitQ, thread, pThread->taskCount, pTask, pTask->MVABase);
    }
    else if (0 == waitQ)  // SW timeout
    {
        do
        {
            // SW timeout and no IRQ received 
            
            // suspend thread first in order to dump consistent status as sw timeout happened
            if(0 > cmdq_suspend_HW_thread(thread))
            {
                cmdq_attach_error_task(pTask, thread);
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                CMDQ_AEE("CMDQ", "suspend thread %d failed after sw timeout\n", thread);
                return -EFAULT;
            }
 
            // check pending IRQ
            irqFlag = DISP_REG_GET(DISP_REG_CMDQ_THRx_IRQ_FLAG(thread));
            if(irqFlag & (CMDQ_THR_IRQ_FALG_INSTN_TIMEOUT| CMDQ_THR_IRQ_FALG_INVALID_INSTN))
            {
                cmdq_handle_error_unlocked(thread, irqFlag, false);
            }
            else if(irqFlag & CMDQ_THR_IRQ_FALG_EXEC_CMD)
            {
                cmdq_handle_done_unlocked(thread, irqFlag, false);
            }

            // double confirm task state 
            if(TASK_STATE_DONE == pTask->taskState)
            {
                // ok case, resume HW thread 
                status = 0;
                cmdq_resume_HW_thread(thread);
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                break; 
            }
            if(TASK_STATE_ERROR == pTask->taskState)
            {
                // CMDQ err
                // if state changes to _ERR,
                // except thread's taskCount, HW/SW cookie count, GCE/HW reset haved been fixed, and previous task are done

                // so print log only, continous to other handle
                CMDQ_ERR("execute error, thread[%d], pTask[0x%p], irqFlag[%x]\n", thread, pTask, pTask->irqFlag);
            }

            // here, pTask's taskState is BUSY
            // this means we did not reach EOC, did not have error IRQ
            
            // record current PC
            currPC = DISP_REG_GET(DISP_REG_CMDQ_THRx_PC(thread));

            // dump error info first
            CMDQ_ERR("SW timeout of task 0x%p on thread %d\n", pTask, thread);
            cmdq_attach_error_task(pTask, thread);
            throwAEE = true;
            cmdq_core_parse_error(pTask, thread,
                                  &module,
                                  &instA,
                                  &instB);

            status = -ETIMEDOUT;

            // remove all task            
            CMDQ_ERR("WAIT: remove all tasks in thread %d because 0x%p sw timeout(taskState:%d)\n", thread, pTask, pTask->taskState);
            cmdq_remove_all_task_in_thread_unlocked(thread);

            // if sw timeout task is executing, 
            // reset HW engine 
            if((currPC >= pTask->MVABase) && (currPC <= (pTask->MVABase + pTask->blockSize)))
            {
                status = cmdq_reset_hw_engine(pTask->engineFlag);
                if (-EFAULT == status)
                {
                    cmdq_attach_error_task(pTask, thread);
                    spin_unlock_irqrestore(&gCmdqExecLock, flags);
                    CMDQ_AEE("MDP", "reset hardware engine 0x%08x after sw timeout, thread:%d, task:0x%p\n", pTask->engineFlag, thread, pTask);
                    return status;
                }
            }

            // reset CMDQ  thread
            CMDQ_ERR("WAIT: reset thread %d, taskCount:%d\n", thread, pThread->taskCount);
            status = cmdq_disable_HW_thread (thread); 
            if(0 > status)
            {
                cmdq_attach_error_task(pTask, thread);
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                CMDQ_AEE("CMDQ", "disable(reset) HW thread %d failed\n", thread);
                return status;
            }
            alreadyResetHWThread = true; 

            // release SW lock
            spin_unlock_irqrestore(&gCmdqExecLock, flags);

            // raise AEE
            if (throwAEE)
            {
                const uint32_t op = (instA & 0xFF000000) >> 24;
                
                switch(op)
                {
                case CMDQ_CODE_WFE:
                    CMDQ_AEE("CMDQ", "%s SW timeout, INST:(0x%08x, 0x%08x), OP:WAIT EVENT:%s\n", 
                             module, 
                             instA, instB,
                             cmdq_core_get_event_name(instA & (~0xFF000000)));
                    break;
                default:
                    CMDQ_AEE("CMDQ", "%s SW timeout, INST:(0x%08x, 0x%08x), OP:%s\n", module, instA, instB, cmdq_core_parse_op(op));
                    break;
                }
            }

            return -EFAULT;
        }while(0);
    }


    // reset and disable thread
    if (0 >= pThread->taskCount && !alreadyResetHWThread)
    {
        if(0 > cmdq_disable_HW_thread (thread))
        {
            cmdq_attach_error_task(pTask, thread);
            spin_unlock_irqrestore(&gCmdqExecLock, flags);
            CMDQ_AEE("CMDQ", "disable(reset) HW thread %d failed\n", thread);
            return -EFAULT;
        }
    }

    spin_unlock_irqrestore(&gCmdqExecLock, flags);
    CMDQ_MSG("Execute the task 0x%p on thread %d end\n", pTask, thread);

    return status;
}

static int32_t cmdq_debug_check_ROT_port(TaskStruct *pTask)
{
#if CMDQ_DEBUG_WROT_PORT_CHECK
    int32_t status;
    M4U_PORT_STRUCT portConfig = {0}; 
    
    status = 0; 
    do
    {
        portConfig.ePortID = MDP_ROTO;
        if((pTask->debugPortData.MDP_ROTO) && (0 != m4u_check_port_va_or_pa(&portConfig)))
        {
            status = -(7001);
            break; 
        }

        portConfig.ePortID = MDP_ROTCO;
        if((pTask->debugPortData.MDP_ROTCO) && (0 != m4u_check_port_va_or_pa(&portConfig)))
        {
            status = -(7002);
            break; 
        }

        portConfig.ePortID = MDP_ROTVO;
        if((pTask->debugPortData.MDP_ROTVO) && (0 != m4u_check_port_va_or_pa(&portConfig)))
        {
            status = -(7003);
            break; 
        }        
    }while(0);

    if(0 > status)
    {
        CMDQ_ERR("MDP_ROT port check failed[%d], portId[%d]\n", status, portConfig.ePortID); 
    }
    
    return status; 
#endif
}

static int32_t cmdq_exec_task_sync(TaskStruct *pTask,
                                   int32_t    thread)
{
    int32_t      status;
    ThreadStruct *pThread;
    u_long       flags;
    int32_t      loop;
    int32_t      count;
    int32_t      minimum;
    int32_t      cookie;
    int32_t      prev;
    TaskStruct   *pPrev;
    TaskStruct   *pLast;
    int32_t      index;
    uint32_t timeout_ms;
    uint32_t start_predump_ms;
    uint32_t predump_duration_ms; 
    uint32_t predump_max_count; 

    CMDQ_MSG("Execute the task 0x%p on thread %d begin, pid: %d\n", pTask, thread, current->pid);
    CMDQ_PROF_START("CMDQ_EXEC");

    pThread = &(gCmdqContext.thread[thread]);

    spin_lock_irqsave(&gCmdqExecLock, flags);
    smp_mb();

    pTask->thread = thread;
    pTask->taskState = TASK_STATE_BUSY; 

    // sw timeout setting
    timeout_ms          = gCmdqContext.swTimeoutDurationMS;
    start_predump_ms    = gCmdqContext.predumpStartTimeMS;
    predump_duration_ms = gCmdqContext.predumpDurationMS; 
    predump_max_count   = gCmdqContext.predumpMaxRetryCount;   

#if 0
    // debug only
    if(0 < atomic_read(&gCmdqDebugProgressiveTimeout))
    {
        start_predump_ms = start_predump_ms / 2;
        CMDQ_LOG("EXEC: progressive timeout: %d, oroginal: %d\n", start_predump_ms, gCmdqContext.predumpStartTimeMS); 
    }
#endif

    if (pThread->taskCount <= 0)
    {
        CMDQ_MSG("EXEC: new HW thread(%d)\n", thread);
   
        DISP_REG_SET(DISP_REG_CMDQ_THRx_RESET(thread), 0x01);
        
        loop = 0;
        while(0x1 == (DISP_REG_GET(DISP_REG_CMDQ_THRx_RESET(thread))))
        {
            if(loop > CMDQ_MAX_LOOP_COUNT)
            {
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                CMDQ_AEE("CMDQ", "Reset HW thread %d failed\n", thread);
                return -EFAULT;
            }
            loop++;
        }

        //
        // register interrupt source condition that we want to receive in ISR: 
        //  .EXEC_CMD: ok case
        //  .INSTN_TIMEOUT: 
        //      there is no simple way to cancle one executing task from a HW thread, 
        //      in order to prevent double err handle (the side effect is inconsistent cookie/thread's task count, PC etc),
        //      we disable HW timeout, use predump monitor and SW timeout to handle HW instr timeout, 
        //      e.g. predump always show same PC
        //  .INVALID_INSTN (asserted by unknown op code): 
        //      in order to prevent infinite INVALID_INSTN ISR, which is casued by resuming thread before correct HW thread PC from invalid instn,
        //      treat INVALID INSTN err as one type of SW timeout
        //
        #if 1
        DISP_REG_SET(DISP_REG_CMDQ_THRx_IRQ_FLAG_EN(thread), CMDQ_THR_IRQ_FALG_EXEC_CMD);  
        #else        
        DISP_REG_SET(DISP_REG_CMDQ_THRx_IRQ_FLAG_EN(thread), CMDQ_THR_IRQ_FALG_EXEC_CMD | CMDQ_THR_IRQ_FALG_INSTN_TIMEOUT | CMDQ_THR_IRQ_FALG_INVALID_INSTN);  
        #endif
        // HW thread config
        DISP_REG_SET(DISP_REG_CMDQ_THRx_INSTN_TIMEOUT_CYCLES(thread), CMDQ_MAX_INST_CYCLE);
        DISP_REG_SET(DISP_REG_CMDQ_THRx_PC(thread), pTask->MVABase);
        DISP_REG_SET(DISP_REG_CMDQ_THRx_END_ADDR(thread), pTask->MVABase + pTask->blockSize);  

        minimum = DISP_REG_GET(DISP_REG_CMDQ_THRx_EXEC_CMDS_CNT(thread)) & 0x0FFFF;

        pThread->nextCookie = minimum + 1;
        if (pThread->nextCookie >= 65536)  // Reach the maximum cookie
        {
            pThread->nextCookie = 0;
        }

        cookie = pThread->nextCookie;
        pThread->waitCookie = cookie;

        pThread->pCurTask[cookie % CMDQ_MAX_TASK_COUNT] = pTask;
        pThread->taskCount = 1; // reset task count to 1  

        pThread->nextCookie += 1;
        if (pThread->nextCookie >= 65536)  // Reach the maximum cookie
        {
            pThread->nextCookie = 0;
        }

        CMGQ_GET_CURRENT_TIME(pTask->trigger);

        // HACK: debug ROT port config issue
        #if CMDQ_DEBUG_WROT_PORT_CHECK
        if(0 > cmdq_debug_check_ROT_port(pTask))
        {
            status = -EFAULT; 
            spin_unlock_irqrestore(&gCmdqExecLock, flags);
            CMDQ_AEE("MDP", "port config issue, thread:%d, pTask:p, engineFlag:0x%08x\n", thread, pTask, pTask, pTask->engineFlag);
            return status; 
        }
        #endif
        cmdq_enable_HW_thread(thread);
    }
    else
    {
        CMDQ_MSG("EXEC: reuse HW thread(%d)\n", thread);
        
        if(0 > cmdq_suspend_HW_thread(thread))
        {
            spin_unlock_irqrestore(&gCmdqExecLock, flags);
            CMDQ_AEE("CMDQ", "suspend HW thread %d failed when insert task[0x%p]\n", thread, pTask);
            return -EFAULT;
        }

        // reset timeout cycle
        DISP_REG_SET(DISP_REG_CMDQ_THRx_INSTN_TIMEOUT_CYCLES(thread), CMDQ_MAX_INST_CYCLE);

        cookie  = pThread->nextCookie;

        // Boundary case tested: EOC have been executed, but JUMP is not executed
        // Thread PC: 0x9edc0dd8, End: 0x9edc0de0, Curr Cookie: 1, Next Cookie: 2
        if((DISP_REG_GET(DISP_REG_CMDQ_THRx_PC(thread)) == (DISP_REG_GET(DISP_REG_CMDQ_THRx_END_ADDR(thread)) - 8)) ||  // PC = END - 8, EOC is executed
           (DISP_REG_GET(DISP_REG_CMDQ_THRx_PC(thread)) == (DISP_REG_GET(DISP_REG_CMDQ_THRx_END_ADDR(thread)) - 0)))    // PC = END - 0, All CMDs are executed
        {
            CMDQ_MSG("Set new task's MVA to thread current PC\n");

            DISP_REG_SET(DISP_REG_CMDQ_THRx_PC(thread), pTask->MVABase);
            DISP_REG_SET(DISP_REG_CMDQ_THRx_END_ADDR(thread), pTask->MVABase + pTask->blockSize); 

            pThread->pCurTask[cookie % CMDQ_MAX_TASK_COUNT] = pTask;
            pThread->taskCount++;
        }
        else
        {
            CMDQ_MSG("Connect new task's MVA to previous one\n");

            // Current task that shuld be processed
            minimum = (DISP_REG_GET(DISP_REG_CMDQ_THRx_EXEC_CMDS_CNT(thread)) & 0x0FFFF) + 1;
            if (minimum >= 65536)
            {
                minimum = 0;
            }

            // Calculate loop count to adjust the tasks' order
            if (minimum <= cookie)
            {
                loop = cookie - minimum;
            }
            else
            {
                // Counter wrapped
                loop = (65535 - minimum + 1) + cookie;
            }

            CMDQ_MSG("Reorder task in range [%d, %d] with count %d\n", minimum, cookie, loop);

            if (loop < 1)
            {
                CMDQ_AEE("CMDQ", "Invalid task count(%d) for reorder\n", loop);
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                return -EFAULT;
            }
            else
            {
                CMDQ_MSG("Reorder %d tasks for performance begin, cookie=%d\n", loop, cookie);

                pLast = pTask;  // Default last task

                // Adjust tasks' order according to their priorities
                for (index = (cookie % CMDQ_MAX_TASK_COUNT); loop > 0; loop--, index--)
                {
                    if (index < 0)
                    {
                        index = CMDQ_MAX_TASK_COUNT - 1;
                    }
                
                    prev = index - 1;
                    if (prev < 0)
                    {
                        prev = CMDQ_MAX_TASK_COUNT - 1;
                    }

                    pPrev = pThread->pCurTask[prev];

                    // Maybe the job is killed, search a new one
                    count = CMDQ_MAX_TASK_COUNT;
                    while((NULL == pPrev) && (count > 0))
                    {
                        prev = prev - 1;
                        if (prev < 0)
                        {
                            prev = CMDQ_MAX_TASK_COUNT - 1;
                        }

                        pPrev = pThread->pCurTask[prev];
                        loop--;
                        index--;
                        count--;
                   }

                    if (NULL != pPrev)
                    {
                        if (loop > 1)
                        {
                            if (pPrev->priority < pTask->priority)
                            {
                                CMDQ_MSG("Switch prev(%d) and current(%d) order\n", prev, index);

                                pThread->pCurTask[index] = pPrev;
                                pPrev->pCMDEnd[ 0] = pTask->pCMDEnd[ 0];
                                pPrev->pCMDEnd[-1] = pTask->pCMDEnd[-1];

                                // Boot priority for the task
                                pPrev->priority += CMDQ_MIN_AGE_VALUE;
                                pPrev->reorder++;

                                pThread->pCurTask[prev]  = pTask;
                                pTask->pCMDEnd[ 0] = 0x10000001;     //Jump: Absolute
                                pTask->pCMDEnd[-1] = pPrev->MVABase; //Jump to here

                                if (pLast == pTask)
                                {
                                    pLast = pPrev;
                                }
                            }
                            else
                            {
                                CMDQ_MSG("Set current(%d) order for the new task\n", index);
                            
                                CMDQ_MSG("Original PC 0x%08x, end 0x%08x\n", pPrev->MVABase, pPrev->MVABase + pPrev->blockSize);
                                CMDQ_MSG("Original instruction 0x%08x, 0x%08x\n", pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);
                
                                pThread->pCurTask[index] = pTask;
                                pPrev->pCMDEnd[ 0] = 0x10000001;     //Jump: Absolute
                                pPrev->pCMDEnd[-1] = pTask->MVABase; //Jump to here
                                break;
                            }
                        }
                        else
                        {
                            CMDQ_MSG("Set current(%d) order for the new task\n", index);
                        
                            CMDQ_MSG("Original PC 0x%08x, end 0x%08x\n", pPrev->MVABase, pPrev->MVABase + pPrev->blockSize);
                            CMDQ_MSG("Original instruction 0x%08x, 0x%08x\n", pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);
                
                            pThread->pCurTask[index] = pTask;
                            pPrev->pCMDEnd[ 0] = 0x10000001;     //Jump: Absolute
                            pPrev->pCMDEnd[-1] = pTask->MVABase; //Jump to here
                            break;
                        }
                    }
                    else
                    {
                        cmdq_attach_error_task(pTask, thread);
                        spin_unlock_irqrestore(&gCmdqExecLock, flags);
                        CMDQ_AEE("CMDQ", "Invalid task state for reorder %d %d\n", index, loop);
                        return -EFAULT;
                    }
                }
            }

            CMDQ_MSG("Reorder %d tasks for performance end\n", loop);

            DISP_REG_SET(DISP_REG_CMDQ_THRx_END_ADDR(thread), pLast->MVABase + pLast->blockSize); 
            pThread->taskCount++;
        }

        pThread->nextCookie += 1;
        if (pThread->nextCookie >= 65536)  // Reach the maximum cookie
        {
            pThread->nextCookie = 0;
        }
 
        CMGQ_GET_CURRENT_TIME(pTask->trigger);

        // HACK: debug ROT port config issue
        #if CMDQ_DEBUG_WROT_PORT_CHECK
        if(0 > cmdq_debug_check_ROT_port(pTask))
        {
            status = -EFAULT; 
            spin_unlock_irqrestore(&gCmdqExecLock, flags);
            CMDQ_AEE("MDP", "port config issue, thread:%d, pTask:p, engineFlag:0x%08x\n", thread, pTask, pTask, pTask->engineFlag);
            return status; 
        }
        #endif
        cmdq_resume_HW_thread(thread);
    }

    spin_unlock_irqrestore(&gCmdqExecLock, flags);

    CMDQ_PROF_END("CMDQ_EXEC");

    status = cmdq_core_wait_task_done(pTask, thread, start_predump_ms, predump_duration_ms, predump_max_count);
    return status;
}


void cmdq_handle_error_unlocked(int32_t thread, uint32_t value, const bool attachWarning)
{
    int32_t         cookie;
    int32_t         count;
    int32_t         inner;    
    uint32_t        taskCount;
    struct timeval  gotIRQ;
    ThreadStruct    *pThread;
    TaskStruct      *pTask;
    int32_t         *hwPC;

    CMDQ_ERR("handle_err_unlocked, thr[%d], irqValue[%02x]\n", thread, value);

    // get IRQ time
    CMGQ_GET_CURRENT_TIME(gotIRQ);
    
    // init basic info
    pThread = &(gCmdqContext.thread[thread]);    
    taskCount = pThread->taskCount;
    pTask = NULL;
    hwPC = NULL;
    
    // we assume error happens BEFORE EOC
    // because it wouldn't be error if this interrupt is issue by EOC.
    // So we should inc by 1 to locate "current" task
    cookie = (DISP_REG_GET(DISP_REG_CMDQ_THRx_EXEC_CMDS_CNT(thread)) & 0x0000FFFF) + 1;
    if (cookie >= 65536)
    {
        cookie -= 65536;
    }

    // set the issued task to error state
    // check task staus to prevent that the error task had beed process to ERROR state when sw timeout
    pTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_COUNT];
    if (pTask && TASK_STATE_BUSY == pTask->taskState)
    {
        // dump PC
        hwPC = cmdq_core_dump_pc(pTask, thread, "ERR");
        // error task process
        if(attachWarning)
        {
            cmdq_attach_warning_task(pTask, thread, taskCount, cookie, value, gotIRQ); 
        }
        pTask->gotIRQ  = gotIRQ;        
        pTask->irqFlag = value;
        cmdq_remove_task_from_thread_unlocked(pThread, (cookie % CMDQ_MAX_TASK_COUNT), TASK_STATE_ERROR); 
    }

    // Set the remain tasks to done state
    if (pThread->waitCookie <= cookie)
    {
        count = cookie - pThread->waitCookie + 1;
    }
    else
    {
        // Counter wrapped
        count = (65535 - pThread->waitCookie + 1) + (cookie + 1);
    }

    for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_COUNT); count > 0; count--, inner++)
    {
        if (inner >= CMDQ_MAX_TASK_COUNT)
        {
            inner = 0;
        }
        
        if (NULL != pThread->pCurTask[inner])
        {            
            pTask = pThread->pCurTask[inner];
            if(attachWarning)
            {
                cmdq_attach_warning_task(pTask, thread, taskCount, cookie, value, gotIRQ); 
            }
            
            pTask->gotIRQ  = gotIRQ;        
            pTask->irqFlag = value; 
            cmdq_remove_task_from_thread_unlocked(pThread, inner, TASK_STATE_DONE); 
        }
    }    

    pThread->waitCookie = cookie + 1;
    if (pThread->waitCookie >= 65536)
    {
        pThread->waitCookie -= 65536;
    }
}


void cmdq_handle_done_unlocked(int32_t thread, uint32_t value, const bool attachWarning)
{
    int32_t         cookie;
    int32_t         count;
    int32_t         inner;    
    uint32_t        taskCount;
    struct timeval  gotIRQ;
    ThreadStruct    *pThread;
    TaskStruct      *pTask;

    CMDQ_MSG("handle_done_unlocked, thread: %d, irqValue: 0x%08x\n", thread, value);

    // get IRQ time
    CMGQ_GET_CURRENT_TIME(gotIRQ);

    // init basic info
    pThread = &(gCmdqContext.thread[thread]);    
    taskCount = pThread->taskCount;
    pTask = NULL;

    cookie = DISP_REG_GET(DISP_REG_CMDQ_THRx_EXEC_CMDS_CNT(thread)) & 0x0000FFFF;
    if (pThread->waitCookie <= cookie)
    {
        count = cookie - pThread->waitCookie + 1;
    }
    else
    {
        // Counter wrapped
        count = (65535 - pThread->waitCookie + 1) + (cookie + 1);
    }

    for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_COUNT); count > 0; count--, inner++)
    {
        if (inner >= CMDQ_MAX_TASK_COUNT)
        {
            inner = 0;
        }
            
        if (NULL != pThread->pCurTask[inner])
        {    
            pTask = pThread->pCurTask[inner];
            if(attachWarning)
            {
                cmdq_attach_warning_task(pTask, thread, taskCount, cookie, value, gotIRQ); 
            }        
            pTask->gotIRQ  = gotIRQ;        
            pTask->irqFlag = value; 
            cmdq_remove_task_from_thread_unlocked(pThread, inner, TASK_STATE_DONE); 
        }
    }    

    pThread->waitCookie = cookie + 1;
    if (pThread->waitCookie >= 65536)
    {
        pThread->waitCookie -= 65536;
    }
}

void cmdqHandleIRQ(int32_t thread, const uint32_t value)
{
    unsigned long flags;
    bool      isInvalidInstruction;
    bool      isAlreadySuspended;
    const uint32_t validIrqFlag = (CMDQ_THR_IRQ_FALG_INVALID_INSTN | CMDQ_THR_IRQ_FALG_INSTN_TIMEOUT | CMDQ_THR_IRQ_FALG_EXEC_CMD); 
    bool attachWarning = false;

    if (0 == (value & validIrqFlag))
    {
        CMDQ_VERBOSE("IRQ: thread %d got interrupt but IRQ flag is 0x%08x\n", thread, value);
        return;
    }

    // get SW lock
    spin_lock_irqsave(&gCmdqExecLock, flags);
    smp_mb();

    // we must suspend thread before query cookie
    isAlreadySuspended = (0 < (DISP_REG_GET(DISP_REG_CMDQ_THRx_STATUS(thread)) & 0x2)) ? true : false;
    isInvalidInstruction = (0 < (value & CMDQ_THR_IRQ_FALG_INVALID_INSTN)) ? true : false;
    if (false == isAlreadySuspended || isInvalidInstruction)
    {
        CMDQ_VERBOSE("IRQ: thread %d status: %x, start to suspend\n", thread, DISP_REG_GET(DISP_REG_CMDQ_THRx_STATUS(thread)));
        cmdq_suspend_HW_thread(thread);
    }

    // warning task check    
    if (0x0 == DISP_REG_GET(DISP_REG_CMDQ_THRx_EN(thread)))
    {     
        attachWarning = true; 
        CMDQ_ERR("Thr[%d] disabled in cmdqHandleIRQ, value = %x\n", thread, value);
    }

    do
    {
        if(value & (CMDQ_THR_IRQ_FALG_INVALID_INSTN | CMDQ_THR_IRQ_FALG_INSTN_TIMEOUT))
        {
            cmdq_handle_error_unlocked(thread, value, attachWarning);
        }
        else if (value & CMDQ_THR_IRQ_FALG_EXEC_CMD)
        {
            cmdq_handle_done_unlocked(thread, value, attachWarning);
        }
    
        // IRQ value may mix success and error result, e.g. value = 0x3 = 1 means HW timeout + 1 success
        // so clear up IRQ flag with irq value, not 0x12, to prevent duplicate IRQ handle
        DISP_REG_SET(DISP_REG_CMDQ_THRx_IRQ_FLAG(thread), ~value);
    }while(0);


    //
    // HW thread continues to execute from last instrction after driver resumes HW thread 
    // in invalid instruciton case, 
    // driver MUST correct PC from invalid instruciton and clean irq status before resume HW thread
    // otherwise, we will trap into inifinite invalid instruciton IRQ loop
    //
    // so do not resume if it is invalide instruction.
    // let user space handle this situation.
    //
    if (false == isAlreadySuspended && false == isInvalidInstruction)
    {
        cmdq_resume_HW_thread(thread);
    }

    // release lock    
    spin_unlock_irqrestore(&gCmdqExecLock, flags);
    
    // wake up other
    smp_mb();    
    wake_up(&gCmdWaitQueue[thread]);
}


void cmdqHandleError(int32_t thread, uint32_t value)
{
    cmdqHandleIRQ(thread, value);
}


void cmdqHandleDone(int32_t thread, uint32_t value)
{
    CMDQ_PROF_START("CMDQ_IRQ_DONE");
    cmdqHandleIRQ(thread, value);
    CMDQ_PROF_END("CMDQ_IRQ_DONE");
}


int32_t cmdqSubmitTask(cmdqCommandStruct *pCommandDesc)
{
    struct timeval start;
    struct timeval done;
    TaskStruct      *pTask;
    int32_t         thread;
    int32_t         retry;
    int32_t         status;
    u_long          flags;
    RecordStruct    *pRecord;
    uint32_t engineFlag = pCommandDesc->engineFlag;

    CMGQ_GET_CURRENT_TIME(start);

    pTask = cmdq_acquire_task(pCommandDesc);
    if (NULL == pTask)
    {
        CMDQ_ERR("Acquire task failed\n");
        return -EFAULT;
    }

    thread = cmdq_acquire_thread(engineFlag);
    if (CMDQ_INVALID_THREAD == thread)
    {
        CMDQ_ERR("Acquire thread failed\n");
        cmdq_release_task(pTask);
        return -EFAULT;
    }

    retry = 0;
    do
    {
        status = cmdq_exec_task_sync(pTask,
                                     thread);
        if ((status >= 0) ||
            (TASK_STATE_KILLED == pTask->taskState) ||
            (TASK_STATE_ERROR  == pTask->taskState))
        {
            break;
        }

        ++retry;
    } while(retry < CMDQ_MAX_RETRY_COUNT);

    cmdq_release_thread(thread,
                        engineFlag);

    CMGQ_GET_CURRENT_TIME(done);

    spin_lock_irqsave(&gCmdqRecordLock, flags);
    smp_mb();

    pRecord = &(gCmdqContext.record[gCmdqContext.lastID]);
    gCmdqContext.lastID++;
    if (gCmdqContext.lastID >= CMDQ_MAX_RECORD_COUNT)
    {
        gCmdqContext.lastID = 0;
    }

    gCmdqContext.recNum++;
    if (gCmdqContext.recNum >= CMDQ_MAX_RECORD_COUNT)
    {
        gCmdqContext.recNum = CMDQ_MAX_RECORD_COUNT;
    }

    spin_unlock_irqrestore(&gCmdqRecordLock, flags);

    // Record scenario
    pRecord->scenario  = pTask->scenario;
    pRecord->priority  = pTask->priority;
    pRecord->reorder   = pTask->reorder;

    // Record time
    pRecord->start     = start;
    pRecord->trigger   = pTask->trigger;
    pRecord->gotIRQ    = pTask->gotIRQ;
    pRecord->wakedUp   = pTask->wakedUp;
    pRecord->done      = done;

    cmdq_release_task(pTask);

    return status;
}


void cmdqDeInitialize()
{
    TaskStruct *pTask = NULL;
    
    // proc destory
    cmdq_deinit_procfs();

    pTask = &(gCmdqContext.taskInfo[0]);

    dma_free_coherent(NULL, CMDQ_MAX_DMA_BUF_SIZE, pTask->pVABase, pTask->MVABase);
}

bool cmdq_core_should_print_msg(void)
{
    return (0 < atomic_read(&gCmdqDebugLevel)); 
}

void cmdq_debug_set_progressive_timeout(bool enable)
{
    atomic_set(&gCmdqDebugProgressiveTimeout, (enable?(1):(0)));
}

void cmdq_debug_set_sw_timeout(
            const uint32_t sw_timeout_ms,
            const uint32_t predump_start_time_ms,
            const uint32_t predump_duration_ms)
{

    u_long       flags = 0;
    uint32_t     swTimeout = sw_timeout_ms;
    uint32_t     predumpStartTime = predump_start_time_ms;
    uint32_t     predumpDuration = predump_duration_ms;
    uint32_t     predumpMaxCount = 0;
    uint32_t     checksum        = 0; 

    do
    {
        checksum        = (swTimeout - predumpStartTime) % predumpDuration;
        predumpMaxCount = (swTimeout - predumpStartTime) / predumpDuration; 

        if(0 < predumpMaxCount && 0 == checksum)
        {
            // ok parameter, passed!
            break; 
        }

        swTimeout        = CMDQ_DEFAULT_TIMEOUT_MS;
        predumpStartTime = CMDQ_DEFAULT_PREDUMP_START_TIME_MS;
        predumpDuration  = CMDQ_DEFAULT_PREDUMP_TIMEOUT_MS; 
        predumpMaxCount  = CMDQ_DEFAULT_PREDUMP_RETRY_COUNT;  
    }while(0);

    spin_lock_irqsave(&gCmdqExecLock, flags);
    gCmdqContext.swTimeoutDurationMS = swTimeout;
    gCmdqContext.predumpStartTimeMS  = predumpStartTime;
    gCmdqContext.predumpDurationMS   = predumpDuration; 
    gCmdqContext.predumpMaxRetryCount= predumpMaxCount;  
    spin_unlock_irqrestore(&gCmdqExecLock, flags);
    
}


static int __init cmdq_mdp_init(void)
{
    return 0;
}

static void __exit cmdq_mdp_exit(void)
{
    return;
}
    
module_init(cmdq_mdp_init);
module_exit(cmdq_mdp_exit);

