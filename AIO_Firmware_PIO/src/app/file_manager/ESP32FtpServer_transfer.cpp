// FtpServer data-transfer cluster (PR-3.3 split).
// Bodies byte-identical to the pre-split file. Handles the PASV-mode
// data connection lifecycle and the per-tick RETR / STOR pumps that
// handleFTP() drives once transferStatus is set by processCommand.

#include "ESP32FtpServer_internal.h"
#include "SD.h"

boolean FtpServer::dataConnect()
{
    unsigned long startTime = millis();
    // wait 5 seconds for a data connection
    if (!data.connected())
    {
        while (!dataServer.hasClient() && millis() - startTime < 10000)
        //    while (!dataServer.available() && millis() - startTime < 10000)
        {
            // delay(100);
            yield();
        }
        if (dataServer.hasClient())
        {
            //    if (dataServer.available()) {
            data.stop();
            data = dataServer.available();
#ifdef FTP_DEBUG
            Serial.println("ftpdataserver client....");
#endif
        }
    }

    return data.connected();
}

boolean FtpServer::doRetrieve()
{
    if (data.connected())
    {
        // int16_t nb = file.readBytes((uint8_t*) buf, FTP_BUF_SIZE );
        int16_t nb = file.readBytes(buf, FTP_BUF_SIZE);
        if (nb > 0)
        {
            data.write((uint8_t *)buf, nb);
            bytesTransfered += nb;
            return true;
        }
    }
    closeTransfer();
    return false;
}

boolean FtpServer::doStore()
{
    if (data.connected())
    {
        int16_t nb = data.readBytes((uint8_t *)buf, FTP_BUF_SIZE);
        if (nb > 0)
        {
            // Serial.println( millis() << " " << nb << endl;
            file.write((uint8_t *)buf, nb);
            bytesTransfered += nb;
        }
        return true;
    }
    closeTransfer();
    return false;
}

void FtpServer::closeTransfer()
{
    uint32_t deltaT = (int32_t)(millis() - millisBeginTrans);
    if (deltaT > 0 && bytesTransfered > 0)
    {
        client.println("226-File successfully transferred");
        client.println("226 " + String(deltaT) + " ms, " + String(bytesTransfered / deltaT) + " kbytes/s");
    }
    else
        client.println("226 File successfully transferred");

    file.close();
    data.stop();
}

void FtpServer::abortTransfer()
{
    if (transferStatus > 0)
    {
        file.close();
        data.stop();
        client.println("426 Transfer aborted");
#ifdef FTP_DEBUG
        Serial.println("Transfer aborted!");
#endif
    }
    transferStatus = 0;
}
