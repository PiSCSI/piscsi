#define	BPI_M2_01	-1
#define	BPI_M2_03	GPIO_PH19
#define	BPI_M2_05	GPIO_PH18
#define	BPI_M2_07	GPIO_PH09
#define	BPI_M2_09	-1
#define	BPI_M2_11	GPIO_PG07
#define	BPI_M2_13	GPIO_PG06
#define	BPI_M2_15	GPIO_PG09
#define	BPI_M2_17	-1
#define	BPI_M2_19	GPIO_PG15
#define	BPI_M2_21	GPIO_PG16
#define	BPI_M2_23	GPIO_PG14
#define	BPI_M2_25	-1
#define	BPI_M2_27	GPIO_PB06
#define	BPI_M2_29	GPIO_PB00
#define	BPI_M2_31	GPIO_PB01
#define	BPI_M2_33	GPIO_PB02
#define	BPI_M2_35	GPIO_PB03
#define	BPI_M2_37	GPIO_PB04
#define	BPI_M2_39	-1

#define	BPI_M2_02	-1
#define	BPI_M2_04	-1
#define	BPI_M2_06	-1
#define	BPI_M2_08	GPIO_PE04
#define	BPI_M2_10	GPIO_PE05
#define	BPI_M2_12	GPIO_PH10
#define	BPI_M2_14	-1
#define	BPI_M2_16	GPIO_PH11
#define	BPI_M2_18	GPIO_PH12
#define	BPI_M2_20	-1
#define	BPI_M2_22	GPIO_PG08
#define	BPI_M2_24	GPIO_PG13
#define	BPI_M2_26	GPIO_PG12
#define	BPI_M2_28	GPIO_PB05
#define	BPI_M2_30	-1
#define	BPI_M2_32	GPIO_PB07
#define	BPI_M2_34	-1
#define	BPI_M2_36	GPIO_PE06
#define	BPI_M2_38	GPIO_PE07
#define	BPI_M2_40	GPIO_PM02

//map wpi gpio_num(index) to bp bpio_num(element)
int pinToGpio_BPI_M2 [64] =
{
   BPI_M2_11, BPI_M2_12,        //0, 1
   BPI_M2_13, BPI_M2_15,        //2, 3
   BPI_M2_16, BPI_M2_18,        //4, 5
   BPI_M2_22, BPI_M2_07,        //6, 7
   BPI_M2_03, BPI_M2_05,        //8, 9
   BPI_M2_24, BPI_M2_26,        //10, 11
   BPI_M2_19, BPI_M2_21,        //12, 13
   BPI_M2_23, BPI_M2_08,        //14, 15
   BPI_M2_10,        -1,        //16, 17
          -1,        -1,        //18, 19
          -1, BPI_M2_29,        //20, 21
   BPI_M2_31, BPI_M2_33,        //22, 23
   BPI_M2_35, BPI_M2_37,        //24, 25
   BPI_M2_32, BPI_M2_36,        //26, 27
   BPI_M2_38, BPI_M2_40,        //28. 29
   BPI_M2_27, BPI_M2_28,        //30, 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map bcm gpio_num(index) to bp gpio_num(element)
int pinTobcm_BPI_M2 [64] =
{
  BPI_M2_27, BPI_M2_28,  //0, 1
  BPI_M2_03, BPI_M2_05,  //2, 3
  BPI_M2_07, BPI_M2_29,  //4, 5
  BPI_M2_31, BPI_M2_26,  //6, 7
  BPI_M2_24, BPI_M2_21,  //8, 9
  BPI_M2_19, BPI_M2_23,  //10, 11
  BPI_M2_32, BPI_M2_33,  //12, 13
  BPI_M2_08, BPI_M2_10,  //14, 15
  BPI_M2_36, BPI_M2_11,  //16, 17
  BPI_M2_12, BPI_M2_35,	 //18, 19
  BPI_M2_38, BPI_M2_40,  //20, 21
  BPI_M2_15, BPI_M2_16,  //22, 23
  BPI_M2_18, BPI_M2_22,  //24, 25
  BPI_M2_37, BPI_M2_13,  //26, 27
  -1, -1,
  -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // ... 63
} ;

//map phys_num(index) to bp gpio_num(element)
int physToGpio_BPI_M2 [64] =
{
          -1,                //0
          -1,        -1,     //1, 2
   BPI_M2_03,        -1,     //3, 4
   BPI_M2_05,        -1,     //5, 6
   BPI_M2_07, BPI_M2_08,     //7, 8
          -1, BPI_M2_10,     //9, 10
   BPI_M2_11, BPI_M2_12,     //11, 12
   BPI_M2_13,        -1,     //13, 14
   BPI_M2_15, BPI_M2_16,     //15, 16
          -1, BPI_M2_18,     //17, 18
   BPI_M2_19,        -1,     //19, 20
   BPI_M2_21, BPI_M2_22,     //21, 22
   BPI_M2_23, BPI_M2_24,     //23, 24
          -1, BPI_M2_26,     //25, 26
   BPI_M2_27, BPI_M2_28,     //27, 28
   BPI_M2_29,        -1,     //29, 30
   BPI_M2_31, BPI_M2_32,     //31, 32      
   BPI_M2_33,        -1,     //33, 34
   BPI_M2_35, BPI_M2_36,     //35, 36
   BPI_M2_37, BPI_M2_38,     //37, 38
          -1, BPI_M2_40,     //39, 40
   -1,   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //41-> 55
   -1,   -1, -1, -1, -1, -1, -1, -1 // 56-> 63
} ;

#define M2_I2C_DEV             "/dev/i2c-2"
#define M2_SPI_DEV             "/dev/spidev1.0"

#define M2_I2C_OFFSET  2
#define M2_SPI_OFFSET  2
#define M2_PWM_OFFSET  4
