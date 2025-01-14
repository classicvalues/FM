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
 *  File Manager (FM) Child task (low priority command handler)
 */

#include "cfe.h"
#include "fm_platform_cfg.h"
#include "fm_msg.h"
#include "fm_tbl.h"
#include "fm_events.h"

#include <string.h>

/*
 * UT Assert
 */
#include "fm_test_utils.h"

/*
 * UT includes
 */
#include "uttest.h"
#include "utassert.h"
#include "utstubs.h"

/*
**********************************************************************************
**          TEST CASE FUNCTIONS
**********************************************************************************
*/

uint8 call_count_CFE_EVS_SendEvent;

/************************/
/* Table Init Tests     */
/************************/
void Test_FM_TableInit_Success(void)
{
    int32 Result;

    UT_SetDefaultReturnValue(UT_KEY(CFE_TBL_Register), CFE_SUCCESS);

    Result = FM_TableInit();

    /* Assert */
    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));
    UtAssert_INT32_EQ(Result, CFE_SUCCESS);
    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 0);
}

void Test_FM_TableInit_Fail(void)
{
    int32 Result;

    UT_SetDefaultReturnValue(UT_KEY(CFE_TBL_Register), -1);

    Result = FM_TableInit();

    /* Assert */
    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));
    UtAssert_INT32_EQ(Result, -1);
    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 0);
}

/************************/
/* Table Init Tests     */
/************************/

void Test_FM_ValidateTable_Success(void)
{
    FM_FreeSpaceTable_t DummyTable;

    for (int i = 0; i < FM_TABLE_ENTRY_COUNT; i++)
    {
        if ((i % 2) == 0)
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_DISABLED;
        }
        else
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_ENABLED;
        }
        snprintf(DummyTable.FileSys[i].Name, OS_MAX_PATH_LEN, "Test");
    }

    int32 strCmpResult;
    char  ExpectedEventString[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    snprintf(ExpectedEventString, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify results: good entries = %%d, bad = %%d, unused = %%d");

    int32 Result = FM_ValidateTable(&DummyTable);

    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));

    /* Assert */
    UtAssert_INT32_EQ(Result, CFE_SUCCESS);

    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 1);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventID, FM_TABLE_VERIFY_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventType, CFE_EVS_EventType_INFORMATION);

    strCmpResult = strncmp(ExpectedEventString, context_CFE_EVS_SendEvent[0].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[0].Spec);
}

void Test_FM_ValidateTable_NullTable(void)
{
    int32 strCmpResult;
    char  ExpectedEventString[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    snprintf(ExpectedEventString, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify error - null pointer detected");

    int32 Result = FM_ValidateTable(NULL);

    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));

    /* Assert */
    UtAssert_INT32_EQ(Result, FM_TABLE_VALIDATION_ERR);

    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 1);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventID, FM_TABLE_VERIFY_NULL_PTR_ERR_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventType, CFE_EVS_EventType_ERROR);

    strCmpResult = strncmp(ExpectedEventString, context_CFE_EVS_SendEvent[0].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[0].Spec);
}

void Test_FM_ValidateTable_UnusedEntry(void)
{
    FM_FreeSpaceTable_t DummyTable;

    for (int i = 0; i < FM_TABLE_ENTRY_COUNT; i++)
    {
        if ((i % 2) == 0)
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_DISABLED;
        }
        else
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_ENABLED;
        }
        snprintf(DummyTable.FileSys[i].Name, OS_MAX_PATH_LEN, "Test");
    }
    DummyTable.FileSys[0].State = FM_TABLE_ENTRY_UNUSED;

    int32 strCmpResult;
    char  ExpectedEventString[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    snprintf(ExpectedEventString, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify results: good entries = %%d, bad = %%d, unused = %%d");

    int32 Result = FM_ValidateTable(&DummyTable);

    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));

    /* Assert */
    UtAssert_INT32_EQ(Result, CFE_SUCCESS);

    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 1);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventID, FM_TABLE_VERIFY_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventType, CFE_EVS_EventType_INFORMATION);

    strCmpResult = strncmp(ExpectedEventString, context_CFE_EVS_SendEvent[0].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[0].Spec);
}

