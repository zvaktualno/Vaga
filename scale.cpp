#include <Arduino.h>
#include <stdint.h>
#include "scale.h"

Scale::Scale(uint8_t _dataPin, uint8_t _sckPin, QueueHandle_t *txqueue, QueueHandle_t *rxqueue)
{
	dataPin = _dataPin;
	sckPin = _sckPin;
	pinMode(sckPin, OUTPUT);
	pinMode(dataPin, INPUT);
	TXQueue = txqueue;
	RXQueue = rxqueue;
}

void Scale::setGain(SCALEGAIN _gain)
{
	switch (_gain)
	{
	case SCALE_GAIN_128:
		gain = 1;
		break;
	case SCALE_GAIN_64:
		gain = 3;
		break;
	default:
		gain = 3;
		break;
	}
}
void Scale::tare(void)
{
	sendMessage("Zero point set");
	offset = readRawVal();
}

uint8_t Scale::ready(void)
{
	return (uint8_t)!digitalRead(dataPin);
}

uint8_t Scale::getByte(void)
{
	uint8_t value = 0;
	for (uint8_t i = 0; i < 8; ++i)
	{
		digitalWrite(sckPin, 1);
		delayMicroseconds(1);
		value |= digitalRead(dataPin) << (7 - i);
		digitalWrite(sckPin, 0);
		delayMicroseconds(1);
	}
	return value;
}

int32_t Scale::readRawVal(void)
{

	while (!ready())
		delay(1);

	uint8_t data[3] = {0};

	portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
	portENTER_CRITICAL(&mux);

	data[2] = getByte();
	data[1] = getByte();
	data[0] = getByte();

	for (uint8_t i = 0; i < gain; i++)
	{
		digitalWrite(sckPin, 1);
		delayMicroseconds(1);
		digitalWrite(sckPin, 0);
		delayMicroseconds(1);
	}

	portEXIT_CRITICAL(&mux);

	// pad
	if (data[2] & 0x80)
		data[3] = 0xFF;
	else
		data[3] = 0x00;

	int32_t value = 0;

	if (data[2] & 0x80)
		value = 0xFFl << 24;

	value = ((uint32_t)(data[2]) << 16 |
			 (uint32_t)(data[1]) << 8 |
			 (uint32_t)(data[0]) << 0);

	return value;
}

int32_t Scale::val(void)
{
	int32_t val = readRawVal();

	val -= offset;
	return val;
}

float Scale::getGrams(void)
{
	if (!calibrated)
	{
		return 0;
		sendMessage("WARNING! Scale is not calibrated.");
	}
	weightGrams = val() / bitsPerGram;
	return weightGrams;
}

float Scale::getKilograms(void)
{
	return getGrams() / 1000.0f;
}

void Scale::sendMessage(char *msg)
{
	QUEUE_ELEMENT message;
	strcpy(message.text, msg);
	xQueueSend(*TXQueue, (void *)&message, 0);
}

void Scale::sendIntMessage(uint32_t number)
{
	char message[60] = {0};
	sprintf(message, "%lu\n", number);
	sendMessage(message);
}

void Scale::calibrate(void)
{
	sendMessage("Calibrating");

	calibrated = 0;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	sendMessage("Remove all weight.");
	sendMessage("Setting zero point in: ");

	for (uint8_t i = 0; i < 5; i++)
	{
		sendIntMessage(4 - i);
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
	}
	tare();
	sendMessage("Put a weight between 100g and 500g on the scale.");
	int32_t calVal;
	xLastWakeTime = xTaskGetTickCount();
	while (1)
	{
		uint32_t cal_val = abs(val());
		if (cal_val > 100000)
			break;
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
	}

	sendMessage("Weight detected.");
	xLastWakeTime = xTaskGetTickCount();
	vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
	float weight_mass = 125.83;
	int32_t currentWeight = val();
	bitsPerGram = (float)currentWeight / weight_mass;
	calibrated = 1;
}