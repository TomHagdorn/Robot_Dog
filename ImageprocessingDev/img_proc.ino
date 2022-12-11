/*!
    \file camera_dump.ino

    \brief Dump array captured from camera to serial.

    The image captured is stored in a form of 1D array. This image is a grayscale image.
    The contrast and exposure is set automatically.

    \author Tom Hagdorn 
*/

/*!
    \brief Check for ESP32 board.
*/

#if !defined ESP32
#error Wrong board selected
#endif

#define CAMERA_MODEL_AI_THINKER

#include "esp_camera.h" ///< Header file for camera obtained from https://github.com/espressif/

#include "driver/ledc.h" ///< To enable onboard Illumination/flash LED pin attached on 4

#include "soc/soc.h"          //! Used to disable brownout detection
#include "soc/rtc_cntl_reg.h" //! Used to disable brownout detection

#include <vector.h>
//Depenencies for image processing



// define a vector data type to represent a point in the image
using point_vector_t = Vector<int, 2>;


//! Image resolution:
/*!
    default = "const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA"

    Other available Frame Sizes:
    160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA),
    320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA),
    1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
*/

const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_QQVGA;

//! Image Format
/*!
    Other Available formats:
    YUV422, GRAYSCALE, RGB565, JPEG, RGB888
*/

#define PIXFORMAT PIXFORMAT_GRAYSCALE;

int pixel_threshold = 100;


#define IMAGE_WIDTH 160  ///< Image size Width
#define IMAGE_HEIGHT 120 ///< Image size Height

//! Camera exposure
/*!
    Range: (0 - 1200)
    If gain and exposure both set to zero then auto adjust is enabled
*/

int cameraImageExposure = 0;

//! Image gain
/*!
    Range: (0 - 30)
    If gain and exposure both set to zero then auto adjust is enabled
*/
int cameraImageGain = 0;

const uint8_t ledPin = 4;                  ///< onboard Illumination/flash LED pin (4)
int ledBrightness = 16;                    ///< Initial brightness (0 - 255)
const int pwmFrequency = 50000;            ///< PWM settings for ESP32
const uint8_t ledChannel = LEDC_CHANNEL_0; ///< Camera timer0
const uint8_t pwmResolution = 8;           ///< resolution (8 = from 0 to 255)

const int serialSpeed = 115200; ///< Serial data speed to use

//! Camera setting
/*!
    Camera settings for CAMERA_MODEL_AI_THINKER OV2640
    Based on CameraWebServer sample code by ESP32 Arduino

*/
#if defined(CAMERA_MODEL_AI_THINKER)
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
#endif

uint32_t lastCamera = 0; ///< To store time value for repeated capture
/**************************************************************************/
/*!
  \brief  Setup function

  Initialization for following:
    disable Brownout detection
    camera

*/
/**************************************************************************/
void setup()
{
    Serial.begin(serialSpeed); ///< Initialize serial communication
    Serial.println("\n");

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); ///< Disable 'brownout detector'

    Serial.print(("\nInitialising camera: "));
    if (initialiseCamera())
    {
        Serial.println("OK");
    }
    else
    {
        Serial.println("Error!");
    }

    setupOnBoardFlash();
    Serial.println("Setup complete\n");
    setLedBrightness(ledBrightness);
}
/**************************************************************************/
/*!
  \brief  Loop function
  Capture image every 10 seconds
*/
/**************************************************************************/
void loop()
{

    if ((unsigned long)(millis() - lastCamera) >= 10000UL)
    {
        lastCamera = millis(); // reset timer
        capture_still();
               // function call -> capture image
    }
}
/**************************************************************************/
/**
  Initialise Camera
  Set camera parameters
  Based on CameraWebServer sample code by ESP32 Arduino
  \return true: successful, false: failed
 */
/**************************************************************************/
bool initialiseCamera()
{
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
    config.pixel_format = PIXFORMAT;
    config.frame_size = FRAME_SIZE_IMAGE;
    config.jpeg_quality = 10;
    config.fb_count = 1;

    // Check the esp32cam board has a PSRAM chip installed (extra memory used for storing captured images)
    // Note: if not using "AI thinker esp32 cam" in the Arduino IDE, PSRAM must be enabled
    if (!psramFound())
    {
        Serial.println("Warning: No PSRam found so defaulting to image size 'CIF'");
        config.frame_size = FRAMESIZE_CIF;
    }

    esp_err_t camerr = esp_camera_init(&config); // initialise the camera
    if (camerr != ESP_OK)
    {
        Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);
    }

    cameraImageSettings(); // Apply custom camera settings

    return (camerr == ESP_OK); // Return boolean result of camera initialisation
}
/**************************************************************************/
/**
  Camera Image Settings
  Set Image parameters
  Based on CameraWebServer sample code by ESP32 Arduino
  \return true: successful, false: failed
 */
