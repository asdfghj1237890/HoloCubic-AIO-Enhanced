// FtpServer utility helpers (PR-3.3 split).
//
// Path manipulation (basename / cwd / makePath) + MDTM-format
// datetime parsing/formatting. Bodies byte-identical to the
// pre-split file. The two TU-static helpers in the original
// (get_file_basename, get_file_cwd) lost their `static` qualifier
// so processCommand in _commands.cpp can still reach them; their
// declarations live in ESP32FtpServer_internal.h.

#include "ESP32FtpServer_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * get file basename
 */
const char *get_file_basename(const char *path)
{
    // 获取最后一个'/'所在的下标
    const char *ret = path;
    for (const char *cur = path; *cur != 0; ++cur)
    {
        if (*cur == '/')
        {
            ret = cur + 1;
        }
    }
    return ret;
}

/*
 * get file path
 */
String get_file_cwd(const char *path)
{
    // 获取最后一个'/'所在的下标
    const char *ret = path;
    int len = 0;
    char tmp[128] = {0};
    for (const char *cur = path; *cur != 0; ++cur)
    {
        if (*cur == '/')
        {
            ret = cur;
        }
    }

    if (ret == path)
    {
        len = 1;
    }
    else
    {
        len = ret - path;
    }

    memcpy(tmp, path, len);
    return String(tmp);
}

// Make complete path/name from cwdName and parameters
//
// 3 possible cases: parameters can be absolute path, relative path or only the name
//
// parameters:
//   fullName : where to store the path/name
//
// return:
//    true, if done

boolean FtpServer::makePath(char *fullName)
{
    return makePath(fullName, parameters);
}

boolean FtpServer::makePath(char *fullName, char *param)
{
    if (param == NULL)
        param = parameters;

    // Caller contract (every makePath call site): fullName points at a
    // char[FTP_CWD_SIZE] buffer, so the FTP_CWD_SIZE cap below is sound.
    // Root or empty?
    if (strcmp(param, "/") == 0 || strlen(param) == 0)
    {
        snprintf(fullName, FTP_CWD_SIZE, "/");
        return true;
    }
    // If relative path, concatenate with current dir
    if (param[0] != '/')
    {
        snprintf(fullName, FTP_CWD_SIZE, "%s", cwdName);
        // strncat(dst, src, n) treats `n` as the max number of chars to copy
        // FROM src, not the total destination size. The previous calls passed
        // FTP_CWD_SIZE (total) as `n` — happened to be safe for a 1-char "/"
        // and short relative names, but unsafe for any near-cap-length param.
        // strlcat takes the total dest size and truncates correctly.
        if (fullName[strlen(fullName) - 1] != '/')
            strlcat(fullName, "/", FTP_CWD_SIZE);
        strlcat(fullName, param, FTP_CWD_SIZE);
    }
    else
        snprintf(fullName, FTP_CWD_SIZE, "%s", param);
    // If ends with '/', remove it
    uint16_t strl = strlen(fullName) - 1;
    if (fullName[strl] == '/' && strl > 1)
        fullName[strl] = 0;

    if (strlen(fullName) < FTP_CWD_SIZE)
        return true;

    client.println("500 Command line too long");
    return false;
}

// Calculate year, month, day, hour, minute and second
//   from first parameter sent by MDTM command (YYYYMMDDHHMMSS)
//
// parameters:
//   pyear, pmonth, pday, phour, pminute and psecond: pointer of
//     variables where to store data
//
// return:
//    0 if parameter is not YYYYMMDDHHMMSS
//    length of parameter + space

uint8_t FtpServer::getDateTime(uint16_t *pyear, uint8_t *pmonth, uint8_t *pday,
                               uint8_t *phour, uint8_t *pminute, uint8_t *psecond)
{
    char dt[15];

    // Date/time are expressed as a 14 digits long string
    //   terminated by a space and followed by name of file
    if (strlen(parameters) < 15 || parameters[14] != ' ')
        return 0;
    for (uint8_t i = 0; i < 14; i++)
        if (!isdigit(parameters[i]))
            return 0;

    strncpy(dt, parameters, 14);
    dt[14] = 0;
    *psecond = atoi(dt + 12);
    dt[12] = 0;
    *pminute = atoi(dt + 10);
    dt[10] = 0;
    *phour = atoi(dt + 8);
    dt[8] = 0;
    *pday = atoi(dt + 6);
    dt[6] = 0;
    *pmonth = atoi(dt + 4);
    dt[4] = 0;
    *pyear = atoi(dt);
    return 15;
}

// Create string YYYYMMDDHHMMSS from date and time
//
// parameters:
//    date, time
//    tstr: where to store the string. Must be at least 15 characters long
//
// return:
//    pointer to tstr

char *FtpServer::makeDateTimeStr(char *tstr, uint16_t date, uint16_t time)
{
    sprintf(tstr, "%04u%02u%02u%02u%02u%02u",
            ((date & 0xFE00) >> 9) + 1980, (date & 0x01E0) >> 5, date & 0x001F,
            (time & 0xF800) >> 11, (time & 0x07E0) >> 5, (time & 0x001F) << 1);
    return tstr;
}
