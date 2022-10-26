#define	BPI_M1P_01	-1
#define	BPI_M1P_03	GPIO_PB21
#define	BPI_M1P_05	GPIO_PB20
#define	BPI_M1P_07	GPIO_PI03
#define	BPI_M1P_09	-1
#define	BPI_M1P_11	GPIO_PI19
#define	BPI_M1P_13	GPIO_PI18
#define	BPI_M1P_15	GPIO_PI17
#define	BPI_M1P_17	-1
#define	BPI_M1P_19	GPIO_PI12
#define	BPI_M1P_21	GPIO_PI13
#define	BPI_M1P_23	GPIO_PI11
#define	BPI_M1P_25	-1
#define	BPI_M1P_27	GPIO_PI01
#define	BPI_M1P_29	GPIO_PB05
#define	BPI_M1P_31	GPIO_PB06
#define	BPI_M1P_33	GPIO_PB07
#define	BPI_M1P_35	GPIO_PB08
#define	BPI_M1P_37	GPIO_PB03
#define	BPI_M1P_39	-1

#define	BPI_M1P_02	-1
#define	BPI_M1P_04	-1
#define	BPI_M1P_06	-1
#define	BPI_M1P_08	GPIO_PH00
#define	BPI_M1P_10	GPIO_PH01
#define	BPI_M1P_12	GPIO_PH02
#define	BPI_M1P_14	-1
#define	BPI_M1P_16	GPIO_PH20
#define	BPI_M1P_18	GPIO_PH21
#define	BPI_M1P_20	-1
#define	BPI_M1P_22	GPIO_PI16
#define	BPI_M1P_24	GPIO_PI10
#define	BPI_M1P_26	GPIO_PI14
#define	BPI_M1P_28	GPIO_PI00
#define	BPI_M1P_30	-1
#define	BPI_M1P_32	GPIO_PB12
#define	BPI_M1P_34	-1
#define	BPI_M1P_36	GPIO_PI21
#define	BPI_M1P_38	GPIO_PI20
#define	BPI_M1P_40	GPIO_PB13

//map wpi gpio_num(index) to bp bpio_num(element)
int pinToGpio_BPI_M1P [64] =
{
   BPI_M1P_11, BPI_M1P_12,        //0, 1
   BPI_M1P_13, BPI_M1P_15,        //2, 3
   BPI_M1P_16, BPI_M1P_18,        //4, 5
   BPI_M1P_22, BPI_M1P_07,        //6, 7
   BPI_M1P_03, BPI_M1P_05,        //8, 9
   BPI_M1P_24, BPI_M1P_26,        //10, 11
   BPI_M1P_19, BPI_M1P_21,        //12, 13
   BPI_M1P_23, BPI_M1P_08,        //14, 15
   BPI_M1P_10,        -1,        //16, 17
          -1,        -1,        //18, 19
          -1, BPI_M1P_29,        //20, 21
   BPI_M1P_31, BPI_M1P_33,        //22, 23
   BPI_M1P_35, BPI_M1P_37,        //24, 25
   BPI_M1P_32, BPI_M1P_36,        //26, 27
   BPI_M1P_38, BPI_M1P_40,        //28. 29
   BPI_M1P_27, BPI_M1P_28,        //30, 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map bcm gpio_num(index) to bp gpio_num(element)
int pinTobcm_BPI_M1P [64] =
{
  BPI_M1P_27, BPI_M1P_28,  //0, 1
  BPI_M1P_03, BPI_M1P_05,  //2, 3
  BPI_M1P_07, BPI_M1P_29,  //4, 5
  BPI_M1P_31, BPI_M1P_26,  //6, 7
  BPI_M1P_24, BPI_M1P_21,  //8, 9
  BPI_M1P_19, BPI_M1P_23,  //10, 11
  BPI_M1P_32, BPI_M1P_33,  //12, 13
  BPI_M1P_08, BPI_M1P_10,  //14, 15
  BPI_M1P_36, BPI_M1P_11,  //16, 17
  BPI_M1P_12, BPI_M1P_35,	 //18, 19
  BPI_M1P_38, BPI_M1P_40,  //20, 21
  BPI_M1P_15, BPI_M1P_16,  //22, 23
  BPI_M1P_18, BPI_M1P_22,  //24, 25
  BPI_M1P_37, BPI_M1P_13,  //26, 27
  -1, -1,
  -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map phys_num(index) to bp gpio_num(element)
int physToGpio_BPI_M1P [64] =
{
          -1,                //0
          -1,        -1,     //1, 2
   BPI_M1P_03,        -1,     //3, 4
   BPI_M1P_05,        -1,     //5, 6
   BPI_M1P_07, BPI_M1P_08,     //7, 8
          -1, BPI_M1P_10,     //9, 10
   BPI_M1P_11, BPI_M1P_12,     //11, 12
   BPI_M1P_13,        -1,     //13, 14
   BPI_M1P_15, BPI_M1P_16,     //15, 16
          -1, BPI_M1P_18,     //17, 18
   BPI_M1P_19,        -1,     //19, 20
   BPI_M1P_21, BPI_M1P_22,     //21, 22
   BPI_M1P_23, BPI_M1P_24,     //23, 24
          -1, BPI_M1P_26,     //25, 26
   BPI_M1P_27, BPI_M1P_28,     //27, 28
   BPI_M1P_29,        -1,     //29, 30
   BPI_M1P_31, BPI_M1P_32,     //31, 32      
   BPI_M1P_33,        -1,     //33, 34
   BPI_M1P_35, BPI_M1P_36,     //35, 36
   BPI_M1P_37, BPI_M1P_38,     //37, 38
          -1, BPI_M1P_40,     //39, 40
   -1,   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //41-> 55
   -1,   -1, -1, -1, -1, -1, -1, -1 // 56-> 63
} ;

#define M1P_I2C_DEV             "/dev/i2c-2"
#define M1P_SPI_DEV             "/dev/spidev0.0"

#define M1P_I2C_OFFSET  2
#define M1P_SPI_OFFSET  2
#define M1P_PWM_OFFSET  2
