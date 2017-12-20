/***************************************************************************//**
 *   @file   pe4312.h
 *   @brief  
 *   @author 
********************************************************************************/
#ifndef __PE4312_H__
#define __PE4312_H__

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include <stdint.h>

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/

/* Channels */
#define ADF4350_RX_CHANNEL	0
#define ADF4350_TX_CHANNEL	1

/* Registers */
#define ADF4350_REG0	0
#define ADF4350_REG1	1
#define ADF4350_REG2	2
#define ADF4350_REG3	3
#define ADF4350_REG4	4
#define ADF4350_REG5	5

/* REG0 Bit Definitions */
#define ADF4350_REG0_FRACT(x)					(((x) & 0xFFF) << 3)
#define ADF4350_REG0_INT(x)						(((x) & 0xFFFF) << 15)

/* REG1 Bit Definitions */
#define ADF4350_REG1_MOD(x)						(((x) & 0xFFF) << 3)
#define ADF4350_REG1_PHASE(x)					(((x) & 0xFFF) << 15)
#define ADF4350_REG1_PRESCALER					(1 << 27)

/* REG2 Bit Definitions */
#define ADF4350_REG2_COUNTER_RESET_EN			(1 << 3)
#define ADF4350_REG2_CP_THREESTATE_EN			(1 << 4)
#define ADF4350_REG2_POWER_DOWN_EN				(1 << 5)
#define ADF4350_REG2_PD_POLARITY_POS			(1 << 6)
#define ADF4350_REG2_LDP_6ns					(1 << 7)
#define ADF4350_REG2_LDP_10ns					(0 << 7)
#define ADF4350_REG2_LDF_FRACT_N				(0 << 8)
#define ADF4350_REG2_LDF_INT_N					(1 << 8)
#define ADF4350_REG2_CHARGE_PUMP_CURR_uA(x)		(((((x)-312) / 312) & 0xF) << 9)
#define ADF4350_REG2_DOUBLE_BUFF_EN				(1 << 13)
#define ADF4350_REG2_10BIT_R_CNT(x)				((x) << 14)
#define ADF4350_REG2_RDIV2_EN					(1 << 24)
#define ADF4350_REG2_RMULT2_EN					(1 << 25)
#define ADF4350_REG2_MUXOUT(x)					((x) << 26)
#define ADF4350_REG2_NOISE_MODE(x)				((x) << 29)

/* REG3 Bit Definitions */
#define ADF4350_REG3_12BIT_CLKDIV(x)			((x) << 3)
#define ADF4350_REG3_12BIT_CLKDIV_MODE(x)		((x) << 16)
#define ADF4350_REG3_12BIT_CSR_EN				(1 << 18)
#define ADF4351_REG3_CHARGE_CANCELLATION_EN		(1 << 21)
#define ADF4351_REG3_ANTI_BACKLASH_3ns_EN		(1 << 22)
#define ADF4351_REG3_BAND_SEL_CLOCK_MODE_HIGH	(1 << 23)

/* REG4 Bit Definitions */
#define ADF4350_REG4_OUTPUT_PWR(x)				((x) << 3)
#define ADF4350_REG4_RF_OUT_EN					(1 << 5)
#define ADF4350_REG4_AUX_OUTPUT_PWR(x)			((x) << 6)
#define ADF4350_REG4_AUX_OUTPUT_EN				(1 << 8)
#define ADF4350_REG4_AUX_OUTPUT_FUND			(1 << 9)
#define ADF4350_REG4_AUX_OUTPUT_DIV				(0 << 9)
#define ADF4350_REG4_MUTE_TILL_LOCK_EN			(1 << 10)
#define ADF4350_REG4_VCO_PWRDOWN_EN				(1 << 11)
#define ADF4350_REG4_8BIT_BAND_SEL_CLKDIV(x)	((x) << 12)
#define ADF4350_REG4_RF_DIV_SEL(x)				((x) << 20)
#define ADF4350_REG4_FEEDBACK_DIVIDED			(0 << 23)
#define ADF4350_REG4_FEEDBACK_FUND				(1 << 23)

