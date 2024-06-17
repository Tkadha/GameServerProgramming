#define _CRT_SECURE_NO_WARNINGS

#include <windows.h> 
#include <iostream>
#include <stdio.h>  
#include <mutex>
// #define UNICODE

#define NAME_LEN 50  

#include "DataBase.h"

std::mutex db_mutex;


void show_error() {
    printf("error\n");
}

void DisplayError(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
    setlocale(LC_ALL, "korean");
    std::wcout.imbue(std::locale("korean"));
    SQLSMALLINT iRec = 0;
    SQLINTEGER  iError;
    WCHAR wszMessage[1000];
    WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
    if (RetCode == SQL_INVALID_HANDLE) {
        fwprintf(stderr, L"Invalid handle!\n");
        return;
    }

    while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
        (SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
        // Hide data truncated..
        if (wcsncmp(wszState, L"01004", 5)) {
            fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
        }
    }
}


CDataBase::~CDataBase()
{
    CloseDB();
}

bool CDataBase::InitializeDB()
{
    std::lock_guard<std::mutex> lock(db_mutex);

    retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

    retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

    retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

    SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

    retcode = SQLConnect(hdbc, (SQLWCHAR*)L"2020182042", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
        DisplayError(hdbc, SQL_HANDLE_DBC, retcode);
        return false;
    }

    retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) return false;

    return true;
}

void CDataBase::CloseDB()
{
    std::lock_guard<std::mutex> lock(db_mutex); 

    if (hstmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    }
    if (hdbc) {
        SQLDisconnect(hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    }
    if (henv) {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
    }
}

bool CDataBase::UpdateUserData(char* userId, Data data)
{
    db_mutex.lock();

    wchar_t wname[20];
    mbstowcs(wname, userId, strlen(userId) + 1);
    SQLWCHAR sqlUpdateQuery[256]{};
    swprintf(sqlUpdateQuery, sizeof(sqlUpdateQuery) / sizeof(SQLWCHAR),
        L"EXEC storeUserData '%ls', %d, %d, %d, %d, %d, %d",
        wname, data.x, data.y, data.hp, data.maxhp, data.level, data.exp);

    retcode = SQLExecDirect(hstmt, sqlUpdateQuery, SQL_NTS);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        db_mutex.unlock();
        return true;
    }
    else {
        DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
        db_mutex.unlock();
        return false;
    }
}

bool CDataBase::FetchUserData()
{
    db_mutex.lock();

    SQLWCHAR user_id[NAME_LEN];
    SQLSMALLINT user_x, user_y, user_hp, user_maxhp, user_level, user_exp;
    SQLLEN cbId = 0, cbuserX = 0, cbuserY = 0, cbuserHp = 0, cbuserMaxhp = 0, cbuserLevel = 0, cbuserExp = 0;

    retcode = SQLExecDirect(hstmt, 
        (SQLWCHAR*)L"SELECT user_id, user_x, user_y, user_hp, user_max_hp, user_level, user_exp FROM dbo.user_data ORDER BY 1", SQL_NTS);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

        retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, user_id, 100, &cbId);
        retcode = SQLBindCol(hstmt, 2, SQL_C_SHORT, &user_x, 10, &cbuserX);
        retcode = SQLBindCol(hstmt, 3, SQL_C_SHORT, &user_y, 10, &cbuserY);
        retcode = SQLBindCol(hstmt, 4, SQL_C_SHORT, &user_hp, 10, &cbuserHp);
        retcode = SQLBindCol(hstmt, 5, SQL_C_SHORT, &user_maxhp, 10, &cbuserMaxhp);
        retcode = SQLBindCol(hstmt, 6, SQL_C_SHORT, &user_level, 10, &cbuserLevel);
        retcode = SQLBindCol(hstmt, 7, SQL_C_SHORT, &user_exp, 10, &cbuserExp);

        for (int i = 0;; i++) {
            retcode = SQLFetch(hstmt);
            if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
                DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                fwprintf(stdout, L"%d: %ls %d %d %d %d %d %d\n", i + 1, user_id, user_x, user_y, user_hp, user_maxhp, user_level, user_exp);
            }
            else {
                SQLFreeStmt(hstmt, SQL_UNBIND);
                SQLCloseCursor(hstmt);
                break;
            }
        }
        db_mutex.unlock();
        return true;
    }
    else {
        DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
        db_mutex.unlock();
        return false;
    }
}

