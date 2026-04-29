// Save handlers + file ops for the web_setting page (PR-3.1 split).
// save*Conf() functions translate POSTed form fields back into the
// app_controller->send_to(APP_MESSAGE_SET_PARAM, ...) calls that
// persist them, then re-render via Send_HTML. The file ops
// (File_Download / File_Upload / File_Delete + helpers) are bundled
// here because they share the same "endpoint -> mutate state ->
// respond" shape and the same SD-card / Send_HTML dependencies.
//
// Originally lived alongside the i18n table and page chrome inside a
// 1257-line web_setting.cpp.

#include "network.h"
#include "common.h"
#include "server.h"
#include "web_setting.h"
#include "web_setting_internal.h"
#include "app/app_conf.h"
#include "FS.h"
#include "HardwareSerial.h"
#include <esp32-hal.h>

// Module-local state shared between the file ops handlers below.
boolean sd_present = true;

// Glass UI: after a successful save, redirect back to the originating
// setting page with ?saved=1 so the GLASS_JS in init_page_footer fires
// the toast. Replaces the old "<h1>设置成功!</h1>" inline page that
// stranded the user with no nav back. preserveLang() carries the
// current ?lang= forward so the redirected page stays in the user's
// chosen language.
static void post_save_redirect(const char *settingPath)
{
    String dest(settingPath);
    dest += "?saved=1";
    if (server.hasArg("lang")) {
        dest += "&lang=";
        dest += server.arg("lang");
    }
    server.sendHeader("Location", dest);
    server.send(303);
}

String file_size(int bytes)
{
    String fsize = "";
    if (bytes < 1024)
        fsize = String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        fsize = String(bytes / 1024.0, 3) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
    else
        fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
    return fsize;
}

void saveSysConf(void)
{

    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"ssid_0",
                            (void *)server.arg("ssid_0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"password_0",
                            (void *)server.arg("password_0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"power_mode",
                            (void *)server.arg("power_mode").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"backLight",
                            (void *)server.arg("backLight").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"rotation",
                            (void *)server.arg("rotation").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mpu_order",
                            (void *)server.arg("mpu_order").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"auto_calibration_mpu",
                            (void *)server.arg("auto_calibration_mpu").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"auto_start_app",
                            (void *)server.arg("auto_start_app").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/sys_setting");
}

void saveRgbConf(void)
{

    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"min_brightness",
                            (void *)server.arg("min_brightness").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"max_brightness",
                            (void *)server.arg("max_brightness").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"time",
                            (void *)server.arg("time").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/rgb_setting");
}

void saveWeatherConf(void)
{

    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"api_key",
                            (void *)server.arg("api_key").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"city_name",
                            (void *)server.arg("city_name").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"weatherUpdataInterval",
                            (void *)server.arg("weatherUpdataInterval").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"timeUpdataInterval",
                            (void *)server.arg("timeUpdataInterval").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"language",
                            (void *)server.arg("language").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/weather_setting");
}

void saveWeatherOldConf(void)
{

    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"cityname",
                            (void *)server.arg("cityname").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"language",
                            (void *)server.arg("language").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"weather_key",
                            (void *)server.arg("weather_key").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"weatherUpdataInterval",
                            (void *)server.arg("weatherUpdataInterval").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"timeUpdataInterval",
                            (void *)server.arg("timeUpdataInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/weather_old_setting");
}

void saveBiliConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Bili",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"bili_uid",
                            (void *)server.arg("bili_uid").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Bili",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"updataInterval",
                            (void *)server.arg("updataInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/bili_setting");
}

void saveStockConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Stock",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"stock_symbol",
                            (void *)server.arg("stock_symbol").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Stock",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"market_type",
                            (void *)server.arg("market_type").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Stock",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"updataInterval",
                            (void *)server.arg("updataInterval").c_str());
    // Persist data to flash
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/stock_setting");
}

void savePictureConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Picture",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"switchInterval",
                            (void *)server.arg("switchInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Picture", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/picture_setting");
}

void saveMediaConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Media",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"switchFlag",
                            (void *)server.arg("switchFlag").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Media",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"powerFlag",
                            (void *)server.arg("powerFlag").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/media_setting");
}

void saveScreenConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Screen share",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"powerFlag",
                            (void *)server.arg("powerFlag").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Screen share", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/screen_setting");
}

void saveHeartbeatConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"role",
                            (void *)server.arg("role").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"qq_num",
                            (void *)server.arg("qq_num").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_server",
                            (void *)server.arg("mqtt_server").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_port",
                            (void *)server.arg("mqtt_port").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_user",
                            (void *)server.arg("mqtt_user").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_password",
                            (void *)server.arg("mqtt_password").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/heartbeat_setting");
}

void saveAnniversaryConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"event_name0",
                            (void *)server.arg("event_name0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"target_date0",
                            (void *)server.arg("target_date0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"event_name1",
                            (void *)server.arg("event_name1").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"target_date1",
                            (void *)server.arg("target_date1").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/anniversary_setting");
}

void savePCResourceConf(void)
{
    app_controller->send_to(SERVER_APP_NAME, "PC Resource",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"pc_ipaddr",
                            (void *)server.arg("pc_ipaddr").c_str());
    app_controller->send_to(SERVER_APP_NAME, "PC Resource",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"sensorUpdataInterval",
                            (void *)server.arg("sensorUpdataInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
    post_save_redirect("/pc_resource_setting");
}

void File_Delete()
{
    // Glass-styled delete form. Single text-field row + Confirm/Back
    // row-actions, mirroring the layout of every *_setting page so this
    // page stops looking like raw browser-default HTML.
    Send_HTML(
        F("<form method=\"POST\" action=\"/delete_result\"><div class=\"card\">"
          "<div class=\"card-head\"><div><div class=\"card-title\">Delete File</div>"
          "<div class=\"card-sub\">Absolute path on SD, e.g. /image/foo.jpg</div></div></div>"
          "<div class=\"card-body\">"
          "<div class=\"field\"><label>File Path</label>"
          "<input type=\"text\" name=\"delete_filepath\" placeholder=\"/image/...\" class=\"mono\">"
          "<span></span></div>"
          "</div>"
          "<div class=\"row-actions\">"
          "<button type=\"submit\" class=\"btn primary\">\xE2\x9C\x93 Confirm Delete</button>"
          "<a href=\"/\" class=\"btn ghost\">Back</a>"
          "</div></div></form>"));
}

void delete_result(void)
{
    String del_file = server.arg("delete_filepath");
    boolean ret = tf.deleteFile(del_file);
    if (ret)
    {
        webpage = F("<div class=\"card\"><div class=\"card-head\">"
                    "<div><div class=\"card-title\">Delete Result</div></div></div>"
                    "<div class=\"result-msg ok\"><span class=\"icon\">\xE2\x9C\x93</span>"
                    "File deleted successfully.</div>"
                    "<div class=\"row-actions\">"
                    "<a href=\"/delete\" class=\"btn\">Back to Delete</a>"
                    "<a href=\"/\" class=\"btn ghost\">Home</a>"
                    "</div></div>");
        tf.listDir("/image", 250);
    }
    else
    {
        webpage = F("<div class=\"card\"><div class=\"card-head\">"
                    "<div><div class=\"card-title\">Delete Result</div></div></div>"
                    "<div class=\"result-msg err\"><span class=\"icon\">!</span>"
                    "Delete failed \xE2\x80\x94 check the file path.</div>"
                    "<div class=\"row-actions\">"
                    "<a href=\"/delete\" class=\"btn\">Back to Delete</a>"
                    "<a href=\"/\" class=\"btn ghost\">Home</a>"
                    "</div></div>");
    }
    tf.listDir("/image", 250);
    Send_HTML(webpage);
}

void File_Download()
{ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
    if (server.hasArg("download"))
    { // Download argument was received
        sd_file_download(server.arg("download"));
    }
    else
    {
        SelectInput("Enter filename to download", "download", "download");
    }
}

void sd_file_download(const String &filename)
{
    if (sd_present)
    {
        File download = tf.open("/" + filename);
        if (download)
        {
            server.sendHeader("Content-Type", "text/text");
            server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
            server.sendHeader("Connection", "close");
            server.streamFile(download, "application/octet-stream");
            download.close();
        }
        else
            ReportFileNotPresent(String("download"));
    }
    else
        ReportSDNotPresent();
}

void File_Upload()
{
    tf.listDir("/image", 250);

    webpage = webpage_header;
    // Glass-styled upload form. Same .card / .field / .row-actions
    // skeleton as the *_setting pages so the file-ops trio looks like
    // first-class app pages.
    webpage += F("<form method=\"POST\" action=\"/fupload\" enctype=\"multipart/form-data\"><div class=\"card\">"
                 "<div class=\"card-head\"><div><div class=\"card-title\">Upload File</div>"
                 "<div class=\"card-sub\">Saved to SD root \xE2\x80\x94 use absolute paths via Delete to clean up</div></div></div>"
                 "<div class=\"card-body\">"
                 "<div class=\"field\"><label>Choose File</label>"
                 "<input type=\"file\" name=\"fupload\" id=\"fupload\">"
                 "<span></span></div>"
                 "</div>"
                 "<div class=\"row-actions\">"
                 "<button type=\"submit\" class=\"btn primary\">\xE2\x86\x91 Upload</button>"
                 "<a href=\"/\" class=\"btn ghost\">Back</a>"
                 "</div></div></form>");
    webpage += webpage_footer;
    server.send(200, "text/html", webpage);
}

File UploadFile;
void handleFileUpload()
{                                                   // upload a new file to the Filing system
    HTTPUpload &uploadFileStream = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                                    // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
    String filename = uploadFileStream.filename;
    if (uploadFileStream.status == UPLOAD_FILE_START)
    {
        // String filename = uploadFileStream.filename;
        // if (!filename.startsWith("/image"))
        filename = "/image/" + filename;
        Serial.print(F("Upload File Name: "));
        Serial.println(filename);
        tf.deleteFile(filename);                    // Remove a previous version, otherwise data is appended the file again
        UploadFile = tf.open(filename, FILE_WRITE); // Open the file for writing in SPIFFS (create it, if doesn't exist)
    }
    else if (uploadFileStream.status == UPLOAD_FILE_WRITE)
    {
        if (UploadFile)
            UploadFile.write(uploadFileStream.buf, uploadFileStream.currentSize); // Write the received bytes to the file
    }
    else if (uploadFileStream.status == UPLOAD_FILE_END)
    {
        if (UploadFile) // If the file was successfully created
        {
            UploadFile.close(); // Close the file again
            Serial.print(F("Upload Size: "));
            Serial.println(uploadFileStream.totalSize);
            webpage = webpage_header;
            webpage += F("<h3>File was successfully uploaded</h3>");
            webpage += F("<h2>Uploaded File Name: ");
            webpage += filename + "</h2>";
            webpage += F("<h2>File Size: ");
            webpage += file_size(uploadFileStream.totalSize) + "</h2><br>";
            webpage += webpage_footer;
            server.send(200, "text/html", webpage);
            tf.listDir("/image", 250);
        }
        else
        {
            ReportCouldNotCreateFile(String("upload"));
        }
    }
}

void SelectInput(String heading, String command, String arg_calling_name)
{
    // Generic single-input form helper (used by File_Download). Glass
    // skeleton matches the other file-ops pages.
    //
    // Pre-fix bug: the original emitted `<type='submit' ...>` (a
    // bare `<type>` element with no leading tag name) so the submit
    // button never rendered — Download was effectively unusable from
    // the browser. The Glass redesign emits a real <button>.
    webpage = F("<form method=\"POST\" action=\"/");
    webpage += command;
    webpage += F("\"><div class=\"card\">"
                 "<div class=\"card-head\"><div><div class=\"card-title\">");
    webpage += heading;
    webpage += F("</div></div></div>"
                 "<div class=\"card-body\">"
                 "<div class=\"field\"><label>File Name</label>"
                 "<input type=\"text\" name=\"");
    webpage += arg_calling_name;
    webpage += F("\" placeholder=\"path on SD\" class=\"mono\">"
                 "<span></span></div>"
                 "</div>"
                 "<div class=\"row-actions\">"
                 "<button type=\"submit\" class=\"btn primary\">\xE2\x86\x93 Submit</button>"
                 "<a href=\"/\" class=\"btn ghost\">Back</a>"
                 "</div></div></form>");
    Send_HTML(webpage);
}

// Shared error-card emitter for the three Report* helpers. Same .card
// shape as result_msg in delete_result so error pages don't look like
// a different generation of the UI from success pages.
static String render_error_card(const __FlashStringHelper *title,
                                const __FlashStringHelper *body,
                                const String &back_target)
{
    String html = F("<div class=\"card\"><div class=\"card-head\">"
                    "<div><div class=\"card-title\">");
    html += title;
    html += F("</div></div></div>"
              "<div class=\"result-msg err\"><span class=\"icon\">!</span>");
    html += body;
    html += F("</div>"
              "<div class=\"row-actions\">"
              "<a href=\"/");
    html += back_target;
    html += F("\" class=\"btn\">Back</a>"
              "<a href=\"/\" class=\"btn ghost\">Home</a>"
              "</div></div>");
    return html;
}

void ReportSDNotPresent()
{
    webpage = render_error_card(F("SD Card"), F("No SD card detected."), F(""));
    Send_HTML(webpage);
}

void ReportFileNotPresent(const String &target)
{
    webpage = render_error_card(F("File Not Found"), F("The requested file does not exist on the SD card."), target);
    Send_HTML(webpage);
}

void ReportCouldNotCreateFile(const String &target)
{
    webpage = render_error_card(F("Upload Failed"), F("Could not create the uploaded file (SD write-protected or full?)."), target);
    Send_HTML(webpage);
}
