#define	BPI_M2P_01	-1
#define	BPI_M2P_03	GPIO_PA12
#define	BPI_M2P_05	GPIO_PA11
#define	BPI_M2P_07	GPIO_PA06
#define	BPI_M2P_09	-1
#define	BPI_M2P_11	GPIO_PA01
#define	BPI_M2P_13	GPIO_PA00
#define	BPI_M2P_15	GPIO_PA03
#define	BPI_M2P_17	-1
#define	BPI_M2P_19	GPIO_PC00
#define	BPI_M2P_21	GPIO_PC01
#define	BPI_M2P_23	GPIO_PC02
#define	BPI_M2P_25	-1
#define	BPI_M2P_27	GPIO_PA19
#define	BPI_M2P_29	GPIO_PA07
#define	BPI_M2P_31	GPIO_PA08
#define	BPI_M2P_33	GPIO_PA09
#define	BPI_M2P_35	GPIO_PA10
#define	BPI_M2P_37	GPIO_PA17
#define	BPI_M2P_39	-1

#define	BPI_M2P_02	-1
#define	BPI_M2P_04	-1
#define	BPI_M2P_06	-1
#define	BPI_M2P_08	GPIO_PA13
#define	BPI_M2P_10	GPIO_PA14
#define	BPI_M2P_12	GPIO_PA16
#define	BPI_M2P_14	-1
#define	BPI_M2P_16	GPIO_PA15
#define	BPI_M2P_18	GPIO_PC04
#define	BPI_M2P_20	-1
#define	BPI_M2P_22	GPIO_PA02
#define	BPI_M2P_24	GPIO_PC03
#define	BPI_M2P_26	GPIO_PC07
#define	BPI_M2P_28	GPIO_PA18
#define	BPI_M2P_30	-1
#define	BPI_M2P_32	GPIO_PL02
#define	BPI_M2P_34	-1
#define	BPI_M2P_36	GPIO_PL04
#define	BPI_M2P_38	GPIO_PA21
#define	BPI_M2P_40	GPIO_PA20

//map wpi gpio_num(index) to bp bpio_num(element)
int pinToGpio_BPI_M2P [64] =
{
   BPI_M2P_11, BPI_M2P_12,        //0, 1
   BPI_M2P_13, BPI_M2P_15,        //2, 3
   BPI_M2P_16, BPI_M2P_18,        //4, 5
   BPI_M2P_22, BPI_M2P_07,        //6, 7
   BPI_M2P_03, BPI_M2P_05,        //8, 9
   BPI_M2P_24, BPI_M2P_26,        //10, 11
   BPI_M2P_19, BPI_M2P_21,        //12, 13
   BPI_M2P_23, BPI_M2P_08,        //14, 15
   BPI_M2P_10,        -1,        //16, 17
          -1,        -1,        //18, 19
          -1, BPI_M2P_29,        //20, 21
   BPI_M2P_31, BPI_M2P_33,        //22, 23
   BPI_M2P_35, BPI_M2P_37,        //24, 25
   BPI_M2P_32, BPI_M2P_36,        //26, 27
   BPI_M2P_38, BPI_M2P_40,        //28. 29
   BPI_M2P_27, BPI_M2P_28,        //30, 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map bcm gpio_num(index) to bp gpio_num(element)
int pinTobcm_BPI_M2P [64] =
{
  BPI_M2P_27, BPI_M2P_28,  //0, 1
  BPI_M2P_03, BPI_M2P_05,  //2, 3
  BPI_M2P_07, BPI_M2P_29,  //4, 5
  BPI_M2P_31, BPI_M2P_26,  //6, 7
  BPI_M2P_24, BPI_M2P_21,  //8, 9
  BPI_M2P_19, BPI_M2P_23,  //10, 11
  BPI_M2P_32, BPI_M2P_33,  //12, 13
  BPI_M2P_08, BPI_M2P_10,  //14, 15
  BPI_M2P_36, BPI_M2P_11,  //16, 17
  BPI_M2P_12, BPI_M2P_35,	 //18, 19
  BPI_M2P_38, BPI_M2P_40,  //20, 21
  BPI_M2P_15, BPI_M2P_16,  //22, 23
  BPI_M2P_18, BPI_M2P_22,  //24, 25
  BPI_M2P_37, BPI_M2P_13,  //26, 27
  -1, -1,
  -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map phys_num(index) to bp gpio_num(element)
int physToGpio_BPI_M2P [64] =
{
          -1,                //0
          -1,        -1,     //1, 2
   BPI_M2P_03,        -1,     //3, 4
   BPI_M2P_05,        -1,     //5, 6
   BPI_M2P_07, BPI_M2P_08,     //7, 8
          -1, BPI_M2P_10,     //9, 10
   BPI_M2P_11, BPI_M2P_12,     //11, 12
   BPI_M2P_13,        -1,     //13, 14
   BPI_M2P_15, BPI_M2P_16,     //15, 16
          -1, BPI_M2P_18,     //17, 18
   BPI_M2P_19,        -1,     //19, 20
   BPI_M2P_21, BPI_M2P_22,     //21, 22
   BPI_M2P_23, BPI_M2P_24,     //23, 24
          -1, BPI_M2P_26,     //25, 26
   BPI_M2P_27, BPI_M2P_28,     //27, 28
   BPI_M2P_29,        -1,     //29, 30
   BPI_M2P_31, BPI_M2P_32,     //31, 32      
   BPI_M2P_33,        -1,     //33, 34
   BPI_M2P_35, BPI_M2P_36,     //35, 36
   BPI_M2P_37, BPI_M2P_38,     //37, 38
          -1, BPI_M2P_40,     //39, 40
   -1,   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //41-> 55
   -1,   -1, -1, -1, -1, -1, -1, -1 // 56-> 63
} ;

#define M2P_I2C_DEV		"/dev/i2c-0"
#define M2P_SPI_DEV		"/dev/spidev0.0"

#define M2P_I2C_OFFSET  2
#define M2P_SPI_OFFSET  3
#define M2P_PWM_OFFSET  3