/**************************************************************************/
bool cameraImageSettings()
{

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        Serial.println("Error: problem reading camera sensor settings");
        return 0;
    }

    // if both set to zero enable auto adjust
    if (cameraImageExposure == 0 && cameraImageGain == 0)
    {
        // enable auto adjust
        s->set_gain_ctrl(s, 1);     // auto gain on
        s->set_exposure_ctrl(s, 1); // auto exposure on
        s->set_awb_gain(s, 1);      // Auto White Balance enable (0 or 1)
    }
    else
    {
        // Apply manual settings
        s->set_gain_ctrl(s, 0);                   // auto gain off
        s->set_awb_gain(s, 1);                    // Auto White Balance enable (0 or 1)
        s->set_exposure_ctrl(s, 0);               // auto exposure off
        s->set_agc_gain(s, cameraImageGain);      // set gain manually (0 - 30)
        s->set_aec_value(s, cameraImageExposure); // set exposure manually  (0-1200)
    }

    return true;
}
/**************************************************************************/
/**
  Camera Image Settings
  Set Image parameters
  Based on CameraWebServer sample code by ESP32 Arduino
  \return true: successful, false: failed
 */
/**************************************************************************/
esp_err_t camera_capture(){
    //acquire a frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        return ESP_FAIL;
    }
    //threshold the image
    for (int i = 0; i < fb->len; i++) {
        // threshold the pixel at the current index
        fb->buf[i] = (fb->buf[i] > 128) ? 255 : 0;
    }
    
    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    //print serial ok
    Serial.println("Camera Capture OK");
    return ESP_OK;
}
/**************************************************************************/
/**
  Setup On Board Flash
  Initialize on board LED with pwm channel
 */
/**************************************************************************/

void setupOnBoardFlash()
{
    ledcSetup(ledChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(ledPin, ledChannel);
}
/**************************************************************************/
/**
  Set Led Brightness
  Set pwm value to change brightness of LED
 */
/**************************************************************************/
void setLedBrightness(byte ledBrightness)
{
    ledcWrite(ledChannel, ledBrightness);
}


// Calculates the point with the highest density of white pixels in the
// region of the image bounded by the specified y-coordinates.
// define a function to return the point with the highest white pixel density in a given horizontal region of the frame buffer
int get_max_density_point(const camera_fb_t *fb, int start_y, int end_y)
{
    // initialize the maximum density and the corresponding x-coordinate to zero
    int max_density = 0;
    int max_x = 0;

    // iterate over the rows in the given region of the image
    for (int y = start_y; y <= end_y; y++) {
        // initialize the current density to zero
        int cur_density = 0;

        // iterate over the columns in the current row
        for (int x = 0; x < fb->width; x++) {
            // get the current pixel value
            uint8_t pixel = fb->buf[y * fb->width + x];

            // if the pixel is white (i.e., its value is above the threshold)
            // then increment the current density
            if (pixel >= pixel_threshold) {
                cur_density++;
            }
        }

        // if the current density is greater than the maximum density so far,
        // then update the maximum density and the corresponding x-coordinate
        if (cur_density > max_density) {
            max_density = cur_density;
            max_x = x;
        }
    }

    // return the x-coordinate of the point with the highest density
    return max_x;
}

// Function that counts the amount of white pixels in the region of the image
// bounded by the specified x and y-coordinates. Returns if the threshold for white pixels is met.
bool countWhitePixels(int x1, int x2, int y1, int y2)
{
    // The current amount of white pixels
    int whitePixels = 0;

    // Loop through the region of the image bounded by the specified x and y-coordinates
    for (int y = y1; y < y2; y++)
    {
        for (int x = x1; x < x2; x++)
        {
            // Count the white pixels at the current point
            if (image[x][y] == 1)
            {
                whitePixels++;
            }
        }
    }

    // Return if the threshold for white pixels is met
    if (whitePixels > pixel_threshold)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// Function that splits the image into 3 horizontal regions and returns a vector
// containing the y-coordinates of the split points.
std::vector<int> splitImage()
{
    // The size of each region is 1/3 of the image height
    int regionSize = IMAGE_HEIGHT / 3;

    // The vector that will be returned, containing the y-coordinates
    // of the split points.
    std::vector<int> splitPoints;

    // The first split point is at the top of the image
    splitPoints.push_back(0);

    // The next two split points are at the middle and bottom of the
    // first and second regions, respectively.
    splitPoints.push_back(regionSize);
    splitPoints.push_back(2 * regionSize);

    // The last split point is at the bottom of the image
    splitPoints.push_back(IMAGE_HEIGHT);

    // Return the vector of split points.
    return splitPoints;
}

// Function that splits the image into 6 vertical regions and returns a vector
// containing the x-coordinates of the split points.
std::vector<int> splitImageVertically()
{
    // The size of each region is 1/6 of the image width
    int regionSize = IMAGE_WIDTH / 6;

    // The vector that will be returned, containing the x-coordinates
    // of the split points.
    std::vector<int> splitPoints;

    // The first split point is at the left of the image
    splitPoints.push_back(0);

    // The next five split points are at the middle of the first, second,
    // third, fourth, and fifth regions, respectively.
    splitPoints.push_back(regionSize);
    splitPoints.push_back(2 * regionSize);
    splitPoints.push_back(3 * regionSize);
    splitPoints.push_back(4 * regionSize);
    splitPoints.push_back(5 * regionSize);

    // The last split point is at the right of the image
    splitPoints.push_back(IMAGE_WIDTH);

    // Return the vector of split points.
    return splitPoints;
}


