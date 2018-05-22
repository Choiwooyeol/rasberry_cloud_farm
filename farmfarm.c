#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <sys/types.h>
#include <errno.h>
#include <wiringPi.h>
#include <stdint.h>
#include <string.h> 
#include <signal.h> 
#include <softPwm.h>
#include <wiringPiSPI.h> 
#include <time.h>

/*****************

2013041023

choi wooyeol

*****************/

/////////////////DB set///////////////
#define DBHOST "localhost"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "test"

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;
//////////////////////////////////////

/////////////LED, FAN//////////////

#define RGBLEDPOWER  19 //BCM_GPIO 19
#define RED 8 //27
#define GREEN   7 //28
#define BLUE    9 //29
#define FAN 6

///////////////////////////////

///////////Light sensor////////////////
#define CS_MCP3208 8 //GPIO 8
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000 //1Mhz
///////////////////////////////////////

///////////THREAD//////////

pthread_mutex_t led_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t led_cond		= PTHREAD_COND_INITIALIZER;

pthread_mutex_t temp_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t temp_cond	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t light_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t light_cond	= PTHREAD_COND_INITIALIZER;

pthread_mutex_t fan_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fan_cond		= PTHREAD_COND_INITIALIZER;

pthread_mutex_t send_mutex	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t send_cond	= PTHREAD_COND_INITIALIZER;
//////////////////////////////////////

#define MAX_SIZE 1000
#define UPLOAD_TIME 10
#define MAXTIMINGS 85

//////////////////////////////////////

int ret_temp;
int ret_light;

//temp set
int count_temp = 0;
int fill_temp = 0;
int use_temp = 0;
int arr_temp[MAX_SIZE];

//light set
int count_light = 0;
int fill_light = 0;
int use_light = 0;
int arr_light[MAX_SIZE];

static int DHTPIN = 7;
//static int DHTPIN = 11;
static int dht22_dat[5] = {0,0,0,0,0};

void sig_handler(int signo);
int read_dht22_dat(); // temp
int read_mcp3208_adc(unsigned char adcChannel); // light
int put_temp(int value);
int get_temp();
int put_light(int value);
int get_light();
void *getSensor(void *arg);
void *sendSensor(void *arg);
void *ControllFAN(void *arg);
void *ControllLED(void *arg);

int main (int argc, char* argv[])
{
	signal(SIGINT, (void*)sig_handler);
	pthread_t pt[4]; // 0 get 1 set 2 fan 3 led
	//pthread_t uploader_thread;
	//pthread_t detect_thread;
	//pthread_t fan_thread;
     
    	// MySQL connection
    	connector = mysql_init(NULL);
    	if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
    	{
        	fprintf(stderr, "%s\n", mysql_error(connector));
        	return 0;
    	}
    	if (wiringPiSetup() == -1)
        	exit(EXIT_FAILURE) ;

    	if (wiringPiSetupGpio() == -1)
        	fprintf(stdout, "Unable to start wiringPi :%s\n", strerror(errno));

    	if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
    	{
        	fprintf(stdout, "wiringPiSPISetup Failed :%s\n", strerror(errno));
        	return 1;
    	}

    	if (setuid(getuid()) < 0)
    	{
        	perror("Dropping privileges failed\n");
        	exit(EXIT_FAILURE);
    	}
    	printf("DB opened.\n"); 
	pinMode(CS_MCP3208, OUTPUT);
   	pinMode(RGBLEDPOWER, OUTPUT);
	while(1){
		pthread_create(&pt[0], NULL, getSensor, "get");
		pthread_create(&pt[1], NULL, sendSensor, "set");
		pthread_create(&pt[2], NULL, ControllFAN, "fan");
		pthread_create(&pt[3], NULL, ControllLED, "led");
		
		//sleep(3);
		
		pthread_join(pt[0], NULL);
	    	pthread_join(pt[1], NULL);
		pthread_join(pt[2], NULL);
		pthread_join(pt[3], NULL);
	}
    	mysql_close(connector);
	
    	return 0;
}

//////////signal_handler////////////
void sig_handler(int signo){
    mysql_close(connector);	
    printf("MYSQL is closed\n");
    digitalWrite(RGBLEDPOWER, 0);
	digitalWrite(FAN, 0);
	printf("process stop\n");
	exit(0);
	}

int put_temp(int value){
	arr_temp[fill_temp] = value;
	fill_temp = (fill_temp+1) % MAX_SIZE;
	count_temp++;
}

int get_temp(){
	int tmp = arr_temp[use_temp];
	use_temp = (use_temp+1) % MAX_SIZE;
	count_temp--;
	return tmp;
}

