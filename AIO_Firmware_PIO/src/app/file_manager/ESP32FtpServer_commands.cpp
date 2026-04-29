// FtpServer command dispatch (PR-3.3 split).
//
// processCommand() — the single big switch over `command` that turns
// each FTP verb into a client.println response and (for transfer
// commands) sets `transferStatus` so handleFTP picks them up. Body
// byte-identical to the pre-split file. Calls the formerly-static
// helpers via the internal header.

#include "ESP32FtpServer_internal.h"
#include "SD.h"
#include "WiFi.h"

boolean FtpServer::processCommand()
{
    ///////////////////////////////////////
    //                                   //
    //      ACCESS CONTROL COMMANDS      //
    //                                   //
    ///////////////////////////////////////
    Serial.print("processCommand: ");
    Serial.println(command);
    //
    //  CDUP - Change to Parent Directory
    //
    if (!strcmp(command, "CDUP"))
    {
        String cwd = get_file_cwd(cwdName);
        // strncpy(dst, src, n) does NOT NUL-terminate when src length >= n;
        // snprintf is the simpler always-terminating equivalent here.
        snprintf(cwdName, sizeof(cwdName), "%s", cwd.c_str());
        client.println("250 Ok. Current directory is " + String(cwdName));
    }
    //
    //  CWD - Change Working Directory
    //
    else if (!strcmp(command, "CWD"))
    {

        char path[FTP_CWD_SIZE];
        // 以下是兼容xftp
        // if (strcmp(parameters, ".") == 0) // 'CWD .' is the same as PWD command
        // {
        //     client.println("257 \"" + String(cwdName) + "\" is your current directory");
        // }
        // // else if (strcmp(parameters, "/") == 0) // 'CWD .' is the same as PWD command
        // else if (parameters[0] == '/') // 'CWD .' is the same as PWD command
        // {
        //     snprintf(cwdName, FTP_CWD_SIZE, "/");
        //     client.println("257 \"" + String(cwdName) + "\" is your current directory");
        // }
        // else
        // {
        //     if (strcmp(cwdName, "/") == 0)
        //     {
        //         snprintf(cwdName, FTP_CWD_SIZE, "%s%s", cwdName, parameters);
        //     }
        //     else
        //     {
        //         snprintf(cwdName, FTP_CWD_SIZE, "%s/%s", cwdName, parameters);
        //     }
        //     client.println("257 \"" + String(cwdName) + "\" is your current directory");
        // }
        // // else
        // // {
        // //     client.println("250 Ok. Current directory is " + String(cwdName));
        // // }

        // 以下是兼容windows自带的ftp
        if (makePath(path, NULL) && SD.exists(path))
        {
            snprintf(cwdName, sizeof(cwdName), "%s", path);
            client.println("250 Ok. Current directory is " + String(cwdName));
        }
        Serial.print("cwdName -> ");
        Serial.println(cwdName);
    }
    //
    //  PWD - Print Directory
    //
    else if (!strcmp(command, "PWD"))
    {
        client.println("257 \"" + String(cwdName) + "\" is your current directory");
    }
    //
    //  QUIT
    //
    else if (!strcmp(command, "QUIT"))
    {
        disconnectClient();
        return false;
    }

    ///////////////////////////////////////
    //                                   //
    //    TRANSFER PARAMETER COMMANDS    //
    //                                   //
    ///////////////////////////////////////

    //
    //  MODE - Transfer Mode
    //
    else if (!strcmp(command, "MODE"))
    {
        if (!strcmp(parameters, "S"))
            client.println("200 S Ok");
        // else if( ! strcmp( parameters, "B" ))
        //  client.println( "200 B Ok\r\n";
        else
            client.println("504 Only S(tream) is suported");
    }
    //
    //  PASV - Passive Connection management
    //
    else if (!strcmp(command, "PASV"))
    {
        if (data.connected())
            data.stop();
        // dataServer.begin();
        // dataIp = Ethernet.localIP();
        dataIp = WiFi.localIP();
        dataPort = FTP_DATA_PORT_PASV;
// data.connect( dataIp, dataPort );
// data = dataServer.available();
#ifdef FTP_DEBUG
        Serial.println("Connection management set to passive");
        Serial.println("Data port set to " + String(dataPort));
#endif
        client.println("227 Entering Passive Mode (" + String(dataIp[0]) + "," + String(dataIp[1]) + "," + String(dataIp[2]) + "," + String(dataIp[3]) + "," + String(dataPort >> 8) + "," + String(dataPort & 255) + ").");
        dataPassiveConn = true;
    }
    //
    //  PORT - Data Port
    //
    else if (!strcmp(command, "PORT"))
    {
        if (data)
            data.stop();
        // get IP of data client
        dataIp[0] = atoi(parameters);
        char *p = strchr(parameters, ',');
        for (uint8_t i = 1; i < 4; i++)
        {
            dataIp[i] = atoi(++p);
            p = strchr(p, ',');
        }
        // get port of data client
        dataPort = 256 * atoi(++p);
        p = strchr(p, ',');
        dataPort += atoi(++p);
        if (p == NULL)
            client.println("501 Can't interpret parameters");
        else
        {

            client.println("200 PORT command successful");
            dataPassiveConn = false;
        }
    }
    //
    //  STRU - File Structure
    //
    else if (!strcmp(command, "STRU"))
    {
        if (!strcmp(parameters, "F"))
            client.println("200 F Ok");
        // else if( ! strcmp( parameters, "R" ))
        //  client.println( "200 B Ok\r\n";
        else
            client.println("504 Only F(ile) is suported");
    }
    //
    //  TYPE - Data Type
    //
    else if (!strcmp(command, "TYPE"))
    {
        if (!strcmp(parameters, "A"))
            client.println("200 TYPE is now ASII");
        else if (!strcmp(parameters, "I"))
            client.println("200 TYPE is now 8-bit binary");
        else
            client.println("504 Unknow TYPE");
    }

    ///////////////////////////////////////
    //                                   //
    //        FTP SERVICE COMMANDS       //
    //                                   //
    ///////////////////////////////////////

    //
    //  ABOR - Abort
    //
    else if (!strcmp(command, "ABOR"))
    {
        abortTransfer();
        client.println("226 Data connection closed");
    }
    //
    //  DELE - Delete a File
    //
    else if (!strcmp(command, "DELE"))
    {
        char path[FTP_CWD_SIZE];
        if (strlen(parameters) == 0)
            client.println("501 No file name");
        else if (makePath(path))
        {
            if (!SD.exists(path))
                client.println("550 File " + String(parameters) + " not found");
            else
            {
                if (SD.remove(path))
                    client.println("250 Deleted " + String(parameters));
                else
                    client.println("450 Can't delete " + String(parameters));
            }
        }
    }
    //
    //  LIST - List
    //
    else if (!strcmp(command, "LIST"))
    {
        if (!dataConnect())
        {
            client.println("425 No data connection");
        }
        else
        {
            client.println("150 Accepted data connection");
            uint16_t nm = 0;
            File dir = SD.open(cwdName);
            if ((!dir) || (!dir.isDirectory()))
                client.println("550 Can't open directory " + String(cwdName));
            else
            {
                File file = dir.openNextFile();
                while (file)
                {
                    String fn, fs;
                    fn = get_file_basename(file.name());
#ifdef FTP_DEBUG
                    Serial.println("File Name = " + fn);
#endif
                    fs = String(file.size());
                    if (file.isDirectory())
                    {
                        data.println("01-01-2000  00:00AM <DIR> " + fn);
                    }
                    else
                    {
                        data.println("01-01-2000  00:00AM " + fs + " " + fn);
                        //          data.println( " " + fn );
                    }
                    nm++;
                    file = dir.openNextFile();
                }
                client.println("226 " + String(nm) + " matches total");
            }
            data.stop();
        }
    }
    //
    //  MLSD - Listing for Machine Processing (see RFC 3659)
    //
    else if (!strcmp(command, "MLSD"))
    {
        if (!dataConnect())
            client.println("425 No data connection MLSD");
        else
        {
            client.println("150 Accepted data connection");
            uint16_t nm = 0;
            //      Dir dir= SD.openDir(cwdName);
            File dir = SD.open(cwdName);
            char dtStr[15];
            //  if(!SD.exists(cwdName))
            if ((!dir) || (!dir.isDirectory()))
                client.println("550 Can't open directory " + String(cwdName));
            //        client.println( "550 Can't open directory " +String(parameters) );
            else
            {
                //        while( dir.next())
                File file = dir.openNextFile();
                //        while( dir.openNextFile())
                while (file)
                {
                    String fn, fs;
                    fn = file.name();
                    fn.remove(0, 1);
                    fs = String(file.size());
                    if (file.isDirectory())
                    {
                        data.println("Type=dir;Size=" + fs + ";" + "modify=20000101000000;" + " " + fn);
                        //            data.println( "Type=dir;modify=20000101000000; " + fn);
                    }
                    else
                    {
                        // data.println( "Type=file;Size=" + fs + ";"+"modify=20000101160656;" +" " + fn);
                        data.println("Type=file;Size=" + fs + ";" + "modify=20000101000000;" + " " + fn);
                    }
                    nm++;
                    file = dir.openNextFile();
                }
                client.println("226-options: -a -l");
                client.println("226 " + String(nm) + " matches total");
            }
            data.stop();
        }
    }
    //
    //  NLST - Name List
    //
    else if (!strcmp(command, "NLST"))
    {
        if (!dataConnect())
            client.println("425 No data connection");
        else
        {
            client.println("150 Accepted data connection");
            uint16_t nm = 0;
            //      Dir dir=SD.openDir(cwdName);
            File dir = SD.open(cwdName);
            if (!SD.exists(cwdName))
                client.println("550 Can't open directory " + String(parameters));
            else
            {
                File file = dir.openNextFile();
                //        while( dir.next())
                while (file)
                {
                    //          data.println( dir.fileName());
                    data.println(file.name());
                    nm++;
                    file = dir.openNextFile();
                }
                client.println("226 " + String(nm) + " matches total");
            }
            data.stop();
        }
    }
    //
    //  NOOP
    //
    else if (!strcmp(command, "NOOP"))
    {
        // dataPort = 0;
        client.println("200 Zzz...");
    }
    //
    //  RETR - Retrieve
    //
    else if (!strcmp(command, "RETR"))
    {
        char path[FTP_CWD_SIZE];
        if (strlen(parameters) == 0)
            client.println("501 No file name");
        else if (makePath(path))
        {
            file = SD.open(path, "r");
            if (!file)
                client.println("550 File " + String(parameters) + " not found");
            else if (!file)
                client.println("450 Can't open " + String(parameters));
            else if (!dataConnect())
                client.println("425 No data connection");
            else
            {
#ifdef FTP_DEBUG
                Serial.println("Sending " + String(parameters));
#endif
                client.println("150-Connected to port " + String(dataPort));
                client.println("150 " + String(file.size()) + " bytes to download");
                millisBeginTrans = millis();
                bytesTransfered = 0;
                transferStatus = 1;
            }
        }
    }
    //
    //  STOR - Store
    //
    else if (!strcmp(command, "STOR"))
    {
        char path[FTP_CWD_SIZE];
        if (strlen(parameters) == 0)
            client.println("501 No file name");
        else if (makePath(path))
        {
            file = SD.open(path, "w");
            if (!file)
                client.println("451 Can't open/create " + String(parameters));
            else if (!dataConnect())
            {
                client.println("425 No data connection");
                file.close();
            }
            else
            {
#ifdef FTP_DEBUG
                Serial.println("Receiving " + String(parameters));
#endif
                client.println("150 Connected to port " + String(dataPort));
                millisBeginTrans = millis();
                bytesTransfered = 0;
                transferStatus = 2;
            }
        }
    }
    //
    //  MKD - Make Directory
    //
    else if (!strcmp(command, "MKD"))
    {
        char path[FTP_CWD_SIZE] = {0};
        if (makePath(path))
        {
            if (SD.exists(path))
            {
                client.println("521 Can't create \"" + String(parameters) + ", Directory exists");
            }
            else
            {
                if (SD.mkdir(path))
                {
                    client.println("257 Directory created.\r\n");
                }
                else
                {
                    client.println("550 Can't create " + String(parameters)); // not support on espyet
                }
            }
        }
    }
    //
    //  RMD - Remove a Directory
    //
    else if (!strcmp(command, "RMD"))
    {
        char path[FTP_CWD_SIZE] = {0};
        if (makePath(path))
        {
            if (SD.rmdir(path))
            {
                client.println("450 Directory deleted.\r\n");
            }
            else
            {
                client.println("550 RMDIR failed Directory %s not exists.\r\n" + String(path));
            }
        }
    }
    //
    //  RNFR - Rename From
    //
    else if (!strcmp(command, "RNFR"))
    {
        buf[0] = 0;
        if (strlen(parameters) == 0)
            client.println("501 No file name");
        else if (makePath(buf))
        {
            if (!SD.exists(buf))
                client.println("550 File " + String(parameters) + " not found");
            else
            {
#ifdef FTP_DEBUG
                Serial.println("Renaming " + String(buf));
#endif
                client.println("350 RNFR accepted - file exists, ready for destination");
                rnfrCmd = true;
            }
        }
    }
    //
    //  RNTO - Rename To
    //
    else if (!strcmp(command, "RNTO"))
    {
        char path[FTP_CWD_SIZE];
        char dir[FTP_FIL_SIZE];
        if (strlen(buf) == 0 || !rnfrCmd)
            client.println("503 Need RNFR before RNTO");
        else if (strlen(parameters) == 0)
            client.println("501 No file name");
        else if (makePath(path))
        {
            if (SD.exists(path))
                client.println("553 " + String(parameters) + " already exists");
            else
            {
#ifdef FTP_DEBUG
                Serial.println("Renaming " + String(buf) + " to " + String(path));
#endif
                if (SD.rename(buf, path))
                    client.println("250 File successfully renamed or moved");
                else
                    client.println("451 Rename/move failure");
            }
        }
        rnfrCmd = false;
    }

    ///////////////////////////////////////
    //                                   //
    //   EXTENSIONS COMMANDS (RFC 3659)  //
    //                                   //
    ///////////////////////////////////////

    //
    //  FEAT - New Features
    //
    else if (!strcmp(command, "FEAT"))
    {
        client.println("211-Extensions suported:");
        client.println(" MLSD");
        client.println("211 End.");
    }
    //
    //  MDTM - File Modification Time (see RFC 3659)
    //
    else if (!strcmp(command, "MDTM"))
    {
        client.println("550 Unable to retrieve time");
    }

    //
    //  SIZE - Size of the file
    //
    else if (!strcmp(command, "SIZE"))
    {
        char path[FTP_CWD_SIZE];
        if (strlen(parameters) == 0)
            client.println("501 No file name");
        else if (makePath(path))
        {
            file = SD.open(path, "r");
            if (!file)
                client.println("450 Can't open " + String(parameters));
            else
            {
                client.println("213 " + String(file.size()));
                file.close();
            }
        }
    }
    //
    //  SITE - System command
    //
    else if (!strcmp(command, "SITE"))
    {
        client.println("500 Unknow SITE command " + String(parameters));
    }
    //
    //  Unrecognized commands ...
    //
    else
        client.println("500 Unknow command");

    return true;
}