void Test_FM_ValidateTable_BadEntryState(void)
{
    FM_FreeSpaceTable_t DummyTable;

    for (int i = 0; i < FM_TABLE_ENTRY_COUNT; i++)
    {
        if ((i % 2) == 0)
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_DISABLED;
        }
        else
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_ENABLED;
        }

        snprintf(DummyTable.FileSys[i].Name, OS_MAX_PATH_LEN, "Test");
    }

    DummyTable.FileSys[0].State = 99;
    DummyTable.FileSys[1].State = 99;

    int32 strCmpResult;
    char  ExpectedEventString[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    char  ExpectedEventString2[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    snprintf(ExpectedEventString, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Table verify error: index = %%d, invalid state = %%d");

    snprintf(ExpectedEventString2, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify results: good entries = %%d, bad = %%d, unused = %%d");

    int32 Result = FM_ValidateTable(&DummyTable);

    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));

    /* Assert */
    UtAssert_INT32_EQ(Result, FM_TABLE_VALIDATION_ERR);

    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 2);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventID, FM_TABLE_VERIFY_BAD_STATE_ERR_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventType, CFE_EVS_EventType_ERROR);

    strCmpResult = strncmp(ExpectedEventString, context_CFE_EVS_SendEvent[0].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[0].Spec);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[1].EventID, FM_TABLE_VERIFY_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[1].EventType, CFE_EVS_EventType_INFORMATION);

    strCmpResult = strncmp(ExpectedEventString2, context_CFE_EVS_SendEvent[1].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[1].Spec);
}

void Test_FM_ValidateTable_EmptyName(void)
{
    FM_FreeSpaceTable_t DummyTable;

    for (int i = 0; i < FM_TABLE_ENTRY_COUNT; i++)
    {
        if ((i % 2) == 0)
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_DISABLED;
        }
        else
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_ENABLED;
        }
        DummyTable.FileSys[i].Name[0] = '\0';
    }

    int32 strCmpResult;
    char  ExpectedEventString[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    char  ExpectedEventString2[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    snprintf(ExpectedEventString, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify error: index = %%d, empty name string");

    snprintf(ExpectedEventString2, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify results: good entries = %%d, bad = %%d, unused = %%d");

    int32 Result = FM_ValidateTable(&DummyTable);

    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));

    /* Assert */
    UtAssert_INT32_EQ(Result, FM_TABLE_VALIDATION_ERR);

    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 2);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventID, FM_TABLE_VERIFY_EMPTY_ERR_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventType, CFE_EVS_EventType_ERROR);

    strCmpResult = strncmp(ExpectedEventString, context_CFE_EVS_SendEvent[0].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[0].Spec);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[1].EventID, FM_TABLE_VERIFY_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[1].EventType, CFE_EVS_EventType_INFORMATION);

    strCmpResult = strncmp(ExpectedEventString2, context_CFE_EVS_SendEvent[1].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[1].Spec);
}