/* REG5 Bit Definitions */
#define ADF4350_REG5_LD_PIN_MODE_LOW			(0 << 22)
#define ADF4350_REG5_LD_PIN_MODE_DIGITAL		(1 << 22)
#define ADF4350_REG5_LD_PIN_MODE_HIGH			(3 << 22)

/* Specifications */
#define ADF4350_MAX_OUT_FREQ		4400000000ULL /* Hz */
#define ADF4350_MIN_OUT_FREQ		34375000 /* Hz */
#define ADF4350_MIN_VCO_FREQ		2200000000ULL /* Hz */
#define ADF4350_MAX_FREQ_45_PRESC	3000000000ULL /* Hz */
#define ADF4350_MAX_FREQ_PFD		32000000 /* Hz */
#define ADF4350_MAX_BANDSEL_CLK		125000 /* Hz */
#define ADF4350_MAX_FREQ_REFIN		250010000 /* Hz reased to allow for adjustment*/
#define ADF4350_MAX_MODULUS			4095
#define ADF4350_MAX_R_CNT			1023




#define PE4312_MIN_ATTENUATION	     0.0 	// dB
#define PE4312_MAX_ATTENUATION	    31.5 	// dB
#define PE4312_DISPLAY_NAME     "PE43x2"




/******************************************************************************/
/************************ Types Definitions ***********************************/
/******************************************************************************/
struct adf4350_platform_data
{
	uint32_t	clkin;
	uint32_t	channel_spacing;
	uint64_t	power_up_frequency;
 
	uint16_t	ref_div_factor; /* 10-bit R counter */
	uint8_t	    ref_doubler_en;
	uint8_t	    ref_div2_en;
 
	uint32_t    r2_user_settings;
	uint32_t    r3_user_settings;
	uint32_t    r4_user_settings;
	int32_t	    gpio_lock_detect;
};

typedef struct
{
	uint32_t	clkin;
	uint32_t	channel_spacing;
	uint32_t	power_up_frequency;
	uint32_t	reference_div_factor;
	uint8_t		reference_doubler_enable;
	uint8_t		reference_div2_enable;

	/* r2_user_settings */
	uint8_t		phase_detector_polarity_positive_enable;
	uint8_t		lock_detect_precision_6ns_enable;
	uint8_t		lock_detect_function_integer_n_enable;
	uint32_t	charge_pump_current;
	uint32_t	muxout_select;
	uint8_t		low_spur_mode_enable;

	/* r3_user_settings */
	uint8_t		cycle_slip_reduction_enable;
	uint8_t		charge_cancellation_enable;
	uint8_t		anti_backlash_3ns_enable;
	uint8_t		band_select_clock_mode_high_enable;
	uint32_t	clk_divider_12bit;
	uint32_t	clk_divider_mode;

	/* r4_user_settings */
	uint8_t		aux_output_enable;
	uint8_t		aux_output_fundamental_enable;
	uint8_t		mute_till_lock_enable;
	uint32_t	output_power;
	uint32_t	aux_output_power;
}adf4350_init_param;

/******************************************************************************/
/************************ Functions Declarations ******************************/
/******************************************************************************/
/*! Initializes the ADF4350. */
int32_t adf4350_setup(uint32_t spi_device_id, uint8_t slave_select,
		adf4350_init_param init_param);
/*! Writes 4 bytes of data to ADF4350. */
int32_t adf4350_write(uint32_t data);
/*! Stores PLL 0 frequency in Hz. */
int64_t adf4350_out_altvoltage0_frequency(int64_t Hz);
/*! Stores PLL 0 frequency resolution/channel spacing in Hz. */
int32_t adf4350_out_altvoltage0_frequency_resolution(int32_t Hz);
/*! Sets PLL 0 REFin frequency in Hz. */
int64_t adf4350_out_altvoltage0_refin_frequency(int64_t Hz);
/*! Powers down the PLL.  */
int32_t adf4350_out_altvoltage0_powerdown(int32_t pwd);


int pe4312_set_level(float level);


#endif // __PE4312_H__

