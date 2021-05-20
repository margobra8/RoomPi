/*
 * main.c
 *
 *  Created on: 15 mar. 2021
 *      Author: Victoria M. Gullon and Marcos Gomez
 */

#include <stdio.h>
#include <wiringPi.h>

#define DEB

volatile int measurement_flags = 0x00;
volatile int output_flags = 0x00;

#include "libs/systemlib.h"
#include "libs/systemtype.h"
#include "controllers/measurementctrl.h"
#include "controllers/outputctrl.h"

SystemType *roompi_system;

SystemType* systemSetup(void) {

	printf("[LOG] System is being initialized and set up...\n");
	// wiringPi Setup
	wiringPiSetup();

	/* Creation of the attached sensors */
	// DHT11 Temperature and Humidity Creation and Setup
	printf("[LOG-DHT11Sensor] DHT11 Sensor is being initialized and set up...\n");
	DHT11Sensor *dht_sensor = DHT11Sensor__create(1, 29);

	// BH1750 Lux sensor Creation and Setup
	printf("[LOG-BH1750Sensor] BH1750 Sensor is being initialized and set up...\n");
	BH1750Sensor *bh_sensor = BH1750Sensor__create(4, 0x23, CONTINUOUS_H_RES);

	// CCS811 CO2 sensor Creation and Setup
	printf("[LOG-CCS811Sensor]  CCS811Sensor is being initialized and set up...\n");
	CCS811Sensor *ccs_sensor = CCS811Sensor__create(5, CCS811_ADDR_LOW, 0, 3, 2);

	CCS811Sensor__connect(ccs_sensor);

	union ApplicationRegister appreg1 = ccs_sensor->app_register;
	appreg1.meas_mode.reserved2 = 0;             //We dont know what reserved bits do, so let's put them zero
	appreg1.meas_mode.reserved1 = 0;             //We dont know what reserved bits do, so let's put them zero
	appreg1.meas_mode.driveMode = MODE1_EACH_1S; //Lets select the operation mode: MODE0_IDLE MODE1_EACH_1S MODE2_EACH_10S MODE3_EACH_60S MODE4_EACH_250MS
	appreg1.meas_mode.int_data_ready = 1;        //Lets enable the interrupt pin after we have a valid data
	appreg1.meas_mode.int_thresh = 0;            //We are going to disable the interrupts for thresholds

	CCS811Sensor__set_app_register(ccs_sensor, appreg1);
	CCS811Sensor__write_register(ccs_sensor, MEAS_MODE);

	/* Creation of the attached actuators */

	// Buzzer output creation and setup
	printf("[LOG-BuzzerOutput] BuzzerOutput Actuator is being initialized and set up...\n");
	BuzzerOutput *buzzer_actuator = BuzzerOutput__create(2, 26);

	// LED Array (status leds) creation and setup
	printf("[LOG-StatusLEDOutput] StatusLEDOutput Actuator is being initialized and set up...\n");
	int color_pins[] = { 0b00000011, 0b00011100, 0b11100000 };
	StatusLEDOutput *leds_actuator = StatusLEDOutput__create(3, 1, 25, 24, 23, color_pins);
	StatusLEDOutput__set_all_high(leds_actuator);
	//delay(5000);

	// LCD1602 Character display creation and setup
	printf("[LOG-LCD1602Display] LCD1602Display Actuator is being initialized and set up...\n");
	LCD1602Display *lcd_actuator = LCD1602Display__create(0, 15, 255, 16, 1, 10, 11, 31, 26, 1, 4, 5, 6);
	LCD1602Display__begin(lcd_actuator, 16, 2, 0);

	/* Creation of custom characters for the display */
	int clock[8] = { 0x1F, 0x11, 0x0A, 0x04, 0x0E, 0x1F, 0x1F, 0x00 }; // clock symbol for wait operations
	int good[8] = { 0x00, 0x0A, 0x0A, 0x0A, 0x00, 0x11, 0x0E, 0x00 }; // all good
	int general_bad[8] = { 0x00, 0x0A, 0x0A, 0x0A, 0x00, 0x0E, 0x11, 0x00 }; // bad
	int air[8] = { 0x00, 0x0C, 0x05, 0x1B, 0x14, 0x06, 0x00, 0x00 }; // Co2
	int temp[8] = { 0x04, 0x0A, 0x0A, 0x0A, 0x0A, 0x11, 0x13, 0x0E }; // temp
	int humid[8] = { 0x00, 0x04, 0x0A, 0x11, 0x11, 0x1F, 0x0E, 0x00 }; // humid
	int noise[8] = { 0x01, 0x03, 0x07, 0x1F, 0x1F, 0x07, 0x03, 0x01 }; // dBs
	int lux[8] = { 0x04, 0x15, 0x0E, 0x1B, 0x0E, 0x15, 0x04, 0x00 }; // lux
	LCD1602Display__create_char(lcd_actuator, 0, clock);
	LCD1602Display__create_char(lcd_actuator, 1, good);
	LCD1602Display__create_char(lcd_actuator, 2, general_bad);
	LCD1602Display__create_char(lcd_actuator, 3, air);
	LCD1602Display__create_char(lcd_actuator, 4, temp);
	LCD1602Display__create_char(lcd_actuator, 5, humid);
	LCD1602Display__create_char(lcd_actuator, 6, noise);
	LCD1602Display__create_char(lcd_actuator, 7, lux);

	LCD1602Display__clear(lcd_actuator);
	LCD1602Display__set_cursor(lcd_actuator, 0, 0);
	LCD1602Display__print(lcd_actuator, "roomPi      v4.0");
	LCD1602Display__set_cursor(lcd_actuator, 0, 1);
	LCD1602Display__write(lcd_actuator, 0);
	LCD1602Display__print(lcd_actuator, " Iniciando...");


	// System Context creation and initialization
	printf("[LOG] System is starting...\n");
	SystemContext *roompi_system_ctx = SystemContext__create(001, dht_sensor, bh_sensor, ccs_sensor, lcd_actuator, buzzer_actuator, leds_actuator);

	// Measurement subsystem creation and initialization
	MeasurementCtrl *measurement_ctrl = MeasurementCtrl__setup(roompi_system_ctx);

	// Output subsystem creation and initialization
	OutputCtrl *output_ctrl = OutputCtrl__setup(roompi_system_ctx);

	// Create and Setup Root System Type
	SystemType *roompi_system = SystemType__setup(roompi_system_ctx, measurement_ctrl, output_ctrl);

	return roompi_system;
}