bool CDataBase::FindUserData(char* userId)
{
    db_mutex.lock();
    wchar_t wname[20];
    mbstowcs(wname, userId, strlen(userId) + 1);
    SQLWCHAR sqlQuery[256]{};
    swprintf(sqlQuery, sizeof(sqlQuery) / sizeof(SQLWCHAR), L"SELECT 1 FROM dbo.user_data WHERE user_id = '%ls'", wname);

    retcode = SQLExecDirect(hstmt, sqlQuery, SQL_NTS);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        retcode = SQLFetch(hstmt);
        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            SQLCloseCursor(hstmt);
            db_mutex.unlock();
            return true;
        }
        else {
            SQLCloseCursor(hstmt);
            db_mutex.unlock();
            return false;
        }
    }
    else {
        DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
        db_mutex.unlock();
        return false;
    }
}

Data CDataBase::GetUserData(char* userId)
{
    db_mutex.lock();

    Data data{ -1,-1,-1,-1,-1,-1 };
    SQLWCHAR user_id[NAME_LEN];
    SQLSMALLINT user_x, user_y, user_hp, user_maxhp, user_level, user_exp;
    SQLLEN cbId = 0, cbuserX = 0, cbuserY = 0, cbuserHp = 0, cbuserMaxhp = 0, cbuserLevel = 0, cbuserExp = 0;

    wchar_t wname[20];
    mbstowcs(wname, userId, strlen(userId) + 1);
    SQLWCHAR sqlQuery[256]{};
    swprintf(sqlQuery, sizeof(sqlQuery) / sizeof(SQLWCHAR),
        L"SELECT user_id, user_x, user_y, user_hp, user_max_hp, user_level, user_exp FROM dbo.user_data WHERE user_id = '%ls'", wname);
    retcode = SQLExecDirect(hstmt, sqlQuery, SQL_NTS);

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

        retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, user_id, 100, &cbId);
        retcode = SQLBindCol(hstmt, 2, SQL_C_SHORT, &user_x, 10, &cbuserX);
        retcode = SQLBindCol(hstmt, 3, SQL_C_SHORT, &user_y, 10, &cbuserY);
        retcode = SQLBindCol(hstmt, 4, SQL_C_SHORT, &user_hp, 10, &cbuserHp);
        retcode = SQLBindCol(hstmt, 5, SQL_C_SHORT, &user_maxhp, 10, &cbuserMaxhp);
        retcode = SQLBindCol(hstmt, 6, SQL_C_SHORT, &user_level, 10, &cbuserLevel);
        retcode = SQLBindCol(hstmt, 7, SQL_C_SHORT, &user_exp, 10, &cbuserExp);

        for (int i = 0;; i++) {
            retcode = SQLFetch(hstmt);
            if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
                DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                data.x = user_x;
                data.y = user_y;
                data.hp = user_hp;
                data.maxhp = user_maxhp;
                data.level = user_level;
                data.exp = user_exp;
            }
            else {
                SQLFreeStmt(hstmt, SQL_UNBIND);
                SQLCloseCursor(hstmt);
                break;
            }
        }
    }
    else {
        DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
    }
    db_mutex.unlock();
    return data;
}

bool CDataBase::CreateUserData(char* userId)
{
    db_mutex.lock();

    wchar_t wname[20];
    mbstowcs(wname, userId, strlen(userId) + 1);

    SQLWCHAR sqlInsertQuery[256]{};
    swprintf(sqlInsertQuery, sizeof(sqlInsertQuery) / sizeof(SQLWCHAR),
        L"INSERT INTO dbo.user_data (user_id, user_x, user_y, user_hp, user_max_hp, user_level, user_exp) VALUES ('%ls', %d, %d, %d, %d, %d, %d)",
        wname, 1, 1, 10, 10, 1, 0);

    retcode = SQLExecDirect(hstmt, sqlInsertQuery, SQL_NTS);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        db_mutex.unlock();
        return true;
    }
    else {
        DisplayError(hstmt, SQL_HANDLE_STMT, retcode);
        db_mutex.unlock();
        return false;
    }
}
