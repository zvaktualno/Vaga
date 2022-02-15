
#ifndef _SCALE_H_
#define _SCALE_H_

typedef enum
{
	SCALE_GAIN_64 = 3,
	SCALE_GAIN_128 = 1
} SCALEGAIN;

typedef struct
{
	char text[60];
} QUEUE_ELEMENT;

class Scale
{
private:
	uint8_t dataPin = 0;
	uint8_t sckPin = 0;
	uint8_t gain = 1;
	int32_t offset = 0;
	QueueHandle_t *TXQueue;
	QueueHandle_t *RXQueue;
	int32_t readRawVal(void);
	int32_t val(void);
	float bitsPerGram = 1937.70;
	uint8_t calibrated = 1;
	uint8_t ready(void);
	void sendMessage(char *msg);
	void sendIntMessage(uint32_t number);

public:
	Scale(uint8_t _dt_pin, uint8_t _sck_pin, QueueHandle_t *txqueue, QueueHandle_t *rxqueue);
	void tare(void);
	void setGain(SCALEGAIN _gain);
	float getGrams(void);
	float getKilograms(void);
	void calibrate(void);
	uint8_t getByte(void);
	float weightGrams;
};

#endif