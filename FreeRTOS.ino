#define WEBPAGE

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <SPIFFS.h>

AsyncWebServer server(80);
AsyncEventSource dataWebSocket("/data");
void setup_server(void);

#define DEFAULT_WIFI_SSID "tehtnicaWiFi"
#define DEFAULT_WIFI_PASS ""

#include "scale.h"

const int LOADCELL_DOUT_PIN = 15;
const int LOADCELL_SCK_PIN = 2;

xTaskHandle uartDriverHandler;
void uartDriver(void *pvParam);

xTaskHandle readWeightTaskHandler;
void readWeight(void *pvParams);

xTaskHandle calibrateScaleTaskHandler;
void calibrateScaleTask(void *pvParams);

xTaskHandle sendDataTaskHandler;
void sendDataTask(void *pvParams);

xTaskHandle webSocketDriverHandler;
void webSocketDriver(void *pvParam);

QueueHandle_t wsTXQueue;
QueueHandle_t wsRXQueue;
QueueHandle_t uartTXQueue;
QueueHandle_t uartRXQueue;

#ifdef WEBPAGE
Scale scale(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN, &wsTXQueue, &wsRXQueue);
#else
Scale scale(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN, &uartTXQueue, &uartRXQueue);
#endif

void setup()
{
	Serial.begin(115200);

	wsRXQueue = xQueueCreate(5, sizeof(QUEUE_ELEMENT));
	wsTXQueue = xQueueCreate(5, sizeof(QUEUE_ELEMENT));

#ifdef WEBPAGE
	IPAddress local_IP(192, 168, 1, 184);
	IPAddress gateway(192, 168, 1, 1);
	IPAddress subnet(255, 255, 0, 0);

	WiFi.mode(WIFI_AP);
	WiFi.softAP(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
	WiFi.softAPConfig(local_IP, gateway, subnet);

	setup_server();
#endif

	uartRXQueue = xQueueCreate(5, sizeof(QUEUE_ELEMENT));
	uartTXQueue = xQueueCreate(5, sizeof(QUEUE_ELEMENT));

	scale.tare();

	xTaskCreatePinnedToCore(readWeight, "weight", 4 * 2 * 256, NULL, 1, &readWeightTaskHandler, ARDUINO_RUNNING_CORE);
	xTaskCreatePinnedToCore(sendDataTask, "sendData", 8 * 4 * 256, NULL, 1, &sendDataTaskHandler, ARDUINO_RUNNING_CORE);
	xTaskCreatePinnedToCore(webSocketDriver, "ws", 8 * 4 * 256, NULL, 1, &webSocketDriverHandler, ARDUINO_RUNNING_CORE);
	xTaskCreatePinnedToCore(uartDriver, "uart", 8 * 4 * 256, NULL, 1, &uartDriverHandler, ARDUINO_RUNNING_CORE);
}

void loop()
{
	for (;;)
		delay(1);
}

void calibrateScale(void *pvParams)
{
	scale.calibrate();
	vTaskDelete(NULL);
}

void readWeight(void *pvParams) // This is a task.
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	for (;;)
	{
		delay(1);
		float grams = scale.getGrams();
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
	}
}

void uartDriver(void *pvParam)
{
	uint8_t charCount = 0;
	QUEUE_ELEMENT RXdata = {0};
	QUEUE_ELEMENT TXdata = {0};
	while (1)
	{
		delay(1);
		if (xQueueReceive(uartTXQueue, (void *)&TXdata, 0) == pdPASS)
			Serial.println(TXdata.text);
		if (Serial.available() > 0)
		{
			uint8_t readChar = Serial.read();
			RXdata.text[charCount] = readChar;
			charCount++;
			if (charCount == 3)
			{
				RXdata.text[charCount] = 0; // null termination
				charCount = 0;
				xQueueSend(uartRXQueue, (void *)&RXdata, 0);
			}
		}
	}
}

void sendDataTask(void *pvParam)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1)
	{
		delay(1);
		char sendBuffer[20] = {0};
		sprintf(sendBuffer, "%.2f", scale.weightGrams);
#ifdef WEBPAGE
		dataWebSocket.send(sendBuffer, "weight", millis());
#else
		Serial.println(sendBuffer);
#endif

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
	}
}

void webSocketDriver(void *pvParam)
{
	QUEUE_ELEMENT TXdata = {0};
	for (;;)
	{
		delay(1);
		if (xQueueReceive(wsTXQueue, (void *)&TXdata, 0) == pdPASS)
		{

			dataWebSocket.send(TXdata.text, "console", millis());
			Serial.print(TXdata.text);
		}
	}
}

void setup_server(void)
{
	if (!SPIFFS.begin(false))
	{
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}
	dataWebSocket.onConnect([](AsyncEventSourceClient *client)
							{
								if (client->lastId())
								{
									Serial.printf("Client reconnected! Last message ID that it got is: %u\n",
												  client->lastId());
								} });

	server.addHandler(&dataWebSocket);
	server.onNotFound([](AsyncWebServerRequest *request)
					  {
						  AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", "The content you are looking for was not found.");
						  response->addHeader("Connection", "Keep-Alive");
						  request->send(response); });

	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
			  { request->send(SPIFFS, "/home.html", "text/html", false); });

	server.on("/src/bootstrap.bundle.min.js", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse(
					  SPIFFS, "/src/bootstrap.bundle.min.js", "text/javascript");
				  request->send(response);
			  });

	server.on("/src/bootstrap.min.css", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse(
					  SPIFFS, "/src/bootstrap.min.css", "text/css");
				  request->send(response);
			  });

	server.on("/src/bootstrap.min.js", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse(
					  SPIFFS, "/src/bootstrap.min.js", "text/javascript");
				  request->send(response);
			  });
	server.on("/src/jquery.min.js", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse(
					  SPIFFS, "/src/jquery.min.js", "text/javascript");
				  request->send(response);
			  });
	server.on("/calibrate", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  xTaskCreatePinnedToCore(calibrateScale, "calibrate", 16 * 256, NULL, 2, &calibrateScaleTaskHandler, ARDUINO_RUNNING_CORE);
				  request->send(200, "text/html", "");
			  });

	server.on("/tare", HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  scale.tare();
				  request->send(200, "text/html", "");
			  });

	server.begin();
}
