/*
 * FTP Serveur for ESP8266
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva david@nailbuster.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//  2017: modified by @robo8080
//  2021: modified by @ClimbSnail
//
// PR-3.3 split: this TU now owns the FtpServer lifecycle (begin /
// handleFTP state machine / clientConnected / disconnectClient /
// readChar) and the WiFiServer globals. Auth / commands / transfer /
// path-utility members live in ESP32FtpServer_{auth,commands,transfer,
// util}.cpp; shared decls in ESP32FtpServer_internal.h.

#include "ESP32FtpServer_internal.h"
#include <WiFi.h>
#include <WiFiClient.h>
//#include <ESP32WebServer.h>
#include <FS.h>
#include "SD.h"
#include "SPI.h"

WiFiServer ftpServer(FTP_CTRL_PORT);
WiFiServer dataServer(FTP_DATA_PORT_PASV);

void FtpServer::begin(String uname, String pword)
{
    // Tells the ftp server to begin listening for incoming connection
    _FTP_USER = uname;
    _FTP_PASS = pword;

    ftpServer.begin();
    delay(10);
    dataServer.begin();
    delay(10);
    millisTimeOut = (uint32_t)FTP_TIME_OUT * 60 * 1000;
    millisDelay = 0;
    cmdStatus = 0;
    iniVariables();
}

void FtpServer::iniVariables()
{
    // Default for data port
    dataPort = FTP_DATA_PORT_PASV;

    // Default Data connection is Active
    dataPassiveConn = true;

    // Set the root directory
    snprintf(cwdName, sizeof(cwdName), "/");

    rnfrCmd = false;
    transferStatus = 0;
}

void FtpServer::handleFTP()
{
    if ((int32_t)(millisDelay - millis()) > 0)
        return;

    if (ftpServer.hasClient())
    {
        //  if (ftpServer.available()) {
        client.stop();
        client = ftpServer.available();
    }

    if (cmdStatus == 0)
    {
        if (client.connected())
            disconnectClient();
        cmdStatus = 1;
    }
    else if (cmdStatus == 1) // Ftp server waiting for connection
    {
        abortTransfer();
        iniVariables();
#ifdef FTP_DEBUG
        Serial.println("Ftp server waiting for connection on port " + String(FTP_CTRL_PORT));
#endif
        cmdStatus = 2;
    }
    else if (cmdStatus == 2) // Ftp server idle
    {

        if (client.connected()) // A client connected
        {
            clientConnected();
            millisEndConnection = millis() + 10 * 1000; // wait client id during 10 s.
            cmdStatus = 3;
        }
    }
    else if (readChar() > 0) // got response
    {
        if (cmdStatus == 3) // Ftp server waiting for user identity
        {
            if (userIdentity())
                cmdStatus = 4;
            else
                cmdStatus = 0;
        }
        else if (cmdStatus == 4) // Ftp server waiting for user registration
        {
            if (userPassword())
            {
                cmdStatus = 5;
                millisEndConnection = millis() + millisTimeOut;
            }
            else
                cmdStatus = 0;
        }
        else if (cmdStatus == 5) // Ftp server waiting for user command
        {
            if (!processCommand())
                cmdStatus = 0;
            else
                millisEndConnection = millis() + millisTimeOut;
        }
    }
    else if (!client.connected() || !client)
    {
        cmdStatus = 1;
#ifdef FTP_DEBUG
        Serial.println("client disconnected");
#endif
    }

    if (transferStatus == 1) // Retrieve data
    {
        if (!doRetrieve())
            transferStatus = 0;
    }
    else if (transferStatus == 2) // Store data
    {
        if (!doStore())
            transferStatus = 0;
    }
    else if (cmdStatus > 2 && !((int32_t)(millisEndConnection - millis()) > 0))
    {
        client.println("530 Timeout");
        millisDelay = millis() + 200; // delay of 200 ms
        cmdStatus = 0;
    }
}

void FtpServer::clientConnected()
{
#ifdef FTP_DEBUG
    Serial.println("Client connected!");
#endif
    client.println("220--- Welcome to FTP for ESP8266 ---");
    client.println("220---   By David Paiva   ---");
    client.println("220 --   Version " + String(FTP_SERVER_VERSION) + "   --");
    iCL = 0;
}

void FtpServer::disconnectClient()
{
#ifdef FTP_DEBUG
    Serial.println(" Disconnecting client");
#endif
    abortTransfer();
    client.println("221 Goodbye");
    client.stop();
}

// Read a char from client connected to ftp server
//
//  update cmdLine and command buffers, iCL and parameters pointers
//
//  return:
//    -2 if buffer cmdLine is full
//    -1 if line not completed
//     0 if empty line received
//    length of cmdLine (positive) if no empty line received

int8_t FtpServer::readChar()
{
    int8_t rc = -1;

    if (client.available())
    {
        char c = client.read();
        // char c;
        // client.readBytes((uint8_t*) c, 1);
#ifdef FTP_DEBUG
        Serial.print("c ----> ");
        Serial.println(c);
#endif
        if (c == '\\')
            c = '/';
        if (c != '\r')
            if (c != '\n')
            {
                if (iCL < FTP_CMD_SIZE)
                    cmdLine[iCL++] = c;
                else
                    rc = -2; //  Line too long
            }
            else
            {
                cmdLine[iCL] = 0;
                command[0] = 0;
                parameters = NULL;
                // empty line?
                if (iCL == 0)
                    rc = 0;
                else
                {
                    rc = iCL;
                    // search for space between command and parameters
                    parameters = strchr(cmdLine, ' ');
                    if (parameters != NULL)
                    {
                        if (parameters - cmdLine > 4)
                            rc = -2; // Syntax error
                        else
                        {
                            strncpy(command, cmdLine, parameters - cmdLine);
                            command[parameters - cmdLine] = 0;

                            while (*(++parameters) == ' ')
                                ;
#ifdef FTP_DEBUG
                            Serial.print("command ----> ");
                            Serial.print(command);
                            Serial.print("\tparame ----> ");
                            Serial.println(parameters);
#endif
                        }
                    }
                    else if (strlen(cmdLine) > 4)
                        rc = -2; // Syntax error.
                    else
                        snprintf(command, sizeof(command), "%s", cmdLine);
                    iCL = 0;
                }
            }
        if (rc > 0)
            for (uint8_t i = 0; i < strlen(command); i++)
                command[i] = toupper(command[i]);
        if (rc == -2)
        {
            iCL = 0;
            client.println("500 Syntax error");
        }
    }
    return rc;
}
