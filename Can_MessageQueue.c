/**********************************************************************************************************************
 *  COPYRIGHT
 *  -------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) 2022 by Ielektron Technologies Pvt Ltd.                                              All rights reserved.
 *
 *                This software is copyright protected and proprietary to Ielektron Technologies Pvt Ltd.
 *                Ielektron Technologies Pvt Ltd grants to you only those rights as set out in the license conditions.
 *                All other rights remain with Ielektron Technologies Pvt Ltd.
 *  \endverbatim
 *  -------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -----------------------------------------------------------------------------------------------------------------*/
/**        \file  Can_MessageQueue.c
 *        \brief  CAN Message Queue Implementation file.
 *
 *      \details  This file contains the Implementation of the public APIs of the Message Queue Module
 *
 *
 *********************************************************************************************************************/

/**********************************************************************************************************************
*  INCLUDES
*********************************************************************************************************************/

#include "Can_MessageQueue.h"

/**********************************************************************************************************************
*  LOCAL CONSTANT MACROS / LOCAL FUNCTION MACROS
*********************************************************************************************************************/

/**********************************************************************************************************************
*  LOCAL DATA TYPES AND STRUCTURES
*********************************************************************************************************************/

typedef struct 
{
    uint8_t ModuleId;
    uint8_t ApiId;
    uint8_t ErrorId;
} detInfo_t;

/**********************************************************************************************************************
*  GLOBAL DATA
**********************************************************************************************************************/

volatile detInfo_t xDetInfo;

 
/**********************************************************************************************************************
*  LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************************/

static pduHdl find_pos(CanMsgQ_t *q, pduHdl new_msg);
static CanQ_RetType_t insert(CanMsgQ_t *q, uint8_t pos, pduHdl new_msg);

static CBQ_RetType_t CBQ_Reset(CBQ_t *q);
CBQ_RetType_t CBQ_getStatus(CBQ_t *q, uint8_t *len);
static uint8_t CBQ_findNext(CBQ_t *q, uint8_t cur_pos);
static uint8_t CBQ_findPrev(CBQ_t *q, uint8_t cur_pos);

extern void App_Delayms(uint16_t delay);
void Det_ReportError (uint8_t MooduleID, uint8_t API_ID, uint8_t ErrorCode);
void Det_ClearDTC(void);

void Det_ClearDTC(void)
{
    xDetInfo.ModuleId = 0;
    xDetInfo.ApiId = 0;
    xDetInfo.ErrorId = 0;
}

void Det_ReportError (uint8_t ModuleID, uint8_t ApiID, uint8_t ErrorCode)
{
    xDetInfo.ModuleId = ModuleID;
    xDetInfo.ApiId = ApiID;
    xDetInfo.ErrorId = ErrorCode;

    /*App_Delayms(60000);*/
    /*while ( 1 );*/
}

/*
/return 0xff - Same Message ID already present in the queue
*/
static pduHdl find_pos(CanMsgQ_t *q, pduHdl new_msg)
{
    pduHdl pos = q->qfifo.t;
    
    /* This function will check from tail to head-1, 
        if it doesnot find high priority index it will return head*/
    while (pos != q->qfifo.h) {

        if ( (q->getHdl(q->MsgBuffHdl[pos]))->CanId == (q->getHdl(new_msg))->CanId) {
            /*Same ID is already present in the queue, no need to update the positions*/
            pos = 0xFF;
            break;
        } else if ( (q->getHdl(q->MsgBuffHdl[pos]))->CanId > (q->getHdl(new_msg))->CanId) {
            break;
        }
        pos = CBQ_findNext(&q->qfifo,pos);
    }
    
    return pos;
}

static CanQ_RetType_t insert(CanMsgQ_t *q, uint8_t pos, pduHdl new_msg)
{
    CanQ_RetType_t sts = CanQ_Success;
    uint8_t n=0,p=0;

     n = q->qfifo.h;
    /*Shift elements in the range pos to head-1 to pos+1 to head*/
    /*No shift will be made if pos and head are same*/
    while(n!=pos) {
        p=CBQ_findPrev(&q->qfifo,n);
        q->MsgBuffHdl[n] = q->MsgBuffHdl[p];
        n=p;
    }
    q->MsgBuffHdl[pos] = new_msg;

    return sts;
}

CanQ_RetType_t CanQ_Reset(CanMsgQ_t *q)
{
    CanQ_RetType_t sts = CanQ_Success;
    
    CBQ_Reset(&q->qfifo);

    return sts;
}

