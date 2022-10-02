#define	BPI_M2U_01	-1
#define	BPI_M2U_03	GPIO_PB21
#define	BPI_M2U_05	GPIO_PB20
#define	BPI_M2U_07	GPIO_PB03
#define	BPI_M2U_09	-1
#define	BPI_M2U_11	GPIO_PI20
#define	BPI_M2U_13	GPIO_PI21
#define	BPI_M2U_15	GPIO_PH25
#define	BPI_M2U_17	-1
#define	BPI_M2U_19	GPIO_PC00
#define	BPI_M2U_21	GPIO_PC01
#define	BPI_M2U_23	GPIO_PC02
#define	BPI_M2U_25	-1
#define	BPI_M2U_27	GPIO_PI01
#define	BPI_M2U_29	GPIO_PH00
#define	BPI_M2U_31	GPIO_PH01
#define	BPI_M2U_33	GPIO_PH02
#define	BPI_M2U_35	GPIO_PH03
#define	BPI_M2U_37	GPIO_PH04
#define	BPI_M2U_39	-1

#define	BPI_M2U_02	-1
#define	BPI_M2U_04	-1
#define	BPI_M2U_06	-1
#define	BPI_M2U_08	GPIO_PI18
#define	BPI_M2U_10	GPIO_PI19
#define	BPI_M2U_12	GPIO_PI17
#define	BPI_M2U_14	-1
#define	BPI_M2U_16	GPIO_PI16
#define	BPI_M2U_18	GPIO_PH26
#define	BPI_M2U_20	-1
#define	BPI_M2U_22	GPIO_PH27
#define	BPI_M2U_24	GPIO_PC23
#define	BPI_M2U_26	GPIO_PH24
#define	BPI_M2U_28	GPIO_PI00
#define	BPI_M2U_30	-1
#define	BPI_M2U_32	GPIO_PD20
#define	BPI_M2U_34	-1
#define	BPI_M2U_36	GPIO_PH07
#define	BPI_M2U_38	GPIO_PH06
#define	BPI_M2U_40	GPIO_PH05

//map wpi gpio_num(index) to bp bpio_num(element)
int pinToGpio_BPI_M2U [64] =
{
   BPI_M2U_11, BPI_M2U_12,        //0, 1
   BPI_M2U_13, BPI_M2U_15,        //2, 3
   BPI_M2U_16, BPI_M2U_18,        //4, 5
   BPI_M2U_22, BPI_M2U_07,        //6, 7
   BPI_M2U_03, BPI_M2U_05,        //8, 9
   BPI_M2U_24, BPI_M2U_26,        //10, 11
   BPI_M2U_19, BPI_M2U_21,        //12, 13
   BPI_M2U_23, BPI_M2U_08,        //14, 15
   BPI_M2U_10,        -1,        //16, 17
          -1,        -1,        //18, 19
          -1, BPI_M2U_29,        //20, 21
   BPI_M2U_31, BPI_M2U_33,        //22, 23
   BPI_M2U_35, BPI_M2U_37,        //24, 25
   BPI_M2U_32, BPI_M2U_36,        //26, 27
   BPI_M2U_38, BPI_M2U_40,        //28. 29
   BPI_M2U_27, BPI_M2U_28,        //30, 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map bcm gpio_num(index) to bp gpio_num(element)
int pinTobcm_BPI_M2U [64] =
{
  BPI_M2U_27, BPI_M2U_28,  //0, 1
  BPI_M2U_03, BPI_M2U_05,  //2, 3
  BPI_M2U_07, BPI_M2U_29,  //4, 5
  BPI_M2U_31, BPI_M2U_26,  //6, 7
  BPI_M2U_24, BPI_M2U_21,  //8, 9
  BPI_M2U_19, BPI_M2U_23,  //10, 11
  BPI_M2U_32, BPI_M2U_33,  //12, 13
  BPI_M2U_08, BPI_M2U_10,  //14, 15
  BPI_M2U_36, BPI_M2U_11,  //16, 17
  BPI_M2U_12, BPI_M2U_35,	 //18, 19
  BPI_M2U_38, BPI_M2U_40,  //20, 21
  BPI_M2U_15, BPI_M2U_16,  //22, 23
  BPI_M2U_18, BPI_M2U_22,  //24, 25
  BPI_M2U_37, BPI_M2U_13,  //26, 27
  -1, -1,
  -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map phys_num(index) to bp gpio_num(element)
int physToGpio_BPI_M2U [64] =
{
          -1,                //0
          -1,        -1,     //1, 2
   BPI_M2U_03,        -1,     //3, 4
   BPI_M2U_05,        -1,     //5, 6
   BPI_M2U_07, BPI_M2U_08,     //7, 8
          -1, BPI_M2U_10,     //9, 10
   BPI_M2U_11, BPI_M2U_12,     //11, 12
   BPI_M2U_13,        -1,     //13, 14
   BPI_M2U_15, BPI_M2U_16,     //15, 16
          -1, BPI_M2U_18,     //17, 18
   BPI_M2U_19,        -1,     //19, 20
   BPI_M2U_21, BPI_M2U_22,     //21, 22
   BPI_M2U_23, BPI_M2U_24,     //23, 24
          -1, BPI_M2U_26,     //25, 26
   BPI_M2U_27, BPI_M2U_28,     //27, 28
   BPI_M2U_29,        -1,     //29, 30
   BPI_M2U_31, BPI_M2U_32,     //31, 32      
   BPI_M2U_33,        -1,     //33, 34
   BPI_M2U_35, BPI_M2U_36,     //35, 36
   BPI_M2U_37, BPI_M2U_38,     //37, 38
          -1, BPI_M2U_40,     //39, 40
   -1,   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //41-> 55
   -1,   -1, -1, -1, -1, -1, -1, -1 // 56-> 63
} ;

#define M2U_I2C_DEV             "/dev/i2c-2"
#define M2U_SPI_DEV             "/dev/spidev0.0"

#define M2U_I2C_OFFSET  2
#define M2U_SPI_OFFSET  3
#define M2U_PWM_OFFSET  3