int main(int argc, char **argv) {
	roompi_system = systemSetup();

	// si pulso boton activa measurement processing
	// si pulso este otro boton activa next display

	tmr_startms(roompi_system->root_measurement_ctrl->timer, 30000);
	tmr_startms(roompi_system->root_system->sensor_temp_humid->timer, 5000); // fire temp humid fsm every 5 seconds
	tmr_startms(roompi_system->root_system->sensor_light->timer, 5000); // fire light fsm every 5 seconds
	tmr_startms(roompi_system->root_system->sensor_co2->timer, 5000);  // fire co2 fsm every 5 seconds


	// Output system timer
	tmr_startms(roompi_system->root_output_ctrl->timer, 5000);

	//output_flags |= FLAG_NEXT_DISPLAY;
	measurement_flags |= FLAG_LIGHT_PENDING_MEASUREMENT;
	measurement_flags |= FLAG_TEMP_HUMID_PENDING_MEASUREMENT;
	measurement_flags |= FLAG_CO2_PENDING_MEASUREMENT;


	StatusLEDOutput__set_color(roompi_system->root_system->actuator_leds, GREEN);

	while (1) {
		fsm_fire(roompi_system->root_measurement_ctrl->fsm);
		fsm_fire(roompi_system->root_system->sensor_temp_humid->fsm);
		fsm_fire(roompi_system->root_system->sensor_light->fsm);
		fsm_fire(roompi_system->root_system->sensor_co2->fsm);
		fsm_fire(roompi_system->root_output_ctrl->fsm_buzzer);
		fsm_fire(roompi_system->root_output_ctrl->fsm_leds);
		fsm_fire(roompi_system->root_output_ctrl->fsm_info);
		fsm_fire(roompi_system->root_output_ctrl->fsm_warnings);
	}

	SystemType__destroy(roompi_system);

	return 0;
}