void Test_FM_ValidateTable_NameTooLong(void)
{
    FM_FreeSpaceTable_t DummyTable;

    for (int i = 0; i < FM_TABLE_ENTRY_COUNT; i++)
    {
        if ((i % 2) == 0)
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_DISABLED;
        }
        else
        {
            DummyTable.FileSys[i].State = FM_TABLE_ENTRY_ENABLED;
        }

        snprintf(DummyTable.FileSys[i].Name, OS_MAX_PATH_LEN, "Test");
    }

    memset(DummyTable.FileSys[0].Name, 'A', sizeof(DummyTable.FileSys[0].Name));
    memset(DummyTable.FileSys[1].Name, 'A', sizeof(DummyTable.FileSys[1].Name));

    int32 strCmpResult;
    char  ExpectedEventString[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    char  ExpectedEventString2[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
    snprintf(ExpectedEventString, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify error: index = %%d, name too long");

    snprintf(ExpectedEventString2, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH,
             "Free Space Table verify results: good entries = %%d, bad = %%d, unused = %%d");

    int32 Result = FM_ValidateTable(&DummyTable);

    call_count_CFE_EVS_SendEvent = UT_GetStubCount(UT_KEY(CFE_EVS_SendEvent));

    /* Assert */
    UtAssert_INT32_EQ(Result, FM_TABLE_VALIDATION_ERR);

    UtAssert_INT32_EQ(call_count_CFE_EVS_SendEvent, 2);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventID, FM_TABLE_VERIFY_TOOLONG_ERR_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[0].EventType, CFE_EVS_EventType_ERROR);

    strCmpResult = strncmp(ExpectedEventString, context_CFE_EVS_SendEvent[0].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[0].Spec);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[1].EventID, FM_TABLE_VERIFY_EID);

    UtAssert_INT32_EQ(context_CFE_EVS_SendEvent[1].EventType, CFE_EVS_EventType_INFORMATION);

    strCmpResult = strncmp(ExpectedEventString2, context_CFE_EVS_SendEvent[1].Spec, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

    UtAssert_True(strCmpResult == 0, "Event string matched expected result, '%s'", context_CFE_EVS_SendEvent[1].Spec);
}

void Test_FM_AcquireTablePointers_Success(void)
{
    FM_FreeSpaceTable_t DummyTable;

    UT_SetDefaultReturnValue(UT_KEY(CFE_TBL_GetAddress), CFE_SUCCESS);

    FM_GlobalData.FreeSpaceTablePtr = &DummyTable;

    FM_AcquireTablePointers();

    UtAssert_True(FM_GlobalData.FreeSpaceTablePtr != NULL, "FM_GlobalData.FreeSpaceTablePtr != NULL");
}

void Test_FM_AcquireTablePointers_Fail(void)
{
    FM_FreeSpaceTable_t DummyTable;

    UT_SetDefaultReturnValue(UT_KEY(CFE_TBL_GetAddress), CFE_TBL_ERR_NEVER_LOADED);

    FM_GlobalData.FreeSpaceTablePtr = &DummyTable;

    FM_AcquireTablePointers();

    UtAssert_True(FM_GlobalData.FreeSpaceTablePtr == NULL, "FM_GlobalData.FreeSpaceTablePtr == NULL");
}

void Test_FM_ReleaseTablePointers(void)
{
    FM_FreeSpaceTable_t DummyTable;

    FM_GlobalData.FreeSpaceTablePtr = &DummyTable;

    FM_ReleaseTablePointers();

    UtAssert_True(FM_GlobalData.FreeSpaceTablePtr == NULL, "FM_GlobalData.FreeSpaceTablePtr == NULL");
}

/*
 * Register the test cases to execute with the unit test tool
 */
void UtTest_Setup(void)
{
    UtTest_Add(Test_FM_TableInit_Success, FM_Test_Setup, FM_Test_Teardown, "Test_FM_TableInit_Success");

    UtTest_Add(Test_FM_TableInit_Fail, FM_Test_Setup, FM_Test_Teardown, "Test_FM_TableInit_Fail");

    UtTest_Add(Test_FM_ValidateTable_Success, FM_Test_Setup, FM_Test_Teardown, "Test_FM_ValidateTable_Success");

    UtTest_Add(Test_FM_ValidateTable_NullTable, FM_Test_Setup, FM_Test_Teardown, "Test_FM_ValidateTable_NullTable");

    UtTest_Add(Test_FM_ValidateTable_UnusedEntry, FM_Test_Setup, FM_Test_Teardown, "Test_FM_ValidateTable_UnusedEntry");

    UtTest_Add(Test_FM_ValidateTable_BadEntryState, FM_Test_Setup, FM_Test_Teardown,
               "Test_FM_ValidateTable_BadEntryState");

    UtTest_Add(Test_FM_ValidateTable_EmptyName, FM_Test_Setup, FM_Test_Teardown, "Test_FM_ValidateTable_EmptyName");

    UtTest_Add(Test_FM_ValidateTable_NameTooLong, FM_Test_Setup, FM_Test_Teardown, "Test_FM_ValidateTable_NameTooLong");

    UtTest_Add(Test_FM_AcquireTablePointers_Success, FM_Test_Setup, FM_Test_Teardown,
               "Test_FM_AcquireTablePointers_Success");

    UtTest_Add(Test_FM_AcquireTablePointers_Fail, FM_Test_Setup, FM_Test_Teardown, "Test_FM_AcquireTablePointers_Fail");

    UtTest_Add(Test_FM_ReleaseTablePointers, FM_Test_Setup, FM_Test_Teardown, "Test_FM_ReleaseTablePointers");
}
