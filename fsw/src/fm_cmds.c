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
 *  File Manager (FM) Application Ground Commands
 *
 *  Provides functions for the execution of the FM ground commands
 */

#include "cfe.h"
#include "fm_msg.h"
#include "fm_msgdefs.h"
#include "fm_msgids.h"
#include "fm_events.h"
#include "fm_app.h"
#include "fm_cmds.h"
#include "fm_cmd_utils.h"
#include "fm_perfids.h"
#include "fm_platform_cfg.h"
#include "fm_version.h"
#include "fm_verify.h"

#include <string.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- NOOP                                      */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_NoopCmd(const CFE_SB_Buffer_t *BufPtr)
{
    const char *CmdText       = "No-op";
    bool        CommandResult = false;

    /* Verify message length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_NoopCmd_t), FM_NOOP_PKT_ERR_EID, CmdText);

    /* Send command completion event (info) */
    if (CommandResult == true)
    {
        CFE_EVS_SendEvent(FM_NOOP_CMD_EID, CFE_EVS_EventType_INFORMATION, "%s command: FM version %d.%d.%d.%d", CmdText,
                          FM_MAJOR_VERSION, FM_MINOR_VERSION, FM_REVISION, FM_MISSION_REV);
    }

    return (CommandResult);

} /* End of FM_NoopCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Reset Counters                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_ResetCountersCmd(const CFE_SB_Buffer_t *BufPtr)
{
    const char *CmdText       = "Reset Counters";
    bool        CommandResult = false;

    /* Verify message length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_ResetCmd_t), FM_RESET_PKT_ERR_EID, CmdText);

    /* Reset command counters */
    if (CommandResult == true)
    {
        FM_GlobalData.CommandCounter    = 0;
        FM_GlobalData.CommandErrCounter = 0;

        FM_GlobalData.ChildCmdCounter     = 0;
        FM_GlobalData.ChildCmdErrCounter  = 0;
        FM_GlobalData.ChildCmdWarnCounter = 0;

        /* Send command completion event (debug) */
        CFE_EVS_SendEvent(FM_RESET_CMD_EID, CFE_EVS_EventType_DEBUG, "%s command", CmdText);
    }

    return (CommandResult);

} /* End of FM_ResetCountersCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Copy File                                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_CopyFileCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_CopyFileCmd_t *    CmdPtr        = (FM_CopyFileCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Copy File";
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_CopyFileCmd_t), FM_COPY_PKT_ERR_EID, CmdText);

    /* Verify that overwrite argument is valid */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyOverwrite(CmdPtr->Overwrite, FM_COPY_OVR_ERR_EID, CmdText);
    }

    /* Verify that source file exists and is not a directory */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileExists(CmdPtr->Source, sizeof(CmdPtr->Source), FM_COPY_SRC_BASE_EID, CmdText);
    }

    /* Verify target filename per the overwrite argument */
    if (CommandResult == true)
    {
        if (CmdPtr->Overwrite == 0)
        {
            CommandResult = FM_VerifyFileNoExist(CmdPtr->Target, sizeof(CmdPtr->Target), FM_COPY_TGT_BASE_EID, CmdText);
        }
        else
        {
            CommandResult = FM_VerifyFileNotOpen(CmdPtr->Target, sizeof(CmdPtr->Target), FM_COPY_TGT_BASE_EID, CmdText);
        }
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_COPY_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_COPY_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Source, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Target, CmdPtr->Target, OS_MAX_PATH_LEN - 1);
        CmdArgs->Target[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_CopyFileCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Move File                                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_MoveFileCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_MoveFileCmd_t *    CmdPtr        = (FM_MoveFileCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Move File";
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_MoveFileCmd_t), FM_MOVE_PKT_ERR_EID, CmdText);

    /* Verify that overwrite argument is valid */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyOverwrite(CmdPtr->Overwrite, FM_MOVE_OVR_ERR_EID, CmdText);
    }

    /* Verify that source file exists and not a directory */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileExists(CmdPtr->Source, sizeof(CmdPtr->Source), FM_MOVE_SRC_BASE_EID, CmdText);
    }

    /* Verify target filename per the overwrite argument */
    if (CommandResult == true)
    {
        if (CmdPtr->Overwrite == 0)
        {
            CommandResult = FM_VerifyFileNoExist(CmdPtr->Target, sizeof(CmdPtr->Target), FM_MOVE_TGT_BASE_EID, CmdText);
        }
        else
        {
            CommandResult = FM_VerifyFileNotOpen(CmdPtr->Target, sizeof(CmdPtr->Target), FM_MOVE_TGT_BASE_EID, CmdText);
        }
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_MOVE_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_MOVE_CC;

        strncpy(CmdArgs->Source1, CmdPtr->Source, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Target, CmdPtr->Target, OS_MAX_PATH_LEN - 1);
        CmdArgs->Target[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_MoveFileCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Rename File                               */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_RenameFileCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_RenameFileCmd_t *  CmdPtr        = (FM_RenameFileCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Rename File";
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_RenameFileCmd_t), FM_RENAME_PKT_ERR_EID, CmdText);

    /* Verify that source file exists and is not a directory */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileExists(CmdPtr->Source, sizeof(CmdPtr->Source), FM_RENAME_SRC_BASE_EID, CmdText);
    }

    /* Verify that target file does not exist */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileNoExist(CmdPtr->Target, sizeof(CmdPtr->Target), FM_RENAME_TGT_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_RENAME_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_RENAME_CC;

        strncpy(CmdArgs->Source1, CmdPtr->Source, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Target, CmdPtr->Target, OS_MAX_PATH_LEN - 1);
        CmdArgs->Target[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_RenameFileCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Delete File                               */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_DeleteFileCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_DeleteFileCmd_t *  CmdPtr        = (FM_DeleteFileCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Delete File";
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_DeleteFileCmd_t), FM_DELETE_PKT_ERR_EID, CmdText);

    /* Verify that file exists, is not a directory and is not open */
    if (CommandResult == true)
    {
        CommandResult =
            FM_VerifyFileClosed(CmdPtr->Filename, sizeof(CmdPtr->Filename), FM_DELETE_SRC_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_DELETE_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args - might be global or internal CC */
        CFE_MSG_GetFcnCode(&BufPtr->Msg, &CmdArgs->CommandCode);
        strncpy(CmdArgs->Source1, CmdPtr->Filename, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_DeleteFileCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Delete All Files                          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_DeleteAllFilesCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_DeleteAllCmd_t *   CmdPtr                      = (FM_DeleteAllCmd_t *)BufPtr;
    const char *          CmdText                     = "Delete All Files";
    char                  DirWithSep[OS_MAX_PATH_LEN] = "\0";
    FM_ChildQueueEntry_t *CmdArgs                     = NULL;
    bool                  CommandResult               = false;

    /* Verify message length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_DeleteAllCmd_t), FM_DELETE_ALL_PKT_ERR_EID, CmdText);

    /* Verify that the directory exists */
    if (CommandResult == true)
    {
        CommandResult =
            FM_VerifyDirExists(CmdPtr->Directory, sizeof(CmdPtr->Directory), FM_DELETE_ALL_SRC_BASE_EID, CmdText);
    }

    if (CommandResult == true)
    {
        /* Append a path separator to the end of the directory name */
        strncpy(DirWithSep, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        DirWithSep[OS_MAX_PATH_LEN - 1] = '\0';
        FM_AppendPathSep(DirWithSep, OS_MAX_PATH_LEN);

        /* Check for lower priority child task availability */
        CommandResult = FM_VerifyChildTask(FM_DELETE_ALL_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_DELETE_ALL_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Source2, DirWithSep, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source2[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_DeleteAllFilesCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Decompress File                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef FM_INCLUDE_DECOMPRESS

bool FM_DecompressFileCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_DecompressCmd_t *  CmdPtr        = (FM_DecompressCmd_t *)BufPtr;
    const char *          CmdText       = "Decompress File";
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_DecompressCmd_t), FM_DECOM_PKT_ERR_EID, CmdText);

    /* Verify that source file exists, is not a directory and is not open */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileClosed(CmdPtr->Source, sizeof(CmdPtr->Source), FM_DECOM_SRC_BASE_EID, CmdText);
    }

    /* Verify that target file does not exist */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileNoExist(CmdPtr->Target, sizeof(CmdPtr->Target), FM_DECOM_TGT_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_DECOM_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_DECOMPRESS_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Source, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';
        strncpy(CmdArgs->Target, CmdPtr->Target, OS_MAX_PATH_LEN - 1);
        CmdArgs->Target[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_DecompressFileCmd() */

#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Concatenate Files                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_ConcatFilesCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_ConcatCmd_t *      CmdPtr        = (FM_ConcatCmd_t *)BufPtr;
    const char *          CmdText       = "Concat Files";
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_ConcatCmd_t), FM_CONCAT_PKT_ERR_EID, CmdText);

    /* Verify that source file #1 exists, is not a directory and is not open */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileClosed(CmdPtr->Source1, sizeof(CmdPtr->Source1), FM_CONCAT_SRC1_BASE_EID, CmdText);
    }

    /* Verify that source file #2 exists, is not a directory and is not open */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileClosed(CmdPtr->Source2, sizeof(CmdPtr->Source2), FM_CONCAT_SRC2_BASE_EID, CmdText);
    }

    /* Verify that target file does not exist */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyFileNoExist(CmdPtr->Target, sizeof(CmdPtr->Target), FM_CONCAT_TGT_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_CONCAT_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_CONCAT_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Source1, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';
        strncpy(CmdArgs->Source2, CmdPtr->Source2, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source2[OS_MAX_PATH_LEN - 1] = '\0';
        strncpy(CmdArgs->Target, CmdPtr->Target, OS_MAX_PATH_LEN - 1);
        CmdArgs->Target[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_ConcatFilesCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Get File Info                             */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_GetFileInfoCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_GetFileInfoCmd_t * CmdPtr        = (FM_GetFileInfoCmd_t *)BufPtr;
    const char *          CmdText       = "Get File Info";
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    bool                  CommandResult = false;
    uint32                FilenameState = FM_NAME_IS_INVALID;

    /* Verify command packet length */
    CommandResult =
        FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_GetFileInfoCmd_t), FM_GET_FILE_INFO_PKT_ERR_EID, CmdText);

    /* Verify that the source name is valid for a file or directory */
    if (CommandResult == true)
    {
        FilenameState =
            FM_VerifyNameValid(CmdPtr->Filename, sizeof(CmdPtr->Filename), FM_GET_FILE_INFO_SRC_ERR_EID, CmdText);

        if (FilenameState == FM_NAME_IS_INVALID)
        {
            CommandResult = false;
        }
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_FILE_INFO_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_GET_FILE_INFO_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Filename, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        CmdArgs->FileInfoState = FilenameState;
        CmdArgs->FileInfoCRC   = CmdPtr->FileInfoCRC;

        /* Global data set during call to FM_VerifyNameValid */
        CmdArgs->FileInfoSize = FM_GlobalData.FileStatSize;
        CmdArgs->FileInfoTime = FM_GlobalData.FileStatTime;
        CmdArgs->Mode         = FM_GlobalData.FileStatMode;

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_GetFileInfoCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Get List of Open Files                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_GetOpenFilesCmd(const CFE_SB_Buffer_t *BufPtr)
{
    const char *CmdText       = "Get Open Files";
    bool        CommandResult = false;
    uint32      NumOpenFiles  = 0;

    /* Verify command packet length */
    CommandResult =
        FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_GetOpenFilesCmd_t), FM_GET_OPEN_FILES_PKT_ERR_EID, CmdText);
    if (CommandResult == true)
    {
        /* Initialize open files telemetry packet */
        CFE_MSG_Init(&FM_GlobalData.OpenFilesPkt.TlmHeader.Msg, CFE_SB_ValueToMsgId(FM_OPEN_FILES_TLM_MID),
                     sizeof(FM_OpenFilesPkt_t));

        /* Get list of open files and count */
        NumOpenFiles                            = FM_GetOpenFilesData(FM_GlobalData.OpenFilesPkt.OpenFilesList);
        FM_GlobalData.OpenFilesPkt.NumOpenFiles = NumOpenFiles;

        /* Timestamp and send open files telemetry packet */
        CFE_SB_TimeStampMsg(&FM_GlobalData.OpenFilesPkt.TlmHeader.Msg);
        CFE_SB_TransmitMsg(&FM_GlobalData.OpenFilesPkt.TlmHeader.Msg, true);

        /* Send command completion event (debug) */
        CFE_EVS_SendEvent(FM_GET_OPEN_FILES_CMD_EID, CFE_EVS_EventType_DEBUG, "%s command", CmdText);
    }

    return (CommandResult);

} /* End of FM_GetOpenFilesCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Create Directory                          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_CreateDirectoryCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_CreateDirCmd_t *   CmdPtr        = (FM_CreateDirCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Create Directory";
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_CreateDirCmd_t), FM_CREATE_DIR_PKT_ERR_EID, CmdText);

    /* Verify that the directory name is not already in use */
    if (CommandResult == true)
    {
        CommandResult =
            FM_VerifyDirNoExist(CmdPtr->Directory, sizeof(CmdPtr->Directory), FM_CREATE_DIR_SRC_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_CREATE_DIR_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_CREATE_DIR_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_CreateDirectoryCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Delete Directory                          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_DeleteDirectoryCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_DeleteDirCmd_t *   CmdPtr        = (FM_DeleteDirCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Delete Directory";
    bool                  CommandResult = false;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_DeleteDirCmd_t), FM_DELETE_DIR_PKT_ERR_EID, CmdText);

    /* Verify that the directory exists */
    if (CommandResult == true)
    {
        CommandResult =
            FM_VerifyDirExists(CmdPtr->Directory, sizeof(CmdPtr->Directory), FM_DELETE_DIR_SRC_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_DELETE_DIR_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_DELETE_DIR_CC;
        strncpy(CmdArgs->Source1, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_DeleteDirectoryCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Get List of Directory Entries (to file)   */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_GetDirListFileCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_GetDirFileCmd_t *  CmdPtr                      = (FM_GetDirFileCmd_t *)BufPtr;
    const char *          CmdText                     = "Directory List to File";
    char                  DirWithSep[OS_MAX_PATH_LEN] = "\0";
    char                  Filename[OS_MAX_PATH_LEN]   = "\0";
    FM_ChildQueueEntry_t *CmdArgs                     = NULL;
    bool                  CommandResult               = false;

    /* Verify command packet length */
    CommandResult =
        FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_GetDirFileCmd_t), FM_GET_DIR_FILE_PKT_ERR_EID, CmdText);

    /* Verify that source directory exists */
    if (CommandResult == true)
    {
        CommandResult =
            FM_VerifyDirExists(CmdPtr->Directory, sizeof(CmdPtr->Directory), FM_GET_DIR_FILE_SRC_BASE_EID, CmdText);
    }

    /* Verify that target file is not already open */
    if (CommandResult == true)
    {
        /* Use default filename if not specified in the command */
        if (CmdPtr->Filename[0] == '\0')
        {
            strncpy(Filename, FM_DIR_LIST_FILE_DEFNAME, sizeof(Filename) - 1);
            Filename[sizeof(Filename) - 1] = '\0';
        }
        else
        {
            memcpy(Filename, CmdPtr->Filename, sizeof(Filename));
        }

        /* Note: it is OK for this file to overwrite a previous version of the file */
        CommandResult = FM_VerifyFileNotOpen(Filename, sizeof(Filename), FM_GET_DIR_FILE_TGT_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_GET_DIR_FILE_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Append a path separator to the end of the directory name */
        strncpy(DirWithSep, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        DirWithSep[OS_MAX_PATH_LEN - 1] = '\0';
        FM_AppendPathSep(DirWithSep, OS_MAX_PATH_LEN);

        /* Set handshake queue command args */
        CmdArgs->CommandCode     = FM_GET_DIR_FILE_CC;
        CmdArgs->GetSizeTimeMode = CmdPtr->GetSizeTimeMode;
        strncpy(CmdArgs->Source1, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Source2, DirWithSep, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source2[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Target, Filename, OS_MAX_PATH_LEN - 1);
        CmdArgs->Target[OS_MAX_PATH_LEN - 1] = '\0';

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_GetDirListFileCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Get List of Directory Entries (to pkt)    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_GetDirListPktCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_GetDirPktCmd_t *   CmdPtr                      = (FM_GetDirPktCmd_t *)BufPtr;
    const char *          CmdText                     = "Directory List to Packet";
    char                  DirWithSep[OS_MAX_PATH_LEN] = "\0";
    FM_ChildQueueEntry_t *CmdArgs                     = NULL;
    bool                  CommandResult               = false;

    /* Verify command packet length */
    CommandResult =
        FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_GetDirPktCmd_t), FM_GET_DIR_PKT_PKT_ERR_EID, CmdText);

    /* Verify that source directory exists */
    if (CommandResult == true)
    {
        CommandResult =
            FM_VerifyDirExists(CmdPtr->Directory, sizeof(CmdPtr->Directory), FM_GET_DIR_PKT_SRC_BASE_EID, CmdText);
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_GET_DIR_PKT_CHILD_BASE_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];

        /* Append a path separator to the end of the directory name */
        strncpy(DirWithSep, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        DirWithSep[OS_MAX_PATH_LEN - 1] = '\0';
        FM_AppendPathSep(DirWithSep, OS_MAX_PATH_LEN);

        /* Set handshake queue command args */
        CmdArgs->CommandCode     = FM_GET_DIR_PKT_CC;
        CmdArgs->GetSizeTimeMode = CmdPtr->GetSizeTimeMode;
        strncpy(CmdArgs->Source1, CmdPtr->Directory, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';

        strncpy(CmdArgs->Source2, DirWithSep, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source2[OS_MAX_PATH_LEN - 1] = '\0';
        CmdArgs->DirListOffset                = CmdPtr->DirListOffset;

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_GetDirListPktCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Get File System Free Space                */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_GetFreeSpaceCmd(const CFE_SB_Buffer_t *BufPtr)
{
    const char * CmdText       = "Get Free Space";
    bool         CommandResult = false;
    int32        OS_Status     = OS_SUCCESS;
    uint32       i             = 0;
    OS_statvfs_t FileStats;

    memset(&FileStats, 0, sizeof(FileStats));

    /* Verify command packet length */
    CommandResult =
        FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_GetFreeSpaceCmd_t), FM_GET_FREE_SPACE_PKT_ERR_EID, CmdText);
    if (CommandResult == true)
    {
        /* Verify that we have a pointer to the file system table data */
        if (FM_GlobalData.FreeSpaceTablePtr == (FM_FreeSpaceTable_t *)NULL)
        {
            CommandResult = false;

            CFE_EVS_SendEvent(FM_GET_FREE_SPACE_TBL_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s error: file system free space table is not loaded", CmdText);
        }
        else
        {
            /* Initialize the file system free space telemetry packet */
            CFE_MSG_Init(&FM_GlobalData.FreeSpacePkt.TlmHeader.Msg, CFE_SB_ValueToMsgId(FM_FREE_SPACE_TLM_MID),
                         sizeof(FM_FreeSpacePkt_t));

            /* Process enabled file system table entries */
            for (i = 0; i < FM_TABLE_ENTRY_COUNT; i++)
            {
                if (FM_GlobalData.FreeSpaceTablePtr->FileSys[i].State == FM_TABLE_ENTRY_ENABLED)
                {
                    /* Get file system name */
                    strncpy(FM_GlobalData.FreeSpacePkt.FileSys[i].Name,
                            FM_GlobalData.FreeSpaceTablePtr->FileSys[i].Name, OS_MAX_PATH_LEN - 1);
                    FM_GlobalData.FreeSpacePkt.FileSys[i].Name[OS_MAX_PATH_LEN - 1] = '\0';

                    /* Get file system free space */
                    OS_Status = OS_FileSysStatVolume(FM_GlobalData.FreeSpacePkt.FileSys[i].Name, &FileStats);
                    if (OS_Status == OS_SUCCESS)
                    {
                        FM_GlobalData.FreeSpacePkt.FileSys[i].FreeSpace = FileStats.blocks_free;
                    }
                    else
                    {
                        CFE_EVS_SendEvent(FM_OS_SYS_STAT_ERR_EID, CFE_EVS_EventType_ERROR,
                                          "Could not get file system free space for %s. Returned 0x%08X",
                                          FM_GlobalData.FreeSpacePkt.FileSys[i].Name, OS_Status);

                        FM_GlobalData.FreeSpacePkt.FileSys[i].FreeSpace = 0;
                    }
                }
            }

            /* Timestamp and send file system free space telemetry packet */
            CFE_SB_TimeStampMsg(&FM_GlobalData.FreeSpacePkt.TlmHeader.Msg);
            CFE_SB_TransmitMsg(&FM_GlobalData.FreeSpacePkt.TlmHeader.Msg, true);

            /* Send command completion event (debug) */
            CFE_EVS_SendEvent(FM_GET_FREE_SPACE_CMD_EID, CFE_EVS_EventType_DEBUG, "%s command", CmdText);
        }
    }

    return (CommandResult);

} /* End of FM_GetFreeSpaceCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Set Table Entry Enable/Disable State      */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_SetTableStateCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_SetTableStateCmd_t *CmdPtr        = (FM_SetTableStateCmd_t *)BufPtr;
    const char *           CmdText       = "Set Table State";
    bool                   CommandResult = false;

    /* Verify command packet length */
    CommandResult =
        FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_SetTableStateCmd_t), FM_SET_TABLE_STATE_PKT_ERR_EID, CmdText);
    if (CommandResult == true)
    {
        if (FM_GlobalData.FreeSpaceTablePtr == (FM_FreeSpaceTable_t *)NULL)
        {
            /* File system table has not been loaded */
            CommandResult = false;

            CFE_EVS_SendEvent(FM_SET_TABLE_STATE_TBL_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s error: file system free space table is not loaded", CmdText);
        }
        else if (CmdPtr->TableEntryIndex >= FM_TABLE_ENTRY_COUNT)
        {
            /* Table index argument is out of range */
            CommandResult = false;

            CFE_EVS_SendEvent(FM_SET_TABLE_STATE_ARG_IDX_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s error: invalid command argument: index = %d", CmdText, (int)CmdPtr->TableEntryIndex);
        }
        else if ((CmdPtr->TableEntryState != FM_TABLE_ENTRY_ENABLED) &&
                 (CmdPtr->TableEntryState != FM_TABLE_ENTRY_DISABLED))
        {
            /* State argument must be either enabled or disabled */
            CommandResult = false;

            CFE_EVS_SendEvent(FM_SET_TABLE_STATE_ARG_STATE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s error: invalid command argument: state = %d", CmdText, (int)CmdPtr->TableEntryState);
        }
        else if (FM_GlobalData.FreeSpaceTablePtr->FileSys[CmdPtr->TableEntryIndex].State == FM_TABLE_ENTRY_UNUSED)
        {
            /* Current table entry state must not be unused */
            CommandResult = false;

            CFE_EVS_SendEvent(FM_SET_TABLE_STATE_UNUSED_ERR_EID, CFE_EVS_EventType_ERROR,
                              "%s error: cannot modify unused table entry: index = %d", CmdText,
                              (int)CmdPtr->TableEntryIndex);
        }
        else
        {
            /* Update the table entry state as commanded */
            FM_GlobalData.FreeSpaceTablePtr->FileSys[CmdPtr->TableEntryIndex].State = CmdPtr->TableEntryState;

            /* Notify cFE that we have modified the table data */
            CFE_TBL_Modified(FM_GlobalData.FreeSpaceTableHandle);

            /* Send command completion event (info) */
            CFE_EVS_SendEvent(FM_SET_TABLE_STATE_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "%s command: index = %d, state = %d", CmdText, (int)CmdPtr->TableEntryIndex,
                              (int)CmdPtr->TableEntryState);
        }
    }

    return (CommandResult);

} /* End of FM_SetTableStateCmd() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FM command handler -- Set Permissions for a file                */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool FM_SetPermissionsCmd(const CFE_SB_Buffer_t *BufPtr)
{
    FM_SetPermCmd_t *     CmdPtr        = (FM_SetPermCmd_t *)BufPtr;
    FM_ChildQueueEntry_t *CmdArgs       = NULL;
    const char *          CmdText       = "Set Permissions";
    bool                  CommandResult = false;
    bool                  FilenameState = FM_NAME_IS_INVALID;

    /* Verify command packet length */
    CommandResult = FM_IsValidCmdPktLength(&BufPtr->Msg, sizeof(FM_SetPermCmd_t), FM_SET_PERM_ERR_EID, CmdText);

    if (CommandResult == true)
    {
        FilenameState = FM_VerifyNameValid(CmdPtr->FileName, sizeof(CmdPtr->FileName), 0, CmdText);

        if (FilenameState == FM_NAME_IS_INVALID)
        {
            CommandResult = false;
        }
    }

    /* Check for lower priority child task availability */
    if (CommandResult == true)
    {
        CommandResult = FM_VerifyChildTask(FM_SET_PERM_ERR_EID, CmdText);
    }

    /* Prepare command for child task execution */
    if (CommandResult == true)
    {
        CmdArgs = &FM_GlobalData.ChildQueue[FM_GlobalData.ChildWriteIndex];
        /* Set handshake queue command args */
        CmdArgs->CommandCode = FM_SET_FILE_PERM_CC;
        strncpy(CmdArgs->Source1, CmdPtr->FileName, OS_MAX_PATH_LEN - 1);
        CmdArgs->Source1[OS_MAX_PATH_LEN - 1] = '\0';
        CmdArgs->Mode                         = CmdPtr->Mode;

        /* Invoke lower priority child task */
        FM_InvokeChildTask();
    }

    return (CommandResult);

} /* End of FM_SetPermissionsCmd() */

/************************/
/*  End of File Comment */
/************************/
