#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"


#include "board_config.h" 
#include "index_html.h"


// Network Credentials (Access Point)
const char* ssid = "FPV_RC_CAR";    
const char* password = "12345678";  

// PWM Settings
#define PWM_FREQ        1000   // 1000 Hz for DC Motors
#define PWM_RES         8      // 8-bit resolution

#define SPEED_STEER_MAX 255    // Max power for hard turns
#define SPEED_STEER_MIN 180    // Min power to actually move the wheels
int currentSpeed = 200;        // Drive speed (controlled by slider)


typedef struct 
{
    int pin_FWD;       // Forward / Left Pin
    int pin_REV;       // Reverse / Right Pin
} 
Motor_t;

// 4WD SYSTEM: BTS7960 (D0/D1)
Motor_t driveMotors = {D0, D1}; 

// STEERING: DRV8833 (D2/D3)
Motor_t steerMotor = {D2, D3}; 


void Motor_Init(Motor_t m) 
{
    pinMode(m.pin_FWD, OUTPUT);
    pinMode(m.pin_REV, OUTPUT);
    digitalWrite(m.pin_FWD, LOW);
    digitalWrite(m.pin_REV, LOW);
    
    // Attach PWM (ESP32 v3.0 Syntax)
    ledcAttach(m.pin_FWD, PWM_FREQ, PWM_RES);
    ledcAttach(m.pin_REV, PWM_FREQ, PWM_RES);
}

void Motor_Drive(Motor_t m, int speed) 
{
    if (speed > 0) 
    {
        ledcWrite(m.pin_FWD, speed);
        ledcWrite(m.pin_REV, 0);
    } else if (speed < 0) 
    {
        ledcWrite(m.pin_FWD, 0);
        ledcWrite(m.pin_REV, abs(speed));
    } else {
        ledcWrite(m.pin_FWD, 0);
        ledcWrite(m.pin_REV, 0);
    }
}

//SERVER LOGIC

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static esp_err_t index_handler(httpd_req_t *req) 
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

// Updates the Max Speed based on the Slider
static esp_err_t speed_handler(httpd_req_t *req) 
{
    char* buf;
    size_t buf_len;
    char variable[32] = {0,};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) 
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) 
        {
            if (httpd_query_key_value(buf, "value", variable, sizeof(variable)) == ESP_OK)
            {
                currentSpeed = atoi(variable);
            }
        }
        free(buf);
    }
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

//Joystick Commands
static esp_err_t action_handler(httpd_req_t *req) 
{
    char* buf;
    size_t buf_len;
    char variable[32] = {0,};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) 
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) 
        {
            if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) 
            {
                
                // Use 'currentSpeed' for variable throttle
                if (!strcmp(variable, "forward")) 
                { 
                    Motor_Drive(driveMotors, currentSpeed); 
                    Motor_Drive(steerMotor, 0); // Center Steering
                }
                else if (!strcmp(variable, "backward")) 
                { 
                    Motor_Drive(driveMotors, -currentSpeed); 
                    Motor_Drive(steerMotor, 0); 
                }
                else if (!strcmp(variable, "left")) 
                { 
                    Motor_Drive(steerMotor, SPEED_STEER_MAX); 
                }
                else if (!strcmp(variable, "right")) 
                { 
                    Motor_Drive(steerMotor, -SPEED_STEER_MAX); 
                }
                else if (!strcmp(variable, "stop")) 
                { 
                    Motor_Drive(driveMotors, 0); 
                    Motor_Drive(steerMotor, 0); 
                }
                else if (!strcmp(variable, "straight")) 
                { 
                    Motor_Drive(steerMotor, 0); 
                }
            }
        }
        free(buf);
    }
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req) 
{
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char * part_buf[64];
    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
    static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) 
    {
        fb = esp_camera_fb_get();
        if (!fb) 
        {
            res = ESP_FAIL;
        } else 
        {
            if (req->method == HTTP_GET) 
            {
                res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            } else { res = ESP_FAIL; }
            if (res == ESP_OK) {
                size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if (res == ESP_OK) { res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len); }
            esp_camera_fb_return(fb);
            if (res != ESP_OK) break;
        }
    }
    return res;
}

void startCameraServer() 
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
    httpd_uri_t action_uri = { .uri = "/action", .method = HTTP_GET, .handler = action_handler, .user_ctx = NULL };
    httpd_uri_t speed_uri = { .uri = "/speed", .method = HTTP_GET, .handler = speed_handler, .user_ctx = NULL };
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &action_uri);
        httpd_register_uri_handler(camera_httpd, &speed_uri);
    }
    config.server_port += 1;
    config.ctrl_port += 1;
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}


//  MAIN SETUP

void setup() 
{
    Serial.begin(115200);
    
    //  Init Camera (ULTRA-FAST SETTINGS)
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
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    
    
    config.frame_size = FRAMESIZE_QVGA;   // 320x240 (Fastest standard size)
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST; // Only grab the newest frame (Key fix!)
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 15;             // 10-20 is the sweet spot
    config.fb_count = 2;                  // Lower buffer count reduces latency
    // ---------------------

    if (esp_camera_init(&config) != ESP_OK) 
    {
        Serial.println("Camera Init Failed");
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_hmirror(s, 1); // 1 = Flip Horizontal, 0 = Normal
    s->set_vflip(s, 0);   // 1 = Flip Vertical (if mounted upside down)

  
    WiFi.mode(WIFI_AP);
   
    WiFi.softAP(ssid, password); 
    
    Serial.print("AP Started! Connect to: ");
    Serial.println(ssid);
    Serial.print("Go to: http://");
    Serial.println(WiFi.softAPIP());

    //Start Server
    startCameraServer();

    //Init Motors
    Motor_Init(driveMotors);
    Motor_Init(steerMotor);
    Serial.println("Motors Ready");
}

void loop() { delay(10000); }