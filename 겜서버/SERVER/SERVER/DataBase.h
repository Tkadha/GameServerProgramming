#pragma once
#include <sqlext.h>  


struct Data {
    short x, y;
    int hp;
    int maxhp;
    int level;
    int exp;
};
class CDataBase
{
public:
    ~CDataBase();
    bool InitializeDB();
    void CloseDB();
    bool UpdateUserData(char* userId, Data data);
    bool FetchUserData();
    bool FindUserData(char* userId);
    Data GetUserData(char* userId);
    bool CreateUserData(char* userId);
private:
    SQLHENV henv = nullptr;
    SQLHDBC hdbc = nullptr;
    SQLHSTMT hstmt = nullptr;
    SQLRETURN retcode;
};
