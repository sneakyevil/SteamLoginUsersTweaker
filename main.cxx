#include <iostream>
#include <conio.h>
#include <fstream>
#include <Windows.h>

#include "vdf_parser.hpp"

#define COLOR_PRINTF_DEFAULTCOL     15
#define COLOR_PRINTF_CHAR           '^'
void ColorPrintf(const char* p_Format, ...)
{
    va_list m_VAList;
    va_start(m_VAList, p_Format);

    int m_BufferLen = _scprintf(p_Format, m_VAList) + 1;
    char* m_Buffer = new char[m_BufferLen];
    sprintf_s(m_Buffer, m_BufferLen, p_Format, m_VAList);

    va_end(m_VAList);

    static HANDLE m_STDHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(m_STDHandle, COLOR_PRINTF_DEFAULTCOL); // Reset to default color

    char* m_StringCur = m_Buffer;
    
    while (1)
    {
        char* m_ColorChar = strchr(m_StringCur, COLOR_PRINTF_CHAR);
        if (!m_ColorChar)
        {
            if (*m_StringCur != '\0')
                printf(m_StringCur);

            break;
        }

        char m_ColorHexChar = m_ColorChar[1];
        uint8_t m_ColorHex = 0;
        if (m_ColorHexChar >= 'A' && m_ColorHexChar <= 'F')
            m_ColorHex = 0xA + (m_ColorChar[1] - 'A');
        else if (m_ColorHexChar >= '0' && m_ColorHexChar <= '9')
            m_ColorHex = 0x0 + (m_ColorChar[1] - '0');
        
        memset(m_ColorChar, 0, 2);

        if (m_Buffer != m_ColorChar)
            printf(m_StringCur);

        SetConsoleTextAttribute(m_STDHandle, static_cast<WORD>(m_ColorHex));
        m_StringCur = &m_ColorChar[2];
    }

    delete[] m_Buffer;
}

std::string GetSteamPath()
{
    HKEY m_RegKey = 0;
    if (RegOpenKeyA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", &m_RegKey) != ERROR_SUCCESS)
        return "";

    char m_String[MAX_PATH] = { '\0' };
    DWORD m_StringLength = sizeof(m_String);

    DWORD m_Type = REG_SZ;
    RegQueryValueExA(m_RegKey, "SteamPath", 0, &m_Type, reinterpret_cast<LPBYTE>(m_String), &m_StringLength);

    RegCloseKey(m_RegKey);

    return m_String;
}


int main()
{
    SetConsoleTitleA("Steam Login UT");

    std::string m_SteamPath = GetSteamPath();
    if (m_SteamPath.empty())
    {
        MessageBoxA(0, "Couldn't get steam path from registry!", "Steam Login UT", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return 1;
    }

    std::string m_LoginUsersFile = (m_SteamPath + "\\config\\loginusers.vdf");
    std::ifstream m_InputFile(m_LoginUsersFile);
    tyti::vdf::object m_VDF = tyti::vdf::read(m_InputFile);
    m_InputFile.close();

    if (m_VDF.childs.empty())
    {
        MessageBoxA(0, "No login users found inside vdf file!", "Steam Login UT", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return 1;
    }

    struct UserInfo_t
    {
        std::string m_SteamID = "";
        std::string m_Name = "";
        std::string m_PersonaName = "";
        uint32_t m_Timestamp = 0;

        bool m_Active = false;
    };
    std::vector<UserInfo_t> m_UserList;
    size_t m_UserSelected = 0;

    auto UserList_UpdateActive = [&m_UserList]()
    {
        std::map<uint32_t, UserInfo_t*, std::greater<uint32_t>> m_Map;

        for (size_t i = 0; m_UserList.size() > i; ++i)
        {
            UserInfo_t* m_UserInfo = &m_UserList[i];
            m_UserInfo->m_Active = false;

            m_Map[m_UserInfo->m_Timestamp] = m_UserInfo;
        }

        int m_MaxAccounts = 5;
        for (auto& Pair : m_Map)
        {
            Pair.second->m_Active = true;

            --m_MaxAccounts;
            if (m_MaxAccounts == 0)
                break;
        }
    };

    for (auto& Pair : m_VDF.childs)
    {
        UserInfo_t m_UserInfo;
        m_UserInfo.m_SteamID = Pair.first;
        m_UserInfo.m_Name = Pair.second->attribs["AccountName"];
        m_UserInfo.m_PersonaName = Pair.second->attribs["PersonaName"];
        m_UserInfo.m_Timestamp = static_cast<uint32_t>(std::stoul(Pair.second->attribs["Timestamp"]));

        m_UserList.emplace_back(m_UserInfo);
    }

    while (1)
    {
        // Clear Console...
        {
            HANDLE m_STDHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO m_ConsoleScreenBuffer;
            GetConsoleScreenBufferInfo(m_STDHandle, &m_ConsoleScreenBuffer);

            DWORD m_Count = (m_ConsoleScreenBuffer.dwSize.X * m_ConsoleScreenBuffer.dwSize.Y);
            COORD m_Coords = { 0, 0 };
            DWORD m_NumCount = 0;

            FillConsoleOutputCharacterA(m_STDHandle, ' ', m_Count, m_Coords, &m_NumCount);
            FillConsoleOutputAttribute(m_STDHandle, 0, m_Count, m_Coords, &m_NumCount);
            SetConsoleCursorPosition(m_STDHandle, m_Coords);
        }

        ColorPrintf("^FNavigate: ^EUp/Down Arrow\n");
        ColorPrintf("^FSelect: ^EEnter\n");
        ColorPrintf("^FApply: ^ERight Arrow\n");
        printf("\n");

        UserList_UpdateActive();

        for (size_t i = 0; m_UserList.size() > i; ++i)
        {
            UserInfo_t& m_UserInfo = m_UserList[i];

            ColorPrintf("^F|");
            if (m_UserSelected == i)
                ColorPrintf("^F\x1A ");

            std::string m_NameColor = "^";
            m_NameColor += (m_UserInfo.m_Active ? 'A' : 'C');
            m_NameColor += m_UserInfo.m_Name;
            m_NameColor += " ^F(^E" + m_UserInfo.m_PersonaName + "^F)";
            m_NameColor += "\n";

            ColorPrintf(&m_NameColor[0]);
        }


        int m_Char = _getch();
        switch (m_Char)
        {
            case 13: 
            { 
                UserInfo_t& m_UserInfo = m_UserList[m_UserSelected];
                m_UserInfo.m_Timestamp = static_cast<uint32_t>(time(0));
                m_VDF.childs[m_UserInfo.m_SteamID]->attribs["Timestamp"] = std::to_string(m_UserInfo.m_Timestamp);
            }
            break;
            case 72:
            {
                if (m_UserSelected == 0)
                    m_UserSelected = m_UserList.size();

                --m_UserSelected;
            }
            break;
            case 77:
            {
                std::ofstream m_OutputFile(m_LoginUsersFile);
                tyti::vdf::write(m_OutputFile, m_VDF);
                exit(0);
            }
            break;
            case 80:
            {
                ++m_UserSelected;

                if (m_UserSelected >= m_UserList.size())
                    m_UserSelected = 0;
            }
            break;
        }
    }
}
