/************************************************************************
 * NASA Docket No. GSC-18,918-1, and identified as “Core Flight
 * Software System (cFS) File Manager Application Version 2.6.1”
 *
 * Copyright (c) 2021 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *  Core Flight System (CFS) File Manager (FM) Application
 *
 *  The File Manager (FM) Application provides onboard file system
 *  management services by processing commands for copying and moving
 *  files, decompressing files, concatenating files, creating directories,
 *  deleting files and directories, and providing file and directory status.
 *  When the File Manager application receives a housekeeping request
 *  (scheduled within the scheduler application), FM  reports it's housekeeping
 *  status values via telemetry messaging.
 */

#include "cfe.h"
#include "fm_msg.h"
#include "fm_msgdefs.h"
#include "fm_msgids.h"
#include "fm_app.h"
#include "fm_tbl.h"
#include "fm_child.h"
#include "fm_cmds.h"
#include "fm_cmd_utils.h"
#include "fm_events.h"
#include "fm_perfids.h"
#include "fm_platform_cfg.h"
#include "fm_version.h"
#include "fm_verify.h"

#include <string.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM application global data                                      */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

FM_GlobalData_t FM_GlobalData;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM application -- entry point and main loop processor           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FM_AppMain(void)
{
    uint32           RunStatus = CFE_ES_RunStatus_APP_RUN;
    CFE_SB_Buffer_t *BufPtr    = NULL;
    int32            Result    = CFE_SUCCESS;

    /* Performance Log (start time counter) */
    CFE_ES_PerfLogEntry(FM_APPMAIN_PERF_ID);

    /*
    ** Perform application specific initialization...
    */
    Result = FM_AppInit();

    /*
    ** Check for start-up error...
    */
    if (Result != CFE_SUCCESS)
    {
        /*
        ** Set request to terminate main loop...
        */
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Main process loop...
    */
    while (CFE_ES_RunLoop(&RunStatus) == true)
    {
        /* Performance Log (stop time counter) */
        CFE_ES_PerfLogExit(FM_APPMAIN_PERF_ID);

        /* Wait for the next Software Bus message */
        Result = CFE_SB_ReceiveBuffer(&BufPtr, FM_GlobalData.CmdPipe, FM_SB_TIMEOUT);

        /* Performance Log (start time counter) */
        CFE_ES_PerfLogEntry(FM_APPMAIN_PERF_ID);

        if (Result == CFE_SUCCESS)
        {
            if (BufPtr != NULL)
            {
                /* Process Software Bus message */
                FM_ProcessPkt(BufPtr);
            }
            else
            {
                /* Software Bus thought it succeeded but provided a bad pointer */
                CFE_EVS_SendEvent(FM_SB_RECEIVE_NULL_PTR_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Main loop error: SB returned NULL pointer on success");

                /* Set request to terminate main loop */
                RunStatus = CFE_ES_RunStatus_APP_ERROR;
            }
        }
        else if (Result == CFE_SB_TIME_OUT)
        {
            /* Allow cFE the chance to manage tables.  This is typically done
             * during the housekeeping cycle, but if housekeeping is done at
             * less than a 1Hz rate the table management is done here as well. */
            FM_ReleaseTablePointers();
            FM_AcquireTablePointers();
        }
        else
        {
            /* Process Software Bus error */
            CFE_EVS_SendEvent(FM_SB_RECEIVE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Main loop error: SB receive: result = 0x%08X", (unsigned int)Result);

            /* Set request to terminate main loop */
            RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Send an event describing the reason for the termination...
    */
    CFE_EVS_SendEvent(FM_EXIT_ERR_EID, CFE_EVS_EventType_ERROR, "Application terminating: result = 0x%08X",
                      (unsigned int)Result);

    /*
    ** In case cFE Event Services is not working...
    */
    CFE_ES_WriteToSysLog("FM application terminating: result = 0x%08X\n", (unsigned int)Result);

    /*
    ** Performance Log (stop time counter)...
    */
    CFE_ES_PerfLogExit(FM_APPMAIN_PERF_ID);

    /*
    ** Let cFE kill the task (and any child tasks)...
    */
    CFE_ES_ExitApp(RunStatus);

} /* End FM_AppMain */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM application -- startup initialization processor              */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FM_AppInit(void)
{
    const char *ErrText = "Initialization error:";
    int32       Result  = CFE_SUCCESS;

    /* Initialize global data  */
    memset(&FM_GlobalData, 0, sizeof(FM_GlobalData));

    /* Register for event services */
    Result = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);

    if (Result != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(FM_STARTUP_EVENTS_ERR_EID, CFE_EVS_EventType_ERROR,
                          "%s register for event services: result = 0x%08X", ErrText, (unsigned int)Result);
    }
    else
    {
        /* Create Software Bus message pipe */
        Result = CFE_SB_CreatePipe(&FM_GlobalData.CmdPipe, FM_APP_PIPE_DEPTH, FM_APP_PIPE_NAME);
        if (Result != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(FM_STARTUP_CREAT_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s create SB input pipe: result = 0x%08X", ErrText, (unsigned int)Result);
        }
        else
        {
            /* Subscribe to Housekeeping request commands */
            Result = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(FM_SEND_HK_MID), FM_GlobalData.CmdPipe);

            if (Result != CFE_SUCCESS)
            {
                CFE_EVS_SendEvent(FM_STARTUP_SUBSCRIB_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "%s subscribe to HK request: result = 0x%08X", ErrText, (unsigned int)Result);
            }
        }
    }

    /* Keep indentation from getting too deep */
    if (Result == CFE_SUCCESS)
    {
        /* Subscribe to FM ground command packets */
        Result = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(FM_CMD_MID), FM_GlobalData.CmdPipe);

        if (Result != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(FM_STARTUP_SUBSCRIB_GCMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s subscribe to FM commands: result = 0x%08X", ErrText, (unsigned int)Result);
        }
        else
        {
            /* Initialize FM tables */
            Result = FM_TableInit();

            if (Result != CFE_SUCCESS)
            {
                CFE_EVS_SendEvent(FM_STARTUP_TABLE_INIT_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "%s register free space table: result = 0x%08X", ErrText, (unsigned int)Result);
            }
            else
            {
                /* Create low priority child task */
                FM_ChildInit();

                /* Application startup event message */
                CFE_EVS_SendEvent(FM_STARTUP_EID, CFE_EVS_EventType_INFORMATION,
                                  "Initialization complete: version %d.%d.%d.%d", FM_MAJOR_VERSION, FM_MINOR_VERSION,
                                  FM_REVISION, FM_MISSION_REV);
            }
        }
    }

    return (Result);

} /* End of FM_AppInit() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM application -- input packet processor                        */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FM_ProcessPkt(const CFE_SB_Buffer_t *BufPtr)
{
    CFE_SB_MsgId_t MessageID = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&BufPtr->Msg, &MessageID);

    switch (CFE_SB_MsgIdToValue(MessageID))
    {
        /* Housekeeping request */
        case FM_SEND_HK_MID:
            FM_ReportHK((CFE_MSG_CommandHeader_t *)BufPtr);
            break;

        /* FM ground commands */
        case FM_CMD_MID:
            FM_ProcessCmd(BufPtr);
            break;

        default:
            CFE_EVS_SendEvent(FM_MID_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Main loop error: invalid message ID: mid = 0x%08lX",
                              (unsigned long)CFE_SB_MsgIdToValue(MessageID));
            break;
    }

} /* End of FM_ProcessPkt */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM application -- command packet processor                      */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FM_ProcessCmd(const CFE_SB_Buffer_t *BufPtr)
{
    bool              Result      = true;
    CFE_MSG_FcnCode_t CommandCode = 0;

    CFE_MSG_GetFcnCode(&BufPtr->Msg, &CommandCode);

    /* Invoke specific command handler */
    switch (CommandCode)
    {
        case FM_NOOP_CC:
            Result = FM_NoopCmd(BufPtr);
            break;

        case FM_RESET_CC:
            Result = FM_ResetCountersCmd(BufPtr);
            break;

        case FM_COPY_CC:
            Result = FM_CopyFileCmd(BufPtr);
            break;

        case FM_MOVE_CC:
            Result = FM_MoveFileCmd(BufPtr);
            break;

        case FM_RENAME_CC:
            Result = FM_RenameFileCmd(BufPtr);
            break;

        case FM_DELETE_CC:
            Result = FM_DeleteFileCmd(BufPtr);
            break;

        case FM_DELETE_ALL_CC:
            Result = FM_DeleteAllFilesCmd(BufPtr);
            break;
#ifdef FM_INCLUDE_DECOMPRESS
        case FM_DECOMPRESS_CC:
            Result = FM_DecompressFileCmd(BufPtr);
            break;
#endif
        case FM_CONCAT_CC:
            Result = FM_ConcatFilesCmd(BufPtr);
            break;

        case FM_GET_FILE_INFO_CC:
            Result = FM_GetFileInfoCmd(BufPtr);
            break;

        case FM_GET_OPEN_FILES_CC:
            Result = FM_GetOpenFilesCmd(BufPtr);
            break;

        case FM_CREATE_DIR_CC:
            Result = FM_CreateDirectoryCmd(BufPtr);
            break;

        case FM_DELETE_DIR_CC:
            Result = FM_DeleteDirectoryCmd(BufPtr);
            break;

        case FM_GET_DIR_FILE_CC:
            Result = FM_GetDirListFileCmd(BufPtr);
            break;

        case FM_GET_DIR_PKT_CC:
            Result = FM_GetDirListPktCmd(BufPtr);
            break;

        case FM_GET_FREE_SPACE_CC:
            Result = FM_GetFreeSpaceCmd(BufPtr);
            break;

        case FM_SET_TABLE_STATE_CC:
            Result = FM_SetTableStateCmd(BufPtr);
            break;

        case FM_SET_FILE_PERM_CC:
            Result = FM_SetPermissionsCmd(BufPtr);
            break;

        default:
            Result = false;
            CFE_EVS_SendEvent(FM_CC_ERR_EID, CFE_EVS_EventType_ERROR, "Main loop error: invalid command code: cc = %d",
                              CommandCode);
            break;
    }

    if (Result == true)
    {
        /* Increment command success counter */
        if (CommandCode != FM_RESET_CC)
        {
            FM_GlobalData.CommandCounter++;
        }
    }
    else
    {
        /* Increment command error counter */
        FM_GlobalData.CommandErrCounter++;
    }

} // End of FM_ProcessCmd

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM application -- housekeeping request packet processor         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FM_ReportHK(const CFE_MSG_CommandHeader_t *Msg)
{
    const char *CmdText = "HK Request";
    bool        Result  = true;

    /* Verify command packet length */
    Result = FM_IsValidCmdPktLength(&Msg->Msg, sizeof(FM_HousekeepingCmd_t), FM_HK_REQ_ERR_EID, CmdText);

    if (Result == true)
    {
        FM_ReleaseTablePointers();

        FM_AcquireTablePointers();

        /* Initialize housekeeping telemetry message */
        CFE_MSG_Init(&FM_GlobalData.HousekeepingPkt.TlmHeader.Msg, CFE_SB_ValueToMsgId(FM_HK_TLM_MID),
                     sizeof(FM_HousekeepingPkt_t));

        /* Report application command counters */
        FM_GlobalData.HousekeepingPkt.CommandCounter    = FM_GlobalData.CommandCounter;
        FM_GlobalData.HousekeepingPkt.CommandErrCounter = FM_GlobalData.CommandErrCounter;

        FM_GlobalData.HousekeepingPkt.NumOpenFiles = FM_GetOpenFilesData(NULL);

        /* Report child task command counters */
        FM_GlobalData.HousekeepingPkt.ChildCmdCounter     = FM_GlobalData.ChildCmdCounter;
        FM_GlobalData.HousekeepingPkt.ChildCmdErrCounter  = FM_GlobalData.ChildCmdErrCounter;
        FM_GlobalData.HousekeepingPkt.ChildCmdWarnCounter = FM_GlobalData.ChildCmdWarnCounter;

        FM_GlobalData.HousekeepingPkt.ChildQueueCount = FM_GlobalData.ChildQueueCount;

        /* Report current and previous commands executed by the child task */
        FM_GlobalData.HousekeepingPkt.ChildCurrentCC  = FM_GlobalData.ChildCurrentCC;
        FM_GlobalData.HousekeepingPkt.ChildPreviousCC = FM_GlobalData.ChildPreviousCC;

        CFE_SB_TimeStampMsg(&FM_GlobalData.HousekeepingPkt.TlmHeader.Msg);
        CFE_SB_TransmitMsg(&FM_GlobalData.HousekeepingPkt.TlmHeader.Msg, true);
    }

} // End of FM_ReportHK

/************************/
/*  End of File Comment */
/************************/
