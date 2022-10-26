#define	BPI_M64_01	-1
#define	BPI_M64_03	GPIO_PH03
#define	BPI_M64_05	GPIO_PH02
#define	BPI_M64_07	GPIO_PH06
#define	BPI_M64_09	-1
#define	BPI_M64_11	GPIO_PH07
#define	BPI_M64_13	GPIO_PH10
#define	BPI_M64_15	GPIO_PH11
#define	BPI_M64_17	-1
#define	BPI_M64_19	GPIO_PD02
#define	BPI_M64_21	GPIO_PD03
#define	BPI_M64_23	GPIO_PD01
#define	BPI_M64_25	-1
#define	BPI_M64_27	GPIO_PC04
#define	BPI_M64_29	GPIO_PC07
#define	BPI_M64_31	GPIO_PB05
#define	BPI_M64_33	GPIO_PB04
#define	BPI_M64_35	GPIO_PB06
#define	BPI_M64_37	GPIO_PL12
#define	BPI_M64_39	-1

#define	BPI_M64_02	-1
#define	BPI_M64_04	-1
#define	BPI_M64_06	-1
#define	BPI_M64_08	GPIO_PB00
#define	BPI_M64_10	GPIO_PB01
#define	BPI_M64_12	GPIO_PB03
#define	BPI_M64_14	-1
#define	BPI_M64_16	GPIO_PB02
#define	BPI_M64_18	GPIO_PD04
#define	BPI_M64_20	-1
#define	BPI_M64_22	GPIO_PC00
#define	BPI_M64_24	GPIO_PD00
#define	BPI_M64_26	GPIO_PC02
#define	BPI_M64_28	GPIO_PC03
#define	BPI_M64_30	-1
#define	BPI_M64_32	GPIO_PB07
#define	BPI_M64_34	-1
#define	BPI_M64_36	GPIO_PL09
#define	BPI_M64_38	GPIO_PL07
#define	BPI_M64_40	GPIO_PL08

//map wpi gpio_num(index) to bp bpio_num(element)
int pinToGpio_BPI_M64 [64] =
{
   BPI_M64_11, BPI_M64_12,        //0, 1
   BPI_M64_13, BPI_M64_15,        //2, 3
   BPI_M64_16, BPI_M64_18,        //4, 5
   BPI_M64_22, BPI_M64_07,        //6, 7
   BPI_M64_03, BPI_M64_05,        //8, 9
   BPI_M64_24, BPI_M64_26,        //10, 11
   BPI_M64_19, BPI_M64_21,        //12, 13
   BPI_M64_23, BPI_M64_08,        //14, 15
   BPI_M64_10,        -1,        //16, 17
          -1,        -1,        //18, 19
          -1, BPI_M64_29,        //20, 21
   BPI_M64_31, BPI_M64_33,        //22, 23
   BPI_M64_35, BPI_M64_37,        //24, 25
   BPI_M64_32, BPI_M64_36,        //26, 27
   BPI_M64_38, BPI_M64_40,        //28. 29
   BPI_M64_27, BPI_M64_28,        //30, 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map bcm gpio_num(index) to bp gpio_num(element)
int pinTobcm_BPI_M64 [64] =
{
  BPI_M64_27, BPI_M64_28,  //0, 1
  BPI_M64_03, BPI_M64_05,  //2, 3
  BPI_M64_07, BPI_M64_29,  //4, 5
  BPI_M64_31, BPI_M64_26,  //6, 7
  BPI_M64_24, BPI_M64_21,  //8, 9
  BPI_M64_19, BPI_M64_23,  //10, 11
  BPI_M64_32, BPI_M64_33,  //12, 13
  BPI_M64_08, BPI_M64_10,  //14, 15
  BPI_M64_36, BPI_M64_11,  //16, 17
  BPI_M64_12, BPI_M64_35,	 //18, 19
  BPI_M64_38, BPI_M64_40,  //20, 21
  BPI_M64_15, BPI_M64_16,  //22, 23
  BPI_M64_18, BPI_M64_22,  //24, 25
  BPI_M64_37, BPI_M64_13,  //26, 27
  -1, -1,
  -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map phys_num(index) to bp gpio_num(element)
int physToGpio_BPI_M64 [64] =
{
          -1,                //0
          -1,        -1,     //1, 2
   BPI_M64_03,        -1,     //3, 4
   BPI_M64_05,        -1,     //5, 6
   BPI_M64_07, BPI_M64_08,     //7, 8
          -1, BPI_M64_10,     //9, 10
   BPI_M64_11, BPI_M64_12,     //11, 12
   BPI_M64_13,        -1,     //13, 14
   BPI_M64_15, BPI_M64_16,     //15, 16
          -1, BPI_M64_18,     //17, 18
   BPI_M64_19,        -1,     //19, 20
   BPI_M64_21, BPI_M64_22,     //21, 22
   BPI_M64_23, BPI_M64_24,     //23, 24
          -1, BPI_M64_26,     //25, 26
   BPI_M64_27, BPI_M64_28,     //27, 28
   BPI_M64_29,        -1,     //29, 30
   BPI_M64_31, BPI_M64_32,     //31, 32      
   BPI_M64_33,        -1,     //33, 34
   BPI_M64_35, BPI_M64_36,     //35, 36
   BPI_M64_37, BPI_M64_38,     //37, 38
          -1, BPI_M64_40,     //39, 40
   -1,   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //41-> 55
   -1,   -1, -1, -1, -1, -1, -1, -1 // 56-> 63
} ;

#define M64_I2C_DEV		"/dev/i2c-1"
#define M64_SPI_DEV		"/dev/spidev1.0"

#define M64_I2C_OFFSET  2
#define M64_SPI_OFFSET  4
#define M64_PWM_OFFSET  -1

