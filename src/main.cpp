#include "Arduino.h"
#include "WiFi.h"
#include "esp_camera.h"
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>

// Replace with your network credentials
const char *ssid = "Nayaa";
const char *password = "Lancer2003";

// Insert Firebase project API Key
#define API_KEY "AIzaSyB95vOBwd0SgZfYNA6gooayvBOGoHbncOM"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "nayanthanethsara@gmail.com"
#define USER_PASSWORD "Lancer2003"

// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "car-parking-system-18171.appspot.com"

// Insert Firebase Realtime Database URL
#define DATABASE_URL "https://car-parking-system-18171-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Base path and extension for photo files
#define BASE_FILE_PHOTO_PATH "/photo"
#define FILE_EXTENSION ".jpg"
#define INDEX_FILE_PATH "/photoIndex.txt" // Path to store the photo index

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

boolean takeNewPhoto = true;

// Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

void fcsUploadCallback(FCS_UploadStatusInfo info);

bool taskCompleted = false;
int photoIndex = 1; // Start with photo 1

// Function to save the current photo index to LittleFS
void savePhotoIndex()
{
  File file = LittleFS.open(INDEX_FILE_PATH, FILE_WRITE);
  if (file)
  {
    file.println(photoIndex);
    file.close();
    Serial.printf("Photo index %d saved to LittleFS\n", photoIndex);
  }
  else
  {
    Serial.println("Failed to open index file for writing");
  }
}

// Function to load the last saved photo index from LittleFS
void loadPhotoIndex()
{
  File file = LittleFS.open(INDEX_FILE_PATH, FILE_READ);
  if (file)
  {
    photoIndex = file.parseInt();
    file.close();
    Serial.printf("Photo index %d loaded from LittleFS\n", photoIndex);
  }
  else
  {
    Serial.println("No index file found, starting with photoIndex 1");
  }
}

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS(void)
{
  // Dispose first pictures because of bad quality
  camera_fb_t *fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++)
  {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Take a new photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  // Generate a new file name
  String fileName = String(BASE_FILE_PHOTO_PATH) + String(photoIndex) + String(FILE_EXTENSION);
  Serial.printf("Picture file name: %s\n", fileName.c_str());
  File file = LittleFS.open(fileName, FILE_WRITE);

  // Insert the data in the photo file
  if (!file)
  {
    Serial.println("Failed to open file in writing mode");
  }
  else
  {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(fileName);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void initWiFi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
}

void initLittleFS()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  }
  else
  {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}

void initCamera()
{
  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
}

// Function to check the camera status from the database
bool checkCameraStatus()
{
  if (Firebase.RTDB.getString(&fbdo, "CameraStatus"))
  {
    String status = fbdo.stringData();
    Serial.print("Camera Status: ");
    Serial.println(status);
    return status == "ON";
  }
  else
  {
    Serial.println(fbdo.errorReason());
    return false;
  }
}

// Function to update the camera status in the database
void updateCameraStatus(String status)
{
  if (Firebase.RTDB.setString(&fbdo, "CameraStatus", status))
  {
    Serial.print("Camera Status updated to: ");
    Serial.println(status);
  }
  else
  {
    Serial.print("Failed to update camera status: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  // Load the photo index from LittleFS
  loadPhotoIndex();
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  initCamera();

  // Initialize Firebase
  configF.api_key = API_KEY;
  configF.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);
}

void loop()
{
  if (takeNewPhoto)
  {
    capturePhotoSaveLittleFS();
    takeNewPhoto = false;
  }
  delay(1);
  if (Firebase.ready() && !taskCompleted)
  {
    taskCompleted = true;
    Serial.print("Uploading picture... ");

    // Generate the file name for the current photo
    String fileName = String(BASE_FILE_PHOTO_PATH) + String(photoIndex) + String(FILE_EXTENSION);

    // MIME type should be valid to avoid the download problem.
    // The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, fileName.c_str() /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, ("/data/photo" + String(photoIndex) + String(FILE_EXTENSION)).c_str() /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */, fcsUploadCallback))
    {
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
      photoIndex++;     // Increment the photo index after a successful upload
      savePhotoIndex(); // Save the new photo index
    }
    else
    {
      Serial.println(fbdo.errorReason());
    }
  }
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info)
{
  if (info.status == firebase_fcs_upload_status_init)
  {
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  }
  else if (info.status == firebase_fcs_upload_status_upload)
  {
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  }
  else if (info.status == firebase_fcs_upload_status_complete)
  {
    Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("contentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("Metageneration: %lu\n", meta.metageneration);
    Serial.printf("ETag: %s\n", meta.etag.c_str());
    Serial.printf("CRC32: %s\n", meta.crc32.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
    Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
  }
  else if (info.status == firebase_fcs_upload_status_error)
  {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}