int put_light(int value){
	arr_light[fill_light] = value;
	fill_light = (fill_light+1) % MAX_SIZE;
	count_light++;
}

int get_light(){
	int light = arr_light[use_light];
	use_light = (use_light+1) % MAX_SIZE;
	count_light--;
	return light;
}


int read_mcp3208_adc(unsigned char adcChannel)
{
    unsigned char buff[3];
    int adcValue = 0;

    buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
    buff[1] = ((adcChannel & 0x07) << 6);
    buff[2] = 0x00;
    digitalWrite(CS_MCP3208, 0);
    wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);
    printf("%s", buff);

    buff[1] = 0x0f & buff[1];
    adcValue = (buff[1] << 8 ) | buff[2];
    digitalWrite(CS_MCP3208, 1);
    ret_light = adcValue;
    return adcValue;
}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
  < 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

int read_dht22_dat()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40); 
  // prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 50)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
		
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
		
		ret_temp = (int)t;
    return 1;
  }
  else
  {
    //printf("Data not good, skip\n");
    return 0;	
  }
}


void *getSensor(void *arg) {
	
	char ex_time[100];
	int adcValue_light = 0;
	unsigned char adcChannel_light = 0;
    	int ret=0;
	
    while (1) {
	//pthread_mutex_lock(&mutex);
	//pthread_cond_wait(&empty,&mutex);
	while (read_dht22_dat() == 0)
		delay(5);
        pthread_mutex_lock(&temp_mutex);
	while (count_temp == MAX_SIZE)
		pthread_cond_wait(&temp_cond, &temp_mutex);
	put_temp(ret_temp);
	pthread_cond_signal(&temp_cond);
	pthread_mutex_unlock(&temp_mutex);
	//////////FAN
	if(ret_temp > 24) // 집온도가너무높습니다..
		pthread_cond_signal(&fan_cond);
	//////////LIGHT
	pthread_mutex_lock(&light_mutex);
	adcValue_light = read_mcp3208_adc(adcChannel_light);
	pthread_cond_signal(&led_cond);
	pthread_cond_signal(&send_cond);
	while (count_light == MAX_SIZE)
		pthread_cond_wait(&light_cond, &light_mutex);
	put_light(adcValue_light);
	printf("now temp = %d, light = %d\n", ret_temp,ret_light);
	pthread_cond_signal(&light_cond);
	pthread_mutex_unlock(&light_mutex);
	delay(100);
	}
	return NULL;
}

void *sendSensor(void *arg){
	int temp;
    	int light;   
	char query[1024];
	while (1) {
		pthread_mutex_lock(&send_mutex);
		pthread_cond_wait(&send_cond,&send_mutex);
		sleep(UPLOAD_TIME);
		printf("\nQuery sending\n");
		/*
		pthread_mutex_lock(&temp_mutex);
		while (count_temp == 0)
			pthread_cond_wait(&temp_cond, &temp_mutex);
		temp = get_temp();
		pthread_mutex_unlock(&temp_mutex);
		pthread_mutex_lock(&light_mutex);
		while (count_light == 0)
			pthread_cond_wait(&light_cond, &light_mutex);
		light = get_light();
		pthread_mutex_unlock(&light_mutex);
		*/
		temp = ret_temp;
		light= ret_light;
		sprintf(query, "INSERT INTO cloud_farm VALUES (now(),%d,%d)",temp, light);
		if (mysql_query(connector, query))
		{
			fprintf(stderr, "%s\n", mysql_error(connector));
			printf("Write DB error\n");
		}
		printf("\ntemp = %d, light = %d", temp,light);
		pthread_mutex_unlock(&send_mutex);
	}
	return NULL;
}

void *ControllFAN(void *arg) {
	while (1) {
		pthread_mutex_lock(&fan_mutex);
		pthread_cond_wait(&fan_cond, &fan_mutex);
		digitalWrite(FAN, 1);
		sleep(5);
		digitalWrite(FAN, 0);
		sleep(5);
		pthread_mutex_unlock(&fan_mutex);
	}
	return NULL;
}

void *ControllLED(void *arg) {
	while (1) {
		pthread_mutex_lock(&led_mutex);
		pthread_cond_wait(&led_cond, &led_mutex);
		if(ret_light > 10)
			digitalWrite(RGBLEDPOWER, 0);
		else
			digitalWrite(RGBLEDPOWER, 1);
		sleep(1);
		pthread_mutex_unlock(&led_mutex);
	}
	return NULL;
}