CanQ_RetType_t CanQ_Add(CanMsgQ_t *q, pduHdl new_msg)
{
    CanQ_RetType_t sts = CanQ_Success;
    CBQ_RetType_t ret;
    uint8_t h=0,p=0;
    
    if (q->type == Can_PriorityQueue) {
        p = find_pos(q,new_msg);
        if (p == 0xFF) {
            return CanQ_Success;
        }
    }
    
    ret = CBQ_Add(&q->qfifo,&h);

    if (ret == CBQ_Full) {
        return CanQ_Full;
    }

    if (q->type == Can_PriorityQueue) {
        insert(q,p,new_msg);
    } else if (q->type == Can_FifoQueue) {
        q->MsgBuffHdl[h] = new_msg;
    }

    return sts;
}

CanQ_RetType_t CanQ_Read(CanMsgQ_t *q, Can_LLFrame_t *new_msg, pduHdl *hdl)
{
    CanQ_RetType_t sts = CanQ_Success;
    CBQ_RetType_t ret;
    uint8_t t = 0;

    ret = CBQ_Read(&q->qfifo,&t);

    if (ret == CBQ_Empty) {
        return CanQ_Empty;
    }
    
    *new_msg = *(q->getHdl(q->MsgBuffHdl[t]));
    *hdl = q->MsgBuffHdl[t];

    return sts;
}

CanQ_RetType_t CanQ_getStatus(CanMsgQ_t *q, uint8_t *len)
{
    CanQ_RetType_t ret = CanQ_Success;
    CBQ_RetType_t sts;

    sts = CBQ_getStatus(&q->qfifo,len);

    if (sts == CBQ_Empty) {
        return CanQ_Empty;
    }

    if (sts == CBQ_Full) {
        return CanQ_Full;
    }

    return ret;
}

CanQ_RetType_t CanQ_Create(CanMsgQ_t *q, 
							pduHdl *Buff, 
							uint8_t len, 
							getMsgHdlFctPtr func, 
							CanMsgQ_Type_t type
							)
{
    CanQ_RetType_t ret = CanQ_Success;
    
    CBQ_Create(&q->qfifo,len);

    q->getHdl = func;
    q->type = type;
    q->MsgBuffHdl = Buff;

    return ret;
}

CBQ_RetType_t CBQ_Create(CBQ_t *q, uint8_t len )
{
    CBQ_RetType_t ret = CBQ_Success;
    q->h = 0;
    q->t = 0;
    q->max = len;

    return ret;
}

CBQ_RetType_t CBQ_Add(CBQ_t *q, uint8_t *head)
{
    CBQ_RetType_t ret = CBQ_Success;
    uint8_t n=0;
    
    n = CBQ_findNext(q,q->h);
    
    if (n == q->t) {
        return CBQ_Full;
    }
    *head = q->h;
    q->h = n;
    return ret;
}

CBQ_RetType_t CBQ_Read(CBQ_t *q, uint8_t *tail)
{
    CBQ_RetType_t ret = CBQ_Success;

    if(q->h == q->t) {
        return CBQ_Empty;
    }

    *tail = q->t;
    q->t = CBQ_findNext(q,q->t);

    return ret;
}

static CBQ_RetType_t CBQ_Reset(CBQ_t *q)
{
    CBQ_RetType_t ret = CBQ_Success;
    q->h = 0;
    q->t = 0;
    return ret;
}

CBQ_RetType_t CBQ_getStatus(CBQ_t *q, uint8_t *len)
{
    CBQ_RetType_t ret = CBQ_Success;
    uint8_t n=0;

    if(q->h == q->t) {
        return CBQ_Empty;
    }

    n = CBQ_findNext(q,q->h);    
    if (n == q->t) {
        return CBQ_Full;
    }

    if (q->h < q->max ) {
        *len = q->h - q->t;
    } else {
        *len = (q->max - q->t) + q->h;
    }

    return ret;
}

static uint8_t CBQ_findNext(CBQ_t *q, uint8_t cur_pos)
{
    uint8_t nxt = cur_pos + 1;
    if (nxt >= q->max) {
        nxt = 0;
    }
    return nxt;  
}

static uint8_t CBQ_findPrev(CBQ_t *q, uint8_t cur_pos)
{
    uint8_t prev = cur_pos - 1;
    if (prev >= q->max) {
        prev = q->max-1;
    }
    return prev; 
}

/**********************************************************************************************************************
*  END OF FILE: Can_MessageQueue.c
*********************************************************************************************************************/